/*!
 *****************************************************************************
 @file:    DifferentialPulseVoltammetry.c
 @author:  $Author: Your Name $
 @brief:   Differential Pulse Voltammetry measurement sequences.
 @date:    $Date: 2023-12-07 $
 ******************************************************************************/

#include "ad5940.h"
#include <stdio.h>
#include "string.h"
#include "math.h"
#include "DifferentialPulseVoltammetry.h"

/**
 * DPV application configuration - based on SWV
 */



static AppDPVCfg_Type AppDPVCfg = 
{
    .bParaChanged = bFALSE,
    .SeqStartAddr = 0,
    .MaxSeqLen = 0,
    .SeqStartAddrCal = 0,
    .MaxSeqLenCal = 0,
    
    .LFOSCClkFreq = 32000.0,
    .SysClkFreq = 16000000.0,
    .AdcClkFreq = 16000000.0,
    .RcalVal = 10000.0,
    .ADCRefVolt = 1820.0f,
    
    /* DPV Signal Parameters */
    .RampStartVolt = -200.0f,       /* -200mV */
    .RampPeakVolt = 600.0f,         /* +600mV */
    .VzeroStart = 2200.0f,          /* 2.2V */
    .VzeroPeak = 400.0f,            /* 0.4V */
    .StepIncrement = 5.0f,          /* 5mV steps */
    .PulseAmplitude = 50.0f,        /* 50mV pulse */
    .PulsePeriod = 200.0f,          /* 200ms period */
    .PulseWidth = 50.0f,            /* 50ms pulse width */
    .SampleDelay1 = 10.0f,          /* 10ms before pulse */
    .SampleDelay2 = 10.0f,          /* 10ms after pulse */
    .StepNumber = 160,              /* (600-(-200))/5 = 160 steps */
    
    /* Hardware config */
    .LPTIARtiaSel = LPTIARTIA_20K,
    .ExternalRtiaValue = 20000.0f,
    .AdcPgaGain = ADCPGA_1,
    .ADCSinc3Osr = ADCSINC3OSR_4,
    .FifoThresh = 4,
    
    /* Private variables */
    .DPVInited = bFALSE,
    .StopRequired = bFALSE,
    .DPVState = DPV_STATE0,
    .bFirstDACSeq = bTRUE,
    .bInPulse = bFALSE,
    .bSampleAfterPulse = bFALSE,
    .CurrStepPos = 0,
};

AD5940Err AppDPVGetCfg(void *pCfg)
{
    if(pCfg)
    {
        *(AppDPVCfg_Type**)pCfg = &AppDPVCfg;
        return AD5940ERR_OK;
    }
    return AD5940ERR_PARA;
}

AD5940Err AppDPVCtrl(uint32_t Command, void *pPara)
{
    switch (Command)
    {
        case APPCTRL_START:
        {
            WUPTCfg_Type wupt_cfg;
            
            if(AD5940_WakeUp(10) > 10)
                return AD5940ERR_WAKEUP;
            if(AppDPVCfg.DPVInited == bFALSE)
                return AD5940ERR_APPERROR;
            if(AppDPVCfg.DPVState == DPV_STOP)
                return AD5940ERR_APPERROR;
            
            /* DPV timing: Base -> Sample1 -> Pulse -> Sample2 -> Next step */
            wupt_cfg.WuptEn = bTRUE;
            wupt_cfg.WuptEndSeq = WUPTENDSEQ_D;
            wupt_cfg.WuptOrder[0] = SEQID_0;    /* DAC update (base) */
            wupt_cfg.WuptOrder[1] = SEQID_2;    /* ADC sample before pulse */
            wupt_cfg.WuptOrder[2] = SEQID_1;    /* DAC update (pulse) */
            wupt_cfg.WuptOrder[3] = SEQID_2;    /* ADC sample after pulse */
            
            /* Timing setup for DPV */
            wupt_cfg.SeqxSleepTime[SEQID_0] = 1;
            wupt_cfg.SeqxWakeupTime[SEQID_0] = (uint32_t)(AppDPVCfg.LFOSCClkFreq * AppDPVCfg.SampleDelay1 / 1000.0f) - 1;
            
            wupt_cfg.SeqxSleepTime[SEQID_1] = 1;
            wupt_cfg.SeqxWakeupTime[SEQID_1] = (uint32_t)(AppDPVCfg.LFOSCClkFreq * AppDPVCfg.PulseWidth / 1000.0f) - 1;
            
            wupt_cfg.SeqxSleepTime[SEQID_2] = 1;
            wupt_cfg.SeqxWakeupTime[SEQID_2] = (uint32_t)(AppDPVCfg.LFOSCClkFreq * AppDPVCfg.SampleDelay2 / 1000.0f) - 1;
            
            AD5940_WUPTCfg(&wupt_cfg);
            break;
        }
        case APPCTRL_STOPNOW:
        {
            if(AD5940_WakeUp(10) > 10)
                return AD5940ERR_WAKEUP;
            AD5940_WUPTCtrl(bFALSE);
            AD5940_WUPTCtrl(bFALSE);
            break;
        }
        case APPCTRL_STOPSYNC:
        {
            AppDPVCfg.StopRequired = bTRUE;
            break;
        }
        case APPCTRL_SHUTDOWN:
        {
            AppDPVCtrl(APPCTRL_STOPNOW, 0);
            AD5940_ShutDownS();
            break;
        }
        default:
            break;
    }
    return AD5940ERR_OK;
}

