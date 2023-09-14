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
 *   sensor to warm it up, you should see the signal raise
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
 *   This demo uses the 'arduino-sht' library available via the Arduino IDE 
 *   Library Manager, or from https://github.com/sensirion/arduino-sht
 */

#include <Wire.h>

#include "SHTSensor.h"
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

// ----------------------------------------
ArduinoLEDMatrix matrix;
SHTSensor sht(SHTSensor::SHT4X);

// display range; adjusted in setup()
uint8_t minVal = 0;
uint8_t maxVal = 100;

void setup() 
{
  matrix.begin();
  Wire1.begin();
  Serial.begin(115200);

  delay(1000); // let serial console settle

  if (!sht.init(SHT_I2C_INTERFACE)) {
      Serial.print("init(): failed. Sketch halted\n");
      for (;;) delay(500);
  }

  //  Calculate min value (baseline)
  bool baselineInitialized = false;
  while (!baselineInitialized) {
    if (sht.readSample()) {
        minVal = (uint8_t)(BASELINE_SCALE_FACTOR*sht.getTemperature());
        maxVal = minVal + TEMP_RANGE;
        Serial.print("minValue: ");
        Serial.println(minVal);
        baselineInitialized = true;
    } else {
        Serial.print("Error in readSample() during baseline calculation; retrying\n");
        delay(500);
    }
  }
}

void loop() 
{
  if (sht.readSample()) {
    uint8_t displayValue = (uint8_t)((sht.getTemperature()-minVal) / (maxVal-minVal) * ROWS);
    insert_right(frame, displayValue, true);
    matrix.renderBitmap(frame, ROWS, COLUMNS);
  } else {
      Serial.print("Error in readSample()\n");
  }
  delay(REFRESH_DELAY);
}
