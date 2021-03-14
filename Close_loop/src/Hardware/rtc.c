#include "rtc.h"
#include <stm32f0xx_ll_rtc.h>
#include <stm32f0xx_ll_rcc.h>
#include "main.h"
#include "oled.h"
#include "buttons.h"

int clockMode=CLOCK_MODE_SECOND;

void SystemClock_Config(void)
{
  // 8MHz HSI Clock -> div_2 -> pll_mult_12 = 48MHz

  LL_FLASH_SetLatency(LL_FLASH_LATENCY_1);
	
  if(LL_FLASH_GetLatency() != LL_FLASH_LATENCY_1)
  {
    Error_Handler();  
  }
  LL_RCC_HSI_Enable();
  
  while(LL_RCC_HSI_IsReady() != 1)
  {}
  LL_RCC_HSI_SetCalibTrimming(16);
  LL_RCC_LSI_Enable();
  while(LL_RCC_LSI_IsReady() != 1)
  {}
  LL_RCC_PLL_ConfigDomain_SYS(LL_RCC_PLLSOURCE_HSI_DIV_2, LL_RCC_PLL_MUL_12);
  LL_RCC_PLL_Enable();
	  
  while(LL_RCC_PLL_IsReady() != 1)
  {}
  LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_1);
  LL_RCC_SetAPB1Prescaler(LL_RCC_APB1_DIV_1);
  LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_PLL);
	  
  while(LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_PLL)
  {}
  LL_Init1msTick(48000000);
  LL_SYSTICK_SetClkSource(LL_SYSTICK_CLKSOURCE_HCLK);  

  SysTick->CTRL |= SysTick_CTRL_TICKINT_Msk;                  // Enable SysTick interrupt
  LL_SetSystemCoreClock(48000000);
  LL_RCC_SetUSARTClockSource(LL_RCC_USART1_CLKSOURCE_PCLK1);
  NVIC_SetPriority(SysTick_IRQn, 0);

  
}


/**
  * @brief  Exit Initialization mode 
  * @param  None
  * @retval RTC_ERROR_NONE if no error
  */
uint32_t Exit_RTC_InitMode(void)
{
  LL_RTC_DisableInitMode(RTC);
  
  /* Wait for synchro */
  /* Note: Needed only if Shadow registers is enabled           */
  /*       LL_RTC_IsShadowRegBypassEnabled function can be used */
  return (WaitForSynchro_RTC());
}

/**
  * @brief  Enter in initialization mode
  * @note In this mode, the calendar counter is stopped and its value can be updated
  * @param  None
  * @retval RTC_ERROR_NONE if no error
  */
uint32_t Enter_RTC_InitMode(void)
{
  /* Set Initialization mode */
  LL_RTC_EnableInitMode(RTC);
  
#if (USE_TIMEOUT == 1)
    Timeout = RTC_TIMEOUT_VALUE;
#endif /* USE_TIMEOUT */

  /* Check if the Initialization mode is set */
  while (LL_RTC_IsActiveFlag_INIT(RTC) != 1)
  {

  }
  
  return SUCCESS;
}



/**
  * @brief  Wait until the RTC Time and Date registers (RTC_TR and RTC_DR) are
  *         synchronized with RTC APB clock.
  * @param  None
  * @retval RTC_ERROR_NONE if no error (RTC_ERROR_TIMEOUT will occur if RTC is 
  *         not synchronized)
  */
uint32_t WaitForSynchro_RTC(void)
{
  /* Clear RSF flag */
  LL_RTC_ClearFlag_RS(RTC);



  /* Wait the registers to be synchronised */
  while(LL_RTC_IsActiveFlag_RS(RTC) != 1)
  {

  }
  return SUCCESS;
}


/**
  * @brief  Configure RTC.
  * @note   Peripheral configuration is minimal configuration from reset values.
  *         Thus, some useless LL unitary functions calls below are provided as
  *         commented examples - setting is default configuration from reset.
  * @param  None
  * @retval None
  */