/**
 * Generate initialization sequence - COPY từ SWV
 */
static AD5940Err AppDPVSeqInitGen(void)
{
    AD5940Err error = AD5940ERR_OK;
    const uint32_t *pSeqCmd;
    uint32_t SeqLen;
    AFERefCfg_Type aferef_cfg;
    LPLoopCfg_Type lploop_cfg;
    DSPCfg_Type dsp_cfg;
    
    /* Start sequence generator */
    AD5940_SEQGenCtrl(bTRUE);
    
    AD5940_AFECtrlS(AFECTRL_ALL, bFALSE);
    
    /* Configure AFE Reference */
    aferef_cfg.HpBandgapEn = bTRUE;
    aferef_cfg.Hp1V1BuffEn = bTRUE;
    aferef_cfg.Hp1V8BuffEn = bTRUE;
    aferef_cfg.Disc1V1Cap = bFALSE;
    aferef_cfg.Disc1V8Cap = bFALSE;
    aferef_cfg.Hp1V8ThemBuff = bFALSE;
    aferef_cfg.Hp1V8Ilimit = bFALSE;
    aferef_cfg.Lp1V1BuffEn = bFALSE;
    aferef_cfg.Lp1V8BuffEn = bFALSE;
    aferef_cfg.LpBandgapEn = bTRUE;
    aferef_cfg.LpRefBufEn = bTRUE;
    aferef_cfg.LpRefBoostEn = bFALSE;
    AD5940_REFCfgS(&aferef_cfg);
    
    /* Configure LP Loop */
    lploop_cfg.LpAmpCfg.LpAmpSel = LPAMP0;
    lploop_cfg.LpAmpCfg.LpAmpPwrMod = LPAMPPWR_BOOST3;
    lploop_cfg.LpAmpCfg.LpPaPwrEn = bTRUE;
    lploop_cfg.LpAmpCfg.LpTiaPwrEn = bTRUE;
    lploop_cfg.LpAmpCfg.LpTiaRf = LPTIARF_20K;
    lploop_cfg.LpAmpCfg.LpTiaRload = LPTIARLOAD_SHORT;
    lploop_cfg.LpAmpCfg.LpTiaRtia = AppDPVCfg.LPTIARtiaSel;
    
    if(AppDPVCfg.LPTIARtiaSel == LPTIARTIA_OPEN)
        lploop_cfg.LpAmpCfg.LpTiaSW = LPTIASW(13)|LPTIASW(2)|LPTIASW(4)|LPTIASW(5)|LPTIASW(9);
    else
        lploop_cfg.LpAmpCfg.LpTiaSW = LPTIASW(2)|LPTIASW(4);
    
    lploop_cfg.LpDacCfg.LpdacSel = LPDAC0;
    lploop_cfg.LpDacCfg.DacData6Bit = (uint32_t)((AppDPVCfg.VzeroStart - 200.0f)/DAC6BITVOLT_1LSB);
    lploop_cfg.LpDacCfg.DacData12Bit = (int32_t)((AppDPVCfg.RampStartVolt)/DAC12BITVOLT_1LSB) + lploop_cfg.LpDacCfg.DacData6Bit*64;
    lploop_cfg.LpDacCfg.DataRst = bFALSE;
    lploop_cfg.LpDacCfg.LpDacSW = LPDACSW_VBIAS2LPPA|LPDACSW_VZERO2LPTIA;
    lploop_cfg.LpDacCfg.LpDacRef = LPDACREF_2P5;
    lploop_cfg.LpDacCfg.LpDacSrc = LPDACSRC_MMR;
    lploop_cfg.LpDacCfg.LpDacVbiasMux = LPDACVBIAS_12BIT;
    lploop_cfg.LpDacCfg.LpDacVzeroMux = LPDACVZERO_6BIT;
    lploop_cfg.LpDacCfg.PowerEn = bTRUE;
    AD5940_LPLoopCfgS(&lploop_cfg);
    
    /* Configure DSP */
    AD5940_StructInit(&dsp_cfg, sizeof(dsp_cfg));
    dsp_cfg.ADCBaseCfg.ADCMuxN = ADCMUXN_LPTIA0_N;
    dsp_cfg.ADCBaseCfg.ADCMuxP = ADCMUXP_LPTIA0_P;
    dsp_cfg.ADCBaseCfg.ADCPga = AppDPVCfg.AdcPgaGain;
    
    dsp_cfg.ADCFilterCfg.ADCSinc3Osr = AppDPVCfg.ADCSinc3Osr;
    dsp_cfg.ADCFilterCfg.ADCRate = ADCRATE_800KHZ;
    dsp_cfg.ADCFilterCfg.BpSinc3 = bFALSE;
    dsp_cfg.ADCFilterCfg.Sinc2NotchEnable = bTRUE;
    dsp_cfg.ADCFilterCfg.BpNotch = bTRUE;
    dsp_cfg.ADCFilterCfg.ADCSinc2Osr = ADCSINC2OSR_1067;
    dsp_cfg.ADCFilterCfg.ADCAvgNum = ADCAVGNUM_2;
    AD5940_DSPCfgS(&dsp_cfg);
    
    /* Sequence end */
    AD5940_SEQGenInsert(SEQ_STOP());
    
    /* Stop sequence generator */
    AD5940_SEQGenCtrl(bFALSE);
    
    error = AD5940_SEQGenFetchSeq(&pSeqCmd, &SeqLen);
    if(error == AD5940ERR_OK)
    {
        AD5940_StructInit(&AppDPVCfg.InitSeqInfo, sizeof(AppDPVCfg.InitSeqInfo));
        if(SeqLen >= AppDPVCfg.MaxSeqLen)
            return AD5940ERR_SEQLEN;
        
        AppDPVCfg.InitSeqInfo.SeqId = SEQID_3;
        AppDPVCfg.InitSeqInfo.SeqRamAddr = AppDPVCfg.SeqStartAddr;
        AppDPVCfg.InitSeqInfo.pSeqCmd = pSeqCmd;
        AppDPVCfg.InitSeqInfo.SeqLen = SeqLen;
        AppDPVCfg.InitSeqInfo.WriteSRAM = bTRUE;
        AD5940_SEQInfoCfg(&AppDPVCfg.InitSeqInfo);
    }
    else
        return error;
    return AD5940ERR_OK;
}

