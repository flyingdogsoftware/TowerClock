/*
 * TrueStep Main
 * 
 * This project has been forked from the original S42B firmware created by Vsion Yang.
 * 
 * Contributors:
 * - Vsion Yang (2019.10.31)
 * - Jan Swanepoel
*/

// JaSw: TODO
// - A lot of cleaning up!!!
// - Allow to change and store different serial baud rates
// - See that serial commands does not interfere with motion
// - Support for 0.9° steppers
// - Check for unused pins and make them inputs with pullups
// - Add open/close mode selection to OLED menu


#include "main.h"
#include "oled.h"
#include "display.h"
#include "usart.h"
#include "serial.h"
#include "telemetry.h"
#include "tle5012b.h"
#include "buttons.h"
#include "flash.h"
#include "storage.h"
#include "menu.h"
#include "sine.h"
#include "rtc.h"
       

static void LL_Init(void);
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_SPI1_Init(void);
static void MX_TIM1_Init(void);
static void MX_TIM3_Init(void);
static void MX_TIM6_Init(void);
static void MX_IWDG_Init(void);


void ReadDIPSwitches();
void Output(int32_t theta,uint8_t effort);
int16_t Mod(int32_t xMod,int16_t mMod);
void OneStep(void);
void FineStep(uint16_t stepSize_mdeg, uint16_t current_mA);
uint16_t ReadValue(uint16_t RegValue);
void WriteValue(uint16_t RegAdd,uint16_t RegValue);
uint16_t ReadState(void);
uint16_t ReadAngle(void);
void PID_Cal_value_init(void);
void OledMenu(void);
// Perform software motor step
void SoftStep(uint8_t direction);
// Handle software move operation
void SoftMoveStep();
// Parse bytes received from the UART
void ParseBytes(uint8_t data);
// Stops program execution and loads the bootloader
void InvokeBootloader();



/********************************/
 
// JaSw: Do they need to be signed?
int16_t kp=30;                //
int16_t ki=10;                //
int16_t kd=250;               //


const uint8_t LPFA=125; 
const uint8_t LPFB=3;

int32_t s=0;//
int32_t s_1=0;
int32_t s_sum=0;//
int32_t r=0;
int32_t r_1=0;
uint8_t dir=1;//
int16_t y=0;//
int16_t y_1=0;
int32_t yw=0;//
int32_t yw_1=0;
int16_t advance=0;//
int32_t wrap_count=0;//
int32_t pid_e=0;//
int32_t iterm=0;//
int32_t dterm=0;//
int16_t u=0;//
int32_t stepnumber=0;//
uint8_t stepangle=0;//

uint16_t hccount=0;//
uint8_t closemode;//
uint8_t enmode=0;//

uint8_t Calibration_flag=0;             //
volatile uint8_t Data_update_flag =1;   //
volatile uint16_t Data_update_Count =0; //25ms

int16_t wap1=0;
int16_t wap2=0;
int16_t Motor_speed_count=0; 
uint8_t start_measu_V_flag=0;
uint8_t measure_once_flag=0; 

uint8_t nodeId;                         // JaSw: Motor identification when used in network (CAN). FOR FUTURE USE.
uint8_t menuActive = 0;                 // JaSw: In-menu = 1 else 0
volatile uint32_t tickCount;            // JaSw: Used to count system ticks
uint32_t prevLoopTickCount;
volatile uint32_t tim6Counter;          // JaSw: Increases each time timer6 interrupts (100uS)
uint32_t prevTim6Counter;
bool SoftEnable = 0;                    // JaSw: Software motor enable
uint16_t softMoveStepCount;
uint8_t softMoveDirection;
uint8_t oledClock = 0x00;
bool streamAngle;
bool tuningMode;                        // JaSw: Indicates tuning mode, where some normal features are disabled

uint8_t Currents=0;                     //
uint8_t Motor_Dir=0;                    //
volatile uint8_t Motor_ENmode_flag=0;   //

uint16_t table1[15];                    //
volatile uint8_t Reset_status_flag=0;                    

