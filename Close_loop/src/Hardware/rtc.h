/* ck_apre=LSIFreq/(ASYNC prediv + 1) with LSIFreq=32 kHz RC */
#define RTC_ASYNCH_PREDIV          125-1
/* ck_spre=ck_apre/(SYNC prediv + 1) = 1 Hz */
#define RTC_SYNCH_PREDIV           320-1

#include "stm32f0xx_ll_utils.h"
#include "stm32f0xx_ll_gpio.h"

#define CLOCK_MODE_SECOND 0     // move motor in seconds 1-60
#define CLOCK_MODE_MINUTE 1     // move motor in minutes 1-60
#define CLOCK_MODE_HOUR 2     // move motor in hours 12 + minutes between hours
int clockMode;

void     SystemClock_Config(void);
void     Configure_RTC_Clock(void);
void     Configure_RTC(void);
void Configure_RTC_Calendar(uint32_t y,uint32_t m,uint32_t d,uint32_t hour,uint32_t minute);
uint32_t Enter_RTC_InitMode(void);
uint32_t Exit_RTC_InitMode(void);
uint32_t WaitForSynchro_RTC(void);
void     Show_RTC_Calendar(void);
int moveClock(int val);
void setTimeDateUI(void);
void setClockModeUI(void );