/**
 * Generate ADC control sequence - COPY từ SWV
 */
static AD5940Err AppDPVSeqADCCtrlGen(void)
{
    AD5940Err error = AD5940ERR_OK;
    const uint32_t *pSeqCmd;
    uint32_t SeqLen;
    uint32_t WaitClks;
    ClksCalInfo_Type clks_cal;
    
    clks_cal.DataCount = 1;
    clks_cal.DataType = DATATYPE_SINC3;
    clks_cal.ADCSinc3Osr = AppDPVCfg.ADCSinc3Osr;
    clks_cal.ADCSinc2Osr = ADCSINC2OSR_1067;
    clks_cal.ADCAvgNum = ADCAVGNUM_2;
    clks_cal.RatioSys2AdcClk = AppDPVCfg.SysClkFreq/AppDPVCfg.AdcClkFreq;
    AD5940_ClksCalculate(&clks_cal, &WaitClks);
    
    AD5940_SEQGenCtrl(bTRUE);
    AD5940_SEQGpioCtrlS(AGPIO_Pin2);
    AD5940_AFECtrlS(AFECTRL_ADCPWR, bTRUE);
    AD5940_SEQGenInsert(SEQ_WAIT(16*100));
    AD5940_AFECtrlS(AFECTRL_ADCCNV, bTRUE);
    AD5940_SEQGenInsert(SEQ_WAIT(WaitClks));
    AD5940_AFECtrlS(AFECTRL_ADCPWR|AFECTRL_ADCCNV, bFALSE);
    AD5940_SEQGpioCtrlS(0);
    AD5940_EnterSleepS();
    
    error = AD5940_SEQGenFetchSeq(&pSeqCmd, &SeqLen);
    AD5940_SEQGenCtrl(bFALSE);
    
    if(error == AD5940ERR_OK)
    {
        AD5940_StructInit(&AppDPVCfg.ADCSeqInfo, sizeof(AppDPVCfg.ADCSeqInfo));
        if((SeqLen + AppDPVCfg.InitSeqInfo.SeqLen) >= AppDPVCfg.MaxSeqLen)
            return AD5940ERR_SEQLEN;
        
        AppDPVCfg.ADCSeqInfo.SeqId = SEQID_2;
        AppDPVCfg.ADCSeqInfo.SeqRamAddr = AppDPVCfg.InitSeqInfo.SeqRamAddr + AppDPVCfg.InitSeqInfo.SeqLen;
        AppDPVCfg.ADCSeqInfo.pSeqCmd = pSeqCmd;
        AppDPVCfg.ADCSeqInfo.SeqLen = SeqLen;
        AppDPVCfg.ADCSeqInfo.WriteSRAM = bTRUE;
        AD5940_SEQInfoCfg(&AppDPVCfg.ADCSeqInfo);
    }
    else
        return error;
    return AD5940ERR_OK;
}

