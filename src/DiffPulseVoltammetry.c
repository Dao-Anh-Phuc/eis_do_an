/*!
*****************************************************************************
@file:    DiffPulseVoltammetry.c
@brief:   Differential Pulse Voltammetry (DPV) measurement sequences for AD5940
@date:    2025-08-19
-----------------------------------------------------------------------------

This file ports the SWV flow to DPV with minimal structural changes:
- SEQ3 : Initialization
- SEQ2 : ADC control (1 sample per trigger)
- SEQ0/1: LPDAC update in ping-pong blocks

WUPT running order per DPV point:
  SEQ0 (baseline set)  -> SEQ2 (sample BASE)
  SEQ1 (apply pulse)   -> SEQ2 (sample PULSE)

Pulse is cleared implicitly when the next SEQ0 updates baseline for the next point.
HoldAfterPulse_ms controls delay between PULSE sample and next baseline.

NOTE: In this first cut, both ADC triggers (BASE and PULSE) share the same
      WUPT SeqxWakeupTime[SEQ2]. For best alignment, choose
      PrePulseWait_ms ~= PulseWidth_ms and GuardBase_ms ~= GuardPulse_ms.
      (Advanced: use dual ADC sequences and toggle SEQ2INFO if you need
      different delays.)
*****************************************************************************
*/

#include "ad5940.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "DiffPulseVoltammetry.h"

/* ------------------------------------------------------------------------- */
/* Application configuration (defaults)                                       */
/* ------------------------------------------------------------------------- */
static AppDPVCfg_Type AppDPVCfg =
{
  /* Common */
  .bParaChanged      = bFALSE,
  .SeqStartAddr      = 0,
  .MaxSeqLen         = 0,
  .SeqStartAddrCal   = 0,
  .MaxSeqLenCal      = 0,

  /* System */
  .LFOSCClkFreq      = 32000.0f,
  .SysClkFreq        = 16000000.0f,
  .AdcClkFreq        = 16000000.0f,
  .RcalVal           = 10000.0f,
  .ADCRefVolt        = 1820.0f,

  /* Staircase (baseline) */
  .RampStartVolt     = -500.0f,
  .RampPeakVolt      =  500.0f,
  .RampStep_mV       =   5.0f,
  .StepNumber        =  0,            /* 0 => compute from range/step */

  /* Vzero */
  .VzeroStart        = 2200.0f,
  .VzeroPeak         =  400.0f,

  /* DPV pulse */
  .PulseAmp_mV       =  25.0f,
  .PulseWidth_ms     =  50.0f,
  .PrePulseWait_ms   =  50.0f,
  .HoldAfterPulse_ms =   1.0f,
  .bPulsePositive    =  bTRUE,

  /* Sampling windows */
  .NAvgBase          = 1,
  .NAvgPulse         = 1,
  .GuardBase_ms      = 2.0f,
  .GuardPulse_ms     = 2.0f,

  /* Rx path */
  .LPTIARtiaSel      = LPTIARTIA_20K,
  .ExternalRtiaValue = 20000.0f,
  .AdcPgaGain        = ADCPGA_1,
  .ADCSinc2Osr       = ADCSINC2OSR_1067,
  .ADCSinc3Osr       = ADCSINC3OSR_4,

  /* Digital */
  .FifoThresh        = 4,

  /* Private */
  .DPVInited         = bFALSE,
  .bFirstDACSeq      = bTRUE,
  .RampState             = DPV_STATE0_IDLE,
  .CurrStepPos       = 0,
  .DACCodePerRamp    = 0,
  .DACCodePulse      = 0,
  .CurrRampCode      = 0,
  .CurrVzeroCode     = 0,
  .bDACCodeInc       = bTRUE,
  .bRampOneDir       = bTRUE,
  .StopRequired      = bFALSE,

  .TicksBaseWait     = 0,
  .TicksPulseWidth   = 0,
  .TicksHoldAfter    = 0,
  .TicksGuardBase    = 0,
  .TicksGuardPulse   = 0,
};

/* --------------------------- Forward declarations ------------------------ */
static AD5940Err AppDPVSeqInitGen(void);
static AD5940Err AppDPVSeqADCCtrlGen(void);
static AD5940Err AppDPVSeqDACCtrlGen(void);
static AD5940Err DPV_DacRegUpdate(uint32_t *pDACData);
static int32_t   AppDPVRegModify(int32_t * const pData, uint32_t *pDataCount);
static int32_t   AppDPVDataProcess(int32_t * const pData, uint32_t *pDataCount);

/* ------------------------------------------------------------------------- */
/* Public getters                                                            */
/* ------------------------------------------------------------------------- */
AD5940Err AppDPVGetCfg(void *pCfg)
{
  if(pCfg){ *(AppDPVCfg_Type**)pCfg = &AppDPVCfg; return AD5940ERR_OK; }
  return AD5940ERR_PARA;
}

