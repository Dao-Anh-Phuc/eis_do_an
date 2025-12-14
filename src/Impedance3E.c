/*!
 *****************************************************************************
 @file:    Impedance.c
 @author:  Neo Xu
 @brief:   Electrochemical impedance spectroscopy based on example AD5940_Impedance
 -----------------------------------------------------------------------------

Copyright (c) 2017-2019 Analog Devices, Inc. All Rights Reserved.

This software is proprietary to Analog Devices, Inc. and its licensors.
By using this software you agree to the terms of the associated
Analog Devices Software License Agreement.
 
*****************************************************************************/
#include "AD5940.H"
#include <stdio.h>
#include "string.h"
#include "math.h"
#include "Impedance3E.h"

/* Default LPDAC resolution(2.5V internal reference). */
#define DAC12BITVOLT_1LSB   (2200.0f/4095)  //mV
#define DAC6BITVOLT_1LSB    (DAC12BITVOLT_1LSB*64)  //mV

/* 
  Application configuration structure. Specified by user from template.
  The variables are usable in this whole application.
  It includes basic configuration for sequencer generator and application related parameters
*/
AppIMP3ECfg_Type AppIMP3ECfg = 
{
  .bParaChanged = bFALSE,
  .SeqStartAddr = 0,
  .MaxSeqLen = 0,
  
  .SeqStartAddrCal = 0,
  .MaxSeqLenCal = 0,

  .ImpODR = 20.0,           /* 20.0 Hz*/
  .NumOfData = -1,
  .SysClkFreq = 16000000.0,
  .WuptClkFreq = 32000.0,
  .AdcClkFreq = 16000000.0,
  .RcalVal = 10000.0,

  .DswitchSel = SWD_CE0,
  .PswitchSel = SWP_CE0,
  .NswitchSel = SWN_AIN1,
  .TswitchSel = SWT_AIN1,

  .PwrMod = AFEPWR_LP,

  .LptiaRtiaSel = LPTIARTIA_4K, /* COnfigure RTIA */
  .LpTiaRf = LPTIARF_1M,        /* Configure LPF resistor */
  .LpTiaRl = LPTIARLOAD_100R,
	
  .HstiaRtiaSel = HSTIARTIA_1K,
  .ExcitBufGain = EXCITBUFGAIN_0P25,
  .HsDacGain = HSDACGAIN_0P2,
  .HsDacUpdateRate = 0x1B,
  .DacVoltPP = 300.0,
  .BiasVolt = -0.0f,

  .SinFreq = 100000.0, /* 1000Hz */

  .DftNum = DFTNUM_16384,
  .DftSrc = DFTSRC_SINC3,
  .HanWinEn = bTRUE,

  .AdcPgaGain = ADCPGA_1,
  .ADCSinc3Osr = ADCSINC3OSR_2,
  .ADCSinc2Osr = ADCSINC2OSR_22,

  .ADCAvgNum = ADCAVGNUM_16,

  .SweepCfg.SweepEn = bTRUE,
  .SweepCfg.SweepStart = 1000,
  .SweepCfg.SweepStop = 100000.0,
  .SweepCfg.SweepPoints = 101,
  .SweepCfg.SweepLog = bFALSE,
  .SweepCfg.SweepIndex = 0,

  .FifoThresh = 4,
  .IMP3EInited = bFALSE,
  .StopRequired = bFALSE,
};

/**
   This function is provided for upper controllers that want to change 
   application parameters specially for user defined parameters.
*/
int32_t AppIMP3EGetCfg(void *pCfg)
{
  if(pCfg)
  {
    *(AppIMP3ECfg_Type**)pCfg = &AppIMP3ECfg;
    return AD5940ERR_OK;
  }
  return AD5940ERR_PARA;
}