/**
 * DPV DAC update logic - KHÁC HOÀN TOÀN SWV
 */
static AD5940Err DPVDacRegUpdate(uint32_t *pDACData)
{
    uint32_t VbiasCode, VzeroCode;
    
    switch(AppDPVCfg.DPVState)
    {
        case DPV_STATE0: /* Initialization */
            AppDPVCfg.CurrVzeroCode = (uint32_t)((AppDPVCfg.VzeroStart - 200.0f)/DAC6BITVOLT_1LSB);
            AppDPVCfg.CurrRampCode = AppDPVCfg.RampStartVolt/DAC12BITVOLT_1LSB;
            AppDPVCfg.DACCodePerStep = AppDPVCfg.PulseAmplitude/DAC12BITVOLT_1LSB;
            AppDPVCfg.DACCodePerRamp = AppDPVCfg.StepIncrement/DAC12BITVOLT_1LSB;
            AppDPVCfg.DPVState = DPV_STATE1;
            AppDPVCfg.bInPulse = bFALSE;
            AppDPVCfg.CurrStepPos = 0;
            break;
            
        case DPV_STATE1: /* Base potential - prepare for pulse */
            if(AppDPVCfg.bInPulse == bFALSE) {
                AppDPVCfg.DPVState = DPV_STATE2;
                AppDPVCfg.bInPulse = bTRUE;
            }
            break;
            
        case DPV_STATE2: /* Pulse potential - in pulse */
            if(AppDPVCfg.bInPulse == bTRUE) {
                AppDPVCfg.bInPulse = bFALSE;
                AppDPVCfg.CurrStepPos++;
                AppDPVCfg.CurrRampCode += AppDPVCfg.DACCodePerRamp;
                
                if(AppDPVCfg.CurrStepPos >= AppDPVCfg.StepNumber) {
                    AppDPVCfg.DPVState = DPV_STOP;
                } else {
                    AppDPVCfg.DPVState = DPV_STATE1;
                }
            }
            break;
            
        case DPV_STOP:
            break;
    }
    
    VzeroCode = AppDPVCfg.CurrVzeroCode;
    
    if(AppDPVCfg.bInPulse) {
        /* During pulse: Base + Pulse amplitude */
        VbiasCode = (uint32_t)(VzeroCode*64 + AppDPVCfg.CurrRampCode + AppDPVCfg.DACCodePerStep);
    } else {
        /* Base potential only */
        VbiasCode = (uint32_t)(VzeroCode*64 + AppDPVCfg.CurrRampCode);
    }
    
    /* Boundary check */
    if(VbiasCode > 4095) VbiasCode = 4095;
    if(VzeroCode > 63) VzeroCode = 63;
    
    *pDACData = (VzeroCode<<12)|VbiasCode;
    return AD5940ERR_OK;
}

/**
 * DPV DAC Sequence Generation - MODIFIED từ SWV
 */