/* ------------------------------------------------------------------------- */
/* Control                                                                   */
/* ------------------------------------------------------------------------- */
AD5940Err AppDPVCtrl(uint32_t Command, void *pPara)
{
  switch (Command)
  {
  case APPCTRL_START:
    {
      WUPTCfg_Type wupt_cfg;
  // Khi nhận lệnh START, reset trạng thái và enable sequencer
  AppDPVCfg.StopRequired = bFALSE;
  AppDPVCfg.CurrStepPos = 0;
  AppDPVCfg.RampState = DPV_STATE0_IDLE;
  AppDPVCfg.bFirstDACSeq = bTRUE;
  AD5940_SEQCtrlS(bTRUE); // Enable sequencer
      if(AD5940_WakeUp(10) > 10) return AD5940ERR_WAKEUP;
      if(AppDPVCfg.DPVInited == bFALSE) return AD5940ERR_APPERROR;
      if(AppDPVCfg.RampState == DPV_STOP) return AD5940ERR_APPERROR;

      /* Wakeup order: 0->2->1->2 */
      wupt_cfg.WuptEn      = bTRUE;
      wupt_cfg.WuptEndSeq  = WUPTENDSEQ_D;
  // Additional configuration for the sequencer can be added here if needed
      wupt_cfg.WuptOrder[0]= SEQID_0;   /* baseline set */
      wupt_cfg.WuptOrder[1]= SEQID_2;   /* sample BASE  */
      wupt_cfg.WuptOrder[2]= SEQID_1;   /* pulse on     */
      wupt_cfg.WuptOrder[3]= SEQID_2;   /* sample PULSE */

      /* timing (all values are LFOSC ticks - 1) */
      /* One ADC delay value used for both BASE and PULSE samples. */
      float delay_ms = AppDPVCfg.PrePulseWait_ms - AppDPVCfg.GuardBase_ms;
  // Ensure proper timing configuration
      float delay_p_ms = AppDPVCfg.PulseWidth_ms - AppDPVCfg.GuardPulse_ms;
      if(delay_ms > delay_p_ms) delay_ms = delay_p_ms; /* conservative */
      if(delay_ms < 0.5f) delay_ms = 0.5f;             /* clamp */

      wupt_cfg.SeqxSleepTime[SEQID_2]  = 1; /* minimum */
      wupt_cfg.SeqxWakeupTime[SEQID_2] = (uint32_t)(AppDPVCfg.LFOSCClkFreq*delay_ms/1000.0f) - 1;

      /* After BASE sample -> PULSE ON: keep very short */
      wupt_cfg.SeqxSleepTime[SEQID_1]  = 1;
      wupt_cfg.SeqxWakeupTime[SEQID_1] = 1;

      /* After PULSE sample -> next BASE update */
      float hold_ms = (AppDPVCfg.HoldAfterPulse_ms <= 0.0f) ? 0.5f : AppDPVCfg.HoldAfterPulse_ms;
      wupt_cfg.SeqxSleepTime[SEQID_0]  = 1;
      wupt_cfg.SeqxWakeupTime[SEQID_0] = (uint32_t)(AppDPVCfg.LFOSCClkFreq*hold_ms/1000.0f) - 1;

      AD5940_WUPTCfg(&wupt_cfg);
      AD5940_WUPTCtrl(bTRUE);
      break;
    }
  case APPCTRL_STOPNOW:
    {
      if(AD5940_WakeUp(10) > 10) return AD5940ERR_WAKEUP;
      AD5940_WUPTCtrl(bFALSE); /* try twice to be safe (same as SWV) */
      AD5940_WUPTCtrl(bFALSE);
      break;
    }
  case APPCTRL_STOPSYNC:
    AppDPVCfg.StopRequired = bTRUE; break;
  case APPCTRL_SHUTDOWN:
    AppDPVCtrl(APPCTRL_STOPNOW, 0);
    AD5940_ShutDownS();
    break;
  default: break;
  }
  return AD5940ERR_OK;
}

