#include <SPI.h>
#include <SdFat.h>
#include <Arduino_GFX_Library.h>

// Default pin mapping for common 2.8" UNO-style parallel TFT shields.
// These shields do not use SPI for the LCD; the control/data pins are fixed
// by the shield wiring and the Arduino_GFX Arduino_UNOPAR8 bus drives them directly:
//   LCD_RST=A4, LCD_CS=A3, LCD_RS/LCD_CD=A2, LCD_WR=A1, LCD_RD=A0
//   LCD_D0=8, LCD_D1=9, LCD_D2=2, LCD_D3=3, LCD_D4=4, LCD_D5=5, LCD_D6=6, LCD_D7=7
//
// M5Stack Cardputer uses its built-in SPI ST7789 display and separate uSD SPI pins.
#if defined(ARDUINO_M5STACK_CARDPUTER)
#define LCD_RST 33
#define LCD_DC 34
#define LCD_MOSI 35
#define LCD_SCK 36
#define LCD_CS 37
#define LCD_BL 38
#define SD_CS 12
#define SD_MOSI 14
#define SD_MISO 39
#define SD_SCK 40
#else
#define LCD_RST A4
#define SD_CS 10
// Brandless AliExpress shields marked "2.8 TFT LCD Shield" with this pinout
// are normally 240x320 ILI9341-style panels. A wrong controller init commonly
// leaves the backlight on but the LCD glass white, while Serial still reports
// that images were read and drawn. Set this to ARDUFRAME_TFT_ILI9486 only for
// 3.5-inch 320x480 shields.
#define ARDUFRAME_TFT_ILI9341 1
#define ARDUFRAME_TFT_ILI9486 2
#ifndef ARDUFRAME_TFT_DRIVER
#define ARDUFRAME_TFT_DRIVER ARDUFRAME_TFT_ILI9341
#endif
#endif

const uint16_t FIRST_IMAGE = 1;
const uint16_t LAST_IMAGE = 999;
const unsigned long SLIDE_DURATION_MS = 10000UL;
const unsigned long SERIAL_WAIT_MS = 3000UL;
const unsigned long SERIAL_BAUD = 115200UL;
const uint8_t SD_INIT_SPEEDS_MHZ[] = {4, 1, 10};
const uint16_t BMP_READ_BUFFER_PIXELS = 40;
const uint16_t BMP_SUPPORTED_DEPTH_24 = 24;
const uint16_t BMP_SUPPORTED_DEPTH_32 = 32;
const uint16_t TFT_DIAGNOSTIC_COLOR_DELAY_MS = 450;
const uint16_t BMP_PROGRESS_ROWS = 40;

#if defined(ARDUINO_ARCH_AVR)
typedef const __FlashStringHelper *FlashString;
#else
typedef const char *FlashString;
#endif

#if defined(ARDUINO_M5STACK_CARDPUTER)
Arduino_DataBus *bus = new Arduino_ESP32SPI(LCD_DC, LCD_CS, LCD_SCK, LCD_MOSI);
Arduino_GFX *tft = new Arduino_ST7789(bus, LCD_RST, 1 /* rotation */, true /* IPS */, 135, 240, 52, 40);
#else
Arduino_DataBus *bus = new Arduino_UNOPAR8();
#if ARDUFRAME_TFT_DRIVER == ARDUFRAME_TFT_ILI9341
Arduino_GFX *tft = new Arduino_ILI9341(bus, LCD_RST /* RST */, 1 /* rotation */, false /* IPS */);
#elif ARDUFRAME_TFT_DRIVER == ARDUFRAME_TFT_ILI9486
Arduino_GFX *tft = new Arduino_ILI9486(bus, LCD_RST /* RST */, 1 /* rotation */, false /* IPS */);
#else
#error "Unsupported ARDUFRAME_TFT_DRIVER; use ARDUFRAME_TFT_ILI9341 or ARDUFRAME_TFT_ILI9486"
#endif
#endif
SdFat SD;

uint16_t currentImage = FIRST_IMAGE;

#if !defined(ARDUINO_M5STACK_CARDPUTER)
const uint8_t UNO_TFT_DATA_PINS[8] = {8, 9, 2, 3, 4, 5, 6, 7};
const uint8_t UNO_TFT_RD = A0;
const uint8_t UNO_TFT_WR = A1;
const uint8_t UNO_TFT_RS = A2;
const uint8_t UNO_TFT_CS = A3;
const uint8_t UNO_TFT_RST = A4;