int32_t AppIMP3ECtrl(uint32_t Command, void *pPara)
{
  
  switch (Command)
  {
    case IMP3ECTRL_START:
    {
      WUPTCfg_Type wupt_cfg;

      if(AD5940_WakeUp(10) > 10)  /* Wakeup AFE by read register, read 10 times at most */
        return AD5940ERR_WAKEUP;  /* Wakeup Failed */
      if(AppIMP3ECfg.IMP3EInited == bFALSE)
        return AD5940ERR_APPERROR;
      /* Start it */
      wupt_cfg.WuptEn = bTRUE;
      wupt_cfg.WuptEndSeq = WUPTENDSEQ_A;
      wupt_cfg.WuptOrder[0] = SEQID_0;
      wupt_cfg.SeqxSleepTime[SEQID_0] = 4;
      wupt_cfg.SeqxWakeupTime[SEQID_0] = (uint32_t)(AppIMP3ECfg.WuptClkFreq/AppIMP3ECfg.ImpODR)-4;
      AD5940_WUPTCfg(&wupt_cfg);
      
      AppIMP3ECfg.FifoDataCount = 0;  /* restart */
      break;
    }
    case IMP3ECTRL_STOPNOW:
    {
      if(AD5940_WakeUp(10) > 10)  /* Wakeup AFE by read register, read 10 times at most */
        return AD5940ERR_WAKEUP;  /* Wakeup Failed */
      /* Start Wupt right now */
      AD5940_WUPTCtrl(bFALSE);
      AD5940_WUPTCtrl(bFALSE);
      break;
    }
    case IMP3ECTRL_STOPSYNC:
    {
      AppIMP3ECfg.StopRequired = bTRUE;
      break;
    }
    case IMP3ECTRL_GETFREQ:
      {
        if(pPara == 0)
          return AD5940ERR_PARA;
        if(AppIMP3ECfg.SweepCfg.SweepEn == bTRUE)
          *(float*)pPara = AppIMP3ECfg.FreqofData;
        else
          *(float*)pPara = AppIMP3ECfg.SinFreq;
      }
    break;
    case IMP3ECTRL_SHUTDOWN:
    {
      AppIMP3ECtrl(IMP3ECTRL_STOPNOW, 0);  /* Stop the measurement if it's running. */
      /* Turn off LPloop related blocks which are not controlled automatically by hibernate operation */
      AFERefCfg_Type aferef_cfg;
      LPLoopCfg_Type lploop_cfg;
      memset(&aferef_cfg, 0, sizeof(aferef_cfg));
      AD5940_REFCfgS(&aferef_cfg);
      memset(&lploop_cfg, 0, sizeof(lploop_cfg));
      AD5940_LPLoopCfgS(&lploop_cfg);
      AD5940_EnterSleepS();  /* Enter Hibernate */
    }
    break;
    default:
    break;
  }
  return AD5940ERR_OK;
}