/* ------------------------------------------------------------------------- */
/* Sequences                                                                 */
/* ------------------------------------------------------------------------- */
static AD5940Err AppDPVSeqInitGen(void)
{
  AD5940Err error = AD5940ERR_OK;
  const uint32_t *pSeqCmd; uint32_t SeqLen;
  AFERefCfg_Type aferef_cfg; LPLoopCfg_Type lploop_cfg; DSPCfg_Type dsp_cfg;

  AD5940_SEQGenCtrl(bTRUE);
  AD5940_AFECtrlS(AFECTRL_ALL, bFALSE);

  /* Reference */
  aferef_cfg.HpBandgapEn   = bTRUE;
  aferef_cfg.Hp1V1BuffEn   = bTRUE;
  aferef_cfg.Hp1V8BuffEn   = bTRUE;
  aferef_cfg.Disc1V1Cap    = bFALSE;
  aferef_cfg.Disc1V8Cap    = bFALSE;
  aferef_cfg.Hp1V8ThemBuff = bFALSE;
  aferef_cfg.Hp1V8Ilimit   = bFALSE;
  aferef_cfg.Lp1V1BuffEn   = bFALSE;
  aferef_cfg.Lp1V8BuffEn   = bFALSE;
  aferef_cfg.LpBandgapEn   = bTRUE;
  aferef_cfg.LpRefBufEn    = bTRUE;
  aferef_cfg.LpRefBoostEn  = bFALSE;
  AD5940_REFCfgS(&aferef_cfg);

  /* LP Loop */
  lploop_cfg.LpAmpCfg.LpAmpSel   = LPAMP0;
  lploop_cfg.LpAmpCfg.LpAmpPwrMod= LPAMPPWR_BOOST3;
  lploop_cfg.LpAmpCfg.LpPaPwrEn  = bTRUE;
  lploop_cfg.LpAmpCfg.LpTiaPwrEn = bTRUE;
  lploop_cfg.LpAmpCfg.LpTiaRf    = LPTIARF_20K;
  lploop_cfg.LpAmpCfg.LpTiaRload = LPTIARLOAD_SHORT;
  lploop_cfg.LpAmpCfg.LpTiaRtia  = AppDPVCfg.LPTIARtiaSel;
  if(AppDPVCfg.LPTIARtiaSel == LPTIARTIA_OPEN)
    lploop_cfg.LpAmpCfg.LpTiaSW  = LPTIASW(13)|LPTIASW(2)|LPTIASW(4)|LPTIASW(5)|LPTIASW(9);
  else
    lploop_cfg.LpAmpCfg.LpTiaSW  = LPTIASW(2)|LPTIASW(4);

  lploop_cfg.LpDacCfg.LpdacSel   = LPDAC0;
  lploop_cfg.LpDacCfg.DacData6Bit= (uint32_t)((AppDPVCfg.VzeroStart - 200.0f)/DAC6BITVOLT_1LSB);
  lploop_cfg.LpDacCfg.DacData12Bit = (int32_t)((AppDPVCfg.RampStartVolt)/DAC12BITVOLT_1LSB)
                                     + lploop_cfg.LpDacCfg.DacData6Bit*64;
  lploop_cfg.LpDacCfg.DataRst    = bFALSE;
  lploop_cfg.LpDacCfg.LpDacSW    = LPDACSW_VBIAS2LPPA | LPDACSW_VZERO2LPTIA;
  lploop_cfg.LpDacCfg.LpDacRef   = LPDACREF_2P5;
  lploop_cfg.LpDacCfg.LpDacSrc   = LPDACSRC_MMR;
  lploop_cfg.LpDacCfg.LpDacVbiasMux = LPDACVBIAS_12BIT;
  lploop_cfg.LpDacCfg.LpDacVzeroMux = LPDACVZERO_6BIT;
  lploop_cfg.LpDacCfg.PowerEn    = bTRUE;
  AD5940_LPLoopCfgS(&lploop_cfg);

  /* DSP / ADC */
  AD5940_StructInit(&dsp_cfg, sizeof(dsp_cfg));
  dsp_cfg.ADCBaseCfg.ADCMuxN     = ADCMUXN_LPTIA0_N;
  dsp_cfg.ADCBaseCfg.ADCMuxP     = ADCMUXP_LPTIA0_P;
  dsp_cfg.ADCBaseCfg.ADCPga      = AppDPVCfg.AdcPgaGain;
  dsp_cfg.ADCFilterCfg.ADCSinc3Osr = AppDPVCfg.ADCSinc3Osr;
  dsp_cfg.ADCFilterCfg.ADCRate     = ADCRATE_800KHZ;
  dsp_cfg.ADCFilterCfg.BpSinc3     = bFALSE;
  dsp_cfg.ADCFilterCfg.Sinc2NotchEnable = bTRUE;
  dsp_cfg.ADCFilterCfg.BpNotch     = bTRUE;
  dsp_cfg.ADCFilterCfg.ADCSinc2Osr = AppDPVCfg.ADCSinc2Osr; /* informational */
  dsp_cfg.ADCFilterCfg.ADCAvgNum   = ADCAVGNUM_2;
  AD5940_DSPCfgS(&dsp_cfg);

  AD5940_SEQGenInsert(SEQ_STOP());
  AD5940_SEQGenCtrl(bFALSE);

  /* Save to SRAM as SEQID_3 */
  error = AD5940_SEQGenFetchSeq(&pSeqCmd, &SeqLen);
  if(error != AD5940ERR_OK) return error;
  AD5940_StructInit(&AppDPVCfg.InitSeqInfo, sizeof(AppDPVCfg.InitSeqInfo));
  if(SeqLen >= AppDPVCfg.MaxSeqLen) return AD5940ERR_SEQLEN;
  AppDPVCfg.InitSeqInfo.SeqId      = SEQID_3;
  AppDPVCfg.InitSeqInfo.SeqRamAddr = AppDPVCfg.SeqStartAddr;
  AppDPVCfg.InitSeqInfo.pSeqCmd    = pSeqCmd;
  AppDPVCfg.InitSeqInfo.SeqLen     = SeqLen;
  AppDPVCfg.InitSeqInfo.WriteSRAM  = bTRUE;
  AD5940_SEQInfoCfg(&AppDPVCfg.InitSeqInfo);
  return AD5940ERR_OK;
}

