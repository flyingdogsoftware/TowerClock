#include "main.h"
#include "display.h"
#include "oled.h"
#include "menu.h"
#include "rtc.h"

// Declare menu and menu items (should be in global space)
//struct Menu menuMain;
struct menuItem menuItemSetTimeDate;
struct menuItem menuItemSetClockMode;
struct menuItem menuItemCalibrate;
struct menuItem menuItemCurrent;
struct menuItem menuItemStepSize;
struct menuItem menuItemEnablePin;
struct menuItem menuItemDirectionPin;
struct menuItem menuItemClosedLoopMode;
/*struct menuItem menuItemPID_P;
struct menuItem menuItemPID_I;
struct menuItem menuItemPID_D;
struct menuItem menuItemOLEDFreq;*/
struct menuItem menuItemSave;
struct menuItem menuItemExit;


// Convert to mA by multiplying by 6.5
uint16_t Converter_Current(uint16_t valueIn)
{
  uint16_t valueOut = (valueIn * 13) / 2;
  return valueOut;
}


// Convert to microstep ratio
uint16_t Converter_Stepsize(uint16_t valueIn)
{
  uint16_t valueOut = 64 / valueIn;
  return valueOut;
}


// Override function for the default item value changer
void Changer_StepSize(struct Menu *menu, int16_t val)
{
  uint8_t newVal;

  if (val > 0)
  {
    newVal = *(uint8_t*)menu->items[menu->selectedItemIndex]->variable.value >> 1;
    if (newVal >= menu->items[menu->selectedItemIndex]->variable.minValue)
      *(uint8_t*)menu->items[menu->selectedItemIndex]->variable.value = newVal;
  }
  else
  {
    newVal = *(uint8_t*)menu->items[menu->selectedItemIndex]->variable.value << 1;
    if (newVal <= menu->items[menu->selectedItemIndex]->variable.maxValue)
      *(uint8_t*)menu->items[menu->selectedItemIndex]->variable.value = newVal;
  } 
}


// Override function for the default item value changer
void Changer_ClosedLoopMode(struct Menu *menu, int16_t val)
{  
  if (val == 1)
  {
    closemode = 1;
    PID_Cal_value_init();    
  }    
  else
  {
    closemode = 0;
  }  
}


void ShowStartupScreen()
{
    OLED_Clear();                              
    OLED_ShowString(0,0,"  Tower Clock  ");
}


void ShowInfoScreen()
{
  OLED_Info("Tower Clock","Version 0.5","","");
}


void ShowCalibrateScreen()
{
    OLED_Clear();  
     OLED_Info("NOT","Calibrated","Please Calibrate","");

}


void ShowCalibrateOKScreen()
{
    OLED_ShowString(16,25,"Calibration");
    OLED_ShowString(48,45,"OK!");
}


void ShowCalibrateCompleteScreen()
{
    OLED_Clear();
     OLED_Info("   Finished!  ","  Please Press ","Reset to Reboot","");
}


void ShowBootloaderScreen()
{
    OLED_Clear();
    OLED_ShowString(32,25,"Running");
    OLED_ShowString(16,45,"Bootloader");
}

void ExitMenu()
{
  menuActive = 0;
  OLED_Clear();
  ShowInfoScreen();
}