/* generated code snnipet */
float AppIMP3EGetCurrFreq(void)
{
  if(AppIMP3ECfg.SweepCfg.SweepEn == bTRUE)
    return AppIMP3ECfg.FreqofData;
  else
    return AppIMP3ECfg.SinFreq;
}
///////////////////////////////////////////lllllll
/* Application initialization */
static AD5940Err AppIMP3ESeqCfgGen(void)
{
  AD5940Err error = AD5940ERR_OK;
  const uint32_t *pSeqCmd;
  uint32_t SeqLen;
  AFERefCfg_Type aferef_cfg;
  HSLoopCfg_Type HsLoopCfg;
	LPLoopCfg_Type lploop_cfg;
  DSPCfg_Type dsp_cfg;
  float sin_freq;

  /* Start sequence generator here */
  AD5940_SEQGenCtrl(bTRUE);
  
  AD5940_AFECtrlS(AFECTRL_ALL, bFALSE);  /* Init all to disable state */

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
	
	lploop_cfg.LpDacCfg.LpDacSrc = LPDACSRC_MMR;
  lploop_cfg.LpDacCfg.LpDacSW = LPDACSW_VBIAS2LPPA|LPDACSW_VBIAS2PIN|LPDACSW_VZERO2LPTIA|LPDACSW_VZERO2PIN;
  lploop_cfg.LpDacCfg.LpDacVzeroMux = LPDACVZERO_6BIT;
  lploop_cfg.LpDacCfg.LpDacVbiasMux = LPDACVBIAS_12BIT;
  lploop_cfg.LpDacCfg.LpDacRef = LPDACREF_2P5;
  lploop_cfg.LpDacCfg.DataRst = bFALSE;
  lploop_cfg.LpDacCfg.PowerEn = bTRUE;
  lploop_cfg.LpDacCfg.DacData6Bit = (uint32_t)((AppIMP3ECfg.Vzero-200)/DAC6BITVOLT_1LSB);
	lploop_cfg.LpDacCfg.DacData12Bit =(int32_t)((AppIMP3ECfg.BiasVolt)/DAC12BITVOLT_1LSB) + lploop_cfg.LpDacCfg.DacData6Bit*64;
	if(lploop_cfg.LpDacCfg.DacData12Bit>lploop_cfg.LpDacCfg.DacData6Bit*64)
		lploop_cfg.LpDacCfg.DacData12Bit--;
  lploop_cfg.LpAmpCfg.LpAmpPwrMod = LPAMPPWR_NORM;
  lploop_cfg.LpAmpCfg.LpPaPwrEn = bTRUE;
  lploop_cfg.LpAmpCfg.LpTiaPwrEn = bTRUE;
  lploop_cfg.LpAmpCfg.LpTiaRf = AppIMP3ECfg.LpTiaRf;
  lploop_cfg.LpAmpCfg.LpTiaRload = AppIMP3ECfg.LpTiaRl;
  lploop_cfg.LpAmpCfg.LpTiaRtia = AppIMP3ECfg.LptiaRtiaSel;
  lploop_cfg.LpAmpCfg.LpTiaSW = LPTIASW(5)|LPTIASW(2)|LPTIASW(4)|LPTIASW(12)|LPTIASW(13); 
  
  AD5940_LPLoopCfgS(&lploop_cfg);
	
  HsLoopCfg.HsDacCfg.ExcitBufGain = AppIMP3ECfg.ExcitBufGain;
  HsLoopCfg.HsDacCfg.HsDacGain = AppIMP3ECfg.HsDacGain;
  HsLoopCfg.HsDacCfg.HsDacUpdateRate = AppIMP3ECfg.HsDacUpdateRate;

  HsLoopCfg.HsTiaCfg.DiodeClose = bFALSE;
  HsLoopCfg.HsTiaCfg.HstiaBias = HSTIABIAS_1P1;
  HsLoopCfg.HsTiaCfg.HstiaCtia = 31; /* 31pF + 2pF */
  HsLoopCfg.HsTiaCfg.HstiaDeRload = HSTIADERLOAD_OPEN;
  HsLoopCfg.HsTiaCfg.HstiaDeRtia = HSTIADERTIA_OPEN;
  HsLoopCfg.HsTiaCfg.HstiaRtiaSel = AppIMP3ECfg.HstiaRtiaSel;

  HsLoopCfg.SWMatCfg.Dswitch = AppIMP3ECfg.DswitchSel;
  HsLoopCfg.SWMatCfg.Pswitch = AppIMP3ECfg.PswitchSel;
  HsLoopCfg.SWMatCfg.Nswitch = AppIMP3ECfg.NswitchSel;
  HsLoopCfg.SWMatCfg.Tswitch = SWT_TRTIA|AppIMP3ECfg.TswitchSel;

  HsLoopCfg.WgCfg.WgType = WGTYPE_SIN;
  HsLoopCfg.WgCfg.GainCalEn = bTRUE;
  HsLoopCfg.WgCfg.OffsetCalEn = bTRUE;
  if(AppIMP3ECfg.SweepCfg.SweepEn == bTRUE)
  {
    AppIMP3ECfg.FreqofData = AppIMP3ECfg.SweepCfg.SweepStart;
    AppIMP3ECfg.SweepCurrFreq = AppIMP3ECfg.SweepCfg.SweepStart;
    AD5940_SweepNext(&AppIMP3ECfg.SweepCfg, &AppIMP3ECfg.SweepNextFreq);
    sin_freq = AppIMP3ECfg.SweepCurrFreq;
  }
  else
  {
    sin_freq = AppIMP3ECfg.SinFreq;
    AppIMP3ECfg.FreqofData = sin_freq;
  }
  HsLoopCfg.WgCfg.SinCfg.SinFreqWord = AD5940_WGFreqWordCal(sin_freq, AppIMP3ECfg.SysClkFreq);
	HsLoopCfg.WgCfg.SinCfg.SinAmplitudeWord = (uint32_t)(AppIMP3ECfg.DacVoltPP/800.0f*2047 + 0.5f);
  HsLoopCfg.WgCfg.SinCfg.SinOffsetWord = 0;
  HsLoopCfg.WgCfg.SinCfg.SinPhaseWord = 0;
  AD5940_HSLoopCfgS(&HsLoopCfg);

  dsp_cfg.ADCBaseCfg.ADCMuxN = ADCMUXN_HSTIA_N;
  dsp_cfg.ADCBaseCfg.ADCMuxP = ADCMUXP_HSTIA_P;
  dsp_cfg.ADCBaseCfg.ADCPga = AppIMP3ECfg.AdcPgaGain;
  
  memset(&dsp_cfg.ADCDigCompCfg, 0, sizeof(dsp_cfg.ADCDigCompCfg));
  
  dsp_cfg.ADCFilterCfg.ADCAvgNum = AppIMP3ECfg.ADCAvgNum;
  dsp_cfg.ADCFilterCfg.ADCRate = ADCRATE_800KHZ;	/* Tell filter block clock rate of ADC*/
  dsp_cfg.ADCFilterCfg.ADCSinc2Osr = AppIMP3ECfg.ADCSinc2Osr;
  dsp_cfg.ADCFilterCfg.ADCSinc3Osr = AppIMP3ECfg.ADCSinc3Osr;
  dsp_cfg.ADCFilterCfg.BpNotch = bTRUE;
  dsp_cfg.ADCFilterCfg.BpSinc3 = bFALSE;
  dsp_cfg.ADCFilterCfg.Sinc2NotchEnable = bTRUE;
  dsp_cfg.DftCfg.DftNum = AppIMP3ECfg.DftNum;
  dsp_cfg.DftCfg.DftSrc = AppIMP3ECfg.DftSrc;
  dsp_cfg.DftCfg.HanWinEn = AppIMP3ECfg.HanWinEn;
  
  memset(&dsp_cfg.StatCfg, 0, sizeof(dsp_cfg.StatCfg));
  AD5940_DSPCfgS(&dsp_cfg);
    
  /* Enable all of them. They are automatically turned off during hibernate mode to save power */
  if(AppIMP3ECfg.BiasVolt == 0.0f)
    AD5940_AFECtrlS(AFECTRL_HSTIAPWR|AFECTRL_INAMPPWR|AFECTRL_EXTBUFPWR|\
                AFECTRL_WG|AFECTRL_DACREFPWR|AFECTRL_HSDACPWR|\
                AFECTRL_SINC2NOTCH, bTRUE);
  else
    AD5940_AFECtrlS(AFECTRL_HSTIAPWR|AFECTRL_INAMPPWR|AFECTRL_EXTBUFPWR|\
                AFECTRL_WG|AFECTRL_DACREFPWR|AFECTRL_HSDACPWR|\
                AFECTRL_SINC2NOTCH|AFECTRL_DCBUFPWR, bTRUE);
    /* Sequence end. */
  AD5940_SEQGenInsert(SEQ_STOP()); /* Add one extra command to disable sequencer for initialization sequence because we only want it to run one time. */

  /* Stop here */
  error = AD5940_SEQGenFetchSeq(&pSeqCmd, &SeqLen);
  AD5940_SEQGenCtrl(bFALSE); /* Stop sequencer generator */
  if(error == AD5940ERR_OK)
  {
    AppIMP3ECfg.InitSeqInfo.SeqId = SEQID_1;
    AppIMP3ECfg.InitSeqInfo.SeqRamAddr = AppIMP3ECfg.SeqStartAddr;
    AppIMP3ECfg.InitSeqInfo.pSeqCmd = pSeqCmd;
    AppIMP3ECfg.InitSeqInfo.SeqLen = SeqLen;
    /* Write command to SRAM */
    AD5940_SEQCmdWrite(AppIMP3ECfg.InitSeqInfo.SeqRamAddr, pSeqCmd, SeqLen);
  }
  else
    return error; /* Error */
  return AD5940ERR_OK;
}


