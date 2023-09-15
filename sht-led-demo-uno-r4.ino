/*
 * Copyright (c) 2023 Johannes Winkelmann, jw@smts.ch
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files 
 * (the “Software”), to deal in the Software without restriction, 
 * including without limitation the rights to use, copy, modify, merge, 
 * publish, distribute, sublicense, and/or sell copies of the Software, 
 * and to permit persons to whom the Software is furnished to do so, subject 
 * to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING 
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
 * DEALINGS IN THE SOFTWARE.
 */

/*
 * Usage:
 *   Connect your SHT40 to the QWIIC socket on the Arduino. If you're not
 *   using the QWIIC connector, adapt the 'SHT_I2C_INTERFACE' variable below
 *
 *   After startup, the lowest row should barely come on. If you blow on the
 *   sensor to warm it up, you should see the signal rise
 *
 *   To reset the baseline value, simply reset the board
 *
 * Description:
 *   This is an demo showing how to use the LED matrix of an Arduino Uno R4
 *   Wifi to display the signal from a Sensirion SHT4x humidity temperature
 *   sensor
 *
 *   The orientation is such that the LED matrix is in landscape orientation,
 *   meaning that there are 12 values, with a resolution of 8 bars each.
 *   Furthermore, the most recent value is inserted on the right. 'Right' in
 *   this case is based on the text on the Arduino PCB, i.e. the orientation
 *   is such that the USB port of the board is looking to the left
 *
 *   Note that since the vertical resolution is just 8 bars, I've opted to
 *   choose an initial baseline in the init() function, and only show a
 *   limited range. The range displayed is defined as
 *     min value: 'BASELINE_SCALE_FACTOR' * baseline value
 *     max value: min value + TEMP_RANGE
 *
 *   Note that internally, all values are in degree celsius, although since
 *   the intention is to display the variation, the unit used should have
 *   no practical impact
 *
 * Dependencies:
 *   This demo uses the 'Sensirion I2C SHT4x' library available via the
 *   Arduino IDE Library Manager, or from
 *   https://github.com/Sensirion/arduino-i2c-sht4x
 */

#include <Arduino.h>
#include <Wire.h>
#include "SensirionI2CSht4x.h"
#include "Arduino_LED_Matrix.h"

// ----------------------------------------
// Configuration
static const uint8_t COLUMNS               = 12;
static const uint8_t ROWS                  = 8;
uint8_t              frame[ROWS][COLUMNS]  = { 0 };

static const uint8_t TEMP_RANGE            = 5;
static const uint8_t REFRESH_DELAY         = 250;

static const double  BASELINE_SCALE_FACTOR = 0.98;

static TwoWire&      SHT_I2C_INTERFACE     = Wire1; // Wire1 = Uno R4 QWIIC port

// ----------------------------------------
// Global variables
ArduinoLEDMatrix matrix;
SensirionI2CSht4x sht4x;

// display range; adjusted in setup()
uint8_t minVal = 0;
uint8_t maxVal = 100;

// SHT4x sensor values
float temperature = 0.0;
float humidity = 0.0;

// error messages
uint16_t error;
char errorMessage[128];

// ----------------------------------------
// helper functions
void ledPanic()
{
  uint8_t offFrame[ROWS][COLUMNS]  = { 0 };
  uint8_t onFrame[ROWS][COLUMNS]   = { 0 };
  std::fill_n(&onFrame[0][0], ROWS*COLUMNS, 1);
  
  while (true) {
    matrix.renderBitmap(onFrame, ROWS, COLUMNS);
    delay(500);
    matrix.renderBitmap(offFrame, ROWS, COLUMNS);
    delay(500);
  }
}


// shift bar chart one column to the left, and add the new value in the rightmost column
// note: "left" and "right" are defined based on the PCB silkscreen text
void insert_right(uint8_t frame[ROWS][COLUMNS], uint8_t value, bool fill = true) 
{
  // shift all rows to the left
  for (int columns = 1; columns < COLUMNS; ++columns) {
    for (int rows = 0; rows < ROWS; ++rows) {
      frame[rows][columns-1] = frame[rows][columns];
    }
  }

  // add new value as the rightmost column
  for (int rows = 0; rows < ROWS; ++rows) {
    frame[ROWS-1-rows][COLUMNS-1] = fill ? ((value-1) >= rows) : ((value-1) == rows);
  } 
}


/*
 * Run a high precision measurement on SHT4x sensor
 * @return bool: true on succes, false if measurement failed.
 */
bool Sht4xMeasureHighPrecision() {
  error = sht4x.measureHighPrecision(temperature, humidity);
  if (error)
  {
    Serial.println("Error in measureHighPrecision() during baseline calculation; retry");
    errorToString(error, errorMessage, 256);
    Serial.println(errorMessage);
    return false;
  }
  return true;
}

// ----------------------------------------
// main code
void setup() 
{
  matrix.begin();
  Wire1.begin();
  Serial.begin(115200);

  while (!Serial) {
    // let serial console settle
    delay(100);
  }

  uint32_t serialNumber;

  sht4x.begin(Wire1);

  // probe sensor by reading serial number
  error = sht4x.serialNumber(serialNumber);
  if (error) {
      Serial.print("SHT4x initializationfailed. Sketch halted\n");
      errorToString(error, errorMessage, 128);
      Serial.println(errorMessage);
      ledPanic();
  }

  //  Calculate min value (baseline)
  bool baselineInitialized = false;
  while (!baselineInitialized) {
    if (!Sht4xMeasureHighPrecision())
    {
      delay(500);
      continue;
    }
    minVal = (uint8_t)(BASELINE_SCALE_FACTOR * temperature);
    maxVal = minVal + TEMP_RANGE;
    Serial.print("minValue: ");
    Serial.println(minVal);
    baselineInitialized = true;
  }
}

void loop() 
{
  delay(REFRESH_DELAY);
  if (!Sht4xMeasureHighPrecision()) {
    // measurement failed, try again later.
    return;
  }
  // Update display matrix
  uint8_t displayValue = (uint8_t)((temperature - minVal) / (maxVal - minVal) * ROWS);
  insert_right(frame, displayValue, true);
  matrix.renderBitmap(frame, ROWS, COLUMNS);
}