static AD5940Err AppDPVSeqADCCtrlGen(void)
{
  AD5940Err error = AD5940ERR_OK;
  const uint32_t *pSeqCmd; uint32_t SeqLen; uint32_t WaitClks; ClksCalInfo_Type clks_cal;

  clks_cal.DataCount      = 1; /* one sample */
  clks_cal.DataType       = DATATYPE_SINC3;
  clks_cal.ADCSinc3Osr    = AppDPVCfg.ADCSinc3Osr;
  clks_cal.ADCSinc2Osr    = AppDPVCfg.ADCSinc2Osr;
  clks_cal.ADCAvgNum      = ADCAVGNUM_2;
  clks_cal.RatioSys2AdcClk= AppDPVCfg.SysClkFreq/AppDPVCfg.AdcClkFreq;
  AD5940_ClksCalculate(&clks_cal, &WaitClks);

  AD5940_SEQGenCtrl(bTRUE);
  AD5940_SEQGpioCtrlS(AGPIO_Pin2);
  AD5940_AFECtrlS(AFECTRL_ADCPWR, bTRUE);
  AD5940_SEQGenInsert(SEQ_WAIT(16*100));        /* ~250us power-up */
  AD5940_AFECtrlS(AFECTRL_ADCCNV, bTRUE);       /* start */
  AD5940_SEQGenInsert(SEQ_WAIT(WaitClks));      /* wait data ready */
  AD5940_AFECtrlS(AFECTRL_ADCPWR|AFECTRL_ADCCNV, bFALSE);
  AD5940_SEQGpioCtrlS(0);
  AD5940_EnterSleepS();

  error = AD5940_SEQGenFetchSeq(&pSeqCmd, &SeqLen);
  AD5940_SEQGenCtrl(bFALSE);
  if(error != AD5940ERR_OK) return error;

  AD5940_StructInit(&AppDPVCfg.ADCSeqInfo, sizeof(AppDPVCfg.ADCSeqInfo));
  if((SeqLen + AppDPVCfg.InitSeqInfo.SeqLen) >= AppDPVCfg.MaxSeqLen) return AD5940ERR_SEQLEN;
  AppDPVCfg.ADCSeqInfo.SeqId      = SEQID_2;
  AppDPVCfg.ADCSeqInfo.SeqRamAddr = AppDPVCfg.InitSeqInfo.SeqRamAddr + AppDPVCfg.InitSeqInfo.SeqLen;
  AppDPVCfg.ADCSeqInfo.pSeqCmd    = pSeqCmd;
  AppDPVCfg.ADCSeqInfo.SeqLen     = SeqLen;
  AppDPVCfg.ADCSeqInfo.WriteSRAM  = bTRUE;
  AD5940_SEQInfoCfg(&AppDPVCfg.ADCSeqInfo);
  return AD5940ERR_OK;
}