struct TftRegisterProbe {
  uint8_t command;
  FlashString name;
  uint8_t bytesToRead;
  bool discardFirstByte;
};

const TftRegisterProbe TFT_REGISTER_PROBES[] = {
    {0x00, F("NOP/status or legacy ID"), 2, false},
    {0x04, F("RDDID display ID"), 4, true},
    {0x09, F("RDDST display status"), 5, true},
    {0x0A, F("RDDPM power mode"), 2, true},
    {0x0B, F("RDDMADCTL memory access"), 2, true},
    {0x0C, F("RDDCOLMOD pixel format"), 2, true},
    {0x0D, F("RDDIM image mode"), 2, true},
    {0x0E, F("RDDSM signal mode"), 2, true},
    {0x0F, F("RDDSDR self diagnostic"), 2, true},
    {0xA1, F("RD_DDB display descriptor"), 5, true},
    {0xBF, F("ILI9481/ILI9486 ID"), 6, true},
    {0xD3, F("ILI9341/ILI9488 ID4"), 4, true},
    {0xDA, F("RDID1 manufacturer"), 2, true},
    {0xDB, F("RDID2 driver"), 2, true},
    {0xDC, F("RDID3 module"), 2, true},
    {0xEF, F("ILI9327/ILI9341 extended ID"), 6, true},
};
#endif

uint16_t readLe16(const uint8_t *buffer) {
  return (uint16_t)buffer[0] | ((uint16_t)buffer[1] << 8);
}

uint32_t readLe32(const uint8_t *buffer) {
  return (uint32_t)buffer[0] | ((uint32_t)buffer[1] << 8) | ((uint32_t)buffer[2] << 16) | ((uint32_t)buffer[3] << 24);
}

void logMessage(FlashString message) {
  Serial.println(message);
}

#if defined(ARDUINO_ARCH_AVR)
void logMessage(const char *message) {
  Serial.println(message);
}
#endif

#if !defined(ARDUINO_M5STACK_CARDPUTER)
void setUnoTftDataMode(uint8_t mode) {
  for (uint8_t i = 0; i < 8; i++) {
    pinMode(UNO_TFT_DATA_PINS[i], mode);
  }
}

void writeUnoTftDataBus(uint8_t value) {
  for (uint8_t i = 0; i < 8; i++) {
    digitalWrite(UNO_TFT_DATA_PINS[i], (value & (1 << i)) ? HIGH : LOW);
  }
}

uint8_t readUnoTftDataBus() {
  uint8_t value = 0;
  for (uint8_t i = 0; i < 8; i++) {
    if (digitalRead(UNO_TFT_DATA_PINS[i]) == HIGH) {
      value |= (1 << i);
    }
  }
  return value;
}

void writeUnoTftCommand(uint8_t command) {
  setUnoTftDataMode(OUTPUT);
  digitalWrite(UNO_TFT_RS, LOW);
  digitalWrite(UNO_TFT_RD, HIGH);
  writeUnoTftDataBus(command);
  digitalWrite(UNO_TFT_WR, LOW);
  delayMicroseconds(2);
  digitalWrite(UNO_TFT_WR, HIGH);
  delayMicroseconds(2);
}

uint8_t readUnoTftByte() {
  digitalWrite(UNO_TFT_RD, LOW);
  delayMicroseconds(3);
  uint8_t value = readUnoTftDataBus();
  digitalWrite(UNO_TFT_RD, HIGH);
  delayMicroseconds(3);
  return value;
}

void printHexByte(uint8_t value) {
  if (value < 0x10) {
    Serial.print('0');
  }
  Serial.print(value, HEX);
}

void printHexWord(uint16_t value) {
  Serial.print(F("0x"));
  if (value < 0x1000) {
    Serial.print('0');
  }
  if (value < 0x0100) {
    Serial.print('0');
  }
  if (value < 0x0010) {
    Serial.print('0');
  }
  Serial.print(value, HEX);
}