/**
* @brief Update DAC sequence in SRAM for DPV pattern  
* @details DPV khác SWV: Base → Pulse → Base → Next Step (thay vì sóng vuông liên tục)
* @return return error code
**/
static AD5940Err AppDPVSeqDACCtrlGen(void)
{  
#define DPV_SEQLEN_ONESTEP    4L  /* Commands needed to update LPDAC for DPV */
#define DPV_CURRBLK_BLK0      0   /* Current block is BLOCK0 */
#define DPV_CURRBLK_BLK1      1   /* Current block is BLOCK1 */
  
  AD5940Err error = AD5940ERR_OK;
  uint32_t BlockStartSRAMAddr;
  uint32_t DACData, SRAMAddr;
  uint32_t i;
  uint32_t StepsThisBlock;
  BoolFlag bIsFinalBlk;
  uint32_t SeqCmdBuff[DPV_SEQLEN_ONESTEP];
  
  /* Static variables for DPV sequence management */
  static BoolFlag bCmdForSeq0 = bTRUE;
  static uint32_t DPVSeqBlk0Addr, DPVSeqBlk1Addr;
  static uint32_t StepsRemainning, StepsPerBlock, DPVSeqCurrBlk;
  
  /* Calculate total steps for DPV */
  AppDPVCfg.StepNumber = (uint32_t)((AppDPVCfg.RampPeakVolt - AppDPVCfg.RampStartVolt)/AppDPVCfg.StepIncrement);
  
  if(AppDPVCfg.StepNumber > 1020)
  {
    printf("Error: DPV Steps exceed limit (1020)\n");
    return AD5940ERR_PARA;
  }
  
  /* Initialize DPV sequence parameters */
  if(AppDPVCfg.bFirstDACSeq == bTRUE)
  {
    int32_t DACSeqLenMax;
    StepsRemainning = AppDPVCfg.StepNumber;
    DACSeqLenMax = (int32_t)AppDPVCfg.MaxSeqLen - (int32_t)AppDPVCfg.InitSeqInfo.SeqLen - (int32_t)AppDPVCfg.ADCSeqInfo.SeqLen;
    
    if(DACSeqLenMax < DPV_SEQLEN_ONESTEP*4)
      return AD5940ERR_SEQLEN;
    
    DACSeqLenMax -= DPV_SEQLEN_ONESTEP*2;  /* Reserve commands each block */
    StepsPerBlock = DACSeqLenMax/DPV_SEQLEN_ONESTEP/2;
    DPVSeqBlk0Addr = AppDPVCfg.ADCSeqInfo.SeqRamAddr + AppDPVCfg.ADCSeqInfo.SeqLen;
    DPVSeqBlk1Addr = DPVSeqBlk0Addr + StepsPerBlock*DPV_SEQLEN_ONESTEP;
    DPVSeqCurrBlk = DPV_CURRBLK_BLK0;
    
    /* DPV specific DAC code calculations */
    AppDPVCfg.DACCodePerStep = AppDPVCfg.PulseAmplitude/DAC12BITVOLT_1LSB;  /* Pulse amplitude */
    AppDPVCfg.DACCodePerRamp = AppDPVCfg.StepIncrement/DAC12BITVOLT_1LSB;   /* Step increment */
    
    AppDPVCfg.CurrRampCode = AppDPVCfg.RampStartVolt/DAC12BITVOLT_1LSB;
    AppDPVCfg.DPVState = DPV_STATE0;
    AppDPVCfg.CurrStepPos = 0;
    AppDPVCfg.bInPulse = bFALSE;
    
    bCmdForSeq0 = bTRUE;
  }
  
  if(StepsRemainning == 0) return AD5940ERR_OK;
  
  bIsFinalBlk = StepsRemainning <= StepsPerBlock ? bTRUE : bFALSE;
  if(bIsFinalBlk)
    StepsThisBlock = StepsRemainning;
  else
    StepsThisBlock = StepsPerBlock;
  
  StepsRemainning -= StepsThisBlock;
  
  BlockStartSRAMAddr = (DPVSeqCurrBlk == DPV_CURRBLK_BLK0) ? DPVSeqBlk0Addr : DPVSeqBlk1Addr;
  SRAMAddr = BlockStartSRAMAddr;
  
  /* Generate DPV sequences for this block */
  for(i = 0; i < StepsThisBlock - 1; i++)
  {
    uint32_t CurrAddr = SRAMAddr;
    SRAMAddr += DPV_SEQLEN_ONESTEP;
    
    /* DPV logic: Update DAC based on current state */
    DPVDacRegUpdate(&DACData);
    
    SeqCmdBuff[0] = SEQ_WR(REG_AFE_LPDACDAT0, DACData);
    SeqCmdBuff[1] = SEQ_WAIT(10); /* LPDAC needs 10 clocks to update */
    SeqCmdBuff[2] = SEQ_WR(bCmdForSeq0 ? REG_AFE_SEQ1INFO : REG_AFE_SEQ0INFO,
      (SRAMAddr << BITP_AFE_SEQ1INFO_ADDR) | (DPV_SEQLEN_ONESTEP << BITP_AFE_SEQ1INFO_LEN));
    SeqCmdBuff[3] = SEQ_SLP();
    
    AD5940_SEQCmdWrite(CurrAddr, SeqCmdBuff, DPV_SEQLEN_ONESTEP);
    bCmdForSeq0 = bCmdForSeq0 ? bFALSE : bTRUE;
  }
  
  /* Handle final sequence */
  if(bIsFinalBlk) /* Final block */
  {
    uint32_t CurrAddr = SRAMAddr;
    SRAMAddr += DPV_SEQLEN_ONESTEP;
    
    DPVDacRegUpdate(&DACData);
    SeqCmdBuff[0] = SEQ_WR(REG_AFE_LPDACDAT0, DACData);
    SeqCmdBuff[1] = SEQ_WAIT(10);
    SeqCmdBuff[2] = SEQ_WR(bCmdForSeq0 ? REG_AFE_SEQ1INFO : REG_AFE_SEQ0INFO,
      (SRAMAddr << BITP_AFE_SEQ1INFO_ADDR) | (DPV_SEQLEN_ONESTEP << BITP_AFE_SEQ1INFO_LEN));
    SeqCmdBuff[3] = SEQ_SLP();
    AD5940_SEQCmdWrite(CurrAddr, SeqCmdBuff, DPV_SEQLEN_ONESTEP);
    
    CurrAddr += DPV_SEQLEN_ONESTEP;
    /* Final command to stop sequencer */
    SeqCmdBuff[0] = SEQ_NOP();
    SeqCmdBuff[1] = SEQ_NOP();
    SeqCmdBuff[2] = SEQ_NOP();
    SeqCmdBuff[3] = SEQ_STOP();
    AD5940_SEQCmdWrite(CurrAddr, SeqCmdBuff, DPV_SEQLEN_ONESTEP);
  }
  else /* Not final block */
  {
    uint32_t CurrAddr = SRAMAddr;
    SRAMAddr = (DPVSeqCurrBlk == DPV_CURRBLK_BLK0) ? DPVSeqBlk1Addr : DPVSeqBlk0Addr;
    
    DPVDacRegUpdate(&DACData);
    SeqCmdBuff[0] = SEQ_WR(REG_AFE_LPDACDAT0, DACData);
    SeqCmdBuff[1] = SEQ_WAIT(10);
    SeqCmdBuff[2] = SEQ_WR(bCmdForSeq0 ? REG_AFE_SEQ1INFO : REG_AFE_SEQ0INFO,
      (SRAMAddr << BITP_AFE_SEQ1INFO_ADDR) | (DPV_SEQLEN_ONESTEP << BITP_AFE_SEQ1INFO_LEN));
    SeqCmdBuff[3] = SEQ_INT0(); /* Generate interrupt for next block */
    AD5940_SEQCmdWrite(CurrAddr, SeqCmdBuff, DPV_SEQLEN_ONESTEP);
    
    bCmdForSeq0 = bCmdForSeq0 ? bFALSE : bTRUE;
  }
  
  /* Switch blocks */
  DPVSeqCurrBlk = (DPVSeqCurrBlk == DPV_CURRBLK_BLK0) ? DPV_CURRBLK_BLK1 : DPV_CURRBLK_BLK0;
  
  /* Handle first sequence setup */
  if(AppDPVCfg.bFirstDACSeq)
  {
    AppDPVCfg.bFirstDACSeq = bFALSE;
    
    if(bIsFinalBlk == bFALSE)
    {
      error = AppDPVSeqDACCtrlGen(); /* Generate next block */
      if(error != AD5940ERR_OK) return error;
    }
    
    /* Configure first DAC sequence */
    AppDPVCfg.DACSeqInfo.SeqId = SEQID_0;
    AppDPVCfg.DACSeqInfo.SeqLen = DPV_SEQLEN_ONESTEP;
    AppDPVCfg.DACSeqInfo.SeqRamAddr = BlockStartSRAMAddr;
    AppDPVCfg.DACSeqInfo.WriteSRAM = bFALSE;
    AD5940_SEQInfoCfg(&AppDPVCfg.DACSeqInfo);
  }
  
  return AD5940ERR_OK;
}

