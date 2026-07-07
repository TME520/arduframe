#include <SPI.h>
#include <SdFat.h>

#if __has_include(<Arduino_GFX_Library.h>)
#include <Arduino_GFX_Library.h>
#else
#error "Missing Arduino_GFX_Library.h. Install the Arduino Library Manager package named 'GFX Library for Arduino', or build with the uno-r4-minima/uno-r4-wifi profile in sketch.yaml to install dependencies."
#endif

// Pin mapping for common 2.8" UNO-style parallel TFT shields.
// These shields do not use SPI for the LCD; the control/data pins are fixed
// by the shield wiring and the Arduino_GFX Arduino_UNOPAR8 bus drives them directly:
//   LCD_RST=A4, LCD_CS=A3, LCD_RS/LCD_CD=A2, LCD_WR=A1, LCD_RD=A0
//   LCD_D0=8, LCD_D1=9, LCD_D2=2, LCD_D3=3, LCD_D4=4, LCD_D5=5, LCD_D6=6, LCD_D7=7
#define SD_CS 10

const uint16_t FIRST_IMAGE = 1;
const uint16_t LAST_IMAGE = 999;
const unsigned long SLIDE_DURATION_MS = 10000UL;
const unsigned long SERIAL_WAIT_MS = 3000UL;
const unsigned long SERIAL_BAUD = 115200UL;
const uint8_t SD_INIT_SPEEDS_MHZ[] = {10, 4, 1};
const uint16_t BMP_READ_BUFFER_PIXELS = 40;

Arduino_DataBus *bus = new Arduino_UNOPAR8();
Arduino_GFX *tft = new Arduino_ILI9341(bus, A4 /* RST */, 1 /* rotation */, false /* IPS */);
SdFat SD;

uint16_t currentImage = FIRST_IMAGE;

uint16_t read16(File32 &file) {
  uint16_t value;
  value = (uint8_t)file.read();
  value |= (uint16_t)file.read() << 8;
  return value;
}

uint32_t read32(File32 &file) {
  uint32_t value;
  value = (uint8_t)file.read();
  value |= (uint32_t)file.read() << 8;
  value |= (uint32_t)file.read() << 16;
  value |= (uint32_t)file.read() << 24;
  return value;
}

void logMessage(const __FlashStringHelper *message) {
  Serial.println(message);
}

void logMessage(const char *message) {
  Serial.println(message);
}

void drawStatusMessage() {
  tft->fillScreen(0x0000);
  tft->setCursor(8, 8);
  tft->setTextColor(0xFFFF);
  tft->setTextSize(2);
}

void showStatus(const __FlashStringHelper *message) {
  logMessage(message);
  drawStatusMessage();
  tft->print(message);
}

void showStatus(const char *message) {
  logMessage(message);
  drawStatusMessage();
  tft->print(message);
}

void buildImageName(uint16_t imageNumber, char *buffer, size_t bufferSize) {
  snprintf(buffer, bufferSize, "/slideshow%03u.bmp", imageNumber);
}

bool initializeSdCard() {
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);

  for (uint8_t i = 0; i < sizeof(SD_INIT_SPEEDS_MHZ); i++) {
    uint8_t speedMhz = SD_INIT_SPEEDS_MHZ[i];
    Serial.print(F("Trying SD init at "));
    Serial.print(speedMhz);
    Serial.println(F(" MHz"));

    digitalWrite(SD_CS, HIGH);
    if (SD.begin(SD_CS, SD_SCK_MHZ(speedMhz))) {
      Serial.print(F("SD initialized at "));
      Serial.print(speedMhz);
      Serial.println(F(" MHz"));
      return true;
    }

    Serial.print(F("SD init attempt failed at "));
    Serial.print(speedMhz);
    Serial.println(F(" MHz"));
    delay(250);
  }

  Serial.println(F("SD init failed after all SPI speed attempts"));
  Serial.println(F("Check that the card is fully inserted and formatted as FAT16/FAT32"));
  Serial.println(F("Check shield SD_CS wiring/jumper; this sketch currently uses pin 10"));
  return false;
}