/* ------------------------------------------------------------------------- */
/* DAC step generator (ping-pong blocks, same pattern as SWV)                */
/* ------------------------------------------------------------------------- */
static AD5940Err AppDPVSeqDACCtrlGen(void)
{
#define SEQLEN_ONESTEP    4L
#define CURRBLK_BLK0      0
#define CURRBLK_BLK1      1
  AD5940Err error = AD5940ERR_OK;
  uint32_t BlockStartSRAMAddr, DACData, SRAMAddr; uint32_t i;
  uint32_t StepsThisBlock; BoolFlag bIsFinalBlk; uint32_t SeqCmdBuff[SEQLEN_ONESTEP];

  static BoolFlag  bCmdForSeq0 = bTRUE;
  static uint32_t  DACSeqBlk0Addr, DACSeqBlk1Addr;
  static uint32_t  StepsRemainning, StepsPerBlock, DACSeqCurrBlk;

  /* Calculate StepNumber if not provided */
  if(AppDPVCfg.StepNumber == 0){
    float span = fabsf(AppDPVCfg.RampPeakVolt - AppDPVCfg.RampStartVolt);
    uint32_t est = (uint32_t)floorf(span / AppDPVCfg.RampStep_mV) + 1U;
    AppDPVCfg.StepNumber = est;
  }
  if(AppDPVCfg.StepNumber > 1020){
    printf("Error: StepNumber too large for sequencer SRAM limits.\n");
    while(1){}
  }

  if(AppDPVCfg.bFirstDACSeq == bTRUE)
  {
    int32_t DACSeqLenMax;
    StepsRemainning = AppDPVCfg.StepNumber * 2; /* two DAC writes per DPV point: baseline, pulse */
    DACSeqLenMax = (int32_t)AppDPVCfg.MaxSeqLen - (int32_t)AppDPVCfg.InitSeqInfo.SeqLen - (int32_t)AppDPVCfg.ADCSeqInfo.SeqLen;
    if(DACSeqLenMax < SEQLEN_ONESTEP*4) return AD5940ERR_SEQLEN;
    DACSeqLenMax -= SEQLEN_ONESTEP*2;  /* reserve per block overhead */
    StepsPerBlock = DACSeqLenMax/SEQLEN_ONESTEP/2;
    if(StepsPerBlock == 0) return AD5940ERR_SEQLEN;

    DACSeqBlk0Addr = AppDPVCfg.ADCSeqInfo.SeqRamAddr + AppDPVCfg.ADCSeqInfo.SeqLen;
    DACSeqBlk1Addr = DACSeqBlk0Addr + StepsPerBlock*SEQLEN_ONESTEP;
    DACSeqCurrBlk  = CURRBLK_BLK0;

    /* Pre-compute DAC codes */
    AppDPVCfg.DACCodePerRamp = AppDPVCfg.RampStep_mV / DAC12BITVOLT_1LSB;
    AppDPVCfg.DACCodePulse   = (AppDPVCfg.bPulsePositive ? +1.0f : -1.0f) * (AppDPVCfg.PulseAmp_mV / DAC12BITVOLT_1LSB);
    AppDPVCfg.bDACCodeInc    = (AppDPVCfg.RampPeakVolt >= AppDPVCfg.RampStartVolt) ? bTRUE : bFALSE;
    AppDPVCfg.CurrRampCode   = (AppDPVCfg.RampStartVolt / DAC12BITVOLT_1LSB);
    AppDPVCfg.CurrVzeroCode  = (uint32_t)((AppDPVCfg.VzeroStart - 200.0f)/DAC6BITVOLT_1LSB);
    AppDPVCfg.RampState          = DPV_STATE0_IDLE; /* will emit baseline first */
    AppDPVCfg.CurrStepPos    = 0;
    bCmdForSeq0              = bTRUE; /* Start with SEQ0 */
  }

  if(StepsRemainning == 0) return AD5940ERR_OK;
  bIsFinalBlk   = (StepsRemainning <= StepsPerBlock) ? bTRUE : bFALSE;
  StepsThisBlock= bIsFinalBlk ? StepsRemainning : StepsPerBlock;
  StepsRemainning -= StepsThisBlock;

  BlockStartSRAMAddr = (DACSeqCurrBlk == CURRBLK_BLK0) ? DACSeqBlk0Addr : DACSeqBlk1Addr;
  SRAMAddr = BlockStartSRAMAddr;

  for(i=0; i<StepsThisBlock - 1; i++)
  {
    uint32_t CurrAddr = SRAMAddr; SRAMAddr += SEQLEN_ONESTEP;
    DPV_DacRegUpdate(&DACData);
    SeqCmdBuff[0] = SEQ_WR(REG_AFE_LPDACDAT0, DACData);
    SeqCmdBuff[1] = SEQ_WAIT(10);
    SeqCmdBuff[2] = SEQ_WR(bCmdForSeq0?REG_AFE_SEQ1INFO:REG_AFE_SEQ0INFO,
                           (SRAMAddr<<BITP_AFE_SEQ1INFO_ADDR)|(SEQLEN_ONESTEP<<BITP_AFE_SEQ1INFO_LEN));
    SeqCmdBuff[3] = SEQ_SLP();
    AD5940_SEQCmdWrite(CurrAddr, SeqCmdBuff, SEQLEN_ONESTEP);
    bCmdForSeq0 = bCmdForSeq0?bFALSE:bTRUE;
  }

  /* final in block */
  if(bIsFinalBlk)
  {
    uint32_t CurrAddr = SRAMAddr; SRAMAddr += SEQLEN_ONESTEP;
    DPV_DacRegUpdate(&DACData);
    SeqCmdBuff[0] = SEQ_WR(REG_AFE_LPDACDAT0, DACData);
    SeqCmdBuff[1] = SEQ_WAIT(10);
    SeqCmdBuff[2] = SEQ_WR(bCmdForSeq0?REG_AFE_SEQ1INFO:REG_AFE_SEQ0INFO,
                           (SRAMAddr<<BITP_AFE_SEQ1INFO_ADDR)|(SEQLEN_ONESTEP<<BITP_AFE_SEQ1INFO_LEN));
    SeqCmdBuff[3] = SEQ_SLP();
    AD5940_SEQCmdWrite(CurrAddr, SeqCmdBuff, SEQLEN_ONESTEP);

    CurrAddr += SEQLEN_ONESTEP;
    SeqCmdBuff[0] = SEQ_NOP();
    SeqCmdBuff[1] = SEQ_NOP();
    SeqCmdBuff[2] = SEQ_NOP();
    SeqCmdBuff[3] = SEQ_STOP();
    AD5940_SEQCmdWrite(CurrAddr, SeqCmdBuff, SEQLEN_ONESTEP);
  }
  else
  {
    uint32_t CurrAddr = SRAMAddr;
    SRAMAddr = (DACSeqCurrBlk == CURRBLK_BLK0) ? DACSeqBlk1Addr : DACSeqBlk0Addr;
    DPV_DacRegUpdate(&DACData);
    SeqCmdBuff[0] = SEQ_WR(REG_AFE_LPDACDAT0, DACData);
    SeqCmdBuff[1] = SEQ_WAIT(10);
    SeqCmdBuff[2] = SEQ_WR(bCmdForSeq0?REG_AFE_SEQ1INFO:REG_AFE_SEQ0INFO,
                           (SRAMAddr<<BITP_AFE_SEQ1INFO_ADDR)|(SEQLEN_ONESTEP<<BITP_AFE_SEQ1INFO_LEN));
    SeqCmdBuff[3] = SEQ_INT0();
    AD5940_SEQCmdWrite(CurrAddr, SeqCmdBuff, SEQLEN_ONESTEP);
    bCmdForSeq0 = bCmdForSeq0?bFALSE:bTRUE;
  }

  DACSeqCurrBlk = (DACSeqCurrBlk == CURRBLK_BLK0) ? CURRBLK_BLK1 : CURRBLK_BLK0;
  if(AppDPVCfg.bFirstDACSeq)
  {
    AppDPVCfg.bFirstDACSeq = bFALSE;
    if(bIsFinalBlk == bFALSE){ error = AppDPVSeqDACCtrlGen(); if(error != AD5940ERR_OK) return error; }
    AppDPVCfg.DACSeqInfo.SeqId      = SEQID_0;
    AppDPVCfg.DACSeqInfo.SeqLen     = SEQLEN_ONESTEP;
    AppDPVCfg.DACSeqInfo.SeqRamAddr = BlockStartSRAMAddr;
    AppDPVCfg.DACSeqInfo.WriteSRAM  = bFALSE;
    AD5940_SEQInfoCfg(&AppDPVCfg.DACSeqInfo);
  }
  return AD5940ERR_OK;
}

