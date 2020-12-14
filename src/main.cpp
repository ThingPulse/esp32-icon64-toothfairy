/*
MIT License

Copyright (c) 2020 ThingPulse GmbH

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include <Arduino.h>
#include <FastLED.h>
#include "AudioFileSourceICYStream.h"
#include "AudioFileSourceBuffer.h"
#include "AudioGeneratorMP3.h"
#include "AudioOutputI2S.h"
#include <EasyButton.h>


// ********* User settings *********
const char* ssid = "yourssid";
const char* password = "yourpassw0rd";
const char* audioStreamUrl = "http://0n-80s.radionetz.de:8000/0n-80s.mp3";
// the scentific (non-)consensus seems to be that 2-3min 2-3x/day be sufficient
const uint8_t countdownSeconds = 180;
const uint8_t ledBrightness = 40;
// ********* END User settings *********


// Audio Settings
#define I2S_DOUT      25
#define I2S_BCLK      26
#define I2S_LRC       22
#define MODE_PIN      33

// LED Settings
#define NUM_LEDS      64
#define DATA_PIN      32

#define PUSH_BUTTON   39

CRGB leds[NUM_LEDS];

uint8_t getLedIndex(uint8_t x, uint8_t y);
void drawProgressBar(uint8_t progress);


AudioGeneratorMP3 *mp3;
AudioFileSourceICYStream *file;
AudioFileSourceBuffer *buff;
AudioOutputI2S *out;

EasyButton button(PUSH_BUTTON);
uint32_t countdownStartMillis = 0;
uint8_t countdownProgress = -1;
uint32_t countdownLedSwitchMillis = countdownSeconds * 1000 / NUM_LEDS;
boolean countdownIsRunning = false;

void onButtonPressed() {
  Serial.println("Starting countdown!");
  countdownStartMillis = millis();
  countdownIsRunning = true;
  digitalWrite(MODE_PIN, HIGH);
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(PUSH_BUTTON, INPUT);
  pinMode(MODE_PIN, OUTPUT);
  digitalWrite(MODE_PIN, LOW);

  WiFi.disconnect();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  
  button.begin();
  button.onPressed(onButtonPressed);

  Serial.println("Connecting to WiFi");
  WiFi.begin(ssid, password);
  // Try forever
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(1000);
  }
  Serial.println("Connected");

  audioLogger = &Serial;
  file = new AudioFileSourceICYStream(audioStreamUrl);
  //file->RegisterMetadataCB(MDCallback, (void*)"ICY");
  buff = new AudioFileSourceBuffer(file, 2048);
  //buff->RegisterStatusCB(StatusCallback, (void*)"buffer");
  out = new AudioOutputI2S();
  out->SetPinout(I2S_BCLK, I2S_LRC, I2S_DOUT );
  out->SetGain(0.3);
  mp3 = new AudioGeneratorMP3();
  //mp3->RegisterStatusCB(StatusCallback, (void*)"mp3");
  mp3->begin(buff, out);

  FastLED.addLeds<WS2812B, DATA_PIN>(leds, NUM_LEDS);
  FastLED.setBrightness(ledBrightness);
}

void loop() {
  uint8_t progress = 0;
  // only calculate progress if countdown is running
  if (countdownIsRunning) {
    progress = ((millis() - countdownStartMillis) / countdownLedSwitchMillis) % NUM_LEDS;
  }
  // only update LEDs if countdown status has changed
  if (progress != countdownProgress) {
    countdownProgress = progress;
    drawProgressBar(progress);
    FastLED.show();
  }

  button.read();

  static int lastms = 0;
  if (mp3->isRunning()) {
    if (millis()-lastms > 1000) {
      lastms = millis();
      Serial.printf("Running for %d ms...\n", lastms);
      Serial.flush();
     }
    if (!mp3->loop()) mp3->stop();
  } else {
    Serial.printf("MP3 done\n");
    delay(1000);
  }
}

uint8_t getLedIndex(uint8_t x, uint8_t y) {
  // x = 7 - x;
  if (y % 2 == 0) {
    return y * 8 + x;
  } else {
    return y * 8 + (7 - x);
  }
}

void drawProgressBar(uint8_t progress) {
  uint8_t counter = 0;
  CRGB color = CHSV(counter * 16, 255, 255);
  CRGB black = CRGB(0x0A, 0x0A, 0x0A);
  for (uint8_t y = 0; y < 8; y = y + 4) {
    for (uint8_t x = 0; x < 8; x = x + 2) {
      for (uint8_t xk = 0; xk < 2; xk++) {
        for (uint8_t yk = 0; yk < 4; yk++) {
          leds[getLedIndex(x + xk, y + yk)] = counter >= progress ? color : black;
          counter++;
        }
      }
      color.setHSV((counter / 8) * 32, 255, 255);
    }
  }
}