void Configure_RTC(void)
{
  /*##-1- Enable RTC peripheral Clocks #######################################*/
  /* Enable RTC Clock */ 
  LL_RCC_EnableRTC();

  /*##-2- Disable RTC registers write protection ##############################*/
  LL_RTC_DisableWriteProtection(RTC);

  /*##-3- Enter in initialization mode #######################################*/
  Enter_RTC_InitMode();

  /*##-4- Configure RTC ######################################################*/
  /* Configure RTC prescaler and RTC data registers */
  /* Set Hour Format */
  LL_RTC_SetHourFormat(RTC, LL_RTC_HOURFORMAT_AMPM);
  /* Set Asynch Prediv (value according to source clock) */
  LL_RTC_SetAsynchPrescaler(RTC, RTC_ASYNCH_PREDIV);
  /* Set Synch Prediv (value according to source clock) */
  LL_RTC_SetSynchPrescaler(RTC, RTC_SYNCH_PREDIV);


  /*##-5- Exit of initialization mode #######################################*/
  Exit_RTC_InitMode();
  
  /*##-6- Enable RTC registers write protection #############################*/
  LL_RTC_EnableWriteProtection(RTC);
}
/**
  * @brief  Configure RTC clock.
  * @param  None
  * @retval None
  */
void Configure_RTC_Clock(void)
{
  /*##-1- Enables the PWR Clock and Enables access to the backup domain #######*/
  /* To change the source clock of the RTC feature (LSE, LSI), you have to:
     - Enable the power clock
     - Enable write access to configure the RTC clock source (to be done once after reset).
     - Reset the Back up Domain
     - Configure the needed RTC clock source */
  LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_PWR);
  LL_PWR_EnableBkUpAccess();
  

  LL_RCC_LSI_Enable();

  while (LL_RCC_LSI_IsReady() != 1)
  {
  }
  /* Reset backup domain only if LSI is not yet selected as RTC clock source */
  if (LL_RCC_GetRTCClockSource() != LL_RCC_RTC_CLKSOURCE_LSI)
  {
    LL_RCC_ForceBackupDomainReset();
    LL_RCC_ReleaseBackupDomainReset();
    LL_RCC_SetRTCClockSource(LL_RCC_RTC_CLKSOURCE_LSI);
  }


}

int curSecond=0;
int calViewClear=0;
/**
  * @brief  Display the current time and date.
  * @param  None
  * @retval None
  */
void Show_RTC_Calendar(void)
{
  int second,minute,hour;
  uint32_t temp,temp2=0;
  int32_t tmpf=0;
  second=__LL_RTC_CONVERT_BCD2BIN(LL_RTC_TIME_GetSecond(RTC));
  if (second==curSecond) return;
  if (!calViewClear) {
    OLED_Clear();
    calViewClear=1;
  }
  curSecond=second;
  hour=__LL_RTC_CONVERT_BCD2BIN(LL_RTC_TIME_GetHour(RTC));
  minute=__LL_RTC_CONVERT_BCD2BIN(LL_RTC_TIME_GetMinute(RTC));
  
/* Buffers used for displaying Time and Date */
  char aShowTime[30] = {0};
  char  aShowDate[30] = {0};
  char debug1[30]={0};
  char debug2[30]={0};
  int targetAngle=0;

  /* Note: need to convert in decimal value in using __LL_RTC_CONVERT_BCD2BIN helper macro */
  /* Display time Format : hh:mm:ss */
  sprintf((char*)aShowTime,"%.2d:%.2d:%.2d        ", hour,minute,second);

  /* Display date Format : mm-dd-yy */
  sprintf((char*)aShowDate,"%.2d-%.2d-%.2d", __LL_RTC_CONVERT_BCD2BIN(LL_RTC_DATE_GetMonth(RTC)), 
          __LL_RTC_CONVERT_BCD2BIN(LL_RTC_DATE_GetDay(RTC)), 
          2000 + __LL_RTC_CONVERT_BCD2BIN(LL_RTC_DATE_GetYear(RTC)));

      
  tmpf=ReadAngle() * 0.021972f;
  switch(clockMode) {
        case CLOCK_MODE_SECOND: targetAngle=moveClock(second);   break;
        case CLOCK_MODE_MINUTE: targetAngle=moveClock(minute);   break;
        case CLOCK_MODE_HOUR:   
          temp=hour; 
          if (temp>12) temp-=12;
          temp--; // 0-11
          targetAngle=moveClock(hour*5+minute/12);   
        break;
        default:
          clockMode=CLOCK_MODE_SECOND;  
  }
  
  sprintf((char*)debug1,"%d to %d Deg    ",tmpf, targetAngle); 
  OLED_Info(aShowTime,aShowDate,debug1,"");


} 

