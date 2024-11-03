#include <SPI.h>
#include <Wire.h>
#include <RTClib.h>
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>
// Define pins
int ALARM_PIN = 13;
int MENU_PIN = 8;  // Button for menu navigation and selection
int SELECT_PIN = 6;  // Button for decreasing values
int IRRIGATION_PIN = 12;
int SWITCH_PIN=5;

// EEPROM addresses for storing settings
const int ADDR_SPRAY_MINUTES = 0;
const int ADDR_SPRAY_DURATION = 4;
const int ADDR_START_HOUR = 8;
const int ADDR_START_MINUTE = 12;
const int ADDR_END_HOUR = 16;
const int ADDR_END_MINUTE = 20;
// Define LCD and RTC objects
LiquidCrystal_I2C lcd(0x27, 16, 2);
RTC_DS3231 rtc;

// Variables for irrigation settings
int sprayMinutes = 360;     // Spray interval (every 6 hours)
int sprayDuration = 30; // Spray duration (30 minutes)
int startHour = 6;      // Start time for irrigation (6 AM)
int startMinute = 0;    // Start time minute
int endHour = 18;       // End time for irrigation (6 PM)
int endMinute = 0;      // End time minute
unsigned long nextSprayTime = 0;  // Store next spray time in minutes since midnight
DateTime currentTime;

enum MenuState { MAIN, SET_TIME, SET_INTERVAL, SET_DURATION, SET_START_TIME, SET_END_TIME, EXIT_MENU };
MenuState currentMenu = MAIN;

enum  HourOrMinute { HOUR, MINUTE };
HourOrMinute currentSetting = HOUR;

enum IncreaseOrDecrease { INCREASE, DECREASE };
IncreaseOrDecrease currentAction = INCREASE;

enum TimeSetting { START, END };
TimeSetting currentTimeSetting = START;
unsigned long buttonOnePressTime = 0;
bool buttonOneLongPressDetected = false;
const unsigned long longPressDuration = 2000; // 2 seconds for long press detection

int selectedMenuIndex = 0;
int selectedStartTimeIndex = 0;
int selectedEndTimeIndex = 0;

const int maxMenuItems = 5;  // Updated number of menu items
const int maxTimeItems = 2;  // Updated number of time items

// Prototypes
void displayTimeAndSettings();
void checkIrrigation();
void triggerIrrigation();
void enterMenu();
void handleMenu();
bool detectLongPress(int buttonPin);
void setSprayInterval();
void setSprayDuration();
void setTime();
void setStartTime();
void setEndTime();
void setHourOrMinute(HourOrMinute setting, TimeSetting time, IncreaseOrDecrease action);
void saveSettingsToEEPROM() {
  EEPROM.put(ADDR_SPRAY_MINUTES, sprayMinutes);
  EEPROM.put(ADDR_SPRAY_DURATION, sprayDuration);
  EEPROM.put(ADDR_START_HOUR, startHour);
  EEPROM.put(ADDR_START_MINUTE, startMinute);
  EEPROM.put(ADDR_END_HOUR, endHour);
  EEPROM.put(ADDR_END_MINUTE, endMinute);
}

void loadSettingsFromEEPROM() {
  EEPROM.get(ADDR_SPRAY_MINUTES, sprayMinutes);
  EEPROM.get(ADDR_SPRAY_DURATION, sprayDuration);
  EEPROM.get(ADDR_START_HOUR, startHour);
  EEPROM.get(ADDR_START_MINUTE, startMinute);
  EEPROM.get(ADDR_END_HOUR, endHour);
  EEPROM.get(ADDR_END_MINUTE, endMinute);
  
  // Validate loaded values
  if (sprayMinutes < 30 || sprayMinutes > 1440) sprayMinutes = 360;
  if (sprayDuration < 1 || sprayDuration > 120) sprayDuration = 30;
  if (startHour < 0 || startHour > 23) startHour = 6;
  if (startMinute < 0 || startMinute > 59) startMinute = 0;
  if (endHour < 0 || endHour > 23) endHour = 18;
  if (endMinute < 0 || endMinute > 59) endMinute = 0;
}

