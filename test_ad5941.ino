//EIS / 2#100?10000/50|1$0!
//EIS / 3#100?100000/100|1$0!
// CV / 1#-200?600/100|1$0!
//SWV / 4#-200?600/10|25$10!
//CA  / 5#100?100/200|100$0!
//dpv / 7#-200?600/100|50$50!
#include <Arduino.h>
#include "ad5940.h"
#include "RampTest.h"
#include "Impedance3E.h"
#include "Impedance.h"
#include "SqrWaveVoltammetry.h"
#include "ChronoAmperometric.h"
#include "DifferentialPulseVoltammetry.h"

unsigned long timeStart = 0;
unsigned long timeNow = 0;
uint32_t time1 = 0;

#define APPBUFF_DPV_SIZE 1024
uint32_t AppDPVBuff[APPBUFF_DPV_SIZE];

#define AppCVBuff_CV_SIZE 1024
uint32_t AppCVBuff[AppCVBuff_CV_SIZE];
float LFOSCFreq;    /* Measured LFOSC frequency */

#define APPBUFF_EIS_SIZE 512
uint32_t AppEISBuff[APPBUFF_EIS_SIZE];

#define APPBUFF_EIS_SIZE_3E 512
uint32_t AppEISBuff_3E[APPBUFF_EIS_SIZE_3E];

#define APPBUFF_SWV_SIZE 1024
uint32_t AppSWVBuff[APPBUFF_SWV_SIZE];

#define n 3
#define APPBUFF_AMP_SIZE 1024
uint32_t AppAMPBuff[n][APPBUFF_AMP_SIZE];

float S_Vol, E_Vol;
//SWV
uint32_t Amplitude, RampIncrement, Freq;

//AMP
uint32_t IntCount = 0;
int Time_Run;
int Bias_Vol;
int Time_Interval;

//
int countRepeat, StepNumber = 0, RepeatTimes = 0;
BoolFlag logEn = bFALSE;
String inputString = "";
byte moc1, moc2, moc3, moc4, moc5;

static int32_t RampShowResult(float *pData, uint32_t DataCount)
{
  static uint32_t index;
  /* Print data*/
  for(int i = 0; i < DataCount; i++)
  {
    printf("%d;%.6f\n",index++, pData[i]);
  }
  return 0;
}
int32_t ImpedanceShowResult(uint32_t *pData, uint32_t DataCount)
{
  float freq;

  fImpPol_Type *pImp = (fImpPol_Type*)pData;
  AppIMPCtrl(IMPCTRL_GETFREQ, &freq);

  if (S_Vol == E_Vol)
  {
    timeNow = millis() - timeStart;
    printf("%lu;", timeNow);
  }
  else
  {
    printf("%.2f;", freq);
  }
  /*Process data*/
  float phase;
  for(int i=0;i<DataCount;i++)
  {
    phase = pImp[i].Phase*180/MATH_PI;
    if(phase > 180) phase = phase - 360;
    else if (phase < -180) phase = phase + 360;
    //printf("%f;%f\n", pImp[i].Magnitude, pImp[i].Phase*180/MATH_PI);
    printf("%f;%f\n", pImp[i].Magnitude*1.1, phase);
  }
  return 0;
}

int32_t Impedance3EShowResult(uint32_t *pData, uint32_t DataCount)
{
  float freq;
  float RzReal;
  float RzImage;
  float Radian_Phase;

  fImpPol_Type *pImp = (fImpPol_Type*)pData;
  AppIMP3ECtrl(IMP3ECTRL_GETFREQ, &freq);

  if (S_Vol == E_Vol)
  {
    timeNow = millis() - timeStart;
    printf("%lu;", timeNow);
  }
  else
  {
   // printf("%.2f;", freq);
  }
  /*Process data*/
  float phase;
  for(int i=0;i<DataCount;i++)
  {
    phase = pImp[i].Phase*180/MATH_PI;
    if(phase > 180) phase = phase - 360;
    else if (phase < -180) phase = phase + 360;
    //printf("%f;%f\n", pImp[i].Magnitude, pImp[i].Phase*180/MATH_PI);
    Radian_Phase = radians(phase);
    RzReal = cos(Radian_Phase) * pImp[i].Magnitude*1.1;
    RzImage = -sin(Radian_Phase) * pImp[i].Magnitude;
    printf("%.2f;", freq);
    printf("%f;%f\n", RzReal,RzImage);
    //printf("%f;%f\n", pImp[i].Magnitude, phase);
  }
  return 0;
}

static int32_t RampShowResultSWV(float *pData, uint32_t DataCount)
{
  static uint32_t index;
  /* Print data*/
  for(int i = 0; i < DataCount; i++)
  {
    printf("%d;%.6f\n",index++, pData[i]);
  }
  return 0;
}

int32_t AMPShowResult(float *pData, uint32_t DataCount, float Time)
{
  for(int i=0;i<DataCount;i++)
  {
    printf("%.2f;%.4f\n", Time, pData[i]);
  }
  return 0;
}

