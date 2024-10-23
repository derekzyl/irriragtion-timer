#include <SPI.h>
#include <Wire.h>
#include <RTClib.h>
#include <LiquidCrystal_I2C.h>

// Define pins
int ALARM_PIN = 13;
int MENU_PIN = 8;  // Button for menu navigation and selection
int SELECT_PIN = 6;  // Button for decreasing values
int IRRIGATION_PIN = 12;
int SWITCH_PIN=5;

// Define LCD and RTC objects
LiquidCrystal_I2C lcd(0x27, 16, 2);
RTC_DS3231 rtc;

// Variables for irrigation settings
int sprayHour = 6;     // Spray interval (every 6 hours)
int sprayDuration = 30; // Spray duration (30 minutes)
int startHour = 6;      // Start time for irrigation (6 AM)
int startMinute = 0;    // Start time minute
int endHour = 18;       // End time for irrigation (6 PM)
int endMinute = 0;      // End time minute

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

const int maxMenuItems = 6;  // Updated number of menu items
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

void setup() {
  Serial.begin(9600);

  // Initialize LCD and RTC
  lcd.init();
  lcd.backlight();
  
  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
    while (1);
  }

  if (rtc.lostPower()) {
    Serial.println("RTC lost power, setting the time!");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__))); // Set RTC to compile time if power was lost
  }

  // Initialize pins
  pinMode(ALARM_PIN, OUTPUT);
  pinMode(IRRIGATION_PIN, OUTPUT);
  pinMode(MENU_PIN, INPUT_PULLUP);
  pinMode(SELECT_PIN, INPUT_PULLUP);
  pinMode(SWITCH_PIN, INPUT_PULLUP);

  // Display default info on LCD
  displayTimeAndSettings();
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

void displayTimeAndSettings() {
  currentTime = rtc.now();
  lcd.clear();

  // Display current time at the top
  lcd.setCursor(0, 0);
  lcd.print("Time: ");
  lcd.print(currentTime.hour());
  lcd.print(":");
  lcd.print(currentTime.minute());

  // Display spray interval and duration at the bottom
  lcd.setCursor(0, 1);
  lcd.print("Spray: ");
  lcd.print(sprayHour);
  lcd.print("h/");
  lcd.print(sprayDuration);
  lcd.print("min");
}

void checkIrrigation() {
  // Check if the current time matches the scheduled irrigation time
  if (currentTime.hour() % sprayHour == 0 && currentTime.minute() == 0) {
    triggerIrrigation();
  }
}

void triggerIrrigation() {
  digitalWrite(ALARM_PIN, HIGH);
  digitalWrite(IRRIGATION_PIN, HIGH);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Irrigation ON");
  Serial.println("Irrigation ON");
  delay(1000);  // Delay to ensure the relay is triggered
  digitalWrite(ALARM_PIN, LOW);
  delay(sprayDuration * 60 * 1000);  // sprayDuration in minutes

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
      case 0: lcd.print("> Set Time"); break;
      case 1: lcd.print("> Set Interval"); break;
      case 2: lcd.print("> Set Duration"); break;
      case 3: lcd.print("> Set Start Time"); break;  // Added start time menu option
      case 4: lcd.print("> Set End Time"); break;    // Added end time menu option
      case 5: lcd.print("> Exit"); break;
    }

    if (detectLongPress(SELECT_PIN)) {
      // Long press selects the current menu item
      switch (selectedMenuIndex) {
        case 0: setTime(); break;
        case 1: setSprayInterval(); break;
        case 2: setSprayDuration(); break;
        case 3: setStartTime(); break;  // Added start time logic
        case 4: setEndTime(); break;    // Added end time logic
        case 5: currentMenu = MAIN; return;
      }
    }

    // Short press navigates between menu items
    if (digitalRead(MENU_PIN) == LOW) {
      selectedMenuIndex = (selectedMenuIndex + 1) % maxMenuItems;
      delay(200);  // Debounce delay
    }
  }
}

void setTime() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Set Time:");
  
  // Implement logic for setting the RTC time
}

void setSprayInterval() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Set Interval:");

  while (true) {
    lcd.setCursor(0, 1);
    lcd.print(sprayHour);
    lcd.print(" hours");

    if (digitalRead(SWITCH_PIN) == LOW) {
      sprayHour++;
      delay(200);  // Debounce
    }
    if (digitalRead(SELECT_PIN) == LOW) {
      sprayHour = max(1, sprayHour - 1);  // Don't allow less than 1 hour
      delay(200);
    }

    if (detectLongPress(MENU_PIN)) {
      return;  // Exit back to menu on long press
    }
  }
}

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
      return;  // Exit back to menu on long press
    }
  }
}

void setStartTime() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Set Start Time:");

  while (true) {
    
    lcd.setCursor(0, 1);
    lcd.print(startHour);
    lcd.print(":");
    lcd.print(startMinute);

switch (selectedStartTimeIndex)
{
case 0:{
  
  lcd.setCursor(0, 1);
  lcd.print(startHour);
  lcd.print(":");
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
      return;  // Exit back to menu on long press
    }
  }
}

void setEndTime() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Set End Time:");

  while (true) {
    lcd.setCursor(0, 1);
    lcd.print(endHour);
    lcd.print(":");
    lcd.print(endMinute);

    switch (selectedEndTimeIndex)
    {
    case /* constant-expression */0:{
        
        lcd.setCursor(0, 1);
        lcd.print(endHour);
        lcd.print(":");
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