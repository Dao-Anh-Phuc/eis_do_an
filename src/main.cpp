//EIS / 2#100?10000/50|1$0!
//EIS / 3#100?100000/100|1$0!
// CV / 1#-200?600/100|1$0!
//SWV / 4#-200?600/10|25$10!
//CA  / 5#100?100/200|100$0!
//DPV / 6#-200?600/10|25$50!
//LSV / 7#-200?600/100|1$0!
//ASV / 8#-200?700/100|50$50|1100$8000|1000$120000|-50$10000!
#include <Arduino.h>
#include "ad5940.h"
#include "RampTest.h"
#include "Impedance3E.h"
#include "Impedance.h"
#include "SqrWaveVoltammetry.h"
#include "ChronoAmperometric.h"
#include "DiffPulseVoltammetry.h"

unsigned long timeStart = 0;
unsigned long timeNow = 0;
uint32_t time1 = 0;

#define AppCVBuff_CV_SIZE 1024
uint32_t AppCVBuff[AppCVBuff_CV_SIZE];
float LFOSCFreq;    /* Measured LFOSC frequency */

#define AppLSVBuff_LSV_SIZE 1024
uint32_t AppLSVBuff[AppLSVBuff_LSV_SIZE];

#define APPBUFF_EIS_SIZE 512
uint32_t AppEISBuff[APPBUFF_EIS_SIZE];

#define APPBUFF_EIS_SIZE_3E 512
uint32_t AppEISBuff_3E[APPBUFF_EIS_SIZE_3E];

#define APPBUFF_SWV_SIZE 1024
uint32_t AppSWVBuff[APPBUFF_SWV_SIZE];

#define APPBUFF_DPV_SIZE 1024
uint32_t AppDPVBuff[APPBUFF_DPV_SIZE];

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
uint32_t DPV_Step_mV, DPV_PulseAmp_mV, DPV_PulseWidth_ms;
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