static AD5940Err AppIMP3ESeqMeasureGen(void)
{
  AD5940Err error = AD5940ERR_OK;
  const uint32_t *pSeqCmd;
  uint32_t SeqLen;
  
  uint32_t WaitClks;
  SWMatrixCfg_Type sw_cfg;
  ClksCalInfo_Type clks_cal;
  LPAmpCfg_Type LpAmpCfg;
	
  /* Calculate number of clocks to get data to FIFO */
  clks_cal.DataType = DATATYPE_DFT;
  clks_cal.DftSrc = AppIMP3ECfg.DftSrc;
  clks_cal.DataCount = 1L<<(AppIMP3ECfg.DftNum+2); /* 2^(DFTNUMBER+2) */
  clks_cal.ADCSinc2Osr = AppIMP3ECfg.ADCSinc2Osr;
  clks_cal.ADCSinc3Osr = AppIMP3ECfg.ADCSinc3Osr;
  clks_cal.ADCAvgNum = AppIMP3ECfg.ADCAvgNum;
  clks_cal.RatioSys2AdcClk = AppIMP3ECfg.SysClkFreq/AppIMP3ECfg.AdcClkFreq;
  AD5940_ClksCalculate(&clks_cal, &WaitClks);
  
  /* Start Sequence Generator */
  AD5940_SEQGenCtrl(bTRUE);
  AD5940_SEQGpioCtrlS(AGPIO_Pin2); /* Set GPIO1, clear others that under control */
  AD5940_SEQGenInsert(SEQ_WAIT(16*250));  /* @todo wait 250us? */

  /* Disconnect SE0 from LPTIA*/
	LpAmpCfg.LpAmpPwrMod = LPAMPPWR_NORM;
  LpAmpCfg.LpPaPwrEn = bTRUE;
  LpAmpCfg.LpTiaPwrEn = bTRUE;
  LpAmpCfg.LpTiaRf = AppIMP3ECfg.LpTiaRf;
  LpAmpCfg.LpTiaRload = AppIMP3ECfg.LpTiaRl;
  LpAmpCfg.LpTiaRtia = LPTIARTIA_OPEN; /* Disconnect Rtia to avoid RC filter discharge */
  LpAmpCfg.LpTiaSW = LPTIASW(7)|LPTIASW(8)|LPTIASW(12)|LPTIASW(13); 
	AD5940_LPAMPCfgS(&LpAmpCfg);
  /* Sensor + Rload Measurement */
  sw_cfg.Dswitch = AppIMP3ECfg.DswitchSel;
  sw_cfg.Pswitch = AppIMP3ECfg.PswitchSel;
  sw_cfg.Nswitch = AppIMP3ECfg.NswitchSel;
  sw_cfg.Tswitch = SWT_TRTIA|AppIMP3ECfg.TswitchSel;
  AD5940_SWMatrixCfgS(&sw_cfg);
  
  AD5940_AFECtrlS(AFECTRL_HSTIAPWR|AFECTRL_INAMPPWR|AFECTRL_EXTBUFPWR|\
                AFECTRL_WG|AFECTRL_DACREFPWR|AFECTRL_HSDACPWR|\
                AFECTRL_SINC2NOTCH, bTRUE);
  
																 
  AD5940_AFECtrlS(AFECTRL_ADCPWR|AFECTRL_SINC2NOTCH, bTRUE);  /* Enable Waveform generator */
  //delay for signal settling DFT_WAIT
  AD5940_SEQGenInsert(SEQ_WAIT(16*10));
  AD5940_AFECtrlS(AFECTRL_ADCCNV|AFECTRL_DFT, bTRUE);  /* Start ADC convert and DFT */
  AD5940_SEQGenInsert(SEQ_WAIT(WaitClks));
  //wait for first data ready
  AD5940_AFECtrlS(AFECTRL_HSTIAPWR|AFECTRL_INAMPPWR|AFECTRL_EXTBUFPWR|\
    AFECTRL_WG|AFECTRL_DACREFPWR|AFECTRL_HSDACPWR|\
      AFECTRL_SINC2NOTCH|AFECTRL_DFT|AFECTRL_ADCCNV, bFALSE);
  
  /* RLOAD Measurement */
  sw_cfg.Dswitch = SWD_SE0;
  sw_cfg.Pswitch = SWP_SE0;
  sw_cfg.Nswitch = SWN_SE0LOAD;
  sw_cfg.Tswitch = SWT_SE0LOAD|SWT_TRTIA;
  AD5940_SWMatrixCfgS(&sw_cfg);
  AD5940_AFECtrlS(AFECTRL_HSTIAPWR|AFECTRL_INAMPPWR|AFECTRL_EXTBUFPWR|\
    AFECTRL_WG|AFECTRL_DACREFPWR|AFECTRL_HSDACPWR|AFECTRL_SINC2NOTCH, bTRUE);
  AD5940_SEQGenInsert(SEQ_WAIT(16*10));  //delay for signal settling DFT_WAIT
  AD5940_AFECtrlS(AFECTRL_ADCCNV|AFECTRL_DFT, bTRUE);  /* Start ADC convert and DFT */
  AD5940_SEQGenInsert(SEQ_WAIT(WaitClks));  /* wait for first data ready */
  AD5940_AFECtrlS(AFECTRL_HSTIAPWR|AFECTRL_INAMPPWR|AFECTRL_EXTBUFPWR|\
    AFECTRL_WG|AFECTRL_DACREFPWR|AFECTRL_HSDACPWR|\
      AFECTRL_SINC2NOTCH|AFECTRL_ADCCNV, bFALSE);
  
  /* RCAL Measurement */
  sw_cfg.Dswitch = SWD_RCAL0;
  sw_cfg.Pswitch = SWP_RCAL0;
  sw_cfg.Nswitch = SWN_RCAL1;
  sw_cfg.Tswitch = SWT_RCAL1|SWT_TRTIA;
  AD5940_SWMatrixCfgS(&sw_cfg);
	/* Reconnect LP loop */
	LpAmpCfg.LpTiaRtia = AppIMP3ECfg.LptiaRtiaSel; /* Disconnect Rtia to avoid RC filter discharge */
  LpAmpCfg.LpTiaSW = LPTIASW(5)|LPTIASW(2)|LPTIASW(4)|LPTIASW(12)|LPTIASW(13); 
	AD5940_LPAMPCfgS(&LpAmpCfg);
	
  AD5940_AFECtrlS(AFECTRL_HSTIAPWR|AFECTRL_INAMPPWR|AFECTRL_EXTBUFPWR|\
    AFECTRL_WG|AFECTRL_DACREFPWR|AFECTRL_HSDACPWR|AFECTRL_SINC2NOTCH, bTRUE);
  AD5940_SEQGenInsert(SEQ_WAIT(16*10));  //delay for signal settling DFT_WAIT
  AD5940_AFECtrlS(AFECTRL_ADCCNV|AFECTRL_DFT/*|AFECTRL_SINC2NOTCH*/, bTRUE);  /* Start ADC convert and DFT */
  AD5940_SEQGenInsert(SEQ_WAIT(WaitClks));  /* wait for first data ready */
  AD5940_AFECtrlS(AFECTRL_ADCCNV|AFECTRL_DFT|AFECTRL_WG|AFECTRL_ADCPWR, bFALSE);  /* Stop ADC convert and DFT */
  AD5940_AFECtrlS(AFECTRL_HSTIAPWR|AFECTRL_INAMPPWR|AFECTRL_EXTBUFPWR|\
    AFECTRL_WG|AFECTRL_DACREFPWR|AFECTRL_HSDACPWR|\
      AFECTRL_SINC2NOTCH, bFALSE);
  AD5940_SEQGpioCtrlS(0); /* Clr GPIO1 */
  
  sw_cfg.Dswitch = SWD_OPEN;
  sw_cfg.Pswitch = SWP_OPEN;
  sw_cfg.Nswitch = SWN_OPEN;
  sw_cfg.Tswitch = SWT_OPEN;
  AD5940_SWMatrixCfgS(&sw_cfg);
  
  //AD5940_EnterSleepS();/* Goto hibernate */
  
  /* Sequence end. */
  error = AD5940_SEQGenFetchSeq(&pSeqCmd, &SeqLen);
  AD5940_SEQGenCtrl(bFALSE); /* Stop sequencer generator */

  if(error == AD5940ERR_OK)
  {
    AppIMP3ECfg.MeasureSeqInfo.SeqId = SEQID_0;
    AppIMP3ECfg.MeasureSeqInfo.SeqRamAddr = AppIMP3ECfg.InitSeqInfo.SeqRamAddr + AppIMP3ECfg.InitSeqInfo.SeqLen ;
    AppIMP3ECfg.MeasureSeqInfo.pSeqCmd = pSeqCmd;
    AppIMP3ECfg.MeasureSeqInfo.SeqLen = SeqLen;
    /* Write command to SRAM */
    AD5940_SEQCmdWrite(AppIMP3ECfg.MeasureSeqInfo.SeqRamAddr, pSeqCmd, SeqLen);
  }
  else
    return error; /* Error */
  return AD5940ERR_OK;
}