/** wrong direction
 * move clock to an absolute value 0-59 (seconds or minute )
 * for hour just call moveClock(hour*5)
 * */
/*void moveClock(int val) {
  int angle,targetAngle,steps;
  angle=ReadAngle() * 0.021972f;  // Umrechnung in Grad
  targetAngle=val*6;
  steps=abs(targetAngle-angle)/1.8;
  if ((targetAngle-angle)<=0) {
      steps=(360-angle+targetAngle)/1.8;
  }
  dir=1;
   for(int i=0;i<steps;i++) OneStep();
}*/
int moveClock(int val) {
  int angle,targetAngle,steps;
  angle=ReadAngle() * 0.021972f;  // Umrechnung in Grad
  targetAngle=360-(val*6);
  steps=abs(angle-targetAngle)/1.8;
  if ((angle-targetAngle)<0) {
      steps=(360-targetAngle+angle)/1.8;
  }
  dir=0;
  for(int i=0;i<steps;i++) OneStep();
  return targetAngle;
} 
/**
  * @brief  Configure the current time and date.
  * @param  None
  * @retval None
  */
void Configure_RTC_Calendar(uint32_t y,uint32_t m,uint32_t d,uint32_t hour,uint32_t minute)
{
  int ytmp=y+2000;
  int dtmp=d;
  uint32_t weekday  = (dtmp += m < 3 ? ytmp-- : ytmp - 2, 23*m/9 + dtmp + 4 + ytmp/4- ytmp/100 + ytmp/400)%7;  
  /*##-1- Disable RTC registers write protection ############################*/
  LL_RTC_DisableWriteProtection(RTC);

  /*##-2- Enter in initialization mode ######################################*/
  Enter_RTC_InitMode(); 

  /*##-3- Configure the Date ################################################*/
  /* Note: __LL_RTC_CONVERT_BIN2BCD helper macro can be used if user wants to*/
  /*       provide directly the decimal value:                               */
  /*       LL_RTC_DATE_Config(RTC, LL_RTC_WEEKDAY_MONDAY,                    */
  /*                          __LL_RTC_CONVERT_BIN2BCD(31), (...))           */
  /* Set Date: Friday December 29th 2016 */
  LL_RTC_DATE_Config(RTC, __LL_RTC_CONVERT_BIN2BCD(weekday), __LL_RTC_CONVERT_BIN2BCD(d), __LL_RTC_CONVERT_BIN2BCD(m), __LL_RTC_CONVERT_BIN2BCD(y));
  
  /*##-4- Configure the Time ################################################*/
  /* Set Time: 11:59:55 PM*/
  LL_RTC_TIME_Config(RTC, LL_RTC_TIME_FORMAT_PM, __LL_RTC_CONVERT_BIN2BCD(hour),  __LL_RTC_CONVERT_BIN2BCD(minute), 0x00);
  
  /*##-5- Exit of initialization mode #######################################*/
  Exit_RTC_InitMode();
   
  /*##-6- Enable RTC registers write protection #############################*/
  LL_RTC_EnableWriteProtection(RTC);

  /*##-8- Writes a data in a RTC Backup data Register1 #######################*/
 // LL_RTC_BAK_SetRegister(RTC, LL_RTC_BKP_DR1, RTC_BKP_DATE_TIME_UPDTATED);
}
/**
 * Set mode: CLOCK_MODE_SECOND,CLOCK_MODE_MINUTE or CLOCK_MODE_HOUR
**/
void setClockModeUI(void ) {
  static uint32_t prevTickCount;
  char modeStr[10] = {0};
  prevTickCount=tickCount;
  OLED_Clear();

    while(1) {
      uint8_t key;
      if (!KeyScan(&key)) continue;
      if ((key & KEY_PRESSED_BACK) > 0  && ((tickCount - prevTickCount) > 250)) { // middle button to confirm
          OLED_Clear();
          menuActive=0;
          StoreCurrentParameters();
          return;
      }
      if ((key & KEY_PRESSED_SELECT) > 0)  {   // upper button
        if (clockMode>0) clockMode--; else clockMode=CLOCK_MODE_HOUR;
      }
      if ((key & KEY_PRESSED_CONFIRM ) > 0) { // lower button
        if (clockMode>1) clockMode++; else clockMode=CLOCK_MODE_SECOND;
      }
      switch(clockMode) {
        case CLOCK_MODE_SECOND: strcpy(modeStr, "SECONDS"); break;
        case CLOCK_MODE_MINUTE: strcpy(modeStr, "MINUTES"); break;
        case CLOCK_MODE_HOUR:   strcpy(modeStr, "HOURS  "); break;
        default:
          clockMode=CLOCK_MODE_SECOND;
          strcpy(modeStr, "SECONDS"); 
      }
      OLED_Info("Clock mode:",modeStr,"","");
      prevTickCount = tickCount;
    }
}
/**
 * Menu for setting time and date via buttons
 * 
**/
void setTimeDateUI(void) {
  int hour=__LL_RTC_CONVERT_BCD2BIN(LL_RTC_TIME_GetHour(RTC));
  int minute=__LL_RTC_CONVERT_BCD2BIN(LL_RTC_TIME_GetMinute(RTC));
  int month=__LL_RTC_CONVERT_BCD2BIN(LL_RTC_DATE_GetMonth(RTC));
  int day=   __LL_RTC_CONVERT_BCD2BIN(LL_RTC_DATE_GetDay(RTC)); 
  int year=  __LL_RTC_CONVERT_BCD2BIN(LL_RTC_DATE_GetYear(RTC));
  int change=0;
  char timeStr[20] = {0};
  char dateStr[20] = {0};
  static uint32_t prevTickCount;
  prevTickCount=tickCount;
  while(1) {
    uint8_t key;
    if (!KeyScan(&key)) continue;
    if ((key & KEY_PRESSED_SELECT) > 0)  {   // upper button
      switch (change) {
        case 0: if (hour) hour--; else hour=23; break;
        case 1: if (minute) minute--; else minute=59; break;
        case 2: if (year>1) year--; else year=99; break;
        case 3: if (month>1) month--; else month=12; break;
        case 4: if (day>1) day--; else day=31; break;
      }
    }
    if ((key & KEY_PRESSED_BACK) > 0  && ((tickCount - prevTickCount) > 250)) { // middle button
      change++;
      if (change>4) {
        OLED_Clear();
        Configure_RTC_Calendar(year,month,day,hour,minute);
        menuActive=0;
        return;
      }
    }

    if ((key & KEY_PRESSED_CONFIRM ) > 0) { // lower button
      switch (change) {
        case 0: if (hour>22) hour=0; else hour++;  break;
        case 1: if (minute>58) minute=0; else minute++; break;
        case 2: if (year>98) year=1; else year++; break;
        case 3: if (month>11) month=1; else month++; break;
        case 4: if (day>30) day=1; else day++; break;
      }
    }  
    OLED_Clear();

      switch (change) {
        case 0: sprintf((char*)timeStr,"#%.2d#:%.2d",hour,minute); break;
        case 1: sprintf((char*)timeStr,"%.2d:#%.2d#",hour,minute); break;
        default: 
          sprintf((char*)timeStr,"%.2d:%.2d",hour,minute); 
      }
      switch (change) {
        case 2: sprintf((char*)dateStr,"#%.4d#-%.2d-%.2d",2000+year,month,day); break;
        case 3: sprintf((char*)dateStr,"%.4d-#%.2d#-%.2d",2000+year,month,day); break;
        case 4: sprintf((char*)dateStr,"%.4d-%.2d-#%.2d#",2000+year,month,day); break;
        default: 
          sprintf((char*)dateStr,"%.4d-%.2d-%.2d",2000+year,month,day);
      }
      OLED_Info(timeStr,dateStr,"","");
      prevTickCount = tickCount;

  }
 

}