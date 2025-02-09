#include <FastLED.h>
#define NUM_LEDS 60
#define DATA_PIN 2
#define COLOR_ORDER GRB
#define CHIPSET WS2812B
#define BRIGHTNESS 210
#define VOLTS 5
#define MAX_AMPS 500
// define parameters for the project
#define MATRIX_WIDTH 53                 // width of LED matirx
#define MATRIX_HEIGHT 1                 // height of LED matrix + additional row for minute leds
#define EE_ADDRESS_TIME 10              // eeprom address for time value (persist values during power off)
#define EE_ADDRESS_COLOR 20             // eeprom address for color value (persist values during power off)
#define UPPER_LIGHT_THRSH 930           // upper threshold for lightsensor (above this value brightness is always 20%)
#define LOWER_LIGHT_THRSH 800           // lower threshold for lightsensor (below this value brightness is always 100%)
#define CENTER_ROW (MATRIX_WIDTH / 2)   // id of center row
#define CENTER_COL (MATRIX_HEIGHT / 2)  // id of center column
#include "RTClib.h"                     //https://github.com/adafruit/RTClib
#include <TimeLib.h>
#include <EEPROM.h>

RTC_DS3231 rtc;
// Params for width and height
const uint8_t kMatrixWidth = 8;
const uint8_t kMatrixHeight = 8;

// Param for different pixel layouts
const bool kMatrixSerpentineLayout = true;
const bool kMatrixVertical = false;
// Set 'kMatrixSerpentineLayout' to false if your pixels are
// laid out all running the same way, like this:
//
//     0 >  1 >  2 >  3 >  4
//                         |
//     .----<----<----<----'
//     |
//     5 >  6 >  7 >  8 >  9
//                         |
//     .----<----<----<----'
//     |
//    10 > 11 > 12 > 13 > 14
//                         |
//     .----<----<----<----'
//     |
//    15 > 16 > 17 > 18 > 19
//
// Set 'kMatrixSerpentineLayout' to true if your pixels are
// laid out back-and-forth, like this:
//
//     0 >  1 >  2 >  3 >  4
//                         |
//                         |
//     9 <  8 <  7 <  6 <  5
//     |
//     |
//    10 > 11 > 12 > 13 > 14
//                        |
//                        |
//    19 < 18 < 17 < 16 < 15

uint16_t XY(uint8_t x, uint8_t y) {
  uint16_t i;

  if (kMatrixSerpentineLayout == false) {
    if (kMatrixVertical == false) {
      i = (y * kMatrixWidth) + x;
    } else {
      i = kMatrixHeight * (kMatrixWidth - (x + 1)) + y;
    }
  }

  if (kMatrixSerpentineLayout == true) {
    if (kMatrixVertical == false) {
      if (y & 0x01) {
        // Odd rows run backwards
        uint8_t reverseX = (kMatrixWidth - 1) - x;
        i = (y * kMatrixWidth) + reverseX;
      } else {
        // Even rows run forwards
        i = (y * kMatrixWidth) + x;
      }
    } else {  // vertical positioning
      if (x & 0x01) {
        i = kMatrixHeight * (kMatrixWidth - (x + 1)) + y;
      } else {
        i = kMatrixHeight * (kMatrixWidth - x) - (y + 1);
      }
    }
  }

  return i;
}

// constants
#define NUM_LEDS (kMatrixWidth * kMatrixHeight)
CRGB leds_plus_safety_pixel[NUM_LEDS + 1];
CRGB* const leds(leds_plus_safety_pixel + 1);
// you can use this if this works for you but I needed different colors for each word. 
// const CRGB colors[] = {
//   CRGB::White,
//   CRGB::Yellow,
//   CRGB::Magenta,
//   CRGB::Gray,
//   CRGB::Red,
//   CRGB::Blue
// };