bool drawBmp(const char *filename, int16_t x, int16_t y) {
  File32 bmpFile = SD.open(filename, O_RDONLY);
  if (!bmpFile) {
    Serial.println(F("BMP open failed"));
    return false;
  }

  if (read16(bmpFile) != 0x4D42) {
    Serial.println(F("BMP signature mismatch"));
    bmpFile.close();
    return false;
  }

  (void)read32(bmpFile); // File size.
  (void)read32(bmpFile); // Creator bytes.
  uint32_t imageOffset = read32(bmpFile);
  uint32_t headerSize = read32(bmpFile);
  int32_t bmpWidth = (int32_t)read32(bmpFile);
  int32_t bmpHeight = (int32_t)read32(bmpFile);

  if (read16(bmpFile) != 1) {
    Serial.println(F("BMP planes value is invalid"));
    bmpFile.close();
    return false;
  }

  uint16_t bitDepth = read16(bmpFile);
  uint32_t compression = read32(bmpFile);
  if (headerSize < 40 || bmpWidth <= 0 || bmpHeight == 0 || bitDepth != 24 || compression != 0) {
    Serial.println(F("BMP must be uncompressed 24-bit format"));
    bmpFile.close();
    return false;
  }

  bool flip = true;
  if (bmpHeight < 0) {
    bmpHeight = -bmpHeight;
    flip = false;
  }

  int16_t drawWidth = bmpWidth;
  int16_t drawHeight = bmpHeight;
  if ((x >= tft->width()) || (y >= tft->height())) {
    bmpFile.close();
    return true;
  }
  if ((x + drawWidth - 1) >= tft->width()) {
    drawWidth = tft->width() - x;
  }
  if ((y + drawHeight - 1) >= tft->height()) {
    drawHeight = tft->height() - y;
  }

  uint32_t rowSize = ((uint32_t)bmpWidth * 3 + 3) & ~3;
  uint8_t sdbuffer[3 * BMP_READ_BUFFER_PIXELS];
  uint16_t lcdbuffer[BMP_READ_BUFFER_PIXELS];

  for (int16_t row = 0; row < drawHeight; row++) {
    uint32_t rowPosition = imageOffset + (flip ? (bmpHeight - 1 - row) : row) * rowSize;
    if (!bmpFile.seekSet(rowPosition)) {
      Serial.println(F("BMP row seek failed"));
      bmpFile.close();
      return false;
    }

    int16_t remaining = drawWidth;
    while (remaining > 0) {
      uint16_t pixelsToRead = remaining > BMP_READ_BUFFER_PIXELS ? BMP_READ_BUFFER_PIXELS : remaining;
      uint16_t bytesToRead = pixelsToRead * 3;
      if (bmpFile.read(sdbuffer, bytesToRead) != bytesToRead) {
        Serial.println(F("BMP row read failed"));
        bmpFile.close();
        return false;
      }

      uint8_t *src = sdbuffer;
      for (uint16_t i = 0; i < pixelsToRead; i++) {
        uint8_t b = *src++;
        uint8_t g = *src++;
        uint8_t r = *src++;
        lcdbuffer[i] = tft->color565(r, g, b);
      }

      int16_t chunkX = x + drawWidth - remaining;
      tft->draw16bitRGBBitmap(chunkX, y + row, lcdbuffer, pixelsToRead, 1);
      remaining -= pixelsToRead;
    }
  }

  bmpFile.close();
  return true;
}

void displayImage(uint16_t imageNumber) {
  char filename[22];
  buildImageName(imageNumber, filename, sizeof(filename));

  Serial.print(F("Loading image "));
  Serial.print(imageNumber);
  Serial.print(F(": "));
  Serial.println(filename);

  if (!drawBmp(filename, 0, 0)) {
    Serial.print(F("Image load failed for "));
    Serial.println(filename);

    tft->fillScreen(0x0000);
    tft->setCursor(8, 8);
    tft->setTextColor(0xF800);
    tft->setTextSize(2);
    tft->print(F("Could not load:"));
    tft->setCursor(8, 34);
    tft->print(filename);
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
  Serial.print(F("LCD interface: UNO-style 8-bit parallel shield, SD_CS="));
  Serial.println(SD_CS);

  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);

  Serial.println(F("Initializing ILI9341 TFT over Arduino_UNOPAR8..."));
  if (!tft->begin()) {
    Serial.println(F("TFT begin failed"));
    Serial.flush();
    while (true) {
      delay(1000);
    }
  }
  Serial.println(F("TFT initialized with rotation 1"));
  showStatus(F("Starting SD..."));

  Serial.println(F("Initializing SD card..."));
  if (!initializeSdCard()) {
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