int main(void)
{

    LL_Init();
    SystemClock_Config();


    MX_GPIO_Init();   
    OLED_Init();
    ShowStartupScreen();


    Configure_RTC_Clock();
    Configure_RTC();
    Configure_RTC_Calendar(22,1,1,18,30,00);
    OLED_ShowString(0,0,"RTC Clock init...ok");


    MX_SPI1_Init();
    MX_TIM3_Init();

    TLE5012B_Init();
    Serial_Init();

    LL_mDelay(100);

    STMFLASH_Read(Data_Store_Address, table1, sizeof(table1));
    clockMode=table1[6];
    if (clockMode==CLOCK_MODE_HOUR)   calibrateHour();    // go to calibration after reboot for hour

    // Read parameters                                                         
    
    // Check DIP switches
    // TODO: Maybe remove DIP switch options, can already
    // be set via OLED and serial. Will anyway be overriden.           
    ReadDIPSwitches();
    
    // Check encoder health.
    // TODO: Add message to OLED also
    if (CheckHealth() == false)
      for(uint8_t m=0;m<10;m++)
      {
        LED_H;
        LL_mDelay(200);
        LED_L;
        LL_mDelay(200);	
      } 

    // Apply parameters read from flash
    Currents = table1[1];
    stepangle = table1[3];
    Motor_ENmode_flag = table1[5];
    Motor_Dir = table1[7];
    
    kp = table1[11];
    ki = table1[12];
    kd = table1[13];

    closemode = 0; // table1[14];
    if(closemode > 1)     //vc: if saved value is not initialized yet
      closemode = 0;
    if(closemode == 1)
      PID_Cal_value_init();

    

    

    MX_USART1_UART_Init();                                      //USART Init

    MX_TIM1_Init();                                             //Tim1  Init

    MX_TIM6_Init();                                             //Tim6  Init 

    MX_IWDG_Init();                                             //Idog  Init

    OLED_Clear();
    BuildMenu(); 
    ShowInfoScreen();



//    moveClock(0);
    // Main loop
  //  OLED_Clear();

    while(1)
    { 	


        // Check if bytes are available on UART1 for parsing
        if (UART1_BytesToRead() > 0)
        {
          uint8_t data = UART1_Read();
          ParseBytes(data);
        }

        // Handle OLED menu navigation
        OledMenu(); 

        if (menuActive == 0)  {
                Show_RTC_Calendar();
        } 


    }
}



 
void OledMenu(void)
{
  uint8_t key;
  static uint32_t prevTickCount;

  if (!KeyScan(&key))
    return;

  
  if ((key & KEY_PRESSED_SELECT) > 0)     // lower button
  {
    if (menuActive == 1)
    {
      Menu_Button_Down(&menuMain);
      Menu_Show(&menuMain);
    }
  }

  if ((key & KEY_PRESSED_BACK) > 0)
  {
    // Slow down the rate at which this key can be used
    if ((tickCount - prevTickCount) > 250)
    {
      if (menuActive == 0)
      {
        menuActive = 1;
        OLED_Clear();
        Menu_Show(&menuMain);
      }
      else
        Menu_Select_Edit(&menuMain);
    }    
  }

  if ((key & KEY_PRESSED_CONFIRM) > 0)    // upper button
  {
    if (menuActive == 1)
    {
      Menu_Button_Up(&menuMain);
      Menu_Show(&menuMain);
    }
  }

  prevTickCount = tickCount;
}


void InvokeBootloader()
{
  // Leaving the watchdog enabled, so if no programming attempt is
  // made while in bootloader mode the watchdog times out and restarts
  // normal program execution.

  // Function pointer to bootloader in system memory
  // Referenced from AN2606 Rev.44 Table 141. 
  // The first word is the stack pointer and therefore the 4 byte offset.
  void (*BootMemJump)(void) = (*((uint32_t*) 0x1FFFEC04));

  // De-initialize UART1 as it will be used by the bootloader
  LL_USART_DeInit(USART1);

  // De-initialize RCC
  LL_RCC_DeInit();

  // Stop SysTick timer
  SysTick->CTRL = 0;
  SysTick->LOAD = 0;
  SysTick->VAL = 0;

  // Disable interrupts
  __disable_irq();
  
  __DSB();  
  __ISB();
  
  // Set Main Stack Pointer
  __set_MSP(0x20001000);
  
  // Jump to bootloader
  BootMemJump();
}