/* This function provide application initialize. It can also enable Wupt that will automatically trigger sequence. Or it can configure  */
int32_t AppIMP3EInit(uint32_t *pBuffer, uint32_t BufferSize)
{
  AD5940Err error = AD5940ERR_OK;  
  SEQCfg_Type seq_cfg;
  FIFOCfg_Type fifo_cfg;

  if(AD5940_WakeUp(10) > 10)  /* Wakeup AFE by read register, read 10 times at most */
    return AD5940ERR_WAKEUP;  /* Wakeup Failed */

  /* Configure sequencer and stop it */
  seq_cfg.SeqMemSize = SEQMEMSIZE_2KB;  /* 2kB SRAM is used for sequencer, others for data FIFO */
  seq_cfg.SeqBreakEn = bFALSE;
  seq_cfg.SeqIgnoreEn = bTRUE;
  seq_cfg.SeqCntCRCClr = bTRUE;
  seq_cfg.SeqEnable = bFALSE;
  seq_cfg.SeqWrTimer = 0;
  AD5940_SEQCfg(&seq_cfg);
  
  /* Reconfigure FIFO */
  AD5940_FIFOCtrlS(FIFOSRC_DFT, bFALSE);									/* Disable FIFO firstly */
  fifo_cfg.FIFOEn = bTRUE;
  fifo_cfg.FIFOMode = FIFOMODE_FIFO;
  fifo_cfg.FIFOSize = FIFOSIZE_4KB;                       /* 4kB for FIFO, The reset 2kB for sequencer */
  fifo_cfg.FIFOSrc = FIFOSRC_DFT;
  fifo_cfg.FIFOThresh = AppIMP3ECfg.FifoThresh;              /* DFT result. One pair for RCAL, another for Rz. One DFT result have real part and imaginary part */
  AD5940_FIFOCfg(&fifo_cfg);
  AD5940_INTCClrFlag(AFEINTSRC_ALLINT);

  /* Start sequence generator */
  /* Initialize sequencer generator */
  if((AppIMP3ECfg.IMP3EInited == bFALSE)||\
       (AppIMP3ECfg.bParaChanged == bTRUE))
  {
    if(pBuffer == 0)  return AD5940ERR_PARA;
    if(BufferSize == 0) return AD5940ERR_PARA;   
    AD5940_SEQGenInit(pBuffer, BufferSize);

    /* Generate initialize sequence */
    error = AppIMP3ESeqCfgGen(); /* Application initialization sequence using either MCU or sequencer */
    if(error != AD5940ERR_OK) return error;

    /* Generate measurement sequence */
    error = AppIMP3ESeqMeasureGen();
    if(error != AD5940ERR_OK) return error;

    AppIMP3ECfg.bParaChanged = bFALSE; /* Clear this flag as we already implemented the new configuration */
  }

  /* Initialization sequencer  */
  AppIMP3ECfg.InitSeqInfo.WriteSRAM = bFALSE;
  AD5940_SEQInfoCfg(&AppIMP3ECfg.InitSeqInfo);
  seq_cfg.SeqEnable = bTRUE;
  AD5940_SEQCfg(&seq_cfg);  /* Enable sequencer */
  AD5940_SEQMmrTrig(AppIMP3ECfg.InitSeqInfo.SeqId);
  while(AD5940_INTCTestFlag(AFEINTC_1, AFEINTSRC_ENDSEQ) == bFALSE);
  
  /* Measurement sequence  */
  AppIMP3ECfg.MeasureSeqInfo.WriteSRAM = bFALSE;
  AD5940_SEQInfoCfg(&AppIMP3ECfg.MeasureSeqInfo);

  seq_cfg.SeqEnable = bTRUE;
  AD5940_SEQCfg(&seq_cfg);  /* Enable sequencer, and wait for trigger */
  AD5940_ClrMCUIntFlag();   /* Clear interrupt flag generated before */

  AD5940_AFEPwrBW(AppIMP3ECfg.PwrMod, AFEBW_250KHZ);

	AD5940_WriteReg(REG_AFE_LPTIASW0, 0x3180);
  AppIMP3ECfg.IMP3EInited = bTRUE;  /* IMP application has been initialized. */
  return AD5940ERR_OK;
}

