#include <Adafruit_GFX.h>
#include <MCUFRIEND_kbv.h>

// Standalone TFT bring-up sketch for classic Arduino UNO/Mega-compatible boards.
// MCUFRIEND_kbv does not support every modern core, including Arduino UNO R4,
// so use this on an AVR UNO/Nano/Mega when the main Arduino_GFX sketch leaves
// an UNO-style parallel TFT shield white.

MCUFRIEND_kbv tft;

const unsigned long SERIAL_BAUD = 115200UL;
const unsigned long SERIAL_WAIT_MS = 3000UL;
const uint16_t TRIAL_HOLD_MS = 3000;
const uint16_t UNKNOWN_ID = 0xD3D3;

const uint16_t FORCED_IDS[] = {
    0x9341, // ILI9341
    0x9325, // ILI9325
    0x9328, // ILI9328
    0x9486, // ILI9486
    0x9488, // ILI9488
    0x8357, // HX8357
    0x8347, // HX8347
    0x7783, // ST7783
    0x0154, // S6D0154
    0x1289, // SSD1289
    0x1520, // S6D0154/ILI9320 family clone
    0x1581, // R61581
};

void printHexWord(uint16_t value) {
  Serial.print(F("0x"));
  if (value < 0x1000) Serial.print('0');
  if (value < 0x0100) Serial.print('0');
  if (value < 0x0010) Serial.print('0');
  Serial.print(value, HEX);
}

void printControllerName(uint16_t id) {
  switch (id) {
    case 0x0154: Serial.print(F("S6D0154 / compatible")); break;
    case 0x1289: Serial.print(F("SSD1289")); break;
    case 0x1520: Serial.print(F("S6D0154/ILI9320 clone")); break;
    case 0x1581: Serial.print(F("R61581")); break;
    case 0x7783: Serial.print(F("ST7783")); break;
    case 0x8347: Serial.print(F("HX8347")); break;
    case 0x8357: Serial.print(F("HX8357")); break;
    case 0x9325: Serial.print(F("ILI9325")); break;
    case 0x9328: Serial.print(F("ILI9328")); break;
    case 0x9341: Serial.print(F("ILI9341")); break;
    case 0x9486: Serial.print(F("ILI9486")); break;
    case 0x9488: Serial.print(F("ILI9488")); break;
    default: Serial.print(F("unknown/clone")); break;
  }
}

void drawTrialPattern(uint16_t id, const __FlashStringHelper *label) {
  Serial.print(F("MCUFRIEND trial begin: "));
  printHexWord(id);
  Serial.print(F(" "));
  printControllerName(id);
  Serial.print(F(" "));
  Serial.println(label);

  tft.begin(id);
  tft.setRotation(1);

  int16_t width = tft.width();
  int16_t height = tft.height();
  Serial.print(F("  drawable size: "));
  Serial.print(width);
  Serial.print(F("x"));
  Serial.println(height);

  tft.fillScreen(0x0000);
  delay(150);
  tft.fillRect(0, 0, width / 3, height, 0xF800);
  tft.fillRect(width / 3, 0, width / 3, height, 0x07E0);
  tft.fillRect((width / 3) * 2, 0, width - ((width / 3) * 2), height, 0x001F);
  tft.drawRect(0, 0, width, height, 0xFFFF);
  tft.drawLine(0, 0, width - 1, height - 1, 0xFFE0);
  tft.drawLine(width - 1, 0, 0, height - 1, 0xF81F);
  tft.fillCircle(width / 2, height / 2, min(width, height) / 8, 0xFFFF);
  tft.setCursor(8, 8);
  tft.setTextColor(0xFFFF, 0x0000);
  tft.setTextSize(2);
  tft.print(F("ID "));
  tft.print(id, HEX);
  tft.setCursor(8, 32);
  tft.print(label);

  delay(TRIAL_HOLD_MS);
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  unsigned long serialStart = millis();
  while (!Serial && (millis() - serialStart < SERIAL_WAIT_MS)) {
    delay(10);
  }

  Serial.println(F("MCUFRIEND_kbv TFT probe starting"));
  Serial.println(F("Use this with a classic AVR UNO/Mega-compatible board."));
  Serial.println(F("If compiling for UNO R4 fails, use an AVR board for this probe."));
  Serial.println(F("Expected shield: UNO-style 8-bit parallel TFT shield."));
  Serial.println(F("Watch for the first trial that changes the screen from white."));

  uint16_t readId = tft.readID();
  Serial.print(F("MCUFRIEND_kbv readID(): "));
  printHexWord(readId);
  Serial.print(F(" "));
  printControllerName(readId);
  Serial.println();

  if (readId == 0x0000 || readId == 0xFFFF || readId == UNKNOWN_ID) {
    Serial.println(F("Read ID is not conclusive; trying common forced controller IDs."));
  } else {
    drawTrialPattern(readId, F("readID"));
  }

  for (uint8_t i = 0; i < sizeof(FORCED_IDS) / sizeof(FORCED_IDS[0]); i++) {
    drawTrialPattern(FORCED_IDS[i], F("forced"));
  }

  Serial.println(F("MCUFRIEND trial set complete."));
  Serial.println(F("If every trial stayed white, suspect pin mapping, reset/control wiring, level compatibility, or a non-UNO-parallel shield."));
}

void loop() {
}