// Parse bytes received from the UART
void ParseBytes(uint8_t data)
{
   parseBuffer[pbIndex] = data;
   // check if it is a proper command
  if (parseBuffer[0]!='R' &&  parseBuffer[0]!='G' &&  parseBuffer[0]!='S'  &&  parseBuffer[0]!='M' &&  parseBuffer[0]!='T') {   // Result, Go, Start, Mode, Top
       ResetParser();
       parseBuffer[pbIndex] = data;
  }
   // RES Hour Minutes Seconds Year (0-99) Month Day IP-Part1 IP-Part22 IP-Part3 IP Part4
  if (pbIndex==12 &&  parseBuffer[0]=='R' && parseBuffer[1]=='E' &&  parseBuffer[2]=='S' ) {
        ResetParser();
        Configure_RTC_Calendar((uint32_t)parseBuffer[6],(uint32_t)parseBuffer[7],(uint32_t)parseBuffer[8],(uint32_t)parseBuffer[3],(uint32_t)parseBuffer[4],(uint32_t)parseBuffer[5]);
        ip_1=parseBuffer[9];ip_2=parseBuffer[10];ip_3=parseBuffer[11];ip_4=parseBuffer[12];
        ntp=1;
        return;
  }
  if (pbIndex==2) { // 3 byte commands
    /** move clock hand up,right,down or left GOU, GOR, GOD, GOL**/
    if (parseBuffer[0]=='G' && parseBuffer[1]=='O') {   // GO
        if (parseBuffer[2]=='U' ) {
              ResetParser();
              targetAngle=moveClock(0);
              stopClock=1;
              ntp=0;
              return;
        }
        if (parseBuffer[2]=='R' ) {
              ResetParser();
              targetAngle=moveClock(15);
              stopClock=1;
              ntp=0;
              return;
        }
        if (parseBuffer[2]=='D' ) {
              ResetParser();
              targetAngle=moveClock(30);
              stopClock=1;
              ntp=0;
              return;
        }
        if (parseBuffer[2]=='L' ) {
              ResetParser();
              targetAngle=moveClock(45);
              stopClock=1;
              ntp=0;
              return;
        }      
    }
     
    // STA start running clock 
    if (parseBuffer[0]=='S' && parseBuffer[1]=='T' &&  parseBuffer[2]=='A' ) {
        curMinute=-1;   
        stopClock=0;
        ResetParser();
        return;
    }  
    // set mode
    if (parseBuffer[0]=='M' && parseBuffer[1]=='O') {   // MO
      // MOS mode seconds (not running precise)
      if (parseBuffer[2]=='S' ) {
          ResetParser();
          curMinute=-1;    // send to UART
          clockMode=CLOCK_MODE_SECOND;
          StoreCurrentParameters();
          return;
      }
      // MOM mode minutes 
      if (parseBuffer[2]=='M' ) {
          ResetParser();
          curMinute=-1;   
          clockMode=CLOCK_MODE_MINUTE;
          StoreCurrentParameters();
          return;
      }
      // MOH mode hours+minutes (needs sensor input)
      if (parseBuffer[2]=='H' ) {
          ResetParser();
          curMinute=-1;  
          clockMode=CLOCK_MODE_HOUR;
          calibrateHour();
          StoreCurrentParameters();
          return;
      }
      // MOC crazy mode
      if (parseBuffer[2]=='C' ) {
          ResetParser();
          curMinute=-1;  
          clockMode=CLOCK_MODE_CRAZY;
          StoreCurrentParameters();
          return;
      }
    }

    /* TOP top sensor signal detected - reading correction value
    sensor has to be connected to ESP board
    this sensor could be also connected directly to STM32 board so check in main loop there and call topCorrection() as well
    */
    if (parseBuffer[0]=='T' && parseBuffer[1]=='O' &&  parseBuffer[2]=='P' ) {
        ResetParser();
        topCorrection();
        return;
    }
  } // 3 byte commands


  
  if (pbIndex>15)     ResetParser();
  else pbIndex++;
}


void SoftStep(uint8_t direction)
{
  uint16_t softStep = LL_TIM_GetCounter(TIM1);

  if(direction == 1)
    softStep++;
  else
    softStep--;
  
  // Update step
  LL_TIM_SetCounter(TIM1, softStep);
}


void SoftMoveStep()
{
  // Maybe add more checks, HW override SW?
  // Dalk n enable ook sit, omdat hy in main loop geroep word hardloop hy sodra softMoveStepCount groter as 0 is.
  if (enmode == 1)
  {
    if(softMoveStepCount > 0)
    {
      SoftStep(softMoveDirection);
      softMoveStepCount--;
    }
  }
}


static void LL_Init(void)
{
  LL_APB1_GRP2_EnableClock(LL_APB1_GRP2_PERIPH_SYSCFG);
  NVIC_SetPriority(SVC_IRQn, 0);
  NVIC_SetPriority(PendSV_IRQn, 0);
  NVIC_SetPriority(SysTick_IRQn, 0);
}