/**
 * RTIA Calibration - COPY từ SWV
 */
static AD5940Err AppDPVRtiaCal(void)
{
    fImpPol_Type RtiaCalValue;
    LPRTIACal_Type lprtia_cal;
    AD5940_StructInit(&lprtia_cal, sizeof(lprtia_cal));
    
    lprtia_cal.LpAmpSel = LPAMP0;
    lprtia_cal.bPolarResult = bTRUE;
    lprtia_cal.AdcClkFreq = AppDPVCfg.AdcClkFreq;
    lprtia_cal.SysClkFreq = AppDPVCfg.SysClkFreq;
    lprtia_cal.ADCSinc3Osr = ADCSINC3OSR_4;
    lprtia_cal.ADCSinc2Osr = ADCSINC2OSR_22;
    lprtia_cal.DftCfg.DftNum = DFTNUM_2048;
    lprtia_cal.DftCfg.DftSrc = DFTSRC_SINC2NOTCH;
    lprtia_cal.DftCfg.HanWinEn = bTRUE;
    lprtia_cal.fFreq = AppDPVCfg.AdcClkFreq/4/22/2048*3;
    lprtia_cal.fRcal = AppDPVCfg.RcalVal;
    lprtia_cal.LpTiaRtia = AppDPVCfg.LPTIARtiaSel;
    lprtia_cal.LpAmpPwrMod = LPAMPPWR_NORM;
    lprtia_cal.bWithCtia = bFALSE;
    AD5940_LPRtiaCal(&lprtia_cal, &RtiaCalValue);
    AppDPVCfg.RtiaValue = RtiaCalValue;
    return AD5940ERR_OK;
}