/* Modify registers when AFE wakeup */
int32_t AppIMP3ERegModify(int32_t * const pData, uint32_t *pDataCount)
{
  if(AppIMP3ECfg.NumOfData > 0)
  {
    AppIMP3ECfg.FifoDataCount += *pDataCount/4;
    if(AppIMP3ECfg.FifoDataCount >= AppIMP3ECfg.NumOfData)
    {
      AD5940_WUPTCtrl(bFALSE);
      return AD5940ERR_OK;
    }
  }
  if(AppIMP3ECfg.StopRequired == bTRUE)
  {
    AD5940_WUPTCtrl(bFALSE);
    return AD5940ERR_OK;
  }
  if(AppIMP3ECfg.SweepCfg.SweepEn) /* Need to set new frequency and set power mode */
  {
		/* Check frequency and update FIlter settings */
    AD5940_WGFreqCtrlS(AppIMP3ECfg.SweepNextFreq, AppIMP3ECfg.SysClkFreq);
  }
  return AD5940ERR_OK;
}

/* Depending on the data type, do appropriate data pre-process before return back to controller */
int32_t AppIMP3EDataProcess(int32_t * const pData, uint32_t *pDataCount)
{
  uint32_t DataCount = *pDataCount;
  uint32_t ImpResCount = DataCount/6;
  
  fImpPol_Type * const pOut = (fImpPol_Type*)pData;
  iImpCar_Type * pSrcData = (iImpCar_Type*)pData;
  
  *pDataCount = 0;
  
  DataCount = (DataCount/6)*6;/* We expect Rz+Rload, Rload and RCAL data, . One DFT result has two data in FIFO, real part and imaginary part.  */
  
  /* Convert DFT result to int32_t type */
  for(uint32_t i=0; i<DataCount; i++)
  {
    pData[i] &= 0x3ffff;
    if(pData[i]&(1L<<17)) /* Bit17 is sign bit */
    {
      pData[i] |= 0xfffc0000; /* Data is 18bit in two's complement, bit17 is the sign bit */
    }
  }
  for(uint32_t i=0; i<ImpResCount; i++)
  {
    if(1)
    {
      fImpCar_Type DftRcal, DftRzRload, DftRload, temp1, temp2, res;
      fImpCar_Type DftConst1 = {1.0f, 0};
      /*
        The sign of DFT Image result is added '-1' by hardware. Fix it below.
      */
      DftRzRload.Real = pSrcData->Real;
      DftRzRload.Image = -pSrcData->Image;
      pSrcData++;
      DftRload.Real = pSrcData->Real;
      DftRload.Image = -pSrcData->Image;
      pSrcData++;
      DftRcal.Real = pSrcData->Real;
      DftRcal.Image = -pSrcData->Image;
      pSrcData++;
      /**
        Rz = RloadRz - Rload
        RloadRz = DftRcal/DftRzRload*RCAL;
        Rload = DftRcal/DftRload*RCAL;
        Rz = RloadRz - Rload = 
             (1/DftRzRload - 1/DftRload)*DftRcal*RCAL;
        where RCAL is the RCAL resistor value in Ohm.
      */
      //temp1 = 1/DftRzRload;
      //temp2 = 1/DftRload;
      temp1 = AD5940_ComplexDivFloat(&DftConst1, &DftRzRload); 
      temp2 = AD5940_ComplexDivFloat(&DftConst1, &DftRload); 
      res = AD5940_ComplexSubFloat(&temp1, &temp2);
      res = AD5940_ComplexMulFloat(&res, &DftRcal);
      pOut[i].Magnitude = AD5940_ComplexMag(&res)*AppIMP3ECfg.RcalVal;
      pOut[i].Phase = AD5940_ComplexPhase(&res);
    }
    else
    {
      iImpCar_Type *pDftRcal, *pDftRzRload, *pDftRload;
      
      pDftRzRload = pSrcData++;
      pDftRload = pSrcData++;
      pDftRcal = pSrcData++;
      
      float RzRloadMag, RzRloadPhase;
      float RloadMag, RloadPhase;
      float RzMag,RzPhase;
      float RcalMag, RcalPhase;
      float RzReal, RzImage;
      
      RzReal = pDftRload->Real - pDftRzRload->Real;
      RzImage = pDftRload->Image - pDftRzRload->Image;
      
      RzRloadMag = sqrt((float)pDftRzRload->Real*pDftRzRload->Real+(float)pDftRzRload->Image*pDftRzRload->Image);
      RzRloadPhase = atan2(-pDftRzRload->Image,pDftRzRload->Real);
      RcalMag = sqrt((float)pDftRcal->Real*pDftRcal->Real+(float)pDftRcal->Image*pDftRcal->Image);
      RcalPhase = atan2(-pDftRcal->Image,pDftRcal->Real);
      RzMag = sqrt((float)RzReal*RzReal+(float)RzImage*RzImage);
      RzPhase = atan2(-RzImage,RzReal);
      RloadMag = sqrt((float)pDftRload->Real*pDftRload->Real+(float)pDftRload->Image*pDftRload->Image);
      RloadPhase = atan2(-pDftRload->Image,pDftRload->Real);
      
      RzMag = (AppIMP3ECfg.RcalVal*RcalMag*RzMag)/(RzRloadMag*RloadMag);
      RzPhase = -(RcalPhase + RzPhase - RloadPhase - RzRloadPhase);
     // RzPhase = (RcalPhase + RzPhase - RloadPhase - RzRloadPhase);

      
      pOut[i].Magnitude = RzMag;
      pOut[i].Phase = RzPhase;
    }
  }
  *pDataCount = ImpResCount; 
  AppIMP3ECfg.FreqofData = AppIMP3ECfg.SweepCurrFreq;
  /* Calculate next frequency point */
  if(AppIMP3ECfg.SweepCfg.SweepEn == bTRUE)
  {
    AppIMP3ECfg.FreqofData = AppIMP3ECfg.SweepCurrFreq;
    AppIMP3ECfg.SweepCurrFreq = AppIMP3ECfg.SweepNextFreq;
    AD5940_SweepNext(&AppIMP3ECfg.SweepCfg, &AppIMP3ECfg.SweepNextFreq);
  }

  return 0;
}