static void MX_IWDG_Init(void)
{
  // WDG Timer counts down from reload value
  // Setting window to 4095 disables window mode.
  // Clock is 40kHz (25us) with a prescaler of 256 it would
  // take 25us * 256 * 4096 = 26.21s to reset the MCU. 
  LL_IWDG_Enable(IWDG);
  LL_IWDG_EnableWriteAccess(IWDG);
  LL_IWDG_SetPrescaler(IWDG, LL_IWDG_PRESCALER_256);
  LL_IWDG_SetWindow(IWDG, 4095);                        
  LL_IWDG_SetReloadCounter(IWDG, 0xfff);
  while (LL_IWDG_IsReady(IWDG) != 1)
  {}
  LL_IWDG_ReloadCounter(IWDG);
}


static void MX_SPI1_Init(void)
{
  LL_SPI_InitTypeDef SPI_InitStruct;
  LL_GPIO_InitTypeDef GPIO_InitStruct;
	
  LL_APB1_GRP2_EnableClock(LL_APB1_GRP2_PERIPH_SPI1);
  
  /**SPI1 GPIO Configuration  
  PA4   ------> SPI1_NSS
  PA5   ------> SPI1_SCK
  PA6   ------> SPI1_MISO
  PA7   ------> SPI1_MOSI 
  */

  GPIO_InitStruct.Pin = LL_GPIO_PIN_5;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_ALTERNATE;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_HIGH;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_DOWN;
  GPIO_InitStruct.Alternate = LL_GPIO_AF_0;
  LL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = LL_GPIO_PIN_6;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_ALTERNATE;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_HIGH;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_DOWN;
  GPIO_InitStruct.Alternate = LL_GPIO_AF_0;
  LL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = LL_GPIO_PIN_7;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_ALTERNATE;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_HIGH;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_DOWN;
  GPIO_InitStruct.Alternate = LL_GPIO_AF_0;
  LL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  SPI_InitStruct.TransferDirection = LL_SPI_FULL_DUPLEX;
  SPI_InitStruct.Mode = LL_SPI_MODE_MASTER;
  SPI_InitStruct.DataWidth = LL_SPI_DATAWIDTH_16BIT;
  SPI_InitStruct.ClockPolarity = LL_SPI_POLARITY_LOW;
  SPI_InitStruct.ClockPhase = LL_SPI_PHASE_2EDGE;
  SPI_InitStruct.NSS = LL_SPI_NSS_SOFT;
  SPI_InitStruct.BaudRate = LL_SPI_BAUDRATEPRESCALER_DIV8;
  SPI_InitStruct.BitOrder = LL_SPI_MSB_FIRST;
  SPI_InitStruct.CRCCalculation = LL_SPI_CRCCALCULATION_DISABLE;
  SPI_InitStruct.CRCPoly = 7;
  LL_SPI_Init(SPI1, &SPI_InitStruct);
  LL_SPI_SetStandard(SPI1, LL_SPI_PROTOCOL_MOTOROLA); 
  LL_SPI_Enable(SPI1); 
}

// Initialize Timer1 with STEP pin as clock source
static void MX_TIM1_Init(void)
{
  LL_TIM_InitTypeDef TIM_InitStruct = {0};
  LL_GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* Peripheral clock enable */
  LL_APB1_GRP2_EnableClock(LL_APB1_GRP2_PERIPH_TIM1);
  
  LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOA);
  /**TIM1 GPIO Configuration  
  PA12   ------> TIM1_ETR 
  */
  GPIO_InitStruct.Pin = LL_GPIO_PIN_12;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_ALTERNATE;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_HIGH;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  GPIO_InitStruct.Alternate = LL_GPIO_AF_2;
  LL_GPIO_Init(GPIOA, &GPIO_InitStruct);


  TIM_InitStruct.Prescaler = 0;
  TIM_InitStruct.CounterMode = LL_TIM_COUNTERMODE_UP;
  TIM_InitStruct.Autoreload = 0xFFFF;
  TIM_InitStruct.ClockDivision = LL_TIM_CLOCKDIVISION_DIV1;
  TIM_InitStruct.RepetitionCounter = 0;
  LL_TIM_Init(TIM1, &TIM_InitStruct);
  LL_TIM_DisableARRPreload(TIM1);
  LL_TIM_ConfigETR(TIM1, LL_TIM_ETR_POLARITY_NONINVERTED, LL_TIM_ETR_PRESCALER_DIV1, LL_TIM_ETR_FILTER_FDIV4_N8);
  LL_TIM_SetClockSource(TIM1, LL_TIM_CLOCKSOURCE_EXT_MODE2);
  LL_TIM_SetTriggerOutput(TIM1, LL_TIM_TRGO_RESET);
  LL_TIM_DisableMasterSlaveMode(TIM1);
  LL_TIM_EnableCounter(TIM1);
}