static int32_t AD5940CVPlatformCfg(void)
{
  CLKCfg_Type clk_cfg;
  SEQCfg_Type seq_cfg;
  FIFOCfg_Type fifo_cfg;
  AGPIOCfg_Type gpio_cfg;
  LFOSCMeasure_Type LfoscMeasure;

  /* Use hardware reset */
  AD5940_HWReset();
  AD5940_Initialize();    /* Call this right after AFE reset */
  /* Platform configuration */
  /* Step1. Configure clock */
  clk_cfg.HFOSCEn = bTRUE;
  clk_cfg.HFXTALEn = bFALSE;
  clk_cfg.LFOSCEn = bTRUE;
  clk_cfg.HfOSC32MHzMode = bFALSE;
  clk_cfg.SysClkSrc = SYSCLKSRC_HFOSC;
  clk_cfg.SysClkDiv = SYSCLKDIV_1;
  clk_cfg.ADCCLkSrc = ADCCLKSRC_HFOSC;
  clk_cfg.ADCClkDiv = ADCCLKDIV_1;
  AD5940_CLKCfg(&clk_cfg);
  /* Step2. Configure FIFO and Sequencer*/
  /* Configure FIFO and Sequencer */
  fifo_cfg.FIFOEn = bTRUE;           /* We will enable FIFO after all parameters configured */
  fifo_cfg.FIFOMode = FIFOMODE_FIFO;
  fifo_cfg.FIFOSize = FIFOSIZE_2KB;   /* 2kB for FIFO, The reset 4kB for sequencer */
  fifo_cfg.FIFOSrc = FIFOSRC_SINC3;   /* */
  fifo_cfg.FIFOThresh = 4;            /*  Don't care, set it by application paramter */
  AD5940_FIFOCfg(&fifo_cfg);
  seq_cfg.SeqMemSize = SEQMEMSIZE_4KB;  /* 4kB SRAM is used for sequencer, others for data FIFO */
  seq_cfg.SeqBreakEn = bFALSE;
  seq_cfg.SeqIgnoreEn = bTRUE;
  seq_cfg.SeqCntCRCClr = bTRUE;
  seq_cfg.SeqEnable = bFALSE;
  seq_cfg.SeqWrTimer = 0;
  AD5940_SEQCfg(&seq_cfg);
  /* Step3. Interrupt controller */
  AD5940_INTCCfg(AFEINTC_1, AFEINTSRC_ALLINT, bTRUE);   /* Enable all interrupt in INTC1, so we can check INTC flags */
  AD5940_INTCClrFlag(AFEINTSRC_ALLINT);
  AD5940_INTCCfg(AFEINTC_0, AFEINTSRC_DATAFIFOTHRESH|AFEINTSRC_ENDSEQ|AFEINTSRC_CUSTOMINT0, bTRUE);
  AD5940_INTCClrFlag(AFEINTSRC_ALLINT);
  /* Step4: Configure GPIO */
  gpio_cfg.FuncSet = GP0_INT|GP1_SLEEP|GP2_SYNC;  /* GPIO1 indicates AFE is in sleep state. GPIO2 indicates ADC is sampling. */
  gpio_cfg.InputEnSet = 0;
  gpio_cfg.OutputEnSet = AGPIO_Pin0|AGPIO_Pin1|AGPIO_Pin2;
  gpio_cfg.OutVal = 0;
  gpio_cfg.PullEnSet = 0;
  AD5940_AGPIOCfg(&gpio_cfg);
  /* Measure LFOSC frequency */
  /**@note Calibrate LFOSC using system clock. The system clock accuracy decides measurement accuracy. Use XTAL to get better result. */
  LfoscMeasure.CalDuration = 1000.0;  /* 1000ms used for calibration. */
  LfoscMeasure.CalSeqAddr = 0;        /* Put sequence commands from start address of SRAM */
  LfoscMeasure.SystemClkFreq = 16000000.0f; /* 16MHz in this firmware. */
  AD5940_LFOSCMeasure(&LfoscMeasure, &LFOSCFreq);
  printf("LFOSC Freq:%f\n", LFOSCFreq);
  AD5940_SleepKeyCtrlS(SLPKEY_UNLOCK);         /*  */
  return 0;
}
static int32_t AD5940EISPlatformCfg(void)
{
  CLKCfg_Type clk_cfg;
  FIFOCfg_Type fifo_cfg;
  AGPIOCfg_Type gpio_cfg;

  /* Use hardware reset */
  AD5940_HWReset();
  AD5940_Initialize();
  /* Platform configuration */
  /* Step1. Configure clock */
  clk_cfg.ADCClkDiv = ADCCLKDIV_1;
  clk_cfg.ADCCLkSrc = ADCCLKSRC_HFOSC;
  clk_cfg.SysClkDiv = SYSCLKDIV_1;
  clk_cfg.SysClkSrc = SYSCLKSRC_HFOSC;
  clk_cfg.HfOSC32MHzMode = bFALSE;
  clk_cfg.HFOSCEn = bTRUE;
  clk_cfg.HFXTALEn = bFALSE;
  clk_cfg.LFOSCEn = bTRUE;
  AD5940_CLKCfg(&clk_cfg);
  /* Step2. Configure FIFO and Sequencer*/
  fifo_cfg.FIFOEn = bFALSE;
  fifo_cfg.FIFOMode = FIFOMODE_FIFO;
  fifo_cfg.FIFOSize = FIFOSIZE_4KB;                       /* 4kB for FIFO, The reset 2kB for sequencer */
  fifo_cfg.FIFOSrc = FIFOSRC_DFT;
  fifo_cfg.FIFOThresh = 4;//AppIMPCfg.FifoThresh;        /* DFT result. One pair for RCAL, another for Rz. One DFT result have real part and imaginary part */
  AD5940_FIFOCfg(&fifo_cfg);
  fifo_cfg.FIFOEn = bTRUE;
  AD5940_FIFOCfg(&fifo_cfg);
  
  /* Step3. Interrupt controller */
  AD5940_INTCCfg(AFEINTC_1, AFEINTSRC_ALLINT, bTRUE);   /* Enable all interrupt in INTC1, so we can check INTC flags */
  AD5940_INTCClrFlag(AFEINTSRC_ALLINT);
  AD5940_INTCCfg(AFEINTC_0, AFEINTSRC_DATAFIFOTHRESH, bTRUE); 
  AD5940_INTCClrFlag(AFEINTSRC_ALLINT);
  /* Step4: Reconfigure GPIO */
  gpio_cfg.FuncSet = GP0_INT|GP1_SLEEP|GP2_SYNC;
  gpio_cfg.InputEnSet = 0;
  gpio_cfg.OutputEnSet = AGPIO_Pin0|AGPIO_Pin1|AGPIO_Pin2;
  gpio_cfg.OutVal = 0;
  gpio_cfg.PullEnSet = 0;
  AD5940_AGPIOCfg(&gpio_cfg);
  AD5940_SleepKeyCtrlS(SLPKEY_UNLOCK);  /* Allow AFE to enter sleep mode. */
  return 0;
}