/**
 * AppDPVInit - COPY và MODIFY từ AppSWVInit
 */
AD5940Err AppDPVInit(uint32_t *pBuffer, uint32_t BufferSize)
{
    AD5940Err error = AD5940ERR_OK;
    FIFOCfg_Type fifo_cfg;
    SEQCfg_Type seq_cfg;
    
    if(AD5940_WakeUp(10) > 10)
        return AD5940ERR_WAKEUP;
    
    /* Configure sequencer */
    seq_cfg.SeqMemSize = SEQMEMSIZE_2KB;
    seq_cfg.SeqBreakEn = bFALSE;
    seq_cfg.SeqIgnoreEn = bFALSE;
    seq_cfg.SeqCntCRCClr = bTRUE;
    seq_cfg.SeqEnable = bFALSE;
    seq_cfg.SeqWrTimer = 0;
    AD5940_SEQCfg(&seq_cfg);
    
    /* Initialize sequencer generator */
    if((AppDPVCfg.DPVInited == bFALSE) || (AppDPVCfg.bParaChanged == bTRUE))
    {
        if(pBuffer == 0) return AD5940ERR_PARA;
        if(BufferSize == 0) return AD5940ERR_PARA;
        
        if(AppDPVCfg.LPTIARtiaSel == LPTIARTIA_OPEN)
        {
            AppDPVCfg.RtiaValue.Magnitude = AppDPVCfg.ExternalRtiaValue;
            AppDPVCfg.RtiaValue.Phase = 0;
        }
        else
            AppDPVRtiaCal();
        
        AppDPVCfg.DPVInited = bFALSE;
        AD5940_SEQGenInit(pBuffer, BufferSize);
        
        /* Generate sequences */
        error = AppDPVSeqInitGen();
        if(error != AD5940ERR_OK) return error;
        
        error = AppDPVSeqADCCtrlGen();
        if(error != AD5940ERR_OK) return error;
        
        AppDPVCfg.bParaChanged = bFALSE;
    }
    
    /* Configure FIFO */
    AD5940_FIFOCtrlS(FIFOSRC_SINC3, bFALSE);
    fifo_cfg.FIFOEn = bTRUE;
    fifo_cfg.FIFOSrc = FIFOSRC_SINC3;
    fifo_cfg.FIFOThresh = AppDPVCfg.FifoThresh;
    fifo_cfg.FIFOMode = FIFOMODE_FIFO;
    fifo_cfg.FIFOSize = FIFOSIZE_4KB;
    AD5940_FIFOCfg(&fifo_cfg);
    
    /* Clear interrupts */
    AD5940_INTCClrFlag(AFEINTSRC_ALLINT);
    
    /* Generate DAC sequence */
    AppDPVCfg.bFirstDACSeq = bTRUE;
    error = AppDPVSeqDACCtrlGen();
    if(error != AD5940ERR_OK) return error;
    
    /* Configure and run initialization sequence */
    AppDPVCfg.InitSeqInfo.WriteSRAM = bFALSE;
    AD5940_SEQInfoCfg(&AppDPVCfg.InitSeqInfo);
    
    AD5940_SEQCtrlS(bTRUE);
    AD5940_SEQMmrTrig(AppDPVCfg.InitSeqInfo.SeqId);
    while(AD5940_INTCTestFlag(AFEINTC_1, AFEINTSRC_ENDSEQ) == bFALSE);
    AD5940_INTCClrFlag(AFEINTSRC_ENDSEQ);
    
    AppDPVCfg.ADCSeqInfo.WriteSRAM = bFALSE;
    AD5940_SEQInfoCfg(&AppDPVCfg.ADCSeqInfo);
    
    AppDPVCfg.DACSeqInfo.WriteSRAM = bFALSE;
    AD5940_SEQInfoCfg(&AppDPVCfg.DACSeqInfo);
    
    AD5940_SEQCtrlS(bFALSE);
    AD5940_WriteReg(REG_AFE_SEQCNT, 0);
    AD5940_SEQCtrlS(bTRUE);
    AD5940_ClrMCUIntFlag();
    
    AD5940_AFEPwrBW(AFEPWR_LP, AFEBW_250KHZ);
    
    AppDPVCfg.DPVInited = bTRUE;
    return AD5940ERR_OK;
}