// Calculate next spray time based on current time
void calculateNextSprayTime() {
  currentTime = rtc.now();
  int currentTimeInMinutes = currentTime.hour() * 60 + currentTime.minute();
  int startTimeInMinutes = startHour * 60 + startMinute;
  
  // Calculate how many intervals have passed since start time
  int minutesSinceStart;
  if (currentTimeInMinutes >= startTimeInMinutes) {
    minutesSinceStart = currentTimeInMinutes - startTimeInMinutes;
  } else {
    minutesSinceStart = (24 * 60 + currentTimeInMinutes) - startTimeInMinutes;
  }
  
  // Calculate intervals passed and next interval
  int intervalsPassed = minutesSinceStart / sprayMinutes;
  int nextInterval = (intervalsPassed + 1) * sprayMinutes;
  
  // Calculate next spray time
  nextSprayTime = startTimeInMinutes + nextInterval;
  if (nextSprayTime >= 24 * 60) {
    nextSprayTime -= 24 * 60;
  }
}
void setup() {
  Serial.begin(9600);

  // Initialize LCD and RTC
  lcd.init();
  lcd.backlight();
  
  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
    while (1);
  }
 loadSettingsFromEEPROM();
  if (rtc.lostPower()) {
    Serial.println("RTC lost power, setting the time!");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__))); // Set RTC to compile time if power was lost
  }

  //  RTCTime startTime(02, Month::NOVEMBER, 2024, 14, 27, 00, DayOfWeek::SUNDAY, SaveLight::SAVING_TIME_ACTIVE);

  // RTC.setTime(startTime);
  //  rtc.adjust(DateTime(2024, 11, 03, 14, 34, 20));

  // Initialize pins
  pinMode(ALARM_PIN, OUTPUT);
  pinMode(IRRIGATION_PIN, OUTPUT);
  pinMode(MENU_PIN, INPUT_PULLUP);
  pinMode(SELECT_PIN, INPUT_PULLUP);
  pinMode(SWITCH_PIN, INPUT_PULLUP);

  // Display default info on LCD
  displayTimeAndSettings();

    // Calculate initial next spray time
  calculateNextSprayTime();
}

void loop() {
  // Detect long press to enter menu mode
  if (detectLongPress(MENU_PIN)) {
    enterMenu();
  }

  // Display current time and irrigation settings when not in the menu
  if (currentMenu == MAIN) {
    displayTimeAndSettings();
    checkIrrigation();
  }

  delay(100);
}

// void displayTimeAndSettings() {
//   currentTime = rtc.now();
//   lcd.clear();

void displayTimeAndSettings() {
  currentTime = rtc.now();
  lcd.clear();

  // Display current time at the top
  lcd.setCursor(0, 0);
  lcd.print("T:");
  if (currentTime.hour() < 10) lcd.print("0");
  lcd.print(currentTime.hour());
  lcd.print(":");
  if (currentTime.minute() < 10) lcd.print("0");
  lcd.print(currentTime.minute());
  lcd.print(" ST:");
     lcd.print(startHour);
    lcd.print(":");
    if (startMinute < 10) lcd.print("0");
    lcd.print(startMinute);


  // Display schedule info at the bottom
  lcd.setCursor(0, 1);
  
    // Show hours and minutes for the spray interval
    lcd.print(sprayMinutes / 60);
    lcd.print("h");
    if (sprayMinutes % 60 > 0) {
      lcd.print(sprayMinutes % 60);
      lcd.print("m");
    }
    lcd.print("-");
    lcd.print(sprayDuration);
    lcd.print("m");
    lcd.print(" ET:");
    lcd.print(endHour);
    lcd.print(":");
    if (endMinute < 10) lcd.print("0");
    lcd.print(endMinute);
 
 
}

/**
 * The function `checkIrrigation` determines whether it is within a specified time window and at a
 * spray interval to trigger irrigation accordingly.
 */