static int32_t AD5940EIS3EPlatformCfg(void)
{
  CLKCfg_Type clk_cfg;
  FIFOCfg_Type fifo_cfg;
  AGPIOCfg_Type gpio_cfg;

  /* Use hardware reset */
  AD5940_HWReset();
  AD5940_Initialize();
  /* Platform configuration */
  /* Step1. Configure clock */
  clk_cfg.ADCClkDiv = ADCCLKDIV_1;
  clk_cfg.ADCCLkSrc = ADCCLKSRC_HFOSC;
  clk_cfg.SysClkDiv = SYSCLKDIV_1;
  clk_cfg.SysClkSrc = SYSCLKSRC_HFOSC;
  clk_cfg.HfOSC32MHzMode = bFALSE;
  clk_cfg.HFOSCEn = bTRUE;
  clk_cfg.HFXTALEn = bFALSE;
  clk_cfg.LFOSCEn = bTRUE;
  AD5940_CLKCfg(&clk_cfg);
  /* Step2. Configure FIFO and Sequencer*/
  fifo_cfg.FIFOEn = bFALSE;
  fifo_cfg.FIFOMode = FIFOMODE_FIFO;
  fifo_cfg.FIFOSize = FIFOSIZE_4KB;                       /* 4kB for FIFO, The reset 2kB for sequencer */
  fifo_cfg.FIFOSrc = FIFOSRC_DFT;
  fifo_cfg.FIFOThresh = 6;//AppIMPCfg.FifoThresh;        /* DFT result. One pair for RCAL, another for Rz. One DFT result have real part and imaginary part */
  AD5940_FIFOCfg(&fifo_cfg);
  fifo_cfg.FIFOEn = bTRUE;
  AD5940_FIFOCfg(&fifo_cfg);
  
  /* Step3. Interrupt controller */
  AD5940_INTCCfg(AFEINTC_1, AFEINTSRC_ALLINT, bTRUE);   /* Enable all interrupt in INTC1, so we can check INTC flags */
  AD5940_INTCClrFlag(AFEINTSRC_ALLINT);
  AD5940_INTCCfg(AFEINTC_0, AFEINTSRC_DATAFIFOTHRESH, bTRUE); 
  AD5940_INTCClrFlag(AFEINTSRC_ALLINT);
  /* Step4: Reconfigure GPIO */
  gpio_cfg.FuncSet = GP0_INT|GP1_SLEEP|GP2_SYNC;
  gpio_cfg.InputEnSet = 0;
  gpio_cfg.OutputEnSet = AGPIO_Pin0|AGPIO_Pin1|AGPIO_Pin2;
  gpio_cfg.OutVal = 0;
  gpio_cfg.PullEnSet = 0;
  AD5940_AGPIOCfg(&gpio_cfg);
  AD5940_SleepKeyCtrlS(SLPKEY_UNLOCK);  /* Allow AFE to enter sleep mode. */
  return 0;
}

void AD5940RampStructInit(float S_Vol, float E_Vol, int StepNumber)
{
  AppRAMPCfg_Type *pRampCfg;

  AppRAMPGetCfg(&pRampCfg);
  /* Step1: configure general parmaters */
  pRampCfg->SeqStartAddr = 0x10;                /* leave 16 commands for LFOSC calibration.  */
  pRampCfg->MaxSeqLen = 1024-0x10;              /* 4kB/4 = 1024  */
  pRampCfg->RcalVal = 200.0;                  /* 10kOhm RCAL */
  pRampCfg->ADCRefVolt = 1820.0f;               /* The real ADC reference voltage. Measure it from capacitor C12 with DMM. */
  pRampCfg->FifoThresh = 10;                   /* Maximum value is 2kB/4-1 = 512-1. Set it to higher value to save power. */
  pRampCfg->SysClkFreq = 16000000.0f;           /* System clock is 16MHz by default */
  pRampCfg->LFOSCClkFreq = LFOSCFreq;           /* LFOSC frequency */
  /* Configure ramp signal parameters */
  pRampCfg->RampStartVolt =  S_Vol;           /* -1V */
  pRampCfg->RampPeakVolt = E_Vol;           /* +1V */
  pRampCfg->VzeroStart = 1300.0f;               /* 1.3V */
  pRampCfg->VzeroPeak = 1300.0f;                /* 1.3V */
  pRampCfg->StepNumber = StepNumber;                   /* Total steps. Equals to ADC sample number. Limited to 4095 */
  pRampCfg->RampDuration = 24*1000;            /* X * 1000, where x is total duration of ramp signal. Unit is ms. */
  pRampCfg->SampleDelay = 7.0f;                 /* 7ms. Time between update DAC and ADC sample. Unit is ms. */
  pRampCfg->LPTIARtiaSel = LPTIARTIA_4K;       /* Maximum current decides RTIA value */
  pRampCfg->LPTIARloadSel = LPTIARLOAD_SHORT;
  pRampCfg->AdcPgaGain = ADCPGA_1P5;
}
void AD5940ImpedanceStructInit(float S_Freq, float E_Freq, int numPoints, BoolFlag logEn)
{
  AppIMPCfg_Type *pImpedanceCfg;
  
  AppIMPGetCfg(&pImpedanceCfg);
  /* Step1: configure initialization sequence Info */
  pImpedanceCfg->SeqStartAddr = 0;
  pImpedanceCfg->MaxSeqLen = 512; /* @todo add checker in function */

  pImpedanceCfg->RcalVal = 200.0;
  pImpedanceCfg->SinFreq = 60000.0;
  pImpedanceCfg->FifoThresh = 4;
  
  /* Set switch matrix to onboard(EVAL-AD5940ELECZ) dummy sensor. */
  /* Note the RCAL0 resistor is 10kOhm. */
  pImpedanceCfg->DswitchSel = SWD_CE0;
  pImpedanceCfg->PswitchSel = SWP_RE0;
  pImpedanceCfg->NswitchSel = SWN_AIN0;
  pImpedanceCfg->TswitchSel = SWT_AIN0|SWT_TRTIA;
  /* The dummy sensor is as low as 5kOhm. We need to make sure RTIA is small enough that HSTIA won't be saturated. */
  pImpedanceCfg->HstiaRtiaSel = HSTIARTIA_200;  
  
  /* Configure the sweep function. */
  pImpedanceCfg->SweepCfg.SweepEn = bTRUE;
  pImpedanceCfg->SweepCfg.SweepStart = S_Freq;  /* Start from 1kHz */
  pImpedanceCfg->SweepCfg.SweepStop = E_Freq;   /* Stop at 100kHz */
  pImpedanceCfg->SweepCfg.SweepPoints = numPoints;    /* Points is 101 */
  pImpedanceCfg->SweepCfg.SweepLog = logEn;
  /* Configure Power Mode. Use HP mode if frequency is higher than 80kHz. */
  pImpedanceCfg->PwrMod = AFEPWR_HP;
  /* Configure filters if necessary */
  pImpedanceCfg->ADCSinc3Osr = ADCSINC3OSR_2;   /* Sample rate is 800kSPS/2 = 400kSPS */
  pImpedanceCfg->DftNum = DFTNUM_16384;
  pImpedanceCfg->DftSrc = DFTSRC_SINC3;
}