void printLikelyController(uint16_t id) {
  Serial.print(F(" -> "));
  switch (id) {
    case 0x0154:
      Serial.println(F("S6D0154 / S6D0154-compatible"));
      break;
    case 0x1289:
      Serial.println(F("SSD1289"));
      break;
    case 0x1520:
      Serial.println(F("S6D0154/ILI9320 family clone (reported 0x1520)"));
      break;
    case 0x1581:
      Serial.println(F("R61581"));
      break;
    case 0x1963:
      Serial.println(F("SSD1963"));
      break;
    case 0x4535:
      Serial.println(F("LGDP4535"));
      break;
    case 0x7783:
      Serial.println(F("ST7783"));
      break;
    case 0x8357:
      Serial.println(F("HX8357"));
      break;
    case 0x8989:
      Serial.println(F("SSD1289 clone (reported 0x8989)"));
      break;
    case 0x9320:
      Serial.println(F("ILI9320"));
      break;
    case 0x9325:
      Serial.println(F("ILI9325"));
      break;
    case 0x9327:
      Serial.println(F("ILI9327"));
      break;
    case 0x9328:
      Serial.println(F("ILI9328"));
      break;
    case 0x9341:
      Serial.println(F("ILI9341"));
      break;
    case 0x9481:
      Serial.println(F("ILI9481"));
      break;
    case 0x9486:
      Serial.println(F("ILI9486"));
      break;
    case 0x9488:
      Serial.println(F("ILI9488"));
      break;
    case 0xFFFF:
      Serial.println(F("all bits high; LCD bus may be floating, RD may be disconnected, or controller did not answer"));
      break;
    case 0x0000:
      Serial.println(F("all bits low; LCD bus may be held low, CS/RS/RD mapping may differ, or controller did not answer"));
      break;
    default:
      Serial.println(F("unknown/unlisted controller ID"));
      break;
  }
}

void printUnoShieldIdentificationChecklist() {
  Serial.println(F("TFT physical ID checklist:"));
  Serial.println(F("  1) Remove shield/power before inspecting the back and LCD flex cable."));
  Serial.println(F("  2) Record PCB text, LCD flex text, and any controller IC marking."));
  Serial.println(F("  3) Look for IDs like ILI9341, ILI9325, ILI9328, ILI9486, HX8347, ST7781, or R61505."));
  Serial.println(F("  4) Confirm shield pin labels match RD=A0 WR=A1 RS=A2 CS=A3 RST=A4 and SD_SS=10."));
  Serial.println(F("  5) If the startup color test stays white, share those markings with this serial log."));
}

void probeUnoTftReadRegisters() {
  Serial.println(F("TFT hardware probe: UNO 8-bit read-register sweep"));
  Serial.println(F("Expected shield pins: RD=A0 WR=A1 RS=A2 CS=A3 RST=A4 D0=8 D1=9 D2=2 D3=3 D4=4 D5=5 D6=6 D7=7"));
  pinMode(UNO_TFT_RD, OUTPUT);
  pinMode(UNO_TFT_WR, OUTPUT);
  pinMode(UNO_TFT_RS, OUTPUT);
  pinMode(UNO_TFT_CS, OUTPUT);
  pinMode(UNO_TFT_RST, OUTPUT);
  digitalWrite(UNO_TFT_CS, HIGH);
  digitalWrite(UNO_TFT_RD, HIGH);
  digitalWrite(UNO_TFT_WR, HIGH);
  digitalWrite(UNO_TFT_RS, HIGH);

  Serial.println(F("TFT hardware probe: reset pulse"));
  digitalWrite(UNO_TFT_RST, HIGH);
  delay(10);
  digitalWrite(UNO_TFT_RST, LOW);
  delay(20);
  digitalWrite(UNO_TFT_RST, HIGH);
  delay(150);

  for (uint8_t probeIndex = 0; probeIndex < sizeof(TFT_REGISTER_PROBES) / sizeof(TFT_REGISTER_PROBES[0]); probeIndex++) {
    TftRegisterProbe probe = TFT_REGISTER_PROBES[probeIndex];
    uint8_t values[6] = {0};
    digitalWrite(UNO_TFT_CS, LOW);
    writeUnoTftCommand(probe.command);
    digitalWrite(UNO_TFT_RS, HIGH);
    setUnoTftDataMode(INPUT_PULLUP);
    for (uint8_t i = 0; i < probe.bytesToRead; i++) {
      values[i] = readUnoTftByte();
    }
    digitalWrite(UNO_TFT_CS, HIGH);
    setUnoTftDataMode(OUTPUT);

    Serial.print(F("TFT reg 0x"));
    printHexByte(probe.command);
    Serial.print(F(" ("));
    Serial.print(probe.name);
    Serial.print(F("):"));
    for (uint8_t i = 0; i < probe.bytesToRead; i++) {
      Serial.print(' ');
      printHexByte(values[i]);
    }
    Serial.println();

    uint8_t firstPayloadByte = probe.discardFirstByte ? 1 : 0;
    if (probe.bytesToRead >= firstPayloadByte + 2) {
      uint16_t candidateId = ((uint16_t)values[firstPayloadByte] << 8) | values[firstPayloadByte + 1];
      if (candidateId != 0x0000 && candidateId != 0xFFFF) {
        Serial.print(F("  candidate 16-bit ID from this register: "));
        printHexWord(candidateId);
        printLikelyController(candidateId);
      }
    }
  }

  Serial.println(F("TFT hardware probe complete. If every read is 00 or FF, the shield may not support readback on UNO R4 or the control-pin mapping may differ."));
}
#endif