void checkIrrigation() {
  currentTime = rtc.now();
  int currentHour = currentTime.hour();
  int currentMinute = currentTime.minute();
  
  // Convert current time to minutes for easier comparison
  int currentTimeInMinutes = currentHour * 60 + currentMinute;
  int startTimeInMinutes = startHour * 60 + startMinute;
  int endTimeInMinutes = endHour * 60 + endMinute;
  
  // Check if we're within the allowed time window
  bool isWithinTimeWindow;
  
  // Handle case where end time is on the next day
  if (endTimeInMinutes <= startTimeInMinutes) {
    isWithinTimeWindow = (currentTimeInMinutes >= startTimeInMinutes) || 
                        (currentTimeInMinutes <= endTimeInMinutes);
  } else {
    isWithinTimeWindow = (currentTimeInMinutes >= startTimeInMinutes) && 
                        (currentTimeInMinutes <= endTimeInMinutes);
  }
  
  // Check if we're at a spray interval
  bool isSprayTime = false;
  
  // Calculate the number of minutes since the start time
  int minutesSinceStart;
  if (currentTimeInMinutes >= startTimeInMinutes) {
    minutesSinceStart = currentTimeInMinutes - startTimeInMinutes;
  } else {
    minutesSinceStart = 24 * 60 + currentTimeInMinutes - startTimeInMinutes;
  }
  
  // Check if we're at a spray interval
  if (minutesSinceStart % sprayMinutes == 0) {
    isSprayTime = true;
  }
  
  // Only trigger irrigation if we're within the time window and it's spray time
  if (isWithinTimeWindow && isSprayTime) {
    // Check if the switch is enabled (active low)
    // if (digitalRead(SWITCH_PIN) == LOW) {
      triggerIrrigation();
    // }
  } else {
    // Ensure irrigation is off outside the schedule
    digitalWrite(IRRIGATION_PIN, LOW);
  }
}



/**
 * The function `triggerIrrigation` controls the irrigation system based on specified parameters and
 * displays the status on an LCD screen.
 */
void triggerIrrigation() {
  // Only proceed if we're still within the time window when starting
  currentTime = rtc.now();
  int currentTimeInMinutes = currentTime.hour() * 60 + currentTime.minute();
  int endTimeInMinutes = endHour * 60 + endMinute;
  
  // Calculate maximum duration to avoid running past end time
  int maxDuration;
  if (endTimeInMinutes <= currentTimeInMinutes) {
    maxDuration = (24 * 60 - currentTimeInMinutes + endTimeInMinutes);
  } else {
    maxDuration = endTimeInMinutes - currentTimeInMinutes;
  }
  
  // Use the shorter of sprayDuration or remaining time until end
  int actualDuration = min(sprayDuration, maxDuration);
  
  digitalWrite(ALARM_PIN, HIGH);
  digitalWrite(IRRIGATION_PIN, HIGH);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Irrigation ON");
  lcd.setCursor(0, 1);
  lcd.print("For: ");
  lcd.print(actualDuration);
  lcd.print("min");
  
  Serial.println("Irrigation ON");
  delay(1000);  // Delay to ensure the relay is triggered
  digitalWrite(ALARM_PIN, LOW);
  
  // Run for the calculated duration
  delay(actualDuration * 60UL * 1000UL);  // Convert minutes to milliseconds
  
  digitalWrite(IRRIGATION_PIN, LOW);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Irrigation OFF");
  Serial.println("Irrigation OFF");
}


void enterMenu() {
  currentMenu = MAIN;
  selectedMenuIndex = 0;
  handleMenu();
}

void handleMenu() {
  while (true) {
    // Show menu options
    lcd.clear();
    lcd.setCursor(0, 0);

    switch (selectedMenuIndex) {
      case 0: lcd.print("> Set Interval"); break;
      case 1: lcd.print("> Set Duration"); break;
      case 2: lcd.print("> Set Start Time"); break;  // Added start time menu option
      case 3: lcd.print("> Set End Time"); break;    // Added end time menu option
      case 4: lcd.print("> Exit"); break;
    }

    if (detectLongPress(SELECT_PIN)) {
      // Long press selects the current menu item
      switch (selectedMenuIndex) {
    
        case 0: setSprayInterval(); break;
        case 1: setSprayDuration(); break;
        case 2: setStartTime(); break;  // Added start time logic
        case 3: setEndTime(); break;    // Added end time logic
        case 4: currentMenu = MAIN; return;
      }
    }

    // Short press navigates between menu items
    if (digitalRead(MENU_PIN) == LOW) {
      selectedMenuIndex = (selectedMenuIndex + 1) % maxMenuItems;
      delay(200);  // Debounce delay
    }
  }
}