void AD5940RampStructInit(float S_Vol, float E_Vol, int StepNumber, float scanRate_mVs = 100.0f)
{
  AppRAMPCfg_Type *pRampCfg;
  AppRAMPGetCfg(&pRampCfg);
  
  // Validate scan rate
  if(scanRate_mVs < 1.0f || scanRate_mVs > 1000.0f) {
    Serial.printf("CV Error: Invalid scan rate %.1f mV/s, using 100 mV/s\n", scanRate_mVs);
    scanRate_mVs = 100.0f;
  }
  
  /* Step1: configure general parameters */
  pRampCfg->SeqStartAddr = 0x10;
  pRampCfg->MaxSeqLen = 1024-0x10;
  pRampCfg->RcalVal = 330.0;
  pRampCfg->ADCRefVolt = 1820.0f;
  pRampCfg->FifoThresh = 10;
  pRampCfg->SysClkFreq = 16000000.0f;
  pRampCfg->LFOSCClkFreq = LFOSCFreq;
  
  /* Configure ramp signal parameters */
  pRampCfg->RampStartVolt = S_Vol;
  pRampCfg->RampPeakVolt = E_Vol;
  pRampCfg->VzeroStart = 1300.0f;
  pRampCfg->VzeroPeak = 1300.0f;
  pRampCfg->StepNumber = StepNumber;
  
  // ✅ CRITICAL: Tính duration dựa trên scan rate
  float voltage_range_mV = fabs(E_Vol - S_Vol);  // 800mV
  
  // ✅ CV = forward + reverse sweep, total sweep = voltage_range × 2
  float total_sweep_mV = voltage_range_mV * 2.0f;  // 1600mV
  
  // ✅ Duration = total_sweep / scan_rate (convert to ms)
  uint32_t calculated_duration = (uint32_t)(total_sweep_mV / scanRate_mVs * 1000.0f);
  
  // ✅ Validate duration bounds
  if(calculated_duration < 2000) {
    calculated_duration = 2000; // Minimum 2 seconds
    Serial.printf("CV Warning: Duration increased to 2s minimum\n");
  } else if(calculated_duration > 300000) {
    calculated_duration = 300000; // Maximum 5 minutes
    Serial.printf("CV Warning: Duration capped at 300s maximum\n");
  }
  
  pRampCfg->RampDuration = calculated_duration;
  
  // ✅ Sample delay based on step timing
  float time_per_step_ms = (float)calculated_duration / (StepNumber * 2);  // StepNumber × 2 for bidirectional
  pRampCfg->SampleDelay = min(7.0f, time_per_step_ms / 3.0f);
  if(pRampCfg->SampleDelay < 1.0f) pRampCfg->SampleDelay = 1.0f;
  
  pRampCfg->LPTIARtiaSel = LPTIARTIA_4K;
  pRampCfg->LPTIARloadSel = LPTIARLOAD_SHORT;
  pRampCfg->AdcPgaGain = ADCPGA_1P5;
  
  // ✅ Professional debug output
  Serial.printf("=== CV Professional Configuration ===\n");
  Serial.printf("Voltage Range: %.1f to %.1f mV (%.1f mV total)\n", S_Vol, E_Vol, voltage_range_mV);
  Serial.printf("Step Size: %.2f mV per step\n", voltage_range_mV / (StepNumber - 1));
  Serial.printf("Steps Total: %d\n", StepNumber);
  Serial.printf("Scan Rate: %.1f mV/s\n", scanRate_mVs);
  Serial.printf("Duration: %lu ms (%.1f seconds)\n", calculated_duration, calculated_duration/1000.0f);
  Serial.printf("Time per Step: %.1f ms\n", time_per_step_ms);
  Serial.printf("Sample Delay: %.1f ms\n", pRampCfg->SampleDelay);
  Serial.printf("=====================================\n");
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

void AD5940_CV_Main(float scanRate_mVs = 100.0f)
{
  uint32_t temp;
  uint32_t count = 0;
  AppRAMPCfg_Type *pRampCfg;
  
  AD5940CVPlatformCfg();
  
  // ✅ Set bParaChanged before init
  AppRAMPGetCfg(&pRampCfg);
  pRampCfg->bParaChanged = bTRUE;
  
  AD5940RampStructInit(S_Vol, E_Vol, StepNumber, scanRate_mVs);
  AppRAMPInit(AppCVBuff, AppCVBuff_CV_SIZE);
  AppRAMPCtrl(APPCTRL_START, 0);

  AppRAMPGetCfg(&pRampCfg);
  
  // ✅ CRITICAL FIX: CV bidirectional = StepNumber × 2 × RepeatTimes
  uint32_t expectedCount = (uint32_t)(::StepNumber * RepeatTimes);
  
  Serial.printf("=== CV Debug Info ===\n");
  Serial.printf("S_Vol: %.1f mV, E_Vol: %.1f mV\n", S_Vol, E_Vol);
  Serial.printf("StepNumber: %d, Scan Rate: %.1f mV/s\n", ::StepNumber, scanRate_mVs);
  Serial.printf("RepeatTimes: %d, Expected Count: %lu\n", RepeatTimes, expectedCount);
  Serial.printf("Actual Duration: %lu ms\n", pRampCfg->RampDuration);
  Serial.printf("===================\n");

  unsigned long start_time = millis();
  unsigned long timeout = pRampCfg->RampDuration + 10000;

  while (count < expectedCount)
  {
    AppRAMPGetCfg(&pRampCfg);
    
    if(AD5940_GetMCUIntFlag())
    {
      AD5940_ClrMCUIntFlag();
      temp = AppCVBuff_CV_SIZE;
      AppRAMPISR(AppCVBuff, &temp);
      RampShowResult((float*)AppCVBuff, temp);
      
      count += temp;
      
      if(count % 25 == 0) {
        Serial.printf("CV Progress: %lu/%lu points (%.1f%%) at %.1fs\n", 
                     count, expectedCount, (float)count/expectedCount*100.0f, 
                     (millis() - start_time)/1000.0f);
      }
    }
    
    if(millis() - start_time > timeout) {
      Serial.printf("CV Timeout: Only got %lu/%lu points in %lu ms\n", 
                   count, expectedCount, millis() - start_time);
      break;
    }
    
    if(pRampCfg->bTestFinished == bTRUE)
    {
      AD5940_Delay10us(20000);
      pRampCfg->bTestFinished = bFALSE;
      AD5940_SEQCtrlS(bTRUE);
      AppRAMPCtrl(APPCTRL_START, 0);
    }
  }
  
  Serial.printf("CV Completed: %lu points collected in %.1f seconds (scan rate: %.1f mV/s)\n", 
                count, (millis() - start_time)/1000.0f, scanRate_mVs);
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

static int32_t AD5940_DPV_PlatformCfg(void)
{
  CLKCfg_Type clk_cfg;
  SEQCfg_Type seq_cfg;  
  FIFOCfg_Type fifo_cfg;
  AGPIOCfg_Type gpio_cfg;
  LFOSCMeasure_Type LfoscMeasure;

  /* Use hardware reset */
  AD5940_HWReset();
  AD5940_Initialize();

  /* Step1. Clock */
  clk_cfg.HFOSCEn = bTRUE;
  clk_cfg.HFXTALEn = bFALSE;
  clk_cfg.LFOSCEn = bTRUE;
  clk_cfg.HfOSC32MHzMode = bFALSE;
  clk_cfg.SysClkSrc = SYSCLKSRC_HFOSC;
  clk_cfg.SysClkDiv = SYSCLKDIV_1;
  clk_cfg.ADCCLkSrc = ADCCLKSRC_HFOSC;
  clk_cfg.ADCClkDiv = ADCCLKDIV_1;
  AD5940_CLKCfg(&clk_cfg);

  /* Step2. FIFO & Sequencer */
  fifo_cfg.FIFOEn = bTRUE;
  fifo_cfg.FIFOMode = FIFOMODE_FIFO;
  fifo_cfg.FIFOSize = FIFOSIZE_4KB;
  fifo_cfg.FIFOSrc = FIFOSRC_SINC3;
  fifo_cfg.FIFOThresh = 4;    // nên là bội số của 2 khi raw, nhưng DPV đã “collapse” cặp -> vẫn OK.
  AD5940_FIFOCfg(&fifo_cfg);

  seq_cfg.SeqMemSize = SEQMEMSIZE_2KB;
  seq_cfg.SeqBreakEn = bFALSE;
  seq_cfg.SeqIgnoreEn = bTRUE;
  seq_cfg.SeqCntCRCClr = bTRUE;
  seq_cfg.SeqEnable = bFALSE;
  seq_cfg.SeqWrTimer = 0;
  AD5940_SEQCfg(&seq_cfg);

  /* Step3. Interrupts */
  AD5940_INTCCfg(AFEINTC_1, AFEINTSRC_ALLINT, bTRUE);
  AD5940_INTCClrFlag(AFEINTSRC_ALLINT);
  AD5940_INTCCfg(AFEINTC_0, AFEINTSRC_DATAFIFOTHRESH|AFEINTSRC_ENDSEQ|AFEINTSRC_CUSTOMINT0, bTRUE);
  AD5940_INTCClrFlag(AFEINTSRC_ALLINT);

  /* Step4. GPIO */
  gpio_cfg.FuncSet = GP0_INT|GP1_SLEEP|GP2_SYNC;
  gpio_cfg.InputEnSet = 0;
  gpio_cfg.OutputEnSet = AGPIO_Pin0|AGPIO_Pin1|AGPIO_Pin2;
  gpio_cfg.OutVal = 0;
  gpio_cfg.PullEnSet = 0;
  AD5940_AGPIOCfg(&gpio_cfg);

  /* Step5. Measure LFOSC */
  LfoscMeasure.CalDuration = 1000.0;
  LfoscMeasure.CalSeqAddr = 0;
  LfoscMeasure.SystemClkFreq = 16000000.0f;
  AD5940_LFOSCMeasure(&LfoscMeasure, &LFOSCFreq);
  AD5940_SleepKeyCtrlS(SLPKEY_UNLOCK);
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
void AD5940_DPV_StructInit(float S_Vol, float E_Vol,
                           uint32_t Step_mV, uint32_t PulseAmp_mV, uint32_t PulseWidth_ms)
{
  AppDPVCfg_Type *pDPV;

  AppDPVGetCfg(&pDPV);
  printf("[DPV] StructInit: S_Vol=%.2f, E_Vol=%.2f, Step_mV=%lu, PulseAmp_mV=%lu, PulseWidth_ms=%lu\n", S_Vol, E_Vol, Step_mV, PulseAmp_mV, PulseWidth_ms);

  // Common
  pDPV->SeqStartAddr = 0x10;
  pDPV->MaxSeqLen    = 1024 - 0x10;
  pDPV->RcalVal      = 200.0f;
  pDPV->ADCRefVolt   = 1820.0f;
  pDPV->FifoThresh   = 4; // tăng lên 4 cho đồng bộ các mode khác
  pDPV->SysClkFreq   = 16000000.0f;
  pDPV->LFOSCClkFreq = LFOSCFreq;

  // Rx path
  pDPV->LPTIARtiaSel = LPTIARTIA_4K; // đảm bảo đúng phần cứng
  pDPV->AdcPgaGain   = ADCPGA_1P5;
  pDPV->ADCSinc3Osr  = ADCSINC3OSR_4;
  pDPV->ADCSinc2Osr  = ADCSINC2OSR_1067;

  // Staircase baseline
  pDPV->RampStartVolt = S_Vol;
  pDPV->RampPeakVolt  = E_Vol;
  pDPV->RampStep_mV   = (float)Step_mV;
  pDPV->StepNumber    = 0;          // để thư viện tự tính từ range/step
  pDPV->bDACCodeInc  = (E_Vol >= S_Vol) ? bTRUE : bFALSE;  // hướng tăng/giảm
  pDPV->bRampOneDir  = bTRUE;                              // quét 1 chiều giống SWV bạn đang dùng

  // Vzero (bias) – giữ 1.3 V như bạn dùng ở SWV/CV
  pDPV->VzeroStart    = 1300.0f;
  pDPV->VzeroPeak     = 1300.0f;

  // DPV pulse
  pDPV->PulseAmp_mV       = (float)PulseAmp_mV;
  pDPV->PulseWidth_ms     = (float)PulseWidth_ms;
  pDPV->PrePulseWait_ms   = (float)PulseWidth_ms;  // set = t_pulse cho dễ canh thời
  pDPV->HoldAfterPulse_ms = 1.0f;                  // nhỏ để sang bước kế tiếp nhanh
  pDPV->bPulsePositive    = bTRUE;                 // xung dương; đổi FALSE nếu cần khử

  // Sampling windows
  pDPV->NAvgBase   = 1;
  pDPV->NAvgPulse  = 1;
  pDPV->GuardBase_ms  = 2.0f;
  pDPV->GuardPulse_ms = 2.0f;

  // Flags
  pDPV->bParaChanged = bTRUE;

  // In ra toàn bộ struct DPV sau khi khởi tạo
  printf("[DPV] Struct dump:\n");
  printf("  SeqStartAddr=%lu, MaxSeqLen=%lu\n", pDPV->SeqStartAddr, pDPV->MaxSeqLen);
  printf("  RcalVal=%.2f, ADCRefVolt=%.2f, FifoThresh=%lu\n", pDPV->RcalVal, pDPV->ADCRefVolt, pDPV->FifoThresh);
  printf("  SysClkFreq=%.2f, LFOSCClkFreq=%.2f\n", pDPV->SysClkFreq, pDPV->LFOSCClkFreq);
  printf("  LPTIARtiaSel=%lu, AdcPgaGain=%lu\n", pDPV->LPTIARtiaSel, pDPV->AdcPgaGain);
  printf("  RampStartVolt=%.2f, RampPeakVolt=%.2f, RampStep_mV=%.2f\n", pDPV->RampStartVolt, pDPV->RampPeakVolt, pDPV->RampStep_mV);
  printf("  StepNumber=%lu, bDACCodeInc=%d, bRampOneDir=%d\n", pDPV->StepNumber, pDPV->bDACCodeInc, pDPV->bRampOneDir);
  printf("  VzeroStart=%.2f, VzeroPeak=%.2f\n", pDPV->VzeroStart, pDPV->VzeroPeak);
  printf("  PulseAmp_mV=%.2f, PulseWidth_ms=%.2f, PrePulseWait_ms=%.2f, HoldAfterPulse_ms=%.2f\n", pDPV->PulseAmp_mV, pDPV->PulseWidth_ms, pDPV->PrePulseWait_ms, pDPV->HoldAfterPulse_ms);
  printf("  bPulsePositive=%d\n", pDPV->bPulsePositive);
  printf("  NAvgBase=%d, NAvgPulse=%d, GuardBase_ms=%.2f, GuardPulse_ms=%.2f\n", pDPV->NAvgBase, pDPV->NAvgPulse, pDPV->GuardBase_ms, pDPV->GuardPulse_ms);
  printf("[DPV] StructInit: xong config struct\n");
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

float DPV_Dep_V = -500, DPV_Dep_T = 60000;
void AD5940_DPV_Main(float DPV_Dep_V, float DPV_Dep_T)
{
  uint32_t temp, count = 0;
  if (DPV_Step_mV == 0) DPV_Step_mV = 10;

  // ===== CHỈ MỘT LẦN PLATFORM CONFIG =====
  Serial.println("[DPV] Single PlatformCfg for both phases...");
  AD5940_DPV_PlatformCfg();  // Chỉ dùng DPV platform config
  Serial.println("[DPV] PlatformCfg done");

  // ===== DEPOSITION PHASE (nếu cần) =====
  if(DPV_Dep_T > 0) {
    Serial.println("[DPV] Starting Deposition Phase...");
    
    // Cấu hình chronoamperometry KHÔNG reset platform
    AD5940AMPStructInit_DC_No_Platform(DPV_Dep_V, DPV_Dep_T);
    AppCHRONOAMPInit(AppAMPBuff[0], APPBUFF_AMP_SIZE);
    AppCHRONOAMPCtrl(CHRONOAMPCTRL_START, 0);
    delay(DPV_Dep_T);
    AppCHRONOAMPCtrl(CHRONOAMPCTRL_STOPSYNC, 0);
    Serial.println("[DPV] Deposition Phase completed");
    
    // Delay và reset AD5940 state để chuẩn bị cho DPV
    delay(100);
    AD5940_WakeUp(10);
    AD5940_AFECtrlS(AFECTRL_ALL, bFALSE);  // Tắt tất cả AFE
    delay(50);
  }

  // ===== DPV MEASUREMENT PHASE =====
  Serial.printf("[DPV] Starting DPV: S=%.1f, E=%.1f, step=%lu, Amp=%lumV, Width=%lums\n",
                S_Vol, E_Vol, DPV_Step_mV, DPV_PulseAmp_mV, DPV_PulseWidth_ms);

  Serial.println("[DPV] DPV StructInit...");
  AD5940_DPV_StructInit(S_Vol, E_Vol, DPV_Step_mV, DPV_PulseAmp_mV, DPV_PulseWidth_ms);
  Serial.println("[DPV] DPV StructInit done");

  Serial.println("[DPV] AppDPVInit...");
  AD5940Err err_init = AppDPVInit(AppDPVBuff, APPBUFF_DPV_SIZE);
  Serial.printf("[DPV] AppDPVInit done, return=%d\n", err_init);

  Serial.println("[DPV] Delay before start...");
  AD5940_Delay10us(100000);
  Serial.println("[DPV] Delay done");

  Serial.println("[DPV] AppDPVCtrl START...");
  AD5940_WakeUp(10);
  AD5940Err err_ctrl = AppDPVCtrl(APPCTRL_START, 0);
  Serial.printf("[DPV] AppDPVCtrl START done, return=%d\n", err_ctrl);

  // Debug sequencer state
  uint32_t seq_state = AD5940_ReadReg(REG_AFE_SEQSTAT);
  uint32_t fifo_cnt = AD5940_ReadReg(REG_AFE_FIFOCNT);
  uint32_t int0_flag = AD5940_ReadReg(REG_AFE_INT0);
  Serial.printf("[DPV] Sequencer state: 0x%08lx, FIFO count: %lu, INT0: 0x%08lx\n", seq_state, fifo_cnt, int0_flag);

  int dpvPoints = (int)floor((fabs(E_Vol - S_Vol) / (float)DPV_Step_mV) + 1.0f);
  int target = dpvPoints;
  Serial.printf("[DPV] target points: %d\n", target);

  unsigned long lastBeat = millis();
  while(count < (uint32_t)target)
  {
    if(AD5940_GetMCUIntFlag())
    {
      Serial.println("[DPV] MCU Int Flag detected");
      AD5940_ClrMCUIntFlag();
      temp = APPBUFF_DPV_SIZE;
      AppDPVISR(AppDPVBuff, &temp);
      Serial.printf("[DPV] AppDPVISR returned temp=%lu\n", temp);
      if (temp > 0) {
        RampShowResult((float*)AppDPVBuff, temp);
        count += temp;
        lastBeat = millis();
      }
    }
    
    if (millis() - lastBeat > 200) {
      uint32_t raw = AD5940_INTCGetFlag(AFEINTC_0);
      Serial.printf("[wait] INTC0=0x%08lx FIFO=%lu cnt=%lu/%d\n",
                    raw, AD5940_FIFOGetCnt(), count, target);
      lastBeat = millis();
    }
  }
  Serial.println("[DPV] Main finished");
}

// Tạo hàm chronoamp init KHÔNG reset platform
void AD5940AMPStructInit_DC_No_Platform(float Bias_Vol, float Time_Interval)
{
  float Time_IntervalFloat = Time_Interval / 1000.0;  // ✅ Convert ms to seconds
  AppCHRONOAMPCfg_Type *pAMPCfg; 
  AppCHRONOAMPGetCfg(&pAMPCfg);
  
  // Config struct
  pAMPCfg->WuptClkFreq = LFOSCFreq;
  pAMPCfg->SeqStartAddr = 0;
  pAMPCfg->MaxSeqLen = 512;
  pAMPCfg->RcalVal = 16.5;
  pAMPCfg->NumOfData = -1;
  
  pAMPCfg->AmpODR = Time_IntervalFloat;  // ✅ Use converted value
  pAMPCfg->FifoThresh = 1;
  pAMPCfg->ADCRefVolt = 1.82;
  
  pAMPCfg->ExtRtia = bFALSE;
  pAMPCfg->ExtRtiaVal = 10000000;
  pAMPCfg->LptiaRtiaSel = LPTIARTIA_1K;
  
  pAMPCfg->SensorBias = Bias_Vol;  // ✅ Direct use voltage
  pAMPCfg->Vzero = 1100;
  pAMPCfg->pulseAmplitude = 0;
  pAMPCfg->pulseLength = 0;
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

// ==== LSV settings ====
float LSV_ScanRate_mVs = 50.0f;   // ví dụ 50 mV/s
void AD5940LSVStructInit(float S_Vol, float E_Vol, int StepNumber)
{
  AppRAMPCfg_Type *pRampCfg;
  AppRAMPGetCfg(&pRampCfg);

  pRampCfg->SeqStartAddr = 0x10;
  pRampCfg->MaxSeqLen = 1024-0x10;
  pRampCfg->RcalVal = 200.0;
  pRampCfg->ADCRefVolt = 1820.0f;
  pRampCfg->FifoThresh = 10;
  pRampCfg->SysClkFreq = 16000000.0f;
  pRampCfg->LFOSCClkFreq = LFOSCFreq;

  pRampCfg->RampStartVolt = S_Vol;
  pRampCfg->RampPeakVolt = E_Vol;
  pRampCfg->VzeroStart = 1300.0f;
  pRampCfg->VzeroPeak = 1300.0f;
  pRampCfg->StepNumber = StepNumber;
  float Erange_mV = fabs(E_Vol - S_Vol);               // biên độ quét
  pRampCfg->RampDuration = (uint32_t)(1000.0f * Erange_mV / LSV_ScanRate_mVs);
// đơn vị ms; ví dụ 800 mV @ 50 mV/s -> 16 000 ms

 // pRampCfg->RampDuration = 24*1000;
  pRampCfg->SampleDelay = 7.0f;

  pRampCfg->LPTIARtiaSel = LPTIARTIA_4K;
  pRampCfg->LPTIARloadSel = LPTIARLOAD_SHORT;
  pRampCfg->AdcPgaGain = ADCPGA_1P5;
  pRampCfg->bRampOneDir = bTRUE; // LSV: chỉ một chiều
}

void AD5940_LSV_Main()
{
  uint32_t temp = 0;
  uint32_t count = 0;
  AppRAMPCfg_Type *pRampCfg;
  AD5940CVPlatformCfg();
  AD5940LSVStructInit(S_Vol, E_Vol, StepNumber);
  AppRAMPInit(AppLSVBuff, AppLSVBuff_LSV_SIZE);
  AppRAMPCtrl(APPCTRL_START, 0);
  while (count < (StepNumber * RepeatTimes))
  {
    AppRAMPGetCfg(&pRampCfg);
    if(AD5940_GetMCUIntFlag())
    {
      AD5940_ClrMCUIntFlag();
      temp = AppLSVBuff_LSV_SIZE;
      AppRAMPISR(AppLSVBuff, &temp);
      RampShowResult((float*)AppLSVBuff, temp);
      count += temp;
    }
    if(pRampCfg->bTestFinished == bTRUE)
    {
      AD5940_Delay10us(20000);
      pRampCfg->bTestFinished = bFALSE;
      AD5940_SEQCtrlS(bTRUE);
      AppRAMPCtrl(APPCTRL_START, 0);
    }
  }
}

// ASV

void SetAMP_Duration_WakeupTimer(uint32_t duration_ms) {
  // Cấu hình AD5940 wakeup timer cho đúng duration_ms
  WUPTCfg_Type wupt_cfg;
  uint32_t wupt_ticks = (uint32_t)(duration_ms * LFOSCFreq / 1000.0f);
  
  wupt_cfg.WuptEn = bTRUE;
  wupt_cfg.WuptEndSeq = WUPTENDSEQ_A;
  wupt_cfg.WuptOrder[0] = SEQID_0;
  wupt_cfg.SeqxSleepTime[SEQID_0] = wupt_ticks;
  wupt_cfg.SeqxWakeupTime[SEQID_0] = 1;
  
  AD5940_WUPTCfg(&wupt_cfg);
}
void WaitFor_AMP_EndOfSequence() {
  volatile bool eos_flag = false;
  unsigned long start_time = millis();
  const unsigned long TIMEOUT_MS = 10000; // 10 seconds timeout
  
  while(!eos_flag) {
    if(millis() - start_time > TIMEOUT_MS) {
      Serial.println("ASV Error: Timeout waiting for EOS");
      break;
    }
    
    if(AD5940_GetMCUIntFlag()) {
      uint32_t intc_flag = AD5940_INTCGetFlag(AFEINTC_0);
      if(intc_flag & AFEINTSRC_ENDSEQ) {
        eos_flag = true;
        AD5940_ClrMCUIntFlag();
      }
    }
    delay(1); // Nhỏ để tránh busy wait
  }
}
float ASV_Clean_V, ASV_Clean_T;
float ASV_Dep_V, ASV_Dep_T;
float ASV_Eq_V, ASV_Eq_T;

void AD5940AMPStructInit_DC(int Bias_Vol, float Time_Interval)
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
  pAMPCfg->pulseAmplitude = 0;            /* Pulse amplitude on counter electrode (mV) */
  pAMPCfg->pulseLength = 0;               /* Length of voltage pulse in ms */
    
}


// ===== SỬA TOÀN BỘ ASV_Main =====
void AD5940_ASV_Main(float cleanV, int cleanT, float depV, int depT,
                     float eqV, int eqT, float S_Vol, float E_Vol, int StepNumber) {
  
  // Validation tham số
  if(cleanV <= 0) {
    Serial.printf("ASV Error: cleanV=%.1f must be positive\n", cleanV);
    return;
  }
  if(depV >= 0) {
    Serial.printf("ASV Error: depV=%.1f must be negative\n", depV);
    return;
  }
  if(cleanT <= 0 || depT <= 0 || eqT <= 0) {
    Serial.printf("ASV Error: Time parameters cleanT=%d, depT=%d, eqT=%d must be positive\n", cleanT, depT, eqT);
    return;
  }
  
  Serial.printf("ASV Start: Clean=%.0fmV/%dms, Dep=%.0fmV/%dms, Eq=%.0fmV/%dms\n",
                cleanV, cleanT, depV, depT, eqV, eqT);
  
  // ✅ Platform init CHỈ MỘT LẦN
  AD5940_AMP_PlatformCfg();
  
  // ===== CLEANING PHASE =====
  Serial.println("ASV: Starting Cleaning Phase...");
  AD5940AMPStructInit_DC(cleanV, cleanT);
  AppCHRONOAMPInit(AppAMPBuff[0], APPBUFF_AMP_SIZE);
  AppCHRONOAMPCtrl(CHRONOAMPCTRL_START, 0);
  delay(cleanT);  // ✅ Dùng delay() cho đơn giản
  AppCHRONOAMPCtrl(CHRONOAMPCTRL_STOPSYNC, 0);
  Serial.println("ASV: Cleaning Phase completed");
  
  delay(50); // Delay ngắn giữa các pha
  
  // ===== DEPOSITION PHASE =====
  Serial.println("ASV: Starting Deposition Phase...");
  AD5940AMPStructInit_DC(depV, depT);
  AppCHRONOAMPInit(AppAMPBuff[0], APPBUFF_AMP_SIZE);
  AppCHRONOAMPCtrl(CHRONOAMPCTRL_START, 0);
  delay(depT);  // ✅ Dùng delay()
  AppCHRONOAMPCtrl(CHRONOAMPCTRL_STOPSYNC, 0);
  Serial.println("ASV: Deposition Phase completed");
  
  delay(50); // Delay ngắn
  
  // ===== EQUILIBRIUM PHASE =====
  Serial.println("ASV: Starting Equilibrium Phase...");
  AD5940AMPStructInit_DC(eqV, eqT);
  AppCHRONOAMPInit(AppAMPBuff[0], APPBUFF_AMP_SIZE);
  AppCHRONOAMPCtrl(CHRONOAMPCTRL_START, 0);
  delay(eqT);   // ✅ Dùng delay()
  AppCHRONOAMPCtrl(CHRONOAMPCTRL_STOPSYNC, 0);
  Serial.println("ASV: Equilibrium Phase completed");
  
  delay(100); // Delay dài hơn trước stripping
  
  // ===== STRIPPING PHASE =====
  Serial.println("ASV: Starting Stripping Phase...");
  ::S_Vol = S_Vol;
  ::E_Vol = E_Vol;
  ::StepNumber = StepNumber;
  RepeatTimes = 1;
  AD5940_LSV_Main();
  Serial.println("ASV: Stripping Phase completed");
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
  while (Serial.available()) {
    char inChar = (char)Serial.read();
    if (inChar != '!') {
      inputString += inChar;
    } else {
      // Parse delimiters
      for(int i = 0; i < inputString.length(); i++) {
        if(inputString[i] == '#') {moc1 = i;}
        if(inputString[i] == '?') {moc2 = i;}
        if(inputString[i] == '/') {moc3 = i;}
        if(inputString[i] == '|') {moc4 = i;}
        if(inputString[i] == '$') {moc5 = i;}
      }
      
      // ✅ SPECIAL CV PARSING (method '1') - ĐỂ RIÊNG
      if(inputString[0] == '1') {
        // ✅ DEBUG: In ra command và delimiters
        Serial.printf("CV Command: '%s'\n", inputString.c_str());
        Serial.printf("Length: %d\n", inputString.length());
        Serial.printf("Delimiters: moc1=%d, moc2=%d, moc3=%d, moc4=%d, moc5=%d\n", 
                      moc1, moc2, moc3, moc4, moc5);
        
        // Parse basic parameters
        S_Vol = inputString.substring((moc1 + 1), moc2).toDouble() * 1.0;
        E_Vol = inputString.substring((moc2 + 1), moc3).toInt() * 1.0;
        
        Serial.printf("Parsed: S_Vol=%.1f, E_Vol=%.1f\n", S_Vol, E_Vol);
        
        // ✅ EXTRACT phần giữa "/" và "|"
        String stepAndRate = inputString.substring(moc3 + 1, moc4);
        Serial.printf("StepAndRate substring: '%s'\n", stepAndRate.c_str());
        
        float scanRate_mVs = 100.0f; // Default scan rate
        
        // ✅ TÌM DẤU @ trong stepAndRate
        int atPos = stepAndRate.indexOf('@');
        Serial.printf("@ position in stepAndRate: %d\n", atPos);
        
        if(atPos > 0) {
          // Format: "162@100.0" - có scan rate
          String stepStr = stepAndRate.substring(0, atPos);
          String rateStr = stepAndRate.substring(atPos + 1);
          
          StepNumber = stepStr.toInt();
          scanRate_mVs = rateStr.toFloat();
          
          Serial.printf("CV with scan rate: StepStr='%s', RateStr='%s'\n", 
                       stepStr.c_str(), rateStr.c_str());
          Serial.printf("CV with scan rate: Steps=%d, Rate=%.1f mV/s\n", 
                       StepNumber, scanRate_mVs);
        } else {
          // Format: "162" - không có scan rate
          StepNumber = stepAndRate.toInt();
          Serial.printf("CV default scan rate: StepAndRate='%s' → Steps=%d, Rate=%.1f mV/s\n", 
                       stepAndRate.c_str(), StepNumber, scanRate_mVs);
        }
        
        RepeatTimes = inputString.substring((moc4 + 1), moc5).toInt();
        logEn = (inputString.substring(moc5 + 1).toInt() == 1) ? bTRUE : bFALSE;
        
        Serial.printf("Final parsed: StepNumber=%d, ScanRate=%.1f, RepeatTimes=%d, logEn=%d\n",
                     StepNumber, scanRate_mVs, RepeatTimes, logEn);
        
        // ✅ Call CV với scan rate
        AD5940_CV_Main(scanRate_mVs);
        
        inputString = "";
        ESP.restart();
      }
      // ✅ GIỮ NGUYÊN parsing cho các method khác (2-7)
      else if(inputString[0] == '2' || inputString[0] == '3' || inputString[0] == '4' ||
              inputString[0] == '5' || inputString[0] == '6' || inputString[0] == '7') {
        
        // Standard parsing for non-CV methods
        S_Vol       = inputString.substring((moc1 + 1), moc2).toDouble() * 1.0;
        E_Vol       = inputString.substring((moc2 + 1), moc3).toInt() * 1.0;
        StepNumber  = inputString.substring((moc3 + 1), moc4).toInt();
        RepeatTimes = inputString.substring((moc4 + 1), moc5).toInt();
        logEn       = (inputString.substring(moc5 + 1).toInt() == 1) ? bTRUE : bFALSE;

        // SWV
        RampIncrement = inputString.substring((moc3 + 1), moc4).toInt();
        Amplitude     = inputString.substring((moc4 + 1), moc5).toInt();
        Freq          = inputString.substring(moc5 + 1).toInt();

        // AMP
        Time_Run      = inputString.substring((moc1 + 1), moc2).toInt() * 1.0;
        Bias_Vol      = inputString.substring((moc2 + 1), moc3).toInt() * 1.0;
        Time_Interval = inputString.substring((moc3 + 1), moc4).toInt();

        // DPV
        DPV_Step_mV       = inputString.substring((moc3 + 1), moc4).toInt();
        DPV_PulseAmp_mV   = inputString.substring((moc4 + 1), moc5).toInt();
        DPV_PulseWidth_ms = inputString.substring(moc5 + 1).toInt();
        
        // Execute methods
        if (inputString[0] == '2') {
          AD5940_EIS_Main();
        }
        else if (inputString[0] == '3') {
          AD5940_EIS3E_Main();
        }
        else if (inputString[0] == '4') {
          time1 = (uint32_t)( (fabs(E_Vol - S_Vol) / (float)RampIncrement) * 2.0f );
          AD5940_Sqr_Main();
        }
        else if (inputString[0] == '5') {
          AD5940_AMP_Main(Time_Run, Time_Interval);
        }
        else if(inputString[0] == '6') {
          // ✅ Check if có deposition parameters  
          if(inputString.indexOf("|-") > 0) {
            // DPV with deposition: 6#-200?600/10|25$50|-500$60000!
            Serial.println("DPV: With deposition mode");
            
            // Parse basic DPV parameters trước
            S_Vol = inputString.substring((moc1 + 1), moc2).toDouble() * 1.0;
            E_Vol = inputString.substring((moc2 + 1), moc3).toInt() * 1.0;
            DPV_Step_mV = inputString.substring((moc3 + 1), moc4).toInt();
            DPV_PulseAmp_mV = inputString.substring((moc4 + 1), moc5).toInt();
            DPV_PulseWidth_ms = inputString.substring(moc5 + 1, inputString.indexOf("|-")).toInt();
            
            // Parse deposition parameters
            int dep_start = inputString.indexOf("|-") + 2;  // Sau dấu |-
            String dep_part = inputString.substring(dep_start);
            int dollar_pos = dep_part.indexOf('$');
            
            DPV_Dep_V = dep_part.substring(0, dollar_pos).toFloat();
            DPV_Dep_T = dep_part.substring(dollar_pos + 1).toFloat();
            
            Serial.printf("DPV Deposition: %.1fmV, %.0fms\n", DPV_Dep_V, DPV_Dep_T);
          } else {
            // DPV standard: 6#-200?600/10|25$50!
            Serial.println("DPV: Standard mode");
            // Parse như bình thường
            DPV_Step_mV = inputString.substring((moc3 + 1), moc4).toInt();
            DPV_PulseAmp_mV = inputString.substring((moc4 + 1), moc5).toInt();
            DPV_PulseWidth_ms = inputString.substring(moc5 + 1).toInt();
            DPV_Dep_V = 0;  // No deposition
            DPV_Dep_T = 0;
          }
          
          AD5940_DPV_Main(DPV_Dep_V, DPV_Dep_T);
        }
        else if(inputString[0] == '7') {
          AD5940_LSV_Main();
        }
        
        inputString = "";
        ESP.restart();
      }
      // ✅ GIỮ NGUYÊN ASV parsing (method '8')
      else if(inputString[0] == '8') {
        // ✅ ASV parsing hoàn toàn mới
        Serial.println("ASV: Parsing command...");
        Serial.println("Input: " + inputString);
        
        // Parse stripping parameters trước
        S_Vol      = inputString.substring(moc1 + 1, moc2).toFloat();
        E_Vol      = inputString.substring(moc2 + 1, moc3).toFloat();
        StepNumber = inputString.substring(moc3 + 1, moc4).toInt();
        
        // Tìm TOÀN BỘ các dấu | từ đầu chuỗi
        int pipe_positions[10];  // Array lưu vị trí các dấu |
        int pipe_count = 0;
        
        for(int i = 0; i < inputString.length(); i++) {
          if(inputString[i] == '|') {
            pipe_positions[pipe_count] = i;
            pipe_count++;
          }
        }
        
        // Debug vị trí các dấu |
        Serial.printf("Found %d pipes at positions: ", pipe_count);
        for(int i = 0; i < pipe_count; i++) {
          Serial.printf("%d ", pipe_positions[i]);
        }
        Serial.println();
        
        if(pipe_count >= 3) {
          // ASV có 4 segment: |100$2000|-200$10000|0$2000|
          int seg1_start = pipe_positions[0] + 1;  // Sau | đầu tiên
          int seg1_end   = pipe_positions[1];      // Trước | thứ hai
          
          int seg2_start = pipe_positions[1] + 1;  // Sau | thứ hai  
          int seg2_end   = pipe_positions[2];      // Trước | thứ ba
          
          int seg3_start = pipe_positions[2] + 1;  // Sau | thứ ba
          int seg3_end   = inputString.length();   // Đến cuối chuỗi
          
          // Parse segment 1: "100$2000"
          String seg1 = inputString.substring(seg1_start, seg1_end);
          int dollar_pos1 = seg1.indexOf('$');
          ASV_Clean_V = seg1.substring(0, dollar_pos1).toFloat();
          ASV_Clean_T = seg1.substring(dollar_pos1 + 1).toInt();
          
          // Parse segment 2: "-200$10000"  
          String seg2 = inputString.substring(seg2_start, seg2_end);
          int dollar_pos2 = seg2.indexOf('$');
          ASV_Dep_V = seg2.substring(0, dollar_pos2).toFloat();
          ASV_Dep_T = seg2.substring(dollar_pos2 + 1).toInt();
          
          // Parse segment 3: "0$2000"
          String seg3 = inputString.substring(seg3_start, seg3_end);
          int dollar_pos3 = seg3.indexOf('$');
          ASV_Eq_V = seg3.substring(0, dollar_pos3).toFloat();
          ASV_Eq_T = seg3.substring(dollar_pos3 + 1).toInt();
          
          // Debug segments
          Serial.printf("Seg1: '%s' -> Clean=%.1fmV/%dms\n", seg1.c_str(), ASV_Clean_V, ASV_Clean_T);
          Serial.printf("Seg2: '%s' -> Dep=%.1fmV/%dms\n", seg2.c_str(), ASV_Dep_V, ASV_Dep_T);
          Serial.printf("Seg3: '%s' -> Eq=%.1fmV/%dms\n", seg3.c_str(), ASV_Eq_V, ASV_Eq_T);
        } else {
          Serial.println("ASV Error: Not enough pipe separators!");
          inputString = "";
          return;
        }
        
        // Debug parsed values
        Serial.printf("Final Parsed: Clean=%.1fmV/%dms, Dep=%.1fmV/%dms, Eq=%.1fmV/%dms\n",
                      ASV_Clean_V, ASV_Clean_T, ASV_Dep_V, ASV_Dep_T, ASV_Eq_V, ASV_Eq_T);
        Serial.printf("Stripping: %.1f→%.1fmV, %d steps\n", S_Vol, E_Vol, StepNumber);

        // Gọi hàm ASV
        AD5940_ASV_Main(ASV_Clean_V, ASV_Clean_T, ASV_Dep_V, ASV_Dep_T, ASV_Eq_V, ASV_Eq_T, S_Vol, E_Vol, StepNumber);
        
        inputString = "";
        ESP.restart();
      }
      else {
        Serial.println("Invalid command code!");
        inputString = "";
      }
    }
  }
}