void showTftSelfTestPattern() {
  Serial.println(F("Drawing TFT self-test color bars"));
  int16_t barWidth = tft->width() / 4;
  tft->fillRect(0, 0, barWidth, tft->height(), 0xF800);
  tft->fillRect(barWidth, 0, barWidth, tft->height(), 0x07E0);
  tft->fillRect(barWidth * 2, 0, barWidth, tft->height(), 0x001F);
  tft->fillRect(barWidth * 3, 0, tft->width() - (barWidth * 3), tft->height(), 0xFFFF);
  delay(1000);
}

void showTftDiagnosticColorSequence() {
  Serial.println(F("Drawing TFT diagnostic color sequence: black, white, red, green, blue, color bars"));
  tft->fillScreen(0x0000);
  delay(TFT_DIAGNOSTIC_COLOR_DELAY_MS);
  tft->fillScreen(0xFFFF);
  delay(TFT_DIAGNOSTIC_COLOR_DELAY_MS);
  tft->fillScreen(0xF800);
  delay(TFT_DIAGNOSTIC_COLOR_DELAY_MS);
  tft->fillScreen(0x07E0);
  delay(TFT_DIAGNOSTIC_COLOR_DELAY_MS);
  tft->fillScreen(0x001F);
  delay(TFT_DIAGNOSTIC_COLOR_DELAY_MS);
  showTftSelfTestPattern();
}

void drawStatusMessage() {
  tft->fillScreen(0x0000);
  tft->setCursor(8, 8);
  tft->setTextColor(0xFFFF);
  tft->setTextSize(2);
}

void showStatus(FlashString message) {
  logMessage(message);
  drawStatusMessage();
  tft->print(message);
}

#if defined(ARDUINO_ARCH_AVR)
void showStatus(const char *message) {
  logMessage(message);
  drawStatusMessage();
  tft->print(message);
}
#endif

void buildImageName(uint16_t imageNumber, char *buffer, size_t bufferSize) {
  snprintf(buffer, bufferSize, "/slideshow%03u.bmp", imageNumber);
}

bool initializeSdCard() {
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);
#if defined(ARDUINO_M5STACK_CARDPUTER)
  SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
