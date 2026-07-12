#include <SPI.h>
#include <SdFat.h>
#include <Adafruit_GFX.h>
#include <MCUFRIEND_kbv.h>

/*
  Arduframe for UNO-style 320x240 parallel TFT shields.

  Shield pinout (matching the XC4630 datasheet):
    LCD data D0..D7 : Arduino D8, D9, D2, D3, D4, D5, D6, D7
    LCD RD          : A0
    LCD WR          : A1
    LCD RS/CD       : A2
    LCD CS          : A3
    LCD RESET       : A4
    SD card CS      : D10

  MCUFRIEND_kbv owns the LCD pin mapping and detects the controller.

  IMPORTANT FOR UNO R4:
  Use the UNO-R4-compatible MCUFRIEND_kbv fork referenced in the notes.
*/

#define SD_CS 10
#define TFT_ROTATION 1

/*
  Leave at 0 for automatic controller detection.

  If the Serial Monitor reports a useless ID, try one of these values:
    0x9341  - XC4630 v1 / ILI9341-like revision
    0x7783  - ST7781/ST7783-style revision
    0x8347  - HX8347-style revision

  Example:
    #define ARDUFRAME_FORCE_TFT_ID 0x9341
*/
#define ARDUFRAME_FORCE_TFT_ID 0x0000

// display constants

const uint16_t RED     = 0xF800;
const uint16_t GREEN   = 0x07E0;
const uint16_t BLUE    = 0x001F;
const uint16_t CYAN    = 0x07FF;
const uint16_t MAGENTA = 0xF81F;
const uint16_t YELLOW  = 0xFFE0;
const uint16_t WHITE   = 0xFFFF;
const uint16_t GRAY    = 0x520A;
const uint16_t BLACK   = 0x0000;

const uint16_t DISPLAY_WIDTH   = 320;
const uint16_t DISPLAY_HEIGHT  = 480;

// touch constants

const uint16_t PRESSURE_LEFT = 10;
const uint16_t PRESSURE_RIGHT = 1200;

const int XP = 8;
const int XM = A2;
const int YP = A3;
const int YM = 9;

//  UNO R3
const int XBEGIN = 129;
const int XEND = 902;
const int YBEGIN = 94;
const int YEND = 961;

//  UNO R4 MINIMA
// const int XBEGIN = 177;
// const int XEND = 863;
// const int YBEGIN = 121;
// const int YEND = 950;

// application constants

const uint16_t GRID_W = 240;
const uint16_t CELL_W = GRID_W/3;

const uint16_t GRID_X = (DISPLAY_WIDTH - GRID_W)/2;
const uint16_t GRID_Y = (DISPLAY_HEIGHT - GRID_W)/2;

const int16_t NONE = -1;
const int16_t NAUGHT = 1;
const int16_t CROSS = 0;

const uint16_t CROSS_COLOR = RED;
const uint16_t NAUGHT_COLOR = BLUE;
const uint16_t TIE_COLOR = GRAY;

const uint16_t MARK_W = CELL_W - 40;
const uint16_t MARK_THICKNESS = 9;

const uint16_t MENU_W = 240;
const uint16_t MENU_HEIGHT = 320;

const uint16_t MENU_X = (DISPLAY_WIDTH - MENU_W)/2;
const uint16_t MENU_Y = (DISPLAY_HEIGHT - MENU_HEIGHT)/2;

const uint16_t ITEM_W = 200;
const uint16_t ITEM_H = 40;

const uint16_t ITEM_X = MENU_X + (MENU_W - ITEM_W)/2;

const uint16_t GAME_OVER_X = 80;
const uint16_t GAME_OVER_Y = 40;

const uint16_t WINNER_X = 25;
const uint16_t WINNER_Y = GRID_Y + GRID_W + 40;

const uint16_t TIE_X = GRID_X + CELL_W + 10;
const uint16_t TIE_Y = GRID_Y + GRID_W + 40;

const uint16_t FIRST_IMAGE = 1;
const uint16_t LAST_IMAGE = 999;
const unsigned long SLIDE_DURATION_MS = 10000UL;
const unsigned long SERIAL_WAIT_MS = 3000UL;
const unsigned long SERIAL_BAUD = 115200UL;
const uint8_t SD_INIT_SPEEDS_MHZ[] = {4, 1, 10};
const uint16_t BMP_READ_BUFFER_PIXELS = 40;
const uint16_t BMP_PROGRESS_ROWS = 40;
const uint16_t TFT_FALLBACK_IDS[] = {0x9341, 0x7783, 0x8347, 0x9486, 0x9488, 0x8357};

