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
#include "SPIFFS.h"
#include <FastLED.h>
#include "Audio.h"
#include <EasyButton.h>


// ********* user settings *********
String ssid = "yourssid";
String password = "yourpassw0rd";
String audioStreamUrl = "http://0n-80s.radionetz.de:8000/0n-80s.mp3";
// the scentific (non-)consensus seems to be that 2-3min 2-3x/day be sufficient
uint8_t countdownSeconds = 120;
int volume = 4; // 0-21
// WARNING! Do not go over board with this as to avoid high temperatures and thus molten plastic.
const uint8_t ledBrightness = 40;
// ********* END user settings *********


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

Audio audio;

EasyButton button(PUSH_BUTTON);
uint32_t countdownStartMillis = 0;
uint8_t countdownProgress = -1;
uint32_t countdownLedSwitchMillis = countdownSeconds * 1000 / NUM_LEDS;
boolean countdownIsRunning = false;


// ********* forward declarations *********
void drawProgressBar(uint8_t progress);
uint8_t getLedIndex(uint8_t x, uint8_t y);
void loadPropertiesFromSpiffs();
void onButtonPressed();
// ********* END forward declarations *********


void setup() {
  Serial.begin(115200);
  delay(1000);
  loadPropertiesFromSpiffs();

  pinMode(PUSH_BUTTON, INPUT);
  pinMode(MODE_PIN, OUTPUT);
  digitalWrite(MODE_PIN, LOW);

  WiFi.disconnect();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);

  button.begin();
  button.onPressed(onButtonPressed);

  Serial.println("Connecting to WiFi");
  WiFi.begin(ssid.c_str(), password.c_str());
  // Try forever
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(1000);
  }
  Serial.println("Connected");

  audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
  audio.setVolume(volume);
  audio.connecttohost(audioStreamUrl.c_str());

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

  audio.loop();
}

void onButtonPressed() {
  Serial.println("Starting countdown!");
  countdownStartMillis = millis();
  countdownIsRunning = true;
  digitalWrite(MODE_PIN, HIGH);
}

void loadPropertiesFromSpiffs() {
  if (SPIFFS.begin()) {
    Serial.println("Attempting to read application.properties file from SPIFFS.");
    File f = SPIFFS.open("/application.properties");
    if (f) {
      Serial.println("File exists. Reading and assigning properties.");
      while (f.available()) {
        String key = f.readStringUntil('=');
        String value = f.readStringUntil('\n');
        if (key == "ssid") {
          ssid = value.c_str();
          Serial.println("Using 'ssid' from SPIFFS");
        } else if (key == "password") {
          password = value.c_str();
          Serial.println("Using 'password' from SPIFFS");
        } else if (key == "audioStreamUrl") {
          audioStreamUrl = value.c_str();
          Serial.println("Using 'audioStreamUrl' from SPIFFS");
        } else if (key == "countdownSeconds") {
          countdownSeconds = value.toInt();
          Serial.println("Using 'countdownSeconds' from SPIFFS");
        } else if (key == "volume") {
          volume = value.toFloat();
          Serial.println("Using 'volume' from SPIFFS");
        }
      }
    }
    f.close();
    Serial.println("Effective properties now as follows:");
    Serial.println("\tssid: " + ssid);
    Serial.println("\tpassword: " + password);
    Serial.println("\tvolume: " + String(volume));
    Serial.println("\taudio stream URL: " + audioStreamUrl);
    Serial.println("\tcountdown seconds: " + String(countdownSeconds));
  } else {
    Serial.println("SPIFFS mount failed.");
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

// The schreibfaul1/ESP32-audioI2S audio library will invoke these *if present*
// to dump some information about the audio stream to console.
void audio_info(const char *info) {
  Serial.print("info        ");
  Serial.println(info);
}
void audio_id3data(const char *info) { // id3 metadata
  Serial.print("id3data     ");
  Serial.println(info);
}
void audio_eof_mp3(const char *info) { // end of file
  Serial.print("eof_mp3     ");
  Serial.println(info);
}
void audio_showstation(const char *info) {
  Serial.print("station     ");
  Serial.println(info);
}
void audio_showstreamtitle(const char *info) {
  Serial.print("streamtitle ");
  Serial.println(info);
}
void audio_bitrate(const char *info) {
  Serial.print("bitrate     ");
  Serial.println(info);
}
void audio_commercial(const char *info) { // duration in sec
  Serial.print("commercial  ");
  Serial.println(info);
}
void audio_icyurl(const char *info) { // homepage
  Serial.print("icyurl      ");
  Serial.println(info);
}
void audio_lasthost(const char *info) { // stream URL played
  Serial.print("lasthost    ");
  Serial.println(info);
}
void audio_eof_speech(const char *info) {
  Serial.print("eof_speech  ");
  Serial.println(info);
}