/* ------------------------------------------------------------------------- */
/* DPV DAC update logic                                                       */
/* ------------------------------------------------------------------------- */
static AD5940Err DPV_DacRegUpdate(uint32_t *pDACData)
{
  /* Alternate between baseline and pulse level. After pulse -> advance baseline */
  static BoolFlag bPulsePhaseHigh = bFALSE; /* FALSE: baseline, TRUE: pulse */

  /* Current Vzero code (kept constant here; can be ramped if desired) */
  uint32_t VzeroCode = AppDPVCfg.CurrVzeroCode;
  float ramp_code = AppDPVCfg.CurrRampCode; /* baseline 12-bit code */

  if(bPulsePhaseHigh == bFALSE){
    /* Emit baseline level for current step */
    /* no change */
  } else {
    /* Emit pulse level: baseline +/- Epulse (in 12-bit codes) */
    ramp_code += AppDPVCfg.DACCodePulse;
  }

  // ✅✅✅ DÒNG NÀY ĐÃ SỬA: ĐỔI DẤU + THÀNH -
  int32_t VbiasCode = (int32_t)( (int32_t)(VzeroCode*64) - (int32_t)ramp_code );
  
  if(VbiasCode < (int32_t)(VzeroCode*64)) VbiasCode--;
  if(VbiasCode > 4095) VbiasCode = 4095;
  if(VzeroCode >   63) VzeroCode =   63;
  *pDACData = (VzeroCode<<12) | (uint32_t)VbiasCode;

  /* Toggle phase and advance baseline/step after completing pulse */
  if(bPulsePhaseHigh == bTRUE){
    /* we just output a PULSE level -> next call will be baseline of next step */
    if(AppDPVCfg.bDACCodeInc) AppDPVCfg.CurrRampCode += AppDPVCfg.DACCodePerRamp;
    else                      AppDPVCfg.CurrRampCode -= AppDPVCfg.DACCodePerRamp;
    AppDPVCfg.CurrStepPos++;
  }
  bPulsePhaseHigh = (bPulsePhaseHigh==bFALSE)?bTRUE:bFALSE;

  /* Stop condition */
  if(AppDPVCfg.CurrStepPos >= AppDPVCfg.StepNumber){ AppDPVCfg.RampState = DPV_STOP; }
  return AD5940ERR_OK;
}