/**
 * The function `setTime()` is used to enter time set mode and implement logic for setting the RTC
 * time.
 */
void setTime() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Time set mode");
  
  
  // Implement logic for setting the RTC time
}


/**
 * The function `setSprayInterval` allows the user to set the spray interval in hours and minutes using
 * buttons and displays the current interval on an LCD screen.
 * 
 * @return The function `setSprayInterval()` will return to the menu when a long press is detected on
 * the MENU_PIN.
 */
void setSprayInterval() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Set Interval:");
  
  while (true) {
    lcd.setCursor(0, 1);
    // Display hours and minutes
    lcd.print(sprayMinutes / 60);
    lcd.print("h ");
    lcd.print(sprayMinutes % 60);
    lcd.print("m    ");

    if (digitalRead(SWITCH_PIN) == LOW) {
      // Increment by 30 minutes
      sprayMinutes += 30;
      if (sprayMinutes > 1440) { // Max 24 hours (1440 minutes)
        sprayMinutes = 30;
      }
      delay(200);  // Debounce
    }
    
    if (digitalRead(SELECT_PIN) == LOW) {
      // Decrement by 30 minutes
      sprayMinutes -= 30;
      if (sprayMinutes < 30) { // Minimum 30 minutes
        sprayMinutes = 1440;
      }
      delay(200);
    }

    if (detectLongPress(MENU_PIN)) {
        saveSettingsToEEPROM();
  calculateNextSprayTime();

      return;  // Exit back to menu on long press
    }
  }
}


/**
 * The function `setSprayDuration` allows the user to set the spray duration in minutes using buttons
 * and displays the current duration on an LCD screen.
 * 
 * @return The function `setSprayDuration()` returns to the menu when a long press is detected on the
 * MENU_PIN.
 */
void setSprayDuration() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Set Duration:");

  while (true) {
    lcd.setCursor(0, 1);
    lcd.print(sprayDuration);
    lcd.print(" minutes");

    if (digitalRead(SWITCH_PIN) == LOW) {
      sprayDuration++;
      delay(200);  // Debounce
    }
    if (digitalRead(SELECT_PIN) == LOW) {
      sprayDuration = max(1, sprayDuration - 1);  // Minimum 1 minute
      delay(200);
    }

    if (detectLongPress(MENU_PIN)) {
        saveSettingsToEEPROM();
  // calculateNextSprayTime();
      return;  // Exit back to menu on long press
    }
  }

}

/**
 * The function `setStartTime` in C++ displays and allows the user to set the start time, with options
 * to adjust the hour and minute using buttons and save the settings.
 * 
 * @return In the provided code snippet, the `return;` statement is being used to exit the
 * `setStartTime()` function and return back to the main menu when a long press is detected on the
 * `MENU_PIN`. This action is triggered by the `detectLongPress(MENU_PIN)` function.
 */
