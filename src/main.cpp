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

const char* ssid = "yourssid";
const char* password = "yourpassw0rd";

uint8_t getLedIndex(uint8_t x, uint8_t y);
void drawProgressBar(uint8_t progress);

const char *URL="http://0n-80s.radionetz.de:8000/0n-70s.mp3";

AudioGeneratorMP3 *mp3;
AudioFileSourceICYStream *file;
AudioFileSourceBuffer *buff;
AudioOutputI2S *out;

EasyButton button(PUSH_BUTTON);
uint32_t startMillis = 0;
boolean isRunning = false;

// Callback.
void onPressed() {
    Serial.println("Button has been pressed!");
    startMillis = millis();
    isRunning = true;
    digitalWrite(MODE_PIN, HIGH);
}

void setup() {
  pinMode(PUSH_BUTTON, INPUT);
  pinMode(MODE_PIN, OUTPUT);
  digitalWrite(MODE_PIN, LOW);

  WiFi.disconnect();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  
  WiFi.begin(ssid, password);

  button.begin();

  // Attach callback.
  button.onPressed(onPressed);

  // Try forever
  while (WiFi.status() != WL_CONNECTED) {
    Serial.println("...Connecting to WiFi");
    delay(1000);
  }
  Serial.println("Connected");

  audioLogger = &Serial;
  file = new AudioFileSourceICYStream(URL);
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

}

void loop() {
  if (!isRunning) {
    startMillis = millis();
  }
  uint8_t progress = ((millis() - startMillis) / 3000) % 64;
  drawProgressBar(progress);
  FastLED.setBrightness(50);
  FastLED.show();

  static int lastms = 0;
  button.read();

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
  //x = 7 - x;
  if (y % 2 == 0) {
    return y * 8 + x;
  } else {
    return y*8 + (7 - x);
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
            leds[getLedIndex(x + xk, y + yk)] = counter > progress ? color : black;
            counter++;
          }
      }
      color.setHSV((counter / 8) * 32, 255, 255);
    }
  }
}