void AD5940Impedance3EStructInit(float S_Freq, float E_Freq, int numPoints, BoolFlag logEn)
{
  AppIMP3ECfg_Type *pImpedanceCfg;
  
  AppIMP3EGetCfg(&pImpedanceCfg);
  /* Step1: configure initialization sequence Info */
  pImpedanceCfg->SeqStartAddr = 0;
  pImpedanceCfg->MaxSeqLen = 512; /* @todo add checker in function */

  pImpedanceCfg->RcalVal = 200.0;
  pImpedanceCfg->SinFreq = 1000.0;
  pImpedanceCfg->FifoThresh = 6;

  /* Configure Excitation Waveform 
  *
  *  Output waveform = DacVoltPP * ExcitBufGain * HsDacGain 
  *   
  *   = 300 * 0.25 * 0.2 = 15mV pk-pk
  *
  */
  pImpedanceCfg->DacVoltPP = 300; /* Maximum value is 600mV*/
  pImpedanceCfg->ExcitBufGain = EXCITBUFGAIN_0P25;
  pImpedanceCfg->HsDacGain = HSDACGAIN_0P2;
  
  /* Set switch matrix to onboard(EVAL-AD5940ELECZ) dummy sensor. */
  /* Note the RCAL0 resistor is 10kOhm. */
//  pImpedanceCfg->DswitchSel = SWD_CE0;
//  pImpedanceCfg->PswitchSel = SWP_RE0;
//  pImpedanceCfg->NswitchSel = SWN_AIN0;
//  pImpedanceCfg->TswitchSel = SWT_AIN0|SWT_TRTIA;
  /* Set switch matrix to onboard(EVAL-AD5940ELECZ) gas sensor. */
  pImpedanceCfg->DswitchSel = SWD_CE0;
  pImpedanceCfg->PswitchSel = SWP_RE0;
  pImpedanceCfg->NswitchSel = SWN_SE0LOAD;
  pImpedanceCfg->TswitchSel = SWT_SE0LOAD;

  /* The dummy sensor is as low as 5kOhm. We need to make sure RTIA is small enough that HSTIA won't be saturated. */
  pImpedanceCfg->HstiaRtiaSel = HSTIARTIA_200;  
  pImpedanceCfg->BiasVolt = 0.0;                      // no DC bias.
  // pImpedanceCfg->AdcPgaGain = ADCPGA_1;           // add gain ADC = 1.
  
  /* Configure the sweep function. */
  pImpedanceCfg->SweepCfg.SweepEn = bTRUE;
  pImpedanceCfg->SweepCfg.SweepStart = S_Freq;  /* Start from 1kHz */
  pImpedanceCfg->SweepCfg.SweepStop = E_Freq;   /* Stop at 100kHz */
  pImpedanceCfg->SweepCfg.SweepPoints = numPoints;    /* Points is 101 */
  pImpedanceCfg->SweepCfg.SweepLog = logEn;
  /* Configure Power Mode. Use HP mode if frequency is higher than 80kHz. */
  pImpedanceCfg->PwrMod = AFEPWR_HP;
  /* Configure filters if necessary */
  pImpedanceCfg->ADCSinc3Osr = ADCSINC3OSR_4;   /* Sample rate is 800kSPS/2 = 400kSPS */
  pImpedanceCfg->DftNum = DFTNUM_16384;
  pImpedanceCfg->DftSrc = DFTSRC_SINC3;
}

void AD5940_CV_Main()
{
  uint32_t temp;
  uint32_t count = 0;
  AppRAMPCfg_Type *pRampCfg;
  
  AD5940CVPlatformCfg();
  AD5940RampStructInit(S_Vol, E_Vol, StepNumber);

  AppRAMPInit(AppCVBuff, AppCVBuff_CV_SIZE);    /* Initialize RAMP application. Provide a buffer, which is used to store sequencer commands */
  AppRAMPCtrl(APPCTRL_START, 0);          /* Control IMP measurement to start. Second parameter has no meaning with this command. */

  while (count < (StepNumber * RepeatTimes))
  {
    AppRAMPGetCfg(&pRampCfg);
    if(AD5940_GetMCUIntFlag())
    {
      AD5940_ClrMCUIntFlag();
      temp = AppCVBuff_CV_SIZE;
      AppRAMPISR(AppCVBuff, &temp);
      RampShowResult((float*)AppCVBuff, temp);
      
      count += temp;
    }
    /* Repeat Measurement continuously*/
    if(pRampCfg->bTestFinished ==bTRUE)
    {
      AD5940_Delay10us(20000);
      pRampCfg->bTestFinished = bFALSE;
      AD5940_SEQCtrlS(bTRUE);   /* Enable sequencer, and wait for trigger */
      AppRAMPCtrl(APPCTRL_START, 0);
    }
  }
}

void AD5940_EIS_Main(void)
{
  uint32_t temp;  
  AD5940EISPlatformCfg();
  AD5940ImpedanceStructInit(S_Vol, E_Vol, StepNumber, logEn);
  
  AppIMPInit(AppEISBuff, APPBUFF_EIS_SIZE);    /* Initialize IMP application. Provide a buffer, which is used to store sequencer commands */
  AppIMPCtrl(IMPCTRL_START, 0);          /* Control IMP measurement to start. Second parameter has no meaning with this command. */

  if (S_Vol == E_Vol)
  {
    timeStart = millis();
    timeNow = 0;
    bool measure = true;
    while(measure)
    {
      if(AD5940_GetMCUIntFlag())
      {
        AD5940_ClrMCUIntFlag();
        temp = APPBUFF_EIS_SIZE;
        AppIMPISR(AppEISBuff, &temp);
        ImpedanceShowResult(AppEISBuff, temp);
      }
      if (Serial.available())
      {
        char inChar = (char)Serial.read();
        if (inChar == 's')
        {
          measure = false;
        }
      }
    }
  }
  else
  {
    uint32_t count = 0;
    while(count < (StepNumber * RepeatTimes))
    {
      if(AD5940_GetMCUIntFlag())
      {
        AD5940_ClrMCUIntFlag();
        temp = APPBUFF_EIS_SIZE;
        AppIMPISR(AppEISBuff, &temp);
        ImpedanceShowResult(AppEISBuff, temp);
        count += temp;
      }
    }
  }
}