//Looks like Timer3 is setup as the PWM driving the A4950 
static void MX_TIM3_Init(void)
{
  LL_TIM_InitTypeDef TIM_InitStruct;
  LL_TIM_OC_InitTypeDef TIM_OC_InitStruct;
  LL_GPIO_InitTypeDef GPIO_InitStruct;

  LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_TIM3);
	
  /**TIM3 GPIO Configuration  
  PB4   ------> TIM3_CH1
  PB5   ------> TIM3_CH2 
  */
  GPIO_InitStruct.Pin = PWM1_Pin;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_ALTERNATE;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_HIGH;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  GPIO_InitStruct.Alternate = LL_GPIO_AF_1;
  LL_GPIO_Init(PWM1_GPIO_Port, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = PWM2_Pin;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_ALTERNATE;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_HIGH;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  GPIO_InitStruct.Alternate = LL_GPIO_AF_1;
  LL_GPIO_Init(PWM2_GPIO_Port, &GPIO_InitStruct);

  TIM_InitStruct.Prescaler = 0;
  TIM_InitStruct.CounterMode = LL_TIM_COUNTERMODE_UP;
  TIM_InitStruct.Autoreload = 256;                              // JaSw: Determine PWM freq. 48MHz / 256 = 187.5kHz. 256 = 8bit resolution
  TIM_InitStruct.ClockDivision = LL_TIM_CLOCKDIVISION_DIV1;
  LL_TIM_Init(TIM3, &TIM_InitStruct);

  LL_TIM_DisableARRPreload(TIM3);

  LL_TIM_SetClockSource(TIM3, LL_TIM_CLOCKSOURCE_INTERNAL);

  LL_TIM_OC_EnablePreload(TIM3, LL_TIM_CHANNEL_CH1);

  TIM_OC_InitStruct.OCMode = LL_TIM_OCMODE_PWM1;
  TIM_OC_InitStruct.OCState = LL_TIM_OCSTATE_DISABLE;
  TIM_OC_InitStruct.OCNState = LL_TIM_OCSTATE_DISABLE;
  TIM_OC_InitStruct.CompareValue = 0;
  TIM_OC_InitStruct.OCPolarity = LL_TIM_OCPOLARITY_HIGH;
  LL_TIM_OC_Init(TIM3, LL_TIM_CHANNEL_CH1, &TIM_OC_InitStruct);

  LL_TIM_OC_DisableFast(TIM3, LL_TIM_CHANNEL_CH1);

  LL_TIM_OC_EnablePreload(TIM3, LL_TIM_CHANNEL_CH2);

  TIM_OC_InitStruct.OCState = LL_TIM_OCSTATE_DISABLE;
  TIM_OC_InitStruct.OCNState = LL_TIM_OCSTATE_DISABLE;
  LL_TIM_OC_Init(TIM3, LL_TIM_CHANNEL_CH2, &TIM_OC_InitStruct);

  LL_TIM_OC_DisableFast(TIM3, LL_TIM_CHANNEL_CH2);

  LL_TIM_SetTriggerOutput(TIM3, LL_TIM_TRGO_RESET);

  LL_TIM_DisableMasterSlaveMode(TIM3);
  
  LL_TIM_CC_EnableChannel(TIM3, LL_TIM_CHANNEL_CH1);
  LL_TIM_CC_EnableChannel(TIM3, LL_TIM_CHANNEL_CH2);
  LL_TIM_EnableCounter(TIM3);
}

// Use to perform the PID loop
// Looks like no prescaler so 48MHz / 4800 = 10kHz = 100us
static void MX_TIM6_Init(void)
{
  LL_TIM_InitTypeDef TIM_InitStruct;

  LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_TIM6);
  NVIC_SetPriority(TIM6_IRQn, 1);
  NVIC_EnableIRQ(TIM6_IRQn);

  TIM_InitStruct.Prescaler = 0;
  TIM_InitStruct.CounterMode = LL_TIM_COUNTERMODE_UP;
  TIM_InitStruct.Autoreload = 4800;//
  LL_TIM_Init(TIM6, &TIM_InitStruct);

  LL_TIM_DisableARRPreload(TIM6);
	
  LL_TIM_EnableIT_UPDATE(TIM6);
  LL_TIM_EnableCounter(TIM6);
}