/**

*/
int32_t AppIMP3EISR(void *pBuff, uint32_t *pCount)
{
  uint32_t BuffCount;
  uint32_t FifoCnt;
  BuffCount = *pCount;
  
  *pCount = 0;
  
  if(AD5940_WakeUp(10) > 10)  /* Wakeup AFE by read register, read 10 times at most */
    return AD5940ERR_WAKEUP;  /* Wakeup Failed */
  AD5940_SleepKeyCtrlS(SLPKEY_LOCK);  /* Prohibit AFE to enter sleep mode. */

  if(AD5940_INTCTestFlag(AFEINTC_0, AFEINTSRC_DATAFIFOTHRESH) == bTRUE)
  {
    /* Now there should be 4 data in FIFO */
    FifoCnt = (AD5940_FIFOGetCnt()/6)*6;
    
    if(FifoCnt > BuffCount)
    {
      ///@todo buffer is limited.
    }
    AD5940_FIFORd((uint32_t *)pBuff, FifoCnt);
    AD5940_INTCClrFlag(AFEINTSRC_DATAFIFOTHRESH);
    AppIMP3ERegModify(pBuff, &FifoCnt);   /* If there is need to do AFE re-configure, do it here when AFE is in active state */
    //AD5940_EnterSleepS(); /* Manually put AFE back to hibernate mode. This operation only takes effect when register value is ACTIVE previously */
    AD5940_SleepKeyCtrlS(SLPKEY_UNLOCK);  /* Allow AFE to enter sleep mode. */
    /* Process data */ 
    AppIMP3EDataProcess((int32_t*)pBuff,&FifoCnt); 
    *pCount = FifoCnt;
    return 0;
  }
  
  return 0;
} 