void AD5940_EIS3E_Main(void)
{
  uint32_t temp;  
  AD5940EIS3EPlatformCfg();
  AD5940Impedance3EStructInit(S_Vol, E_Vol, StepNumber, logEn);
  
  AppIMP3EInit(AppEISBuff_3E, APPBUFF_EIS_SIZE_3E);    /* Initialize IMP application. Provide a buffer, which is used to store sequencer commands */
  AppIMP3ECtrl(IMP3ECTRL_START, 0);          /* Control IMP measurement to start. Second parameter has no meaning with this command. */

  if (S_Vol == E_Vol)
  {
    timeStart = millis();
    timeNow = 0;
    bool measure = true;
    while(measure)
    {
      if(AD5940_GetMCUIntFlag())
      {
        AD5940_ClrMCUIntFlag();
        temp = APPBUFF_EIS_SIZE_3E;
        AppIMP3EISR(AppEISBuff_3E, &temp);
        Impedance3EShowResult(AppEISBuff_3E, temp);
      }
      if (Serial.available())
      {
        char inChar = (char)Serial.read();
        if (inChar == 's')
        {
          measure = false;
        }
      }
    }
  }
  else
  {
    uint32_t count = 0;
    while(count < (StepNumber * RepeatTimes))
    {
      if(AD5940_GetMCUIntFlag())
      {
        AD5940_ClrMCUIntFlag();
        temp = APPBUFF_EIS_SIZE_3E;
        AppIMP3EISR(AppEISBuff_3E, &temp);
        Impedance3EShowResult(AppEISBuff_3E, temp);
        count += temp;
      }
    }
  }
}

static int32_t AD5940_Sqr_PlatformCfg(void)
{
  CLKCfg_Type clk_cfg;
  SEQCfg_Type seq_cfg;  
  FIFOCfg_Type fifo_cfg;
  AGPIOCfg_Type gpio_cfg;
  LFOSCMeasure_Type LfoscMeasure;

  /* Use hardware reset */
  AD5940_HWReset();
  AD5940_Initialize();    /* Call this right after AFE reset */
  
  /* Platform configuration */
  /* Step1. Configure clock */
  clk_cfg.HFOSCEn = bTRUE;
  clk_cfg.HFXTALEn = bFALSE;
  clk_cfg.LFOSCEn = bTRUE;
  clk_cfg.HfOSC32MHzMode = bFALSE;
  clk_cfg.SysClkSrc = SYSCLKSRC_HFOSC;
  clk_cfg.SysClkDiv = SYSCLKDIV_1;
  clk_cfg.ADCCLkSrc = ADCCLKSRC_HFOSC;
  clk_cfg.ADCClkDiv = ADCCLKDIV_1;
  AD5940_CLKCfg(&clk_cfg);
  /* Step2. Configure FIFO and Sequencer*/
  fifo_cfg.FIFOEn = bTRUE;           /* We will enable FIFO after all parameters configured */
  fifo_cfg.FIFOMode = FIFOMODE_FIFO;
  fifo_cfg.FIFOSize = FIFOSIZE_4KB;   /* 2kB for FIFO, The reset 4kB for sequencer */
  fifo_cfg.FIFOSrc = FIFOSRC_SINC3;   /* */
  fifo_cfg.FIFOThresh = 4;            /*  Don't care, set it by application paramter */
  AD5940_FIFOCfg(&fifo_cfg);
  seq_cfg.SeqMemSize = SEQMEMSIZE_2KB;  /* 4kB SRAM is used for sequencer, others for data FIFO */
  seq_cfg.SeqBreakEn = bFALSE;
  seq_cfg.SeqIgnoreEn = bTRUE;
  seq_cfg.SeqCntCRCClr = bTRUE;
  seq_cfg.SeqEnable = bFALSE;
  seq_cfg.SeqWrTimer = 0;
  AD5940_SEQCfg(&seq_cfg);
  /* Step3. Interrupt controller */
  AD5940_INTCCfg(AFEINTC_1, AFEINTSRC_ALLINT, bTRUE);   /* Enable all interrupt in INTC1, so we can check INTC flags */
  AD5940_INTCClrFlag(AFEINTSRC_ALLINT);
  AD5940_INTCCfg(AFEINTC_0, AFEINTSRC_DATAFIFOTHRESH|AFEINTSRC_ENDSEQ|AFEINTSRC_CUSTOMINT0, bTRUE); 
  AD5940_INTCClrFlag(AFEINTSRC_ALLINT);
  /* Step4: Configure GPIOs */
  gpio_cfg.FuncSet = GP0_INT|GP1_SLEEP|GP2_SYNC;  /* GPIO1 indicates AFE is in sleep state. GPIO2 indicates ADC is sampling. */
  gpio_cfg.InputEnSet = 0;
  gpio_cfg.OutputEnSet = AGPIO_Pin0|AGPIO_Pin1|AGPIO_Pin2;
  gpio_cfg.OutVal = 0;
  gpio_cfg.PullEnSet = 0;
  AD5940_AGPIOCfg(&gpio_cfg);
  /* Measure LFOSC frequency */
  /**@note Calibrate LFOSC using system clock. The system clock accuracy decides measurement accuracy. Use XTAL to get better result. */
  LfoscMeasure.CalDuration = 1000.0;  /* 1000ms used for calibration. */
  LfoscMeasure.CalSeqAddr = 0;        /* Put sequence commands from start address of SRAM */
  LfoscMeasure.SystemClkFreq = 16000000.0f; /* 16MHz in this firmware. */
  AD5940_LFOSCMeasure(&LfoscMeasure, &LFOSCFreq);
  //printf("LFOSC Freq:%f\n", LFOSCFreq);
 // AD5940_SleepKeyCtrlS(SLPKEY_UNLOCK);         /*  */
  return 0;
}

/**
 * @brief The interface for user to change application paramters.
 * @return return 0.
*/
void AD5940_Sqr_RampStructInit(float S_Vol, float E_Vol, uint32_t RampIncrement, uint32_t Amplitude, uint32_t Freq)
{
  AppSWVCfg_Type *pRampCfg;
  
  AppSWVGetCfg(&pRampCfg);
  /* Step1: configure general parmaters */
  pRampCfg->SeqStartAddr = 0x10;                /* leave 16 commands for LFOSC calibration.  */
  pRampCfg->MaxSeqLen = 1024-0x10;              /* 4kB/4 = 1024  */
  pRampCfg->RcalVal = 200;                  /* 10kOhm RCAL */
  pRampCfg->ADCRefVolt = 1820.0f;               /* The real ADC reference voltage. Measure it from capacitor C12 with DMM. */
  pRampCfg->FifoThresh = 10;                   /* Maximum value is 2kB/4-1 = 512-1. Set it to higher value to save power. */
  pRampCfg->SysClkFreq = 16000000.0f;           /* System clock is 16MHz by default */
  pRampCfg->LFOSCClkFreq = LFOSCFreq;           /* LFOSC frequency */
  pRampCfg->AdcPgaGain = ADCPGA_1P5;
  pRampCfg->ADCSinc3Osr = ADCSINC3OSR_4;
  
  /* Step 2:Configure square wave signal parameters */
  pRampCfg->RampStartVolt = S_Vol;     /* Measurement starts at 0V*/
  pRampCfg->RampPeakVolt = E_Vol;          /* Measurement finishes at -0.4V */
  pRampCfg->VzeroStart = 1300.0f;           /* Vzero is voltage on SE0 pin: 1.3V */
  pRampCfg->VzeroPeak = 1300.0f;          /* Vzero is voltage on SE0 pin: 1.3V */
  pRampCfg->Frequency = Freq;                 /* Frequency of square wave in Hz */
  pRampCfg->SqrWvAmplitude = Amplitude;       /* Amplitude of square wave in mV */
  pRampCfg->SqrWvRampIncrement = RampIncrement; /* Increment in mV*/
  pRampCfg->SampleDelay = 49.1f;             /* Time between update DAC and ADC sample. Unit is ms and must be < (1/Frequency)/2 - 0.2*/
  pRampCfg->LPTIARtiaSel = LPTIARTIA_4K;      /* Maximum current decides RTIA value */
  pRampCfg->bRampOneDir = bTRUE;//bTRUE;      /* Only measure ramp in one direction */
}

