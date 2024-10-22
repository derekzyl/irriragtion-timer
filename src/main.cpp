
 #include <SPI.h>
#include <Wire.h>
#include <RTClib.h>
#include <LiquidCrystal_I2C.h>

// Define pins
int ALARM_PIN = 13;
int BUTTON_PIN_ONE = 8;  // Button for menu navigation and selection
int BUTTON_PIN_TWO = 6;  // Button for decreasing values
int IRRIGATION_PIN = 12;

// Define LCD and RTC objects
LiquidCrystal_I2C lcd(0x27, 16, 2);
RTC_DS3231 rtc;

// Variables for irrigation settings
int sprayHour = 6;     // Spray interval (every 6 hours)
int sprayDuration = 30; // Spray duration (30 minutes)
DateTime currentTime;

enum MenuState { MAIN, SET_TIME, SET_INTERVAL, SET_DURATION, EXIT_MENU };
MenuState currentMenu = MAIN;

unsigned long buttonOnePressTime = 0;
bool buttonOneLongPressDetected = false;
const unsigned long longPressDuration = 2000; // 2 seconds for long press detection

int selectedMenuIndex = 0;
const int maxMenuItems = 4;

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
  pinMode(BUTTON_PIN_ONE, INPUT_PULLUP);
  pinMode(BUTTON_PIN_TWO, INPUT_PULLUP);

  // Display default info on LCD
displayTimeAndSettings();
}

void loop() {
  // Detect long press to enter menu mode
  if (detectLongPress(BUTTON_PIN_ONE)) {
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
  Serial.println("Irrigation ON");
  delay(1000);  // Delay to ensure the relay is triggered
  digitalWrite(ALARM_PIN, LOW);
  delay(sprayDuration * 60 * 1000);  // sprayDuration in minutes

  digitalWrite(IRRIGATION_PIN, LOW);
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
      case 3: lcd.print("> Exit"); break;
    }

    if (detectLongPress(BUTTON_PIN_ONE)) {
      // Long press selects the current menu item
      switch (selectedMenuIndex) {
        case 0: setTime(); break;
        case 1: setSprayInterval(); break;
        case 2: setSprayDuration(); break;
        case 3: currentMenu = MAIN; return;
      }
    }

    // Short press navigates between menu items
    if (digitalRead(BUTTON_PIN_ONE) == LOW) {
      selectedMenuIndex = (selectedMenuIndex + 1) % maxMenuItems;
      delay(200);  // Debounce delay
    }
  }
}

void setTime() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Set Time:");

  // Use BUTTON_PIN_ONE and BUTTON_PIN_TWO to adjust hours and minutes.
  // Implement logic for setting time based on button presses.
}

void setSprayInterval() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Set Interval:");

  while (true) {
    lcd.setCursor(0, 1);
    lcd.print(sprayHour);
    lcd.print(" hours");

    // Use BUTTON_PIN_ONE to increase, BUTTON_PIN_TWO to decrease
    if (digitalRead(BUTTON_PIN_ONE) == LOW) {
      sprayHour++;
      delay(200);  // Debounce
    }
    if (digitalRead(BUTTON_PIN_TWO) == LOW) {
      sprayHour = max(1, sprayHour - 1);  // Don't allow less than 1 hour
      delay(200);
    }

    if (detectLongPress(BUTTON_PIN_ONE)) {
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

    // Use BUTTON_PIN_ONE to increase, BUTTON_PIN_TWO to decrease
    if (digitalRead(BUTTON_PIN_ONE) == LOW) {
      sprayDuration++;
      delay(200);  // Debounce
    }
    if (digitalRead(BUTTON_PIN_TWO) == LOW) {
      sprayDuration = max(1, sprayDuration - 1);  // Minimum 1 minute
      delay(200);
    }

    if (detectLongPress(BUTTON_PIN_ONE)) {
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
    if ((millis() - buttonOnePressTime) >= longPressDuration) {
      buttonOnePressTime = 0;
      return true;
    }
  } else {
    buttonOnePressTime = 0;
  }
  return false;
}