void BuildMenu()
{
  OLED_Clear();

  // Register function that will be used for drawing
  Menu_Register_Draw(OLED_ShowString);

  Menu_Register_Clear(OLED_Clear);

  Menu_Item_Init(&menuItemSetTimeDate);
  menuItemSetTimeDate.title = "Set Time&Date";
  menuItemSetTimeDate.type = MENU_ITEM_TYPE_ACTION;
  menuItemSetTimeDate.action = &setTimeDateUI;

  Menu_Item_Init(&menuItemSetClockMode);
  menuItemSetClockMode.title = "Set Clock Mode";
  menuItemSetClockMode.type = MENU_ITEM_TYPE_ACTION;
  menuItemSetClockMode.action = &setClockModeUI;




  Menu_Item_Init(&menuItemCalibrate);
  menuItemCalibrate.title = "Calibrate";
  menuItemCalibrate.type = MENU_ITEM_TYPE_ACTION;
  menuItemCalibrate.action = &CalibrateEncoder;

  Menu_Item_Init(&menuItemCurrent);
  menuItemCurrent.title = "Current";
  menuItemCurrent.type = MENU_ITEM_TYPE_VARIABLE_UINT8;
  menuItemCurrent.variable.value = &Currents;
  menuItemCurrent.variable.maxValue = 300;                          // Limit max current to just below 2A
  menuItemCurrent.variable.valueConverter = &Converter_Current;

  Menu_Item_Init(&menuItemStepSize);
  menuItemStepSize.title = "Step Size";
  menuItemStepSize.type = MENU_ITEM_TYPE_VARIABLE_UINT8;                            
  menuItemStepSize.variable.value = &stepangle;  
  menuItemStepSize.variable.maxValue = 32;
  menuItemStepSize.variable.minValue = 1; 
  menuItemStepSize.variable.valueConverter = &Converter_Stepsize;
  menuItemStepSize.variable.change = &Changer_StepSize;             // Override the default value change function        

  Menu_Item_Init(&menuItemEnablePin);
  menuItemEnablePin.title = "EN Pin";                   
  menuItemEnablePin.type = MENU_ITEM_TYPE_VARIABLE_UINT8;
  menuItemEnablePin.variable.value = &Motor_ENmode_flag;  
  menuItemEnablePin.variable.maxValue = 1;

  Menu_Item_Init(&menuItemDirectionPin);
  menuItemDirectionPin.title = "DIR Pin";
  menuItemDirectionPin.type = MENU_ITEM_TYPE_VARIABLE_UINT8;
  menuItemDirectionPin.variable.value = &Motor_Dir;
  menuItemDirectionPin.variable.maxValue = 1;

  Menu_Item_Init(&menuItemClosedLoopMode);
  menuItemClosedLoopMode.title = "Closed M";
  menuItemClosedLoopMode.type = MENU_ITEM_TYPE_VARIABLE_UINT8;
  menuItemClosedLoopMode.variable.value = &closemode;
  menuItemClosedLoopMode.variable.maxValue = 1;
  menuItemClosedLoopMode.variable.minValue = 0;
  menuItemClosedLoopMode.variable.change = &Changer_ClosedLoopMode;   // Override the default value change function

 /* Menu_Item_Init(&menuItemPID_P);
  menuItemPID_P.title = "PID P";
  menuItemPID_P.type = MENU_ITEM_TYPE_VARIABLE_UINT16;
  menuItemPID_P.variable.value = &kp; 
  
  Menu_Item_Init(&menuItemPID_I);
  menuItemPID_I.title = "PID I";
  menuItemPID_I.type = MENU_ITEM_TYPE_VARIABLE_UINT16;
  menuItemPID_I.variable.value = &ki;

  Menu_Item_Init(&menuItemPID_D);
  menuItemPID_D.title = "PID D";
  menuItemPID_D.type = MENU_ITEM_TYPE_VARIABLE_UINT16;
  menuItemPID_D.variable.value = &kd;

  Menu_Item_Init(&menuItemOLEDFreq);
  menuItemOLEDFreq.title = "OLED Freq";
  menuItemOLEDFreq.type = MENU_ITEM_TYPE_ACTION_VAR_UINT8;
  menuItemOLEDFreq.variable.value = &oledClock;
  menuItemOLEDFreq.action = &ChangeOLEDClock;*/

  Menu_Item_Init(&menuItemSave);
  menuItemSave.title = "Save";
  menuItemSave.type = MENU_ITEM_TYPE_ACTION;
  menuItemSave.action = &StoreCurrentParameters;

  Menu_Item_Init(&menuItemExit);
  menuItemExit.title = "Exit";
  menuItemExit.type = MENU_ITEM_TYPE_ACTION;
  menuItemExit.action = &ExitMenu;

  Menu_Init(&menuMain);
  Menu_Add_Item(&menuMain, &menuItemSetTimeDate);
  Menu_Add_Item(&menuMain, &menuItemSetClockMode);

  Menu_Add_Item(&menuMain, &menuItemCalibrate);
  Menu_Add_Item(&menuMain, &menuItemCurrent);
  Menu_Add_Item(&menuMain, &menuItemStepSize);
  Menu_Add_Item(&menuMain, &menuItemEnablePin);
  Menu_Add_Item(&menuMain, &menuItemDirectionPin);
  Menu_Add_Item(&menuMain, &menuItemClosedLoopMode);
/*  Menu_Add_Item(&menuMain, &menuItemPID_P);
  Menu_Add_Item(&menuMain, &menuItemPID_I);
  Menu_Add_Item(&menuMain, &menuItemPID_D);
  Menu_Add_Item(&menuMain, &menuItemOLEDFreq);*/
  Menu_Add_Item(&menuMain, &menuItemSave);
  Menu_Add_Item(&menuMain, &menuItemExit);

}


//
void Motor_data_dis(void)
{
  return;
 
    
}