void AD5940_Sqr_Main(void)
{
  uint32_t temp;  
  uint32_t check = 0;
  uint32_t count = 0;
  AD5940_Sqr_PlatformCfg();
  AD5940_Sqr_RampStructInit(S_Vol, E_Vol, RampIncrement, Amplitude, Freq);
  
  //AD5940_McuSetLow();
  AppSWVInit(AppSWVBuff, APPBUFF_SWV_SIZE);    /* Initialize RAMP application. Provide a buffer, which is used to store sequencer commands */
  
  
  AD5940_Delay10us(100000);   /* Add a delay to allow sensor reach equilibrium befor starting the measurement */
  AppSWVCtrl(APPCTRL_START, 0);          /* Control IMP measurement to start. Second parameter has no meaning with this command. */
  while(count < time1)
  {
    if(AD5940_GetMCUIntFlag())
    {
      AD5940_ClrMCUIntFlag();
      temp = APPBUFF_SWV_SIZE;
      AppSWVISR(AppSWVBuff, &temp);
      
      RampShowResult((float*)AppSWVBuff, temp);
      delay(300);
      count += temp;
      printf("End temp =  %d, count = %d, time1 = %d\n",temp, count, time1);
    }
  }
}

/* Initialize AD5940 AMP basic blocks like clock */
static int32_t AD5940_AMP_PlatformCfg(void)
{
  CLKCfg_Type clk_cfg;
  FIFOCfg_Type fifo_cfg;
  AGPIOCfg_Type gpio_cfg;
  LFOSCMeasure_Type LfoscMeasure;

/* Use hardware reset */
  AD5940_HWReset();

  /* Platform configuration */
  AD5940_Initialize();
  /* Step1. Configure clock */
  clk_cfg.ADCClkDiv = ADCCLKDIV_1;
  clk_cfg.ADCCLkSrc = ADCCLKSRC_HFOSC;
  clk_cfg.SysClkDiv = SYSCLKDIV_1;
  clk_cfg.SysClkSrc = SYSCLKSRC_HFOSC;
  clk_cfg.HfOSC32MHzMode = bFALSE;
  clk_cfg.HFOSCEn = bTRUE;
  clk_cfg.HFXTALEn = bFALSE;
  clk_cfg.LFOSCEn = bTRUE;
  AD5940_CLKCfg(&clk_cfg);
  /* Step2. Configure FIFO and Sequencer*/
  fifo_cfg.FIFOEn = bFALSE;
  fifo_cfg.FIFOMode = FIFOMODE_FIFO;
  fifo_cfg.FIFOSize = FIFOSIZE_4KB;                       /* 4kB for FIFO, The reset 2kB for sequencer */
  fifo_cfg.FIFOSrc = FIFOSRC_DFT;
  fifo_cfg.FIFOThresh = 1;//AppAMPCfg.FifoThresh;        /* DFT result. One pair for RCAL, another for Rz. One DFT result have real part and imaginary part */
  AD5940_FIFOCfg(&fifo_cfg);                             /* Disable to reset FIFO. */
  fifo_cfg.FIFOEn = bTRUE;  
  AD5940_FIFOCfg(&fifo_cfg);                             /* Enable FIFO here */
  
  /* Step3. Interrupt controller */
  AD5940_INTCCfg(AFEINTC_1, AFEINTSRC_ALLINT, bTRUE);           /* Enable all interrupt in Interrupt Controller 1, so we can check INTC flags */
  AD5940_INTCClrFlag(AFEINTSRC_ALLINT);
  AD5940_INTCCfg(AFEINTC_0, AFEINTSRC_DATAFIFOTHRESH|AFEINTSRC_ENDSEQ, bTRUE);   /* Interrupt Controller 0 will control GP0 to generate interrupt to MCU */
  AD5940_INTCClrFlag(AFEINTSRC_ALLINT);
  /* Step4: Reconfigure GPIO */
  gpio_cfg.FuncSet = GP6_SYNC|GP5_SYNC|GP4_SYNC|GP2_TRIG|GP1_SYNC|GP0_INT;
  gpio_cfg.InputEnSet = AGPIO_Pin2;
  gpio_cfg.OutputEnSet = AGPIO_Pin0|AGPIO_Pin1|AGPIO_Pin4|AGPIO_Pin5|AGPIO_Pin6;
  gpio_cfg.OutVal = 0;
  gpio_cfg.PullEnSet = 0;
  AD5940_AGPIOCfg(&gpio_cfg);
  
  AD5940_SleepKeyCtrlS(SLPKEY_UNLOCK);  /* Enable AFE to enter sleep mode. */
  /* Measure LFOSC frequency */
  LfoscMeasure.CalDuration = 1000.0;  /* 1000ms used for calibration. */
  LfoscMeasure.CalSeqAddr = 0;
  LfoscMeasure.SystemClkFreq = 16000000.0f; /* 16MHz in this firmware. */
  AD5940_LFOSCMeasure(&LfoscMeasure, &LFOSCFreq);
  printf("Freq:%f\n", LFOSCFreq); 
  
  return 0;
}

