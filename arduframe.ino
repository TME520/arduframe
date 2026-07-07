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

Adafruit_ILI9341 tft(TFT_CS, TFT_DC);
SdFat SD;
Adafruit_ImageReader reader(SD);

uint16_t currentImage = FIRST_IMAGE;

template <typename MessageType>
void showStatus(MessageType message) {
  tft.fillScreen(ILI9341_BLACK);
  tft.setCursor(8, 8);
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(2);
  tft.print(message);
}

void buildImageName(uint16_t imageNumber, char *buffer, size_t bufferSize) {
  snprintf(buffer, bufferSize, "/slideshow%03u.bmp", imageNumber);
}

void displayImage(uint16_t imageNumber) {
  char filename[22];
  buildImageName(imageNumber, filename, sizeof(filename));

  ImageReturnCode result = reader.drawBMP(filename, tft, 0, 0);
  if (result != IMAGE_SUCCESS) {
    tft.fillScreen(ILI9341_BLACK);
    tft.setCursor(8, 8);
    tft.setTextColor(ILI9341_RED);
    tft.setTextSize(2);
    tft.print(F("Could not load:"));
    tft.setCursor(8, 34);
    tft.print(filename);
  }
}

void setup() {
  tft.begin();
  tft.setRotation(1);
  showStatus(F("Starting SD..."));

  if (!SD.begin(SD_CS, SD_SCK_MHZ(10))) {
    showStatus(F("SD init failed"));
    while (true) {
      delay(1000);
    }
  }
}

void loop() {
  displayImage(currentImage);
  delay(SLIDE_DURATION_MS);

  currentImage++;
  if (currentImage > LAST_IMAGE) {
    currentImage = FIRST_IMAGE;
  }
}
