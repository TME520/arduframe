#include <SPI.h>
#include <SdFat.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <Adafruit_ImageReader.h>

// Pin mapping for the Adafruit 2.8" TFT Touch Shield for Arduino.
// If your shield uses different chip-select/data-command pins, change them here.
#define TFT_CS 10
#define TFT_DC 9
#define SD_CS 4

const uint16_t FIRST_IMAGE = 1;
const uint16_t LAST_IMAGE = 999;
const unsigned long SLIDE_DURATION_MS = 10000UL;
const unsigned long SERIAL_WAIT_MS = 3000UL;
const unsigned long SERIAL_BAUD = 115200UL;

Adafruit_ILI9341 tft(TFT_CS, TFT_DC);
SdFat SD;
Adafruit_ImageReader reader(SD);

uint16_t currentImage = FIRST_IMAGE;

void logMessage(const __FlashStringHelper *message) {
  Serial.println(message);
}

void logMessage(const char *message) {
  Serial.println(message);
}

void drawStatusMessage() {
  tft.fillScreen(ILI9341_BLACK);
  tft.setCursor(8, 8);
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(2);
}

void showStatus(const __FlashStringHelper *message) {
  logMessage(message);
  drawStatusMessage();
  tft.print(message);
}

void showStatus(const char *message) {
  logMessage(message);
  drawStatusMessage();
  tft.print(message);
}

void buildImageName(uint16_t imageNumber, char *buffer, size_t bufferSize) {
  snprintf(buffer, bufferSize, "/slideshow%03u.bmp", imageNumber);
}

void displayImage(uint16_t imageNumber) {
  char filename[22];
  buildImageName(imageNumber, filename, sizeof(filename));

  Serial.print(F("Loading image "));
  Serial.print(imageNumber);
  Serial.print(F(": "));
  Serial.println(filename);

  ImageReturnCode result = reader.drawBMP(filename, tft, 0, 0);
  if (result != IMAGE_SUCCESS) {
    Serial.print(F("Image load failed for "));
    Serial.print(filename);
    Serial.print(F(" with ImageReturnCode "));
    Serial.println(result);

    tft.fillScreen(ILI9341_BLACK);
    tft.setCursor(8, 8);
    tft.setTextColor(ILI9341_RED);
    tft.setTextSize(2);
    tft.print(F("Could not load:"));
    tft.setCursor(8, 34);
    tft.print(filename);
  } else {
    Serial.print(F("Image displayed successfully: "));
    Serial.println(filename);
  }
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  unsigned long serialStart = millis();
  while (!Serial && (millis() - serialStart < SERIAL_WAIT_MS)) {
    delay(10);
  }

  Serial.println(F("arduframe starting"));
  Serial.print(F("Serial baud: "));
  Serial.println(SERIAL_BAUD);
  Serial.print(F("TFT_CS="));
  Serial.print(TFT_CS);
  Serial.print(F(", TFT_DC="));
  Serial.print(TFT_DC);
  Serial.print(F(", SD_CS="));
  Serial.println(SD_CS);

  Serial.println(F("Initializing TFT..."));
  tft.begin();
  tft.setRotation(1);
  Serial.println(F("TFT initialized with rotation 1"));
  showStatus(F("Starting SD..."));

  Serial.println(F("Initializing SD card..."));
  if (!SD.begin(SD_CS, SD_SCK_MHZ(10))) {
    showStatus(F("SD init failed"));
    Serial.println(F("Halting because SD init failed"));
    Serial.flush();
    while (true) {
      delay(1000);
    }
  }

  Serial.println(F("SD initialized successfully"));
  Serial.print(F("Slideshow image range: "));
  Serial.print(FIRST_IMAGE);
  Serial.print(F(" to "));
  Serial.println(LAST_IMAGE);
  Serial.print(F("Slide duration ms: "));
  Serial.println(SLIDE_DURATION_MS);
}

void loop() {
  Serial.print(F("Beginning slideshow loop for image "));
  Serial.println(currentImage);
  displayImage(currentImage);
  Serial.print(F("Waiting before next image, ms: "));
  Serial.println(SLIDE_DURATION_MS);
  delay(SLIDE_DURATION_MS);

  currentImage++;
  if (currentImage > LAST_IMAGE) {
    Serial.println(F("Reached end of slideshow range; wrapping to first image"));
    currentImage = FIRST_IMAGE;
  }
}