/* !!Change the application parameters here if you want to change it to none-default value */
void AD5940AMPStructInit(int Bias_Vol, float Time_Interval)
{
  float Time_IntervalFloat = Time_Interval / 1000.0;
  AppCHRONOAMPCfg_Type *pAMPCfg; 
  AppCHRONOAMPGetCfg(&pAMPCfg);
  /* Configure general parameters */
  pAMPCfg->WuptClkFreq = LFOSCFreq;         /* Use measured 32kHz clock freq for accurate wake up timer */
  pAMPCfg->SeqStartAddr = 0;
  pAMPCfg->MaxSeqLen = 512;                 /* @todo add checker in function */
  pAMPCfg->RcalVal = 16.5;
  pAMPCfg->NumOfData = -1;                  /* Never stop until you stop it manually by AppAMPCtrl() function */
  
  pAMPCfg->AmpODR = Time_IntervalFloat;
  pAMPCfg->FifoThresh = 1;
  pAMPCfg->ADCRefVolt = 1.82;             /* Measure voltage on VREF_1V8 pin and add here */
  
  pAMPCfg->ExtRtia = bFALSE;      /* Set to true if using external Rtia */
  pAMPCfg->ExtRtiaVal = 10000000; /* Enter external Rtia value here is using one */
  pAMPCfg->LptiaRtiaSel = LPTIARTIA_1K;   /* Select TIA gain resistor. */
  
  pAMPCfg->SensorBias = Bias_Vol;   /* Sensor bias voltage between reference and sense electrodes*/
  pAMPCfg->Vzero = 1100;
  /* Configure Pulse*/
  pAMPCfg->pulseAmplitude = 500;            /* Pulse amplitude on counter electrode (mV) */
  pAMPCfg->pulseLength = 500;               /* Length of voltage pulse in ms */
    
}

void AD5940_AMP_Main(float Time_Run, int Time_Interval)
{
  uint32_t temp[n];
  float Time = 0;
  float Time_Interval_Float = (float)Time_Interval;
  AppCHRONOAMPCfg_Type *pAMPCfg;
  AppCHRONOAMPGetCfg(&pAMPCfg);
  AD5940_AMP_PlatformCfg();
  
  AD5940AMPStructInit(Bias_Vol, Time_Interval_Float); /* Configure your parameters in this function */
  
  AppCHRONOAMPInit(AppAMPBuff[0], APPBUFF_AMP_SIZE);    /* Initialize AMP application. Provide a buffer, which is used to store sequencer commands */
  
  AppCHRONOAMPCtrl(CHRONOAMPCTRL_START, 0); /* Begin standard amperometric measurement after pulse test is complete */
 
  while(Time <= (Time_Run + 0.2))
  {
    
    /* Check if interrupt flag which will be set when interrupt occurred. */
    if(AD5940_GetMCUIntFlag())
    {
      AD5940_ClrMCUIntFlag(); /* Clear this flag */
      temp[IntCount] = APPBUFF_AMP_SIZE;
      AppCHRONOAMPISR(AppAMPBuff[IntCount], &temp[IntCount]); /* Deal with it and provide a buffer to store data we got */
    
        AMPShowResult((float*)AppAMPBuff[0], temp[0], Time);
        delay(Time_Interval);
        Time = Time + (Time_Interval_Float / 1000.0);
    }
  }
}

// dpv 

static int32_t AD5940DPVPlatformCfg(void)
{
  CLKCfg_Type clk_cfg;
  SEQCfg_Type seq_cfg;
  FIFOCfg_Type fifo_cfg;
  AGPIOCfg_Type gpio_cfg;
  LFOSCMeasure_Type LfoscMeasure;

  AD5940_HWReset();
  AD5940_Initialize();
  clk_cfg.HFOSCEn = bTRUE;
  clk_cfg.HFXTALEn = bFALSE;
  clk_cfg.LFOSCEn = bTRUE;
  clk_cfg.HfOSC32MHzMode = bFALSE;
  clk_cfg.SysClkSrc = SYSCLKSRC_HFOSC;
  clk_cfg.SysClkDiv = SYSCLKDIV_1;
  clk_cfg.ADCCLkSrc = ADCCLKSRC_HFOSC;
  clk_cfg.ADCClkDiv = ADCCLKDIV_1;
  AD5940_CLKCfg(&clk_cfg);

  fifo_cfg.FIFOEn = bTRUE;
  fifo_cfg.FIFOMode = FIFOMODE_FIFO;
  fifo_cfg.FIFOSize = FIFOSIZE_2KB;
  fifo_cfg.FIFOSrc = FIFOSRC_SINC3;
  fifo_cfg.FIFOThresh = 4;
  AD5940_FIFOCfg(&fifo_cfg);

  seq_cfg.SeqMemSize = SEQMEMSIZE_4KB;
  seq_cfg.SeqBreakEn = bFALSE;
  seq_cfg.SeqIgnoreEn = bTRUE;
  seq_cfg.SeqCntCRCClr = bTRUE;
  seq_cfg.SeqEnable = bFALSE;
  seq_cfg.SeqWrTimer = 0;
  AD5940_SEQCfg(&seq_cfg);

  AD5940_INTCCfg(AFEINTC_1, AFEINTSRC_ALLINT, bTRUE);
  AD5940_INTCClrFlag(AFEINTSRC_ALLINT);
  AD5940_INTCCfg(AFEINTC_0, AFEINTSRC_DATAFIFOTHRESH|AFEINTSRC_ENDSEQ|AFEINTSRC_CUSTOMINT0, bTRUE);
  AD5940_INTCClrFlag(AFEINTSRC_ALLINT);

  gpio_cfg.FuncSet = GP0_INT|GP1_SLEEP|GP2_SYNC;
  gpio_cfg.InputEnSet = 0;
  gpio_cfg.OutputEnSet = AGPIO_Pin0|AGPIO_Pin1|AGPIO_Pin2;
  gpio_cfg.OutVal = 0;
  gpio_cfg.PullEnSet = 0;
  AD5940_AGPIOCfg(&gpio_cfg);

  LfoscMeasure.CalDuration = 1000.0;
  LfoscMeasure.CalSeqAddr = 0;
  LfoscMeasure.SystemClkFreq = 16000000.0f;
  AD5940_LFOSCMeasure(&LfoscMeasure, &LFOSCFreq);

  AD5940_SleepKeyCtrlS(SLPKEY_UNLOCK);
  return 0;
}