void setStartTime() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Set Start Time:");

  while (true) {
    
    lcd.setCursor(0, 1);
    lcd.print(startHour);
    lcd.print(":");
    if(startMinute<10){
      lcd.print("0");
    }
    lcd.print(startMinute);

switch (selectedStartTimeIndex)
{
case 0:{
  
  lcd.setCursor(0, 1);
  lcd.print(startHour);
  lcd.print(":");
  // if(startMinute<10){
  //   lcd.print("0");
  // }
  lcd.print(startMinute);
  if (digitalRead(SWITCH_PIN) == LOW) {
    
    setHourOrMinute(HOUR, START, INCREASE);
    delay(200);  // Debounce
  }
  if (digitalRead(SELECT_PIN) == LOW) {
    setHourOrMinute(HOUR, START, DECREASE);
    delay(200);
  }
  /* code */
  break;
}
case 1:{

  lcd.setCursor(0, 1);
  lcd.print(startHour);
  lcd.print(":");
  if(startMinute<10){
    lcd.print("0");
  }
  lcd.print(startMinute);
  if (digitalRead(SWITCH_PIN) == LOW) {
    setHourOrMinute(MINUTE, START, INCREASE);
    
    delay(200);  // Debounce
  }
  if (digitalRead(SELECT_PIN) == LOW) {
    setHourOrMinute(MINUTE, START, DECREASE);
    delay(200);
  }
  /* code */
  break;

}


}
    // if (digitalRead(SWITCH_PIN) == LOW) {
    //   startHour++;
    //   delay(200);  // Debounce
    // }
    // if (digitalRead(SELECT_PIN) == LOW) {
    //   startHour = max(0, startHour - 1);  // Don't allow less than 0 hour
    //   delay(200);
    // }
 
    if(digitalRead(MENU_PIN)==LOW){
      selectedStartTimeIndex=(selectedStartTimeIndex+1)%maxTimeItems;
    }

    if (detectLongPress(MENU_PIN)) {
        saveSettingsToEEPROM();
  calculateNextSprayTime();
      return;  // Exit back to menu on long press
    }
  }
}

/**
 * The function `setEndTime` in C++ displays and allows the user to set the end time, with options to
 * adjust the hour and minute values using buttons, and save settings to EEPROM on long press.
 * 
 * @return In the provided code snippet, the `return;` statement is being used to exit the
 * `setEndTime()` function and return back to the main menu when a long press is detected on the
 * `MENU_PIN`.
 */
void setEndTime() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Set End Time:");

  while (true) {
    lcd.setCursor(0, 1);
    lcd.print(endHour);
    lcd.print(":");
    // if(endMinute<10){
    //   lcd.print("0");
    // }
    lcd.print(endMinute);

    switch (selectedEndTimeIndex)
    {
    case /* constant-expression */0:{
        
        lcd.setCursor(0, 1);
        lcd.print(endHour);
        lcd.print(":");
        if(endMinute<10){
          lcd.print("0");
        }
        lcd.print(endMinute);
        if (digitalRead(SWITCH_PIN) == LOW) {

      setHourOrMinute(HOUR,END, INCREASE);
          delay(200);  // Debounce
        }
        if (digitalRead(SELECT_PIN) == LOW) {
          setHourOrMinute(HOUR, END, DECREASE);
          delay(200);
        }
        /* code */
        break;
    }
      /* code */
      case 1:{
        
        lcd.setCursor(0, 1);
        lcd.print(endHour);
        lcd.print(":");
        lcd.print(endMinute);
        if (digitalRead(SWITCH_PIN) == LOW) {
          setHourOrMinute(MINUTE, END, INCREASE);
          delay(200);  // Debounce
        }
        if (digitalRead(SELECT_PIN) == LOW) {
          setHourOrMinute(MINUTE, END, DECREASE);
          delay(200);
        }
        /* code */
        break;
      }
   
    }

    // if (digitalRead(MENU_PIN) == LOW) {
    //   endHour++;
    //   delay(200);  // Debounce
    // }
    // if (digitalRead(SELECT_PIN) == LOW) {
    //   endHour = max(0, endHour - 1);  // Don't allow less than 0 hour
    //   delay(200);
    // }

if (digitalRead(MENU_PIN)==LOW){    
  selectedEndTimeIndex=(selectedEndTimeIndex+1)%maxTimeItems;
}
    // if (digitalRead(SWITCH_PIN) == LOW) {
    //   endMinute++;
    //   delay(200);  // Debounce
    // }
    // if (digitalRead(SELECT_PIN) == LOW) {
    //   endMinute = max(0, endMinute - 1);  // Don't allow less than 0 minute
    //   delay(200);
    //

    if (detectLongPress(MENU_PIN)) {
        saveSettingsToEEPROM();
  // calculateNextSprayTime();
      return;  // Exit back to menu on long press
    }
  }
}