/* ------------------------------------------------------------------------- */
/* Calibration                                                                */
/* ------------------------------------------------------------------------- */
static AD5940Err AppDPVRtiaCal(void)
{
  fImpPol_Type RtiaCalValue;  LPRTIACal_Type lprtia_cal;  AD5940_StructInit(&lprtia_cal, sizeof(lprtia_cal));
  lprtia_cal.LpAmpSel    = LPAMP0;
  lprtia_cal.bPolarResult= bTRUE;
  lprtia_cal.AdcClkFreq  = AppDPVCfg.AdcClkFreq;
  lprtia_cal.SysClkFreq  = AppDPVCfg.SysClkFreq;
  lprtia_cal.ADCSinc3Osr = ADCSINC3OSR_4;
  lprtia_cal.ADCSinc2Osr = ADCSINC2OSR_22;
  lprtia_cal.DftCfg.DftNum = DFTNUM_2048;
  lprtia_cal.DftCfg.DftSrc = DFTSRC_SINC2NOTCH;
  lprtia_cal.DftCfg.HanWinEn = bTRUE;
  lprtia_cal.fFreq       = AppDPVCfg.AdcClkFreq/4/22/2048*3;
  lprtia_cal.fRcal       = AppDPVCfg.RcalVal;
  lprtia_cal.LpTiaRtia   = AppDPVCfg.LPTIARtiaSel;
  lprtia_cal.LpAmpPwrMod = LPAMPPWR_NORM;
  lprtia_cal.bWithCtia   = bFALSE;
  AD5940_LPRtiaCal(&lprtia_cal, &RtiaCalValue);
  AppDPVCfg.RtiaValue = RtiaCalValue;
  return AD5940ERR_OK;
}

/* ------------------------------------------------------------------------- */
/* Init                                                                       */
/* ------------------------------------------------------------------------- */
AD5940Err AppDPVInit(uint32_t *pBuffer, uint32_t BufferSize)
{
  AD5940Err error = AD5940ERR_OK;  FIFOCfg_Type fifo_cfg; SEQCfg_Type seq_cfg;
  if(AD5940_WakeUp(10) > 10) return AD5940ERR_WAKEUP;

  /* Sequencer core */
  seq_cfg.SeqMemSize   = SEQMEMSIZE_2KB;
  seq_cfg.SeqBreakEn   = bFALSE;
  seq_cfg.SeqIgnoreEn  = bFALSE;
  seq_cfg.SeqCntCRCClr = bTRUE;
  seq_cfg.SeqEnable    = bFALSE;
  seq_cfg.SeqWrTimer   = 0;
  AD5940_SEQCfg(&seq_cfg);

  /* Convert ms -> ticks for WUPT usage */
  AppDPVCfg.TicksBaseWait   = (uint32_t)( (AppDPVCfg.PrePulseWait_ms/1000.0f)   * AppDPVCfg.LFOSCClkFreq );
  AppDPVCfg.TicksPulseWidth = (uint32_t)( (AppDPVCfg.PulseWidth_ms/1000.0f)     * AppDPVCfg.LFOSCClkFreq );
  AppDPVCfg.TicksHoldAfter  = (uint32_t)( (AppDPVCfg.HoldAfterPulse_ms/1000.0f) * AppDPVCfg.LFOSCClkFreq );
  AppDPVCfg.TicksGuardBase  = (uint32_t)( (AppDPVCfg.GuardBase_ms/1000.0f)      * AppDPVCfg.LFOSCClkFreq );
  AppDPVCfg.TicksGuardPulse = (uint32_t)( (AppDPVCfg.GuardPulse_ms/1000.0f)     * AppDPVCfg.LFOSCClkFreq );

  if((AppDPVCfg.DPVInited == bFALSE) || (AppDPVCfg.bParaChanged == bTRUE))
  {
    if(pBuffer == 0 || BufferSize == 0) return AD5940ERR_PARA;
    if(AppDPVCfg.LPTIARtiaSel == LPTIARTIA_OPEN){
      AppDPVCfg.RtiaValue.Magnitude = AppDPVCfg.ExternalRtiaValue; AppDPVCfg.RtiaValue.Phase = 0;
    } else {
      AppDPVRtiaCal();
    }
    AppDPVCfg.DPVInited = bFALSE;

    AD5940_SEQGenInit(pBuffer, BufferSize);
    error = AppDPVSeqInitGen();      if(error != AD5940ERR_OK) return error;
    error = AppDPVSeqADCCtrlGen();   if(error != AD5940ERR_OK) return error;
    AppDPVCfg.bParaChanged = bFALSE;
  }

  /* FIFO */
  AD5940_FIFOCtrlS(FIFOSRC_SINC3, bFALSE);
  fifo_cfg.FIFOEn     = bTRUE;
  fifo_cfg.FIFOSrc    = FIFOSRC_SINC3;
  fifo_cfg.FIFOThresh = AppDPVCfg.FifoThresh; /* should be multiple of 2 */
  fifo_cfg.FIFOMode   = FIFOMODE_FIFO;
  fifo_cfg.FIFOSize   = FIFOSIZE_4KB;
  AD5940_FIFOCfg(&fifo_cfg);

  /* Clear all ints */
  AD5940_INTCClrFlag(AFEINTSRC_ALLINT);

  /* Generate DAC sequence */
  AppDPVCfg.bFirstDACSeq = bTRUE;
  error = AppDPVSeqDACCtrlGen(); if(error != AD5940ERR_OK) return error;

  /* Bind sequences and start */
  AppDPVCfg.InitSeqInfo.WriteSRAM = bFALSE; AD5940_SEQInfoCfg(&AppDPVCfg.InitSeqInfo);
  AD5940_SEQCtrlS(bTRUE); AD5940_SEQMmrTrig(AppDPVCfg.InitSeqInfo.SeqId);
  while(AD5940_INTCTestFlag(AFEINTC_1, AFEINTSRC_ENDSEQ) == bFALSE);
  AD5940_INTCClrFlag(AFEINTSRC_ENDSEQ);

  AppDPVCfg.ADCSeqInfo.WriteSRAM = bFALSE; AD5940_SEQInfoCfg(&AppDPVCfg.ADCSeqInfo);
  AppDPVCfg.DACSeqInfo.WriteSRAM = bFALSE; AD5940_SEQInfoCfg(&AppDPVCfg.DACSeqInfo);
  {
  SEQInfo_Type seq1 = AppDPVCfg.DACSeqInfo;  /* copy địa chỉ/độ dài của block đầu */
  seq1.SeqId    = SEQID_1;                   /* SEQ1 dùng chung RAM trước, sẽ được cập nhật ping-pong */
  seq1.WriteSRAM= bFALSE;                    /* không ghi lại SRAM, chỉ cấu hình con trỏ */
  AD5940_SEQInfoCfg(&seq1);
  }
  AD5940_SEQCtrlS(bFALSE); AD5940_WriteReg(REG_AFE_SEQCNT, 0); AD5940_SEQCtrlS(bTRUE);
  AD5940_ClrMCUIntFlag();
  AD5940_AFEPwrBW(AFEPWR_LP, AFEBW_250KHZ);

  AppDPVCfg.DPVInited = bTRUE;
  return AD5940ERR_OK;
}