static void MX_GPIO_Init(void)
{
  LL_GPIO_InitTypeDef GPIO_InitStruct;
  LL_EXTI_InitTypeDef EXTI_InitStruct;

  LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOC);
  LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOA);
  LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOB);

  LL_GPIO_ResetOutputPin(LED_GPIO_Port, LED_Pin);
  LL_GPIO_ResetOutputPin(IN1_GPIO_Port, IN1_Pin);
  LL_GPIO_ResetOutputPin(IN2_GPIO_Port, IN2_Pin);
  LL_GPIO_ResetOutputPin(IN4_GPIO_Port, IN4_Pin);
  LL_GPIO_ResetOutputPin(IN3_GPIO_Port, IN3_Pin);

/*****************************************/
  GPIO_InitStruct.Pin = CAL_Pin;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_UP;
  LL_GPIO_Init(CAL_GPIO_Port, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = CLOSE_Pin;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_UP;
  LL_GPIO_Init(CLOSE_GPIO_Port, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = SET2_Pin;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_UP;
  LL_GPIO_Init(SET2_GPIO_Port, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = SET1_Pin;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_UP;
  LL_GPIO_Init(SET1_GPIO_Port, &GPIO_InitStruct);
/*****************************************************/
  GPIO_InitStruct.Pin = KEY_Select_Pin;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_UP;
  LL_GPIO_Init(KEY_Select_GPIO_Port, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = KEY_Back_Pin;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_UP;
  LL_GPIO_Init(KEY_Back_GPIO_Port, &GPIO_InitStruct);   
  
  GPIO_InitStruct.Pin = KEY_Confirm_Pin;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_UP;
  LL_GPIO_Init(KEY_Confirm_GPIO_Port, &GPIO_InitStruct); 
  
/***********************************************/  
/***************oled IO**************************************/
  GPIO_InitStruct.Pin = OLED_CS_Pin| OLED_RS_Pin |OLED_SCLK_Pin|OLED_SDIN_Pin;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_OUTPUT;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_HIGH;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  LL_GPIO_Init(GPIOB, &GPIO_InitStruct);
  
  GPIO_InitStruct.Pin = OLED_RST_Pin;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_OUTPUT;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_HIGH;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  LL_GPIO_Init(OLED_RST_GPIO_Port, &GPIO_InitStruct);
  
/*************************************************/
  GPIO_InitStruct.Pin = LED_Pin;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_OUTPUT;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_HIGH;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  LL_GPIO_Init(LED_GPIO_Port, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = IN1_Pin;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_OUTPUT;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_HIGH;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  LL_GPIO_Init(IN1_GPIO_Port, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = IN2_Pin;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_OUTPUT;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_HIGH;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  LL_GPIO_Init(IN2_GPIO_Port, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = IN4_Pin;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_OUTPUT;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_HIGH;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  LL_GPIO_Init(IN4_GPIO_Port, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = IN3_Pin;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_OUTPUT;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_HIGH;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  LL_GPIO_Init(IN3_GPIO_Port, &GPIO_InitStruct);
 /* 
  GPIO_InitStruct.Pin = NSS_Pin;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_OUTPUT;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_HIGH;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  LL_GPIO_Init(NSS_GPIO_Port, &GPIO_InitStruct);
*/
  LL_SYSCFG_SetEXTISource(LL_SYSCFG_EXTI_PORTB, LL_SYSCFG_EXTI_LINE1);
  LL_SYSCFG_SetEXTISource(LL_SYSCFG_EXTI_PORTB, LL_SYSCFG_EXTI_LINE2);
  
  LL_GPIO_SetPinPull(DIRIN_GPIO_Port, DIRIN_Pin, LL_GPIO_PULL_NO);//
  LL_GPIO_SetPinPull(ENIN_GPIO_Port, ENIN_Pin, LL_GPIO_PULL_UP);//
  LL_GPIO_SetPinMode(DIRIN_GPIO_Port, DIRIN_Pin, LL_GPIO_MODE_INPUT);
  LL_GPIO_SetPinMode(ENIN_GPIO_Port, ENIN_Pin, LL_GPIO_MODE_INPUT);

  EXTI_InitStruct.Line_0_31 = LL_EXTI_LINE_1;
  EXTI_InitStruct.LineCommand = ENABLE;
  EXTI_InitStruct.Mode = LL_EXTI_MODE_IT;
  EXTI_InitStruct.Trigger = LL_EXTI_TRIGGER_RISING_FALLING;
  LL_EXTI_Init(&EXTI_InitStruct);

//  EXTI_InitStruct.Line_0_31 = LL_EXTI_LINE_2;
//  EXTI_InitStruct.LineCommand = ENABLE;
//  EXTI_InitStruct.Mode = LL_EXTI_MODE_IT;
//  EXTI_InitStruct.Trigger = LL_EXTI_TRIGGER_RISING_FALLING;
//  LL_EXTI_Init(&EXTI_InitStruct);

  NVIC_SetPriority(EXTI0_1_IRQn, 0);
  NVIC_DisableIRQ(EXTI0_1_IRQn);
//  NVIC_SetPriority(EXTI2_3_IRQn, 1);
//  NVIC_DisableIRQ(EXTI2_3_IRQn);
}


void ReadDIPSwitches()
{
  // Step angle switches
  if((SET1==1) && (SET2==1))
    stepangle=16;
  else if((SET1==0) && (SET2==1))
    stepangle=8;
  else if((SET1==1) && (SET2==0))
    stepangle=4;
  else
    stepangle=2;

  // Open/Close loop switch
  if(CLOSE==0 )
  {
    closemode=1;

    // Set initial values      
    PID_Cal_value_init();

    // r=*(volatile uint16_t*)((ReadValue(READ_ANGLE_VALUE)>>1)*2+0x08008000); 
    // s_sum=r;   
    // y=r;
    // y_1=y;
    // yw=y;  
    // yw_1=yw;
  }
  else
  {
    closemode=0;
  }
}




// Energize coils. The electrical angle is given by theta and the current limit by effort.
void Output(int32_t theta, uint8_t effort) 
{
  // JaSw: To control the current in the coils the voltage on the VRef pin of the A4950
  // needs to be adjusted. This is done through a PWM output and filter circuit which is
  // connected to VRef.
  // One electrical 360° cycle consist of 4 discrete stages which also moves the motor 
  // 4 full steps.
  // The sine lookup table has a size of 4096 for 360°. So each step is 1024 elements 
  // or 90° appart. The code below has a phase_multiplier of 12.5 which means a single
  // full step would require theta of 1024 / 12.5 = 81.92 
  // The effort argument sets the current limit. TIM3 provides the PWM signal to the Vref 
  // pin of the A4950. Rs is 0.2 Ohm, so the current limit is Vref/2. TIM3 reloads on 
  // 256 counts and if we assume 100% duty is 3.3V then each count would represent about
  // (3.3 / 256) / 2 = 6.45 mA. So an effort value of 80 would have a Vref of 1V and a
  // current limit of 0.516A
  
  int16_t v_coil_A;
  int16_t v_coil_B;

  int16_t sin_coil_A;
  int16_t sin_coil_B;
	
  int16_t angle_1;
  int16_t angle_2;
		
  float phase_multiplier = 12.5f;                     // Multiply theta by 12.5 Not really sure what the purpose of this is?
                                                      
  angle_1 = Mod(phase_multiplier * theta, 4096);
  angle_2 = angle_1 + 1024;                           // Sin to Cos conversion (1024 = 90°)
  if(angle_2 > 4096)
    angle_2 -= 4096;

  sin_coil_A = sin_1[angle_1];
  sin_coil_B = sin_1[angle_2];

  // Calculate the timer compare values. 
  // This detemines the PWM duty which again determines the coil current. The sine table
  // is scaled by 1024. So to normalize to 1, the value is divided by 1024.
  v_coil_A = effort * sin_coil_A / 1024;
  v_coil_B = effort * sin_coil_B / 1024;
	
  // Set the TIM3 PWM duty for channel2 and switch the H-Bridge
  if(v_coil_A>=0)  
  {
    LL_TIM_OC_SetCompareCH2(TIM3,v_coil_A);  
	  IN1_HIGH;                                         
    IN2_LOW;                                          
  }
  else  
  {
    LL_TIM_OC_SetCompareCH2(TIM3,-v_coil_A);  
	  IN1_LOW;     
    IN2_HIGH;  
  } 

  // Set the TIM3 PWM duty for channel1 and switch the H-Bridge
  if(v_coil_B>=0)  
  {
    LL_TIM_OC_SetCompareCH1(TIM3,v_coil_B);  
	  IN3_HIGH;  
    IN4_LOW;  
  }
  else 
  {
    LL_TIM_OC_SetCompareCH1(TIM3,-v_coil_B); 
	  IN3_LOW;     
    IN4_HIGH;    
  }
}


// Takes a step of which the size is provided in milli-degrees (max 1800m°) 
void FineStep(uint16_t stepSize_mdeg, uint16_t current_mA )
{
  // This function is currently used more for testing and analysis purposes.

  // Limited to a full step size
  if (stepSize_mdeg > 1800)
    stepSize_mdeg = 1800;

  // Set current max limit
  if (current_mA > 1600)
    current_mA = 1600;

  if(dir) 
    stepnumber+=1;
  else 
    stepnumber-=1;

  // Die float opsie is swaar op geheue, het eerder integers gebruik en met
  // 22 gedeel wat 1000/45.51 is. Moet nog toets of dit akkuraat genoeg is.
  //float stepScale = (stepSize_mdeg / 1000.0f) * 45.51;
  int32_t stepScale = (stepSize_mdeg / 22) * stepnumber;

  // Deel met 6.45 maar gebruik eerder integer wiskunde wat deel met 6.5, 
  // behoort akkuraat genoeg te wees en gebruik minder geheue.
  uint8_t currentScale = (uint8_t)(current_mA / 13) * 2;    
  //uint8_t currentScale = (uint8_t)(current_mA / 6.45);

  Output(stepScale * stepnumber, currentScale);     
  //LL_mDelay(10);
}


// Take one full step (1.8°)
void OneStep(void)
{          
  if(dir) 
    stepnumber+=1;
  else 
    stepnumber-=1;
  
  // The sine table is 4096 elements long. A single 1.8° step would be a 90° electrical 
  // shift, which is 1024 elements. The output function has a multiplier of 12.5 so to 
  // move a single step we need to move 1024 / 12.5 = 81.92 units. 
  Output(81.92f * stepnumber, 80);     
  LL_mDelay(25);
}


int16_t Mod(int32_t xMod,int16_t mMod) 
{
  int16_t temp;
  temp=xMod%mMod;
  if(temp<0)
    return (temp+mMod);
  else
	return  temp;
}

           
void CalibrateEncoder(void) 
{   
 
  FlashLock();


  // Set default values
  table1[0] =0xAA;                
  table1[1] =128;
  table1[2] =16;
  table1[3] =4;
  table1[4] =3;
  table1[5] =0;
  table1[6] =clockMode;
  table1[7] =1;
  table1[8] =1;
  table1[11]=kp;                  
  table1[12]=ki;
  table1[13]=kd;
  table1[14]=closemode;

  Calibration_flag = 0xAA;    
  STMFLASH_Write(Data_Store_Address,table1,sizeof(table1));
  
  
  //NVIC_EnableIRQ(EXTI0_1_IRQn);
  //NVIC_EnableIRQ(EXTI2_3_IRQn);

  //CalibrateEncoder_finish_flag=1; //  

  // Display calibration complete and wait for user restart.
  // TODO: Why restart, maybe auto reload/restart
  for(;;)
  {
    LED_F;
    LL_mDelay(200);
  }
}


// Change the Display Clock Divide Ratio/Oscillator Frequency
// Experimental feature
void ChangeOLEDClock()
{
  OLED_SetDisplayClock(oledClock);
}


void PID_Cal_value_init(void)
{
    s       = 0;            // TIM1 counts which is step counts
    s_1     = 0;            // Previous step count
    
    s_sum   = 0;            // Step sum offset
    r       = 0;            // Desired angle / setpoint
    r_1     = 0;            // Previous desired angle
    y       = 0;            // Output theta / corrected encoder reading
    y_1     = 0;            // Previous output theta
    yw      = 0;            // Wrapped angle
    pid_e   = 0;
    u       = 0;
    dterm   = 0;
    wrap_count=0;
    //LL_TIM_SetCounter(TIM1,0);
    WRITE_REG(TIM1->CNT, 0);
    
    r=*(volatile uint16_t*)((ReadValue(READ_ANGLE_VALUE)>>1)*2+0x08008000); //
    s_sum=r;   //
    y=r;
    y_1=y;
    yw=y;  
    yw_1=yw;
    
}


//
// void Prompt_show(void)
// {
//     OLED_Clear();
//     //OLED_ShowString(0,0,"              ");
//     OLED_ShowString(0,16,"   finished!  ");
//     OLED_ShowString(0,32,"  Please press ");
//     OLED_ShowString(0,48,"Reset Key reboot");
// }

void _Error_Handler(char *file, int line)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  while(1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT

void assert_failed(uint8_t* file, uint32_t line)
{ 
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     tex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */


/**
  * @}
  */

/**
  * @}
  */