MCUFRIEND_kbv tft;
SdFat SD;
uint16_t currentImage = FIRST_IMAGE;
uint16_t activeTftId = 0;

uint16_t readLe16(const uint8_t *buffer) {
  return (uint16_t)buffer[0] | ((uint16_t)buffer[1] << 8);
}

uint32_t readLe32(const uint8_t *buffer) {
  return (uint32_t)buffer[0] |
         ((uint32_t)buffer[1] << 8) |
         ((uint32_t)buffer[2] << 16) |
         ((uint32_t)buffer[3] << 24);
}

uint16_t color565(uint8_t r, uint8_t g, uint8_t b) {
  return ((uint16_t)(r & 0xF8) << 8) |
         ((uint16_t)(g & 0xFC) << 3) |
         ((uint16_t)b >> 3);
}

uint8_t scaleMaskedChannel(uint32_t pixel, uint32_t mask) {
  if (mask == 0) {
    return 0;
  }

  uint8_t shift = 0;
  while (shift < 32 && ((mask >> shift) & 1U) == 0U) {
    shift++;
  }

  uint8_t bits = 0;
  uint32_t shiftedMask = mask >> shift;
  while ((shiftedMask & 1U) != 0U) {
    bits++;
    shiftedMask >>= 1;
  }

  if (bits == 0) {
    return 0;
  }

  uint32_t value = (pixel & mask) >> shift;
  uint32_t maxValue = (bits >= 32) ? 0xFFFFFFFFUL : ((1UL << bits) - 1UL);
  return (uint8_t)((value * 255UL + maxValue / 2UL) / maxValue);
}

void printHex16(uint16_t value) {
  Serial.print(F("0x"));
  if (value < 0x1000) Serial.print('0');
  if (value < 0x0100) Serial.print('0');
  if (value < 0x0010) Serial.print('0');
  Serial.print(value, HEX);
}

bool unusableControllerId(uint16_t id) {
  return id == 0x0000 || id == 0xFFFF || id == 0xD3D3;
}

uint16_t detectTftController() {
  uint16_t detectedId = tft.readID();

  Serial.print(F("MCUFRIEND detected TFT controller ID: "));
  printHex16(detectedId);
  Serial.println();

#if ARDUFRAME_FORCE_TFT_ID != 0x0000
  Serial.print(F("Using forced TFT controller ID: "));
  printHex16(ARDUFRAME_FORCE_TFT_ID);
  Serial.println();
  return ARDUFRAME_FORCE_TFT_ID;
#else
  if (!unusableControllerId(detectedId)) {
    return detectedId;
  }

  Serial.println(F("Controller ID is unusable; falling back to 0x9341."));
  Serial.println(F("If the screen remains white, set ARDUFRAME_FORCE_TFT_ID to 0x7783 or 0x8347 and upload again."));
  return 0x9341;
#endif
}


bool tftHasUsableGeometry() {
  return tft.width() > 0 && tft.height() > 0;
}

bool initializeTftWithId(uint16_t id) {
  tft.begin(id);
  tft.setRotation(TFT_ROTATION);

  Serial.print(F("TFT initialized as ID "));
  printHex16(id);
  Serial.print(F(", drawable size "));
  Serial.print(tft.width());
  Serial.print('x');
  Serial.println(tft.height());

  return tftHasUsableGeometry();
}

void initializeTft() {
  activeTftId = detectTftController();
  if (initializeTftWithId(activeTftId)) {
    return;
  }

  Serial.println(F("TFT reported unusable drawable size; trying known MCUFRIEND-compatible IDs."));

  for (uint8_t i = 0; i < sizeof(TFT_FALLBACK_IDS) / sizeof(TFT_FALLBACK_IDS[0]); i++) {
    uint16_t fallbackId = TFT_FALLBACK_IDS[i];
    if (fallbackId == activeTftId) {
      continue;
    }

    Serial.print(F("Trying fallback TFT controller ID: "));
    printHex16(fallbackId);
    Serial.println();

    if (initializeTftWithId(fallbackId)) {
      activeTftId = fallbackId;
      Serial.print(F("Using fallback TFT controller ID: "));
      printHex16(activeTftId);
      Serial.println();
      return;
    }
  }

  Serial.println(F("No fallback TFT ID produced a usable drawable size."));
  Serial.println(F("Set ARDUFRAME_FORCE_TFT_ID to a controller ID that matches your shield and upload again."));
}