/* ------------------------------------------------------------------------- */
/* ISR helpers                                                                */
/* ------------------------------------------------------------------------- */
static int32_t AppDPVRegModify(int32_t * const pData, uint32_t *pDataCount)
{
  if(AppDPVCfg.StopRequired == bTRUE){ AD5940_WUPTCtrl(bFALSE); return AD5940ERR_OK; }
  return AD5940ERR_OK;
}

static int32_t AppDPVDataProcess(int32_t * const pData, uint32_t *pDataCount)
{
  /* Convert codes -> current (uA), then collapse pairs (pulse - base) */
  uint32_t n = *pDataCount; if(n < 2) return 0;
  float *pf = (float*)pData; uint32_t i;
  for(i=0; i<n; i++){
    pData[i] &= 0xffff;
    pf[i] = AD5940_ADCCode2Volt(pData[i], AppDPVCfg.AdcPgaGain, AppDPVCfg.ADCRefVolt)/AppDPVCfg.RtiaValue.Magnitude * 1e3f;
  }
  /* collapse in-place: [b0,p0,b1,p1,...] -> [p0-b0, p1-b1, ...] */
  uint32_t m = n/2; for(i=0; i<m; i++){ pf[i] = pf[2*i+1] - pf[2*i]; }
  *pDataCount = m; /* now count equals number of DPV points just read */
  return 0;
}

/* ------------------------------------------------------------------------- */
/* Main ISR                                                                  */
/* ------------------------------------------------------------------------- */
AD5940Err AppDPVISR(void *pBuff, uint32_t *pCount)
{
  uint32_t BuffCount = *pCount; uint32_t FifoCnt; uint32_t IntFlag;
  if(AD5940_WakeUp(10) > 10) return AD5940ERR_WAKEUP;
  AD5940_SleepKeyCtrlS(SLPKEY_LOCK);
  *pCount = 0;

  IntFlag = AD5940_INTCGetFlag(AFEINTC_0);
  if(IntFlag & AFEINTSRC_CUSTOMINT0)
  {
    AD5940Err error; AD5940_INTCClrFlag(AFEINTSRC_CUSTOMINT0);
    error = AppDPVSeqDACCtrlGen(); if(error != AD5940ERR_OK) return error;
  }
  if(IntFlag & AFEINTSRC_DATAFIFOTHRESH)
  {
    FifoCnt = AD5940_FIFOGetCnt(); if(FifoCnt > BuffCount){ /* TODO: handle overflow */ }
    AD5940_FIFORd((uint32_t *)pBuff, FifoCnt);
    AD5940_INTCClrFlag(AFEINTSRC_DATAFIFOTHRESH);
    AppDPVRegModify(pBuff, &FifoCnt);
    AppDPVDataProcess((int32_t*)pBuff, &FifoCnt);
    *pCount = FifoCnt; return 0;
  }
  if(IntFlag & AFEINTSRC_ENDSEQ)
  {
    FifoCnt = AD5940_FIFOGetCnt(); AD5940_INTCClrFlag(AFEINTSRC_ENDSEQ);
    AD5940_FIFORd((uint32_t *)pBuff, FifoCnt);
    AppDPVDataProcess((int32_t*)pBuff, &FifoCnt);
    *pCount = FifoCnt; AppDPVCtrl(APPCTRL_STOPNOW, 0);
  }
  return 0;
}
