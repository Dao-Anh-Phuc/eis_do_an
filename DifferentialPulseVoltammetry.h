/*!
 *****************************************************************************
 @file:    DifferentialPulseVoltammetry.h
 @author:  $Author: Your Name $
 @brief:   Differential Pulse Voltammetry header file.
 @version: $Revision: 766 $
 @date:    $Date: 2023-12-07 $
 ******************************************************************************/
#ifndef _DPVTEST_H_
#define _DPVTEST_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include "ad5940.h"
#include <stdio.h>
#include "string.h"
#include "math.h"

/* Do not modify following parameters */
#define ALIGIN_VOLT2LSB     0   
#define DAC12BITVOLT_1LSB   (2200.0f/4095)  //mV
#define DAC6BITVOLT_1LSB    (DAC12BITVOLT_1LSB*64)  //mV

/**
 * DPV State enum - khác với SWV
 */
typedef enum {
    DPV_STATE0 = 0,     /* Initialization */
    DPV_STATE1,         /* Base potential (before pulse) */
    DPV_STATE2,         /* Pulse potential (during pulse) */
    DPV_STATE3,         /* Sample after pulse */
    DPV_STOP            /* Stop measurement */
} DPVState_Type;

/**
 * The DPV application related parameter structure
 */
typedef struct
{
    /* Common configurations */
    BoolFlag  bParaChanged;
    uint32_t  SeqStartAddr;
    uint32_t  MaxSeqLen;
    uint32_t  SeqStartAddrCal;
    uint32_t  MaxSeqLenCal;
    
    /* Application related parameters */ 
    float     LFOSCClkFreq;
    float     SysClkFreq;
    float     AdcClkFreq;
    float     RcalVal;
    float     ADCRefVolt;
    
    /* DPV Signal Parameters */
    float     RampStartVolt;        /* Start voltage (mV) */
    float     RampPeakVolt;         /* Peak voltage (mV) */
    float     VzeroStart;           /* Vzero start (mV) */
    float     VzeroPeak;            /* Vzero peak (mV) */
    float     StepIncrement;        /* Step size (mV) */
    float     PulseAmplitude;       /* Pulse amplitude (mV) */
    float     PulsePeriod;          /* Period between pulses (ms) */
    float     PulseWidth;           /* Pulse width (ms) */
    float     SampleDelay1;         /* Delay before pulse sampling (ms) */
    float     SampleDelay2;         /* Delay after pulse sampling (ms) */
    uint32_t  StepNumber;           /* Total steps */
    
    /* Hardware config */
    uint32_t  LPTIARtiaSel;
    float     ExternalRtiaValue;
    uint32_t  AdcPgaGain;
    uint8_t   ADCSinc3Osr;
    uint32_t  FifoThresh;
    
    /* Private variables */
    BoolFlag  DPVInited;
    fImpPol_Type  RtiaValue;
    SEQInfo_Type  InitSeqInfo;
    SEQInfo_Type  ADCSeqInfo;
    SEQInfo_Type  DACSeqInfo;
    BoolFlag      bFirstDACSeq;
    
    /* DPV Runtime variables */
    DPVState_Type DPVState;
    uint32_t  CurrStepPos;
    float     CurrRampCode;
    float     DACCodePerStep;       /* DAC code for pulse amplitude */
    float     DACCodePerRamp;       /* DAC code for step increment */
    uint32_t  CurrVzeroCode;
    BoolFlag  bInPulse;             /* TRUE = in pulse, FALSE = base */
    BoolFlag  bSampleAfterPulse;    /* Sample timing flag */
    BoolFlag  StopRequired;
    
} AppDPVCfg_Type;

/* Control commands */
#define APPCTRL_START          0
#define APPCTRL_STOPNOW        1
#define APPCTRL_STOPSYNC       2
#define APPCTRL_SHUTDOWN       3

/* Function prototypes */
AD5940Err AppDPVInit(uint32_t *pBuffer, uint32_t BufferSize);
AD5940Err AppDPVGetCfg(void *pCfg);
AD5940Err AppDPVISR(void *pBuff, uint32_t *pCount);
AD5940Err AppDPVCtrl(uint32_t Command, void *pPara);

#ifdef __cplusplus
}
#endif
#endif