void AD5940DPVStructInit(float S_Vol, float E_Vol, float StepIncrement, float PulseAmplitude, float PulseWidth, float PulsePeriod, int StepNumber) {
  AppDPVCfg_Type *pDPVCfg;
  AppDPVGetCfg(&pDPVCfg);

  pDPVCfg->SeqStartAddr = 0;
  pDPVCfg->MaxSeqLen = 1024;
  pDPVCfg->RcalVal = 200.0;
  pDPVCfg->ADCRefVolt = 1820.0f;
  pDPVCfg->LFOSCClkFreq = LFOSCFreq;
  pDPVCfg->SysClkFreq = 16000000.0f;
  pDPVCfg->AdcClkFreq = 16000000.0f;

  pDPVCfg->RampStartVolt = S_Vol;
  pDPVCfg->RampPeakVolt = E_Vol;
  pDPVCfg->VzeroStart = 1300.0f;
  pDPVCfg->VzeroPeak = 1300.0f;
  pDPVCfg->StepIncrement = StepIncrement;
  pDPVCfg->PulseAmplitude = PulseAmplitude;
  pDPVCfg->PulseWidth = PulseWidth;
  pDPVCfg->PulsePeriod = PulsePeriod;
  pDPVCfg->SampleDelay1 = 10.0f;
  pDPVCfg->SampleDelay2 = 10.0f;
  pDPVCfg->StepNumber = StepNumber;

  pDPVCfg->LPTIARtiaSel = LPTIARTIA_4K;
  pDPVCfg->ExternalRtiaValue = 4000.0f;
  pDPVCfg->AdcPgaGain = ADCPGA_1P5;
  pDPVCfg->ADCSinc3Osr = ADCSINC3OSR_4;
  pDPVCfg->FifoThresh = 4;
}

// ...existing code...
void AD5940_DPV_Main(float S_Vol, float E_Vol, float StepIncrement, float PulseAmplitude, float PulseWidth, float PulsePeriod, int StepNumber) {
  uint32_t temp = 0, count = 0;
  AD5940DPVPlatformCfg();
  AD5940DPVStructInit(S_Vol, E_Vol, StepIncrement, PulseAmplitude, PulseWidth, PulsePeriod, StepNumber);

  AppDPVInit(AppDPVBuff, APPBUFF_DPV_SIZE);
  AppDPVCtrl(APPCTRL_START, 0);

  unsigned long startTime = millis();
  int empty_count = 0;
  while (count < StepNumber) {
    if (AD5940_GetMCUIntFlag()) {
      AD5940_ClrMCUIntFlag();
      temp = APPBUFF_DPV_SIZE;
      AppDPVISR(AppDPVBuff, &temp);

      if (temp < 2) {
        empty_count++;
        if (empty_count > 10) break; // Nếu liên tục không có dữ liệu meaningful thì thoát
        delay(10);
        continue;
      }
      empty_count = 0;

      int valid_points = 0;
      for (uint32_t i = 1; i < temp; i += 2) {
        if (count + valid_points >= StepNumber) break;
        float voltage = S_Vol + (count + valid_points) * StepIncrement;
        float current = ((float *)AppDPVBuff)[i];
        printf("%.2f;%.6f\n", voltage, current);
        valid_points++;
      }
      count += valid_points;
    }
    if (millis() - startTime > 20000) {
      printf("END\n");
      return;
    }
  }
  AppDPVCtrl(APPCTRL_STOPNOW, 0);
  printf("END\n");
}
/*******************************************************************************
 * Write code arduino in here
 ******************************************************************************/
void setup() {
  Serial.begin(115200);               // Bắt đầu giao tiếp Serial với tốc độ 115200
  inputString.reserve(200);           // Dự phòng bộ nhớ cho chuỗi nhập
  Serial.println("MCU initialization successful.");
  uint32_t checkInitMCU = AD5940_MCUResourceInit(0); // Hàm khởi tạo tài nguyên MCU
  if(checkInitMCU == 0) {
    Serial.println("MCU initialization successful.");  // In ra nếu khởi tạo thành công
  } else {
    Serial.println("MCU initialization failed!");      // Có thể thêm dòng này để xử lý lỗi
  }
}


void loop() {
  while (Serial.available())
  {
    char inChar = (char)Serial.read();
    if (inChar != '!')
    {
      inputString += inChar;
    }
    else
    {
      for(int i = 0; i < inputString.length(); i++)
      {
        if(inputString[i] == '#') {moc1 = i;}
        if(inputString[i] == '?') {moc2 = i;}
        if(inputString[i] == '/') {moc3 = i;}
        if(inputString[i] == '|') {moc4 = i;}
        if(inputString[i] == '$') {moc5 = i;}
      }
      //Serial.println("cmd code: " + inputString[0]);
      if(inputString[0] == '1' || inputString[0] == '2'||inputString[0] == '3'|| inputString[0] == '4'|| inputString[0] == '5')
      {
      //CV && EIS
      S_Vol = inputString.substring((moc1 + 1), moc2).toDouble() * 1.0;
      E_Vol = inputString.substring((moc2 + 1), moc3).toInt() * 1.0;
      StepNumber = inputString.substring((moc3 + 1), moc4).toInt();
      RepeatTimes = inputString.substring((moc4 + 1), moc5).toInt();
      logEn = (inputString.substring(moc5 + 1).toInt() == 1) ? bTRUE : bFALSE;
      
      //SWV
      RampIncrement = inputString.substring((moc3 + 1), moc4).toInt();
      Amplitude = inputString.substring((moc4 + 1), moc5).toInt();
      Freq = inputString.substring(moc5 + 1).toInt();
      
      //AMP
      Time_Run = inputString.substring((moc1 + 1), moc2).toInt() * 1.0;
      Bias_Vol = inputString.substring((moc2 + 1), moc3).toInt() * 1.0;
      Time_Interval = inputString.substring((moc3 + 1), moc4).toInt();
      }
      
      if (inputString[0] == '1')
      {
        AD5940_CV_Main();
        
      }
      else if (inputString[0] == '2')
      {
        AD5940_EIS_Main();
      }
       else if (inputString[0] == '3')
      {
        AD5940_EIS3E_Main();
      }
      else if (inputString[0] == '4')
      {
        time1 = (E_Vol - S_Vol)/RampIncrement * 2; 
        AD5940_Sqr_Main();
      }
      else if (inputString[0] == '5')
      {
        AD5940_AMP_Main(Time_Run, Time_Interval);
      }
      else if (inputString[0] == '7') 
      {
        float S_Vol = inputString.substring(moc1 + 1, moc2).toFloat();
        float E_Vol = inputString.substring(moc2 + 1, moc3).toFloat();
        float StepIncrement = inputString.substring(moc3 + 1, moc4).toFloat();
        float PulseAmplitude = inputString.substring(moc4 + 1, moc5).toFloat();
        float PulseWidth = inputString.substring(moc5 + 1).toFloat();
        float PulsePeriod = 200.0f; // hoặc nhận thêm từ lệnh nếu muốn
        int StepNumber = (int)((E_Vol - S_Vol) / StepIncrement) + 1;

        AD5940_DPV_Main(S_Vol, E_Vol, StepIncrement, PulseAmplitude, PulseWidth, PulsePeriod, StepNumber);
      }
      inputString = "";
      ESP.restart();
    }
  }
}