uint8_t readGrid(uint8_t x, uint8_t y) {
  uint16_t index = XY(x, y);
  if (index >= 0 && index < NUM_LEDS) {
    return (leds[index] != CRGB::Black);  // Returns 1 if the pixel is not black, 0 otherwise
  }
  return 0;  // Default to off for out-of-bounds
}
void drawOnMatrix() {
  for (int z = 0; z < MATRIX_HEIGHT; z++) {
    for (int s = 0; s < MATRIX_WIDTH; s++) {
      if (readGrid(s, z) != 0) {  // Check if the grid cell is active
        Serial.print("1 ");
        // Check if the current LED is part of "diba'iganed"
        if ((s >= 39) && (s <= 42)) {
          leds[XY(s, z)] = CRGB(66, 66, 245) ;  // Set "diba'iganed" to blue
        } else if ((s >= 28) && (s <= 31)) {
           leds[XY(s, z)] = CRGB(245, 173, 66);  // Set "ashi-aabita" to yellow

        } else {
          leds[XY(s, z)] = CRGB::Magenta;  // set the actual clock word to be magenta
        }

      } else {
        Serial.print("0 ");
        leds[XY(s, z)] = CRGB::Black;  // Turn off the LED for inactive cells
      }
    }
    Serial.println();
  }
  FastLED.show();  // Update the LEDs after drawing
}



void drawPixel(int x, int y, CRGB color) {
  int index = XY(x, y);
  if (index >= 0 && index < NUM_LEDS) {
    leds[index] = color;
  }
}

void clearMatrix() {
  FastLED.clear();
}

void showMatrix() {
  FastLED.show();
}

void drawCircleOnMatrix(int offset) {
  for (int r = 0; r < MATRIX_HEIGHT; r++) {
    for (int c = 0; c < MATRIX_WIDTH; c++) {
      int angle = ((int)((atan2(r - CENTER_ROW, c - CENTER_COL) * (180 / M_PI)) + 180) % 360);
      int hue = (int)(angle * 255.0 / 360 + offset) % 256;
      drawPixel(c, r, CHSV(hue, 255, 255));  // Use HSV for a smooth color wheel
    }
  }
}

uint16_t XYsafe(uint8_t x, uint8_t y) {
  if (x >= kMatrixWidth) return -1;
  if (y >= kMatrixHeight) return -1;
  return XY(x, y);
}


void loop() {
  uint32_t ms = millis();
  FastLED.clear();


  // Convert time to grid representation
  DateTime rtctime = rtc.now();
  Serial.println("converted time");
  uint8_t global_hour = rtctime.hour();
  timeToArray(rtctime.hour(), rtctime.minute(), global_hour);
  // Draw time
  drawOnMatrix();

  // Show updates on LEDs
  FastLED.show();

  delay(500);  // Adjust for your refresh rate
  FastLED.show();
}

void DrawOneFrame(byte startHue8, int8_t yHueDelta8, int8_t xHueDelta8) {
  byte lineStartHue = startHue8;
  for (byte y = 0; y < kMatrixHeight; y++) {
    lineStartHue += yHueDelta8;
    byte pixelHue = lineStartHue;
    for (byte x = 0; x < kMatrixWidth; x++) {
      pixelHue += xHueDelta8;
      leds[XY(x, y)] = CHSV(pixelHue, 255, 255);
    }
  }
}

void setup() {
  // enable serial output
  Serial.begin(9600);
  Serial.println(F(__TIME__));
  FastLED.addLeds<CHIPSET, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setMaxPowerInVoltsAndMilliamps(VOLTS, MAX_AMPS);
  FastLED.setBrightness(BRIGHTNESS);

  FastLED.clear();
  FastLED.show();
  Serial.println("added Fast LEDs");
  rtc.begin();
  // rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  // Initialize the RTC module
  if (!rtc.begin()) {
    Serial.println("RTC not found. Check connections!");
    while (1)
      ;  // Stop here if RTC is not connected
  }

  // Check if the RTC has lost power
  if (rtc.lostPower()) {
    Serial.println("RTC lost power. Setting default time.");
    // Set RTC to compile-time value (you can adjust this as needed)
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
}


// Converts the given time in grid representation
//

void timeToArray(uint8_t hours, uint8_t minutes, uint8_t global_hour) {
  // Clear grid
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  Serial.print("Converting time to array: ");
  Serial.print(hours);
  Serial.print(":");
  Serial.println(minutes);
  Serial.print("diba'iganed");
  leds[XY(39, 0)] = CRGB::Green; 
  leds[XY(40, 0)] = CRGB::Green;
  leds[XY(41, 0)] = CRGB::Green;
  leds[XY(42, 0)] = CRGB::Green;

  // Indicator for "half past" (minutes == 30)
  if (minutes >= 30) {
    Serial.println("Half past the hour!");
    Serial.print("ashi-aabita");
    leds[XY(28, 0)] = CRGB::Green;  // Example LED position for "half past"
    leds[XY(29, 0)] = CRGB::Green;
    leds[XY(30, 0)] = CRGB::Green;
    leds[XY(31, 0)] = CRGB::Green;
  }

// night mode after 10 pm, make it very dim
  if (global_hour >= 22) {
    FastLED.setBrightness(20);
  }

  // Convert hours to 12h format
  if (hours >= 12) {
    hours -= 12;
  }
  if (hours == 12) {
    hours = 0;
  }

  Serial.print(hours);

  // Show hours
  switch (hours) {
    case 0:  // Twelve
      Serial.print("ashi-niizho");
      leds[XY(33, 0)] = CRGB::Blue; // niizho
      leds[XY(34, 0)] = CRGB::Blue; // niizho
      leds[XY(38, 0)] = CRGB::Blue; // ashi
      break;
    case 1:  // One
      Serial.print("bezhigo");
      leds[XY(0, 0)] = CRGB::Blue;
      leds[XY(1, 0)] = CRGB::Blue;
      leds[XY(2, 0)] = CRGB::Blue;
      break;
    case 2:  // Two
      Serial.print("niizho");
      leds[XY(3, 0)] = CRGB::Blue;
      leds[XY(4, 0)] = CRGB::Blue;
      break;
    case 3:  // Three
      Serial.print("niso");
      leds[XY(5, 0)] = CRGB::Blue;
      leds[XY(6, 0)] = CRGB::Blue;
      break;
    case 4:  // Four
      Serial.print("niiyo");
      leds[XY(7, 0)] = CRGB::Blue;
      leds[XY(8, 0)] = CRGB::Blue;
      break;
    case 5:  // Five
      Serial.print("naano");
      leds[XY(9, 0)] = CRGB::Blue;
      leds[XY(10, 0)] = CRGB::Blue;
      break;
    case 6:  // Six
      Serial.print("ingodwaaso");
      leds[XY(11, 0)] = CRGB::Blue;
      leds[XY(12, 0)] = CRGB::Blue;
      leds[XY(13, 0)] = CRGB::Blue;
      break;
    case 7:  // Seven
      Serial.print("niizhwaaso");
      leds[XY(14, 0)] = CRGB::Blue;
      leds[XY(15, 0)] = CRGB::Blue;
      leds[XY(16, 0)] = CRGB::Blue;
      leds[XY(17, 0)] = CRGB::Blue;
      break;
    case 8:  // Eight
      Serial.print("nishwaaso");
      leds[XY(18, 0)] = CRGB::Blue;
      leds[XY(19, 0)] = CRGB::Blue;
      leds[XY(20, 0)] = CRGB::Blue;
      break;
    case 9:  // Nine
      Serial.print("zhaangaso");
      leds[XY(21, 0)] = CRGB::Blue;
      leds[XY(22, 0)] = CRGB::Blue;
      leds[XY(23, 0)] = CRGB::Blue;
      leds[XY(24, 0)] = CRGB::Blue;
      break;
    case 10:  // Ten
      Serial.print("midaaso");
      leds[XY(25, 0)] = CRGB::Blue;
      leds[XY(26, 0)] = CRGB::Blue;
      leds[XY(27, 0)] = CRGB::Blue;
      break;
    case 11:  // Eleven
      Serial.print("ashi-bezhiigo");
      leds[XY(35, 0)] = CRGB::Blue; // bezhigo
      leds[XY(36, 0)] = CRGB::Blue; // bezhigo
      leds[XY(37, 0)] = CRGB::Blue; // bezhigo
      leds[XY(38, 0)] = CRGB::Blue; // ashi
      break;
  }
  Serial.println();
}