#endif

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
  Serial.println(F("Opening BMP file..."));
  File32 bmpFile = SD.open(filename, O_RDONLY);
  if (!bmpFile) {
    Serial.println(F("BMP open failed"));
    return false;
  }

  Serial.println(F("BMP file opened; reading 54-byte header block"));
  uint8_t bmpHeader[54];
  int bytesRead = bmpFile.read(bmpHeader, sizeof(bmpHeader));
  if (bytesRead != (int)sizeof(bmpHeader)) {
    Serial.print(F("BMP header read failed: bytes="));
    Serial.print(bytesRead);
    Serial.print(F(" expected="));
    Serial.println(sizeof(bmpHeader));
    bmpFile.close();
    return false;
  }
  Serial.println(F("BMP header block read complete"));

  if (readLe16(&bmpHeader[0]) != 0x4D42) {
    Serial.print(F("BMP signature mismatch: 0x"));
    Serial.println(readLe16(&bmpHeader[0]), HEX);
    bmpFile.close();
    return false;
  }

  uint32_t imageOffset = readLe32(&bmpHeader[10]);
  uint32_t headerSize = readLe32(&bmpHeader[14]);
  int32_t bmpWidth = (int32_t)readLe32(&bmpHeader[18]);
  int32_t bmpHeight = (int32_t)readLe32(&bmpHeader[22]);

  if (readLe16(&bmpHeader[26]) != 1) {
    Serial.println(F("BMP planes value is invalid"));
    bmpFile.close();
    return false;
  }

  uint16_t bitDepth = readLe16(&bmpHeader[28]);
  uint32_t compression = readLe32(&bmpHeader[30]);
  if (headerSize < 40 || bmpWidth <= 0 || bmpHeight == 0 || compression != 0 ||
      (bitDepth != BMP_SUPPORTED_DEPTH_24 && bitDepth != BMP_SUPPORTED_DEPTH_32)) {
    Serial.print(F("Unsupported BMP: header="));
    Serial.print(headerSize);
    Serial.print(F(" width="));
    Serial.print(bmpWidth);
    Serial.print(F(" height="));
    Serial.print(bmpHeight);
    Serial.print(F(" depth="));
    Serial.print(bitDepth);
    Serial.print(F(" compression="));
    Serial.println(compression);
    Serial.println(F("BMP must be uncompressed 24-bit or 32-bit format"));
    bmpFile.close();
    return false;
  }

  Serial.print(F("BMP header OK: width="));
  Serial.print(bmpWidth);
  Serial.print(F(" height="));
  Serial.print(bmpHeight);
  Serial.print(F(" depth="));
  Serial.print(bitDepth);
  Serial.print(F(" imageOffset="));
  Serial.println(imageOffset);

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

  uint8_t bytesPerPixel = bitDepth / 8;
  uint32_t rowSize = ((uint32_t)bmpWidth * bytesPerPixel + 3) & ~3;
  Serial.print(F("Drawing BMP clipped to "));
  Serial.print(drawWidth);
  Serial.print('x');
  Serial.print(drawHeight);
  Serial.print(F(" rowSize="));
  Serial.println(rowSize);
  uint8_t sdbuffer[4 * BMP_READ_BUFFER_PIXELS];
  uint16_t lcdbuffer[BMP_READ_BUFFER_PIXELS];

  for (int16_t row = 0; row < drawHeight; row++) {
    if ((row % BMP_PROGRESS_ROWS) == 0) {
      Serial.print(F("BMP draw row "));
      Serial.print(row);
      Serial.print('/');
      Serial.println(drawHeight);
    }

    uint32_t rowPosition = imageOffset + (flip ? (bmpHeight - 1 - row) : row) * rowSize;
    if (!bmpFile.seekSet(rowPosition)) {
      Serial.println(F("BMP row seek failed"));
      bmpFile.close();
      return false;
    }

    int16_t remaining = drawWidth;
    while (remaining > 0) {
      uint16_t pixelsToRead = remaining > BMP_READ_BUFFER_PIXELS ? BMP_READ_BUFFER_PIXELS : remaining;
      uint16_t bytesToRead = pixelsToRead * bytesPerPixel;
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
        if (bytesPerPixel == 4) {
          src++; // Ignore the BMP alpha/reserved byte.
        }
        lcdbuffer[i] = tft->color565(r, g, b);
      }

      int16_t chunkX = x + drawWidth - remaining;
      tft->draw16bitRGBBitmap(chunkX, y + row, lcdbuffer, pixelsToRead, 1);
      remaining -= pixelsToRead;
    }
  }

  bmpFile.close();
  Serial.println(F("BMP draw complete"));
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
#if defined(ARDUINO_M5STACK_CARDPUTER)
  Serial.print(F("LCD interface: M5Stack Cardputer built-in ST7789 SPI, SD_CS="));
#else
  Serial.print(F("LCD interface: UNO-style 8-bit parallel shield, SD_CS="));
#endif
  Serial.println(SD_CS);
#if !defined(ARDUINO_M5STACK_CARDPUTER)
  Serial.print(F("UNO TFT driver: "));
#if ARDUFRAME_TFT_DRIVER == ARDUFRAME_TFT_ILI9341
  Serial.println(F("ILI9341 240x320"));
#elif ARDUFRAME_TFT_DRIVER == ARDUFRAME_TFT_ILI9486
  Serial.println(F("ILI9486 320x480"));
#endif
  printUnoShieldIdentificationChecklist();
#endif

  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);

#if defined(ARDUINO_M5STACK_CARDPUTER)
  pinMode(LCD_BL, OUTPUT);
  digitalWrite(LCD_BL, HIGH);
  Serial.println(F("Initializing Cardputer ST7789 TFT over SPI..."));
#else
  Serial.println(F("Initializing UNO parallel TFT over Arduino_UNOPAR8..."));
  probeUnoTftReadRegisters();
#endif
  if (!tft->begin()) {
    Serial.println(F("TFT begin failed"));
    Serial.flush();
    while (true) {
      delay(1000);
    }
  }
  Serial.println(F("TFT initialized with rotation 1"));
  Serial.print(F("TFT drawable size: "));
  Serial.print(tft->width());
  Serial.print(F("x"));
  Serial.println(tft->height());
  showTftDiagnosticColorSequence();
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
