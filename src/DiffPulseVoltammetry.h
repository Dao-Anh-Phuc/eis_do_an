/*!
 *****************************************************************************
 @file:    DiffPulseVoltammetry.h
 @brief:   Differential Pulse Voltammetry (DPV) header file (SWV-style)
 *****************************************************************************
*/
#ifndef _DPVTEST_H_
#define _DPVTEST_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "ad5940.h"
#include <stdio.h>
#include "string.h"
#include "math.h"

/* Do not modify following parameters */
#define ALIGIN_VOLT2LSB     0
#define DAC12BITVOLT_1LSB   (2200.0f/4095)          /* mV */
#define DAC6BITVOLT_1LSB    (DAC12BITVOLT_1LSB*64)  /* mV */

/* The DPV application related parameter structure (SWV-format) */
typedef struct
{
/* Common configurations for all kinds of Application. */
  BoolFlag  bParaChanged;
  uint32_t  SeqStartAddr;
  uint32_t  MaxSeqLen;
  uint32_t  SeqStartAddrCal;     /* not used for DPV */
  uint32_t  MaxSeqLenCal;        /* not used for DPV */

/* Application related parameters */
  float     LFOSCClkFreq;
  float     SysClkFreq;
  float     AdcClkFreq;
  float     RcalVal;
  float     ADCRefVolt;

  /* Describe Staircase (baseline) */
  float     RampStartVolt;       /**< start potential (mV) */
  float     RampPeakVolt;        /**< end potential (mV)   */
  float     VzeroStart;          /**< mV, typically 2400   */
  float     VzeroPeak;           /**< mV, typically 200    */
  uint32_t  StepNumber;          /**< total steps (<=4095) */
  float     RampStep_mV;         /**< step size (mV) */
  BoolFlag  bRampOneDir;         /**< keep for compatibility */
  BoolFlag  bDACCodeInc;         /**< ramp direction flag   */

  /* DPV pulse (replaces SWV square-wave fields) */
  float     PulseAmp_mV;         /**< Epulse (mV)           */
  float     PulseWidth_ms;       /**< t_pulse (ms)          */
  float     PrePulseWait_ms;     /**< wait on baseline before pulse (ms) */
  float     HoldAfterPulse_ms;   /**< optional hold after pulse off (ms) */
  BoolFlag  bPulsePositive;      /**< TRUE:+Epulse, FALSE:-Epulse */

  /* Sampling windows (new for DPV) */
  uint8_t   NAvgBase;            /**< samples at end of baseline */
  uint8_t   NAvgPulse;           /**< samples at end of pulse    */
  float     GuardBase_ms;        /**< guard time before BASE sample */
  float     GuardPulse_ms;       /**< guard time before PULSE sample */

  /* Receive path configuration */
  float     SampleDelay;         /**< kept for compatibility (unused) */
  uint32_t  LPTIARtiaSel;
  float     ExternalRtiaValue;
  uint32_t  AdcPgaGain;          /**< ensure input within ±1.5V */
  uint8_t   ADCSinc3Osr;
  uint8_t   ADCSinc2Osr;         /**< optional if SWV uses it   */

  /* Digital related */
  uint32_t  FifoThresh;

/* Private variables for internal usage (same style as SWV) */
  BoolFlag      DPVInited;
  fImpPol_Type  RtiaValue;
  SEQInfo_Type  InitSeqInfo;
  SEQInfo_Type  ADCSeqInfo;
  BoolFlag      bFirstDACSeq;
  SEQInfo_Type  DACSeqInfo;

  uint32_t  CurrStepPos;
  float     DACCodePerRamp;      /**< DAC codes per ramp step   */
  float     DACCodePulse;        /**< DAC codes for Epulse      */
  float     CurrRampCode;
  uint32_t  CurrVzeroCode;
  BoolFlag  StopRequired;

  /* DPV state machine (kept SWV naming style for variable RampState) */
  enum _DPVState {
    DPV_STATE0_IDLE = 0,    /* quiet/prewait */
    DPV_STATE1_BASE,        /* baseline hold */
    DPV_STATE2_SAMPLE_BASE, /* take BASE sample(s) */
    DPV_STATE3_PULSE_ON,    /* apply Epulse */
    DPV_STATE4_SAMPLE_PLS,  /* take PULSE sample(s) */
    DPV_STATE5_PULSE_OFF,   /* remove Epulse */
    DPV_STATE6_STEP_NEXT,   /* step baseline */
    DPV_STOP
  } RampState;

  /* cached timing in ticks (computed from *_ms and LFOSC) */
  uint32_t  TicksBaseWait, TicksPulseWidth, TicksHoldAfter;
  uint32_t  TicksGuardBase, TicksGuardPulse;

}AppDPVCfg_Type;

#define APPCTRL_START          0
#define APPCTRL_STOPNOW        1
#define APPCTRL_STOPSYNC       2
#define APPCTRL_SHUTDOWN       3

AD5940Err AppDPVInit(uint32_t *pBuffer, uint32_t BufferSize);
AD5940Err AppDPVGetCfg(void *pCfg);
AD5940Err AppDPVISR(void *pBuff, uint32_t *pCount);
AD5940Err AppDPVCtrl(uint32_t Command, void *pPara);
void AD5940_McuSetLow(void);
void AD5940_McuSetHigh(void);

#ifdef __cplusplus
}
#endif
#endif /* _DPVTEST_H_ */