void drawTftSelfTest() {
  const uint16_t BLACK = 0x0000;
  const uint16_t WHITE = 0xFFFF;
  const uint16_t RED = 0xF800;
  const uint16_t GREEN = 0x07E0;
  const uint16_t BLUE = 0x001F;
  const uint16_t YELLOW = 0xFFE0;

  int16_t w = tft.width();
  int16_t h = tft.height();
  int16_t barWidth = w / 4;

  tft.fillScreen(BLACK);
  tft.fillRect(0, 0, barWidth, h, RED);
  tft.fillRect(barWidth, 0, barWidth, h, GREEN);
  tft.fillRect(barWidth * 2, 0, barWidth, h, BLUE);
  tft.fillRect(barWidth * 3, 0, w - barWidth * 3, h, WHITE);

  tft.drawRect(0, 0, w, h, WHITE);
  tft.drawLine(0, 0, w - 1, h - 1, YELLOW);
  tft.drawLine(w - 1, 0, 0, h - 1, YELLOW);

  tft.fillRect(5, 5, 205, 44, BLACK);
  tft.setCursor(10, 10);
  tft.setTextColor(WHITE, BLACK);
  tft.setTextSize(2);
  tft.print(F("TFT OK ID "));
  tft.println(activeTftId, HEX);
  tft.print(tft.width());
  tft.print('x');
  tft.print(tft.height());

  delay(3000);
}

void drawStatusMessage() {
  tft.fillScreen(0x0000);
  tft.setCursor(8, 8);
  tft.setTextColor(0xFFFF, 0x0000);
  tft.setTextSize(2);
}

void showStatus(const __FlashStringHelper *message) {
  Serial.println(message);
  drawStatusMessage();
  tft.print(message);
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

    delay(250);
  }

  Serial.println(F("SD initialization failed."));
  Serial.println(F("Check FAT16/FAT32 formatting, card insertion, and SD chip-select D10."));
  return false;
}