// Detect long press on the specified button pin
bool detectLongPress(int buttonPin) {
  if (digitalRead(buttonPin) == LOW) {
    if (buttonOnePressTime == 0) {
      buttonOnePressTime = millis();
    }
    if ((millis() - buttonOnePressTime) > longPressDuration) {
      buttonOnePressTime = 0;
      return true;
    }
  } else {
    buttonOnePressTime = 0;
  }
  return false;
}


void setHourOrMinute(HourOrMinute setting, TimeSetting timeSetting, IncreaseOrDecrease action)










 {
  if (setting == HOUR) {
    if (action == INCREASE) {
      if (timeSetting == START) {
        startHour++;
        if (startHour > 23) {
          startHour = 0;
        }
      } else {
        endHour++;
        if (endHour > 23) {
          endHour = 0;
        }
      }

      
      
  } else {
   if(timeSetting==START){
    startHour--;
    if(startHour<0){
   
      startHour=23;}
      // value = value % 60;  // 60 minutes in an hour
    } else {
      endHour--;
      if(endHour<0){
        endHour=23;
      }
     
    }
  }
}else{
  if (action == INCREASE) {
    if (timeSetting == START) {
      startMinute++;
      if (startMinute > 59) {
        startMinute = 0;
        setHourOrMinute(HOUR, START, INCREASE);
      }
    } else {
      endMinute++;
      if (endMinute > 59) {
        endMinute = 0;
        setHourOrMinute(HOUR, END, INCREASE);
      }
    }
  } else {
    if (timeSetting == START) {
      startMinute--;
      if (startMinute < 0) {
        startMinute = 59;
        setHourOrMinute(HOUR, START, DECREASE);
      }
    } else {
      endMinute--;
      if (endMinute < 0) {
        endMinute = 59;
        setHourOrMinute(HOUR, END, DECREASE);
      }
    }
  }
}
}











// void setSprayInterval() {
//   lcd.clear();
//   lcd.setCursor(0, 0);
//   lcd.print("Set Interval:");

//   while (true) {
//     lcd.setCursor(0, 1);
//     lcd.print(sprayMinutes);
//     lcd.print("minutes");

//     if (digitalRead(SWITCH_PIN) == LOW) {
//       sprayMinutes++;
//       delay(200);  // Debounce
//     }
//     if (digitalRead(SELECT_PIN) == LOW) {
//       sprayMinutes = max(1, sprayMinutes - 1);  // Don't allow less than 1 hour
//       delay(200);
//     }

//     if (detectLongPress(MENU_PIN)) {
//       return;  // Exit back to menu on long press
//     }
//   }
// }




//   // Display current time at the top
//   lcd.setCursor(0, 0);
//   lcd.print("Time: ");
//   lcd.print(currentTime.hour());
//   lcd.print(":");
//   lcd.print(currentTime.minute());

//   // Display spray interval and duration at the bottom
//   lcd.setCursor(0, 1);
//   lcd.print("Spray: ");
//   lcd.print(sprayMinutes);
//   lcd.print("h/");
//   lcd.print(sprayDuration);
//   lcd.print("min");
// }

// void checkIrrigation() {
//   // Check if the current time matches the scheduled irrigation time
//   if (currentTime.hour() % sprayMinutes == 0 && currentTime.minute() == 0) {
//     triggerIrrigation();
//   }
// }


// void triggerIrrigation() {
//   digitalWrite(ALARM_PIN, HIGH);
//   digitalWrite(IRRIGATION_PIN, HIGH);
//   lcd.clear();
//   lcd.setCursor(0, 0);
//   lcd.print("Irrigation ON");
//   Serial.println("Irrigation ON");
//   delay(1000);  // Delay to ensure the relay is triggered
//   digitalWrite(ALARM_PIN, LOW);
//   delay(sprayDuration * 60 * 1000);  // sprayDuration in minutes

//   digitalWrite(IRRIGATION_PIN, LOW);
//   lcd.clear();
//   lcd.setCursor(0, 0);
//   lcd.print("Irrigation OFF");
//   Serial.println("Irrigation OFF");
// }