/**
 * Register modification in ISR - COPY từ SWV
 */
static int32_t AppDPVRegModify(int32_t * const pData, uint32_t *pDataCount)
{
    if(AppDPVCfg.StopRequired == bTRUE)
    {
        AD5940_WUPTCtrl(bFALSE);
        return AD5940ERR_OK;
    }
    return AD5940ERR_OK;
}

/**
 * DPV Data processing - KHÁC SWV (differential current)
 */
static int32_t AppDPVDataProcess(int32_t * const pData, uint32_t *pDataCount)
{
    uint32_t i, datacount;
    datacount = *pDataCount;
    float *pOut = (float *)pData;
    static float I_before = 0.0f;
    static BoolFlag bExpectingAfter = bFALSE;
    
    for(i = 0; i < datacount; i++)
    {
        pData[i] &= 0xffff;
        float current = AD5940_ADCCode2Volt(pData[i], AppDPVCfg.AdcPgaGain, AppDPVCfg.ADCRefVolt) / AppDPVCfg.RtiaValue.Magnitude * 1e3f;  /* μA */
        
        if(bExpectingAfter == bFALSE) {
            /* This is I_before (before pulse) */
            I_before = current;
            bExpectingAfter = bTRUE;
            pOut[i] = 0.0f; /* No differential current yet */
        } else {
            /* This is I_after (after pulse) */
            pOut[i] = current - I_before; /* Differential current */
            bExpectingAfter = bFALSE;
        }
    }
    return 0;
}

/**
 * DPV ISR - COPY và MODIFY từ AppSWVISR
 */
AD5940Err AppDPVISR(void *pBuff, uint32_t *pCount)
{
    uint32_t BuffCount;
    uint32_t FifoCnt;
    BuffCount = *pCount;
    uint32_t IntFlag;
    
    if(AD5940_WakeUp(10) > 10)
        return AD5940ERR_WAKEUP;
    AD5940_SleepKeyCtrlS(SLPKEY_LOCK);
    *pCount = 0;
    
    IntFlag = AD5940_INTCGetFlag(AFEINTC_0);
    
    if(IntFlag & AFEINTSRC_CUSTOMINT0)
    {
        AD5940Err error;
        AD5940_INTCClrFlag(AFEINTSRC_CUSTOMINT0);
        error = AppDPVSeqDACCtrlGen();
        if(error != AD5940ERR_OK) return error;
    }
    
    if(IntFlag & AFEINTSRC_DATAFIFOTHRESH)
    {
        FifoCnt = AD5940_FIFOGetCnt();
        
        if(FifoCnt > BuffCount)
        {
            /* Buffer limited */
        }
        AD5940_FIFORd((uint32_t *)pBuff, FifoCnt);
        AD5940_INTCClrFlag(AFEINTSRC_DATAFIFOTHRESH);
        AppDPVRegModify(pBuff, &FifoCnt);
        
        /* Process data with differential calculation */
        AppDPVDataProcess((int32_t*)pBuff, &FifoCnt);
        *pCount = FifoCnt;
        return 0;
    }
    
    if(IntFlag & AFEINTSRC_ENDSEQ)
    {
        FifoCnt = AD5940_FIFOGetCnt();
        AD5940_INTCClrFlag(AFEINTSRC_ENDSEQ);
        AD5940_FIFORd((uint32_t *)pBuff, FifoCnt);
        
        /* Process data */
        AppDPVDataProcess((int32_t*)pBuff, &FifoCnt);
        *pCount = FifoCnt;
        AppDPVCtrl(APPCTRL_STOPNOW, 0);
    }
    
    return 0;
}