bool drawBmp(const char *filename, int16_t x, int16_t y) {
  File32 bmpFile = SD.open(filename, O_RDONLY);
  if (!bmpFile) {
    Serial.println(F("BMP open failed"));
    return false;
  }

  uint8_t bmpHeader[54];
  int bytesRead = bmpFile.read(bmpHeader, sizeof(bmpHeader));
  if (bytesRead != (int)sizeof(bmpHeader)) {
    Serial.println(F("BMP header read failed"));
    bmpFile.close();
    return false;
  }

  if (readLe16(&bmpHeader[0]) != 0x4D42) {
    Serial.println(F("Not a BMP file"));
    bmpFile.close();
    return false;
  }

  uint32_t imageOffset = readLe32(&bmpHeader[10]);
  uint32_t headerSize = readLe32(&bmpHeader[14]);
  int32_t bmpWidth = (int32_t)readLe32(&bmpHeader[18]);
  int32_t bmpHeight = (int32_t)readLe32(&bmpHeader[22]);
  uint16_t planes = readLe16(&bmpHeader[26]);
  uint16_t bitDepth = readLe16(&bmpHeader[28]);
  uint32_t compression = readLe32(&bmpHeader[30]);

  bool usesBitfieldMasks = bitDepth == 32 && compression == 3;
  bool supportedCompression = compression == 0 || usesBitfieldMasks;

  uint32_t redMask = 0x00FF0000UL;
  uint32_t greenMask = 0x0000FF00UL;
  uint32_t blueMask = 0x000000FFUL;

  if (usesBitfieldMasks) {
    if (headerSize != 40 && headerSize < 56) {
      Serial.println(F("Unsupported 32-bit BMP mask header"));
      bmpFile.close();
      return false;
    }

    uint8_t maskBytes[16];
    if (!bmpFile.seekSet(54) ||
        bmpFile.read(maskBytes, sizeof(maskBytes)) != (int)sizeof(maskBytes)) {
      Serial.println(F("BMP mask read failed"));
      bmpFile.close();
      return false;
    }

    redMask = readLe32(&maskBytes[0]);
    greenMask = readLe32(&maskBytes[4]);
    blueMask = readLe32(&maskBytes[8]);
  }

  if (headerSize < 40 || planes != 1 || bmpWidth <= 0 || bmpHeight == 0 ||
      !supportedCompression || (bitDepth != 24 && bitDepth != 32)) {
    Serial.println(F("BMP must be uncompressed 24-bit or 32-bit"));
    bmpFile.close();
    return false;
  }

  bool flip = true;
  if (bmpHeight < 0) {
    bmpHeight = -bmpHeight;
    flip = false;
  }

  int32_t clippedWidth32 = bmpWidth;
  int32_t clippedHeight32 = bmpHeight;

  if (x >= tft.width() || y >= tft.height()) {
    bmpFile.close();
    return true;
  }

  if (x + clippedWidth32 > tft.width()) {
    clippedWidth32 = tft.width() - x;
  }
  if (y + clippedHeight32 > tft.height()) {
    clippedHeight32 = tft.height() - y;
  }

  int16_t drawWidth = (int16_t)clippedWidth32;
  int16_t drawHeight = (int16_t)clippedHeight32;
  uint8_t bytesPerPixel = bitDepth / 8;
  uint32_t rowSize = ((uint32_t)bmpWidth * bytesPerPixel + 3UL) & ~3UL;

  uint8_t sdBuffer[4 * BMP_READ_BUFFER_PIXELS];
  uint16_t lcdBuffer[BMP_READ_BUFFER_PIXELS];

  for (int16_t row = 0; row < drawHeight; row++) {
    if ((row % BMP_PROGRESS_ROWS) == 0) {
      Serial.print(F("BMP row "));
      Serial.print(row);
      Serial.print('/');
      Serial.println(drawHeight);
    }

    uint32_t sourceRow = flip ? (uint32_t)(bmpHeight - 1 - row) : (uint32_t)row;
    uint32_t rowPosition = imageOffset + sourceRow * rowSize;

    if (!bmpFile.seekSet(rowPosition)) {
      Serial.println(F("BMP row seek failed"));
      bmpFile.close();
      return false;
    }

    int16_t remaining = drawWidth;
    while (remaining > 0) {
      uint16_t pixelCount = remaining > BMP_READ_BUFFER_PIXELS
                                ? BMP_READ_BUFFER_PIXELS
                                : (uint16_t)remaining;
      uint16_t byteCount = pixelCount * bytesPerPixel;

      if (bmpFile.read(sdBuffer, byteCount) != byteCount) {
        Serial.println(F("BMP row read failed"));
        bmpFile.close();
        return false;
      }

      uint8_t *src = sdBuffer;
      for (uint16_t i = 0; i < pixelCount; i++) {
        uint8_t b = *src++;
        uint8_t g = *src++;
        uint8_t r = *src++;

        if (bytesPerPixel == 4) {
          uint8_t a = *src++;
          if (usesBitfieldMasks) {
            uint32_t pixel = (uint32_t)b |
                             ((uint32_t)g << 8) |
                             ((uint32_t)r << 16) |
                             ((uint32_t)a << 24);
            r = scaleMaskedChannel(pixel, redMask);
            g = scaleMaskedChannel(pixel, greenMask);
            b = scaleMaskedChannel(pixel, blueMask);
          }
        }

        lcdBuffer[i] = color565(r, g, b);
      }

      int16_t chunkX = x + drawWidth - remaining;
      tft.drawRGBBitmap(chunkX, y + row, lcdBuffer, pixelCount, 1);
      remaining -= pixelCount;
    }
  }

  bmpFile.close();
  Serial.println(F("BMP draw complete"));
  return true;
}

void displayImage(uint16_t imageNumber) {
  char filename[22];
  buildImageName(imageNumber, filename, sizeof(filename));

  Serial.print(F("Loading "));
  Serial.println(filename);

  if (!drawBmp(filename, 0, 0)) {
    drawStatusMessage();
    tft.setTextColor(0xF800, 0x0000);
    tft.print(F("Could not load:"));
    tft.setCursor(8, 34);
    tft.print(filename);
  }
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  unsigned long serialStart = millis();
  while (!Serial && millis() - serialStart < SERIAL_WAIT_MS) {
    delay(10);
  }

  Serial.println(F("Arduframe MCUFRIEND build starting"));
  Serial.println(F("LCD pins: D2-D9, A0-A4; SD CS: D10"));

  // Keep the SD device deselected while the parallel LCD is identified.
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);

  initializeTft();
  if (!tftHasUsableGeometry()) {
    while (true) {
      delay(1000);
    }
  }

  // This appears before any SD-card access. If it stays white, the LCD
  // controller ID/library is still wrong; the BMP and SD code are not involved.
  drawTftSelfTest();

  // showStatus("Starting SD...");
  if (!initializeSdCard()) {
    // showStatus("SD init failed");
    while (true) {
      delay(1000);
    }
  }

  // showStatus("SD ready");
  delay(750);
}

void loop() {
  displayImage(currentImage);
  delay(SLIDE_DURATION_MS);

  currentImage++;
  if (currentImage > LAST_IMAGE) {
    currentImage = FIRST_IMAGE;
  }
}
