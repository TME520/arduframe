# arduframe

A digital photo frame for an Arduino UNO R4 with an Arduino UNO-style parallel TFT shield, or an M5Stack Cardputer using its built-in display and microSD slot.

## Hardware

### Arduino UNO R4 with UNO-style TFT shield

This sketch supports Arduino-compatible UNO-style parallel TFT touch shields with an SD card reader. The LCD is driven with `GFX Library for Arduino` (`Arduino_GFX_Library`) using the shield's fixed 8-bit parallel pin mapping. This avoids the `MCU unsupported` compile error from `MCUFRIEND_kbv` on Arduino UNO R4 boards:

- `LCD_RST`: A4
- `LCD_CS`: A3
- `LCD_RS` / `LCD_CD`: A2
- `LCD_WR`: A1
- `LCD_RD`: A0
- `LCD_D0`: pin 8
- `LCD_D1`: pin 9
- `LCD_D2`: pin 2
- `LCD_D3`: pin 3
- `LCD_D4`: pin 4
- `LCD_D5`: pin 5
- `LCD_D6`: pin 6
- `LCD_D7`: pin 7
- SD chip select / `SD_SS`: pin 10
- SD SPI data/clock: pins 11, 12, and 13

The default UNO-style display controller is `ILI9341`, because brandless AliExpress shields marked `2.8 TFT LCD Shield` with this exact pinout are normally 2.8-inch 240x320 panels. If Serial says images are displayed successfully but the screen stays white, the LCD backlight is powered but the controller probably was initialized with the wrong driver. For a larger 3.5-inch 320x480 ILI9486 shield, change the default near the top of `arduframe.ino` to:

```cpp
#define ARDUFRAME_TFT_DRIVER ARDUFRAME_TFT_ILI9486
```

At startup, the sketch draws one second of red/green/blue/white color bars before initializing the SD card. If those bars remain white or invisible, the failure is in the TFT controller/pin initialization rather than BMP decoding or SD reading. The startup log prints the selected controller and drawable size so you can confirm the build, for example `UNO TFT driver: ILI9341 240x320`.

Before the normal graphics-library initialization, UNO-style builds also run a low-level TFT readback probe against the shield's 8-bit parallel bus. The log includes a reset pulse, the expected pin mapping, and a sweep of common display-controller registers such as `0x04`, `0x09`, `0xBF`, `0xD3`, `0xDA`, `0xDB`, `0xDC`, and `0xEF`. When the controller answers, the sketch prints candidate 16-bit IDs and labels for common chips such as `ILI9341`, `ILI9486`, `ILI9325`, `ILI9328`, `HX8357`, `SSD1289`, and related clone IDs. If every read is `00` or `FF`, the LCD read line may be unavailable on that shield, the controller may not support those reads, or the shield may not actually match the UNO control/data pin mapping printed by the sketch.

After initialization, the visible TFT diagnostic is now longer than a single color-bar flash: the sketch fills the whole display black, white, red, green, and blue, then draws red/green/blue/white bars. If the Serial Monitor continues into SD and BMP messages but none of these colors appear, focus on the LCD controller type, reset/control pins, or shield compatibility rather than the SD card or BMP files.

The LCD on this shield is not an SPI display, so `TFT_CS` and `TFT_DC` are not configurable sketch constants. On UNO-style builds the sketch uses `Arduino_UNOPAR8`, whose UNO-shield control/data pins are fixed by the library. The SD card still uses SPI and must use the `SD_CS` value near the top of `arduframe.ino`.

### M5Stack Cardputer

When compiled for `ARDUINO_M5STACK_CARDPUTER`, the sketch uses the Cardputer built-in SPI ST7789 display instead of `Arduino_UNOPAR8`, which is not available for ESP32-S3 boards. The Cardputer pin mapping is:

- LCD reset: GPIO 33
- LCD data/command: GPIO 34
- LCD MOSI: GPIO 35
- LCD SCK: GPIO 36
- LCD chip select: GPIO 37
- LCD backlight: GPIO 38
- microSD chip select: GPIO 12
- microSD MOSI: GPIO 14
- microSD MISO: GPIO 39
- microSD SCK: GPIO 40

## Arduino libraries

Install these libraries with **Sketch > Include Library > Manage Libraries...** in the Arduino IDE:

- `Adafruit GFX Library`
- `GFX Library for Arduino`
- `SdFat - Adafruit Fork`

Arduino IDE 1.8.x does **not** read `sketch.yaml` and will not install those dependencies automatically. If compilation stops with `Arduino_GFX_Library.h: No such file or directory` or the sketch's `Missing Arduino_GFX_Library.h` error, install the exact Library Manager package named `GFX Library for Arduino`, then compile again.

If you already installed it and the same error remains, verify that the IDE is using the same sketchbook location where the library was installed:

1. Open **File > Preferences**.
2. Check **Sketchbook location**.
3. Confirm that the library folder exists under that location's `libraries` directory.
4. Restart the Arduino IDE and compile again.

The sketch also includes `sketch.yaml` build profiles for Arduino CLI users. These profiles record the UNO R4 board package and the required libraries, so dependencies can be installed and reused with a profile-based build:

```sh
arduino-cli lib update-index
arduino-cli core update-index
arduino-cli compile --profile uno-r4-minima .
# or, for an UNO R4 WiFi:
arduino-cli compile --profile uno-r4-wifi .
# or, for an M5Stack Cardputer:
arduino-cli compile --profile m5stack-cardputer .
```

The sketch also uses the standard Arduino `SPI` library. BMP files are decoded directly from `SdFat`, so the Adafruit ImageReader library is not required.

## SD card startup troubleshooting

During startup, the sketch retries SD startup at 10 MHz, 4 MHz, and 1 MHz. This helps with SD cards that are unreliable at the first clock speed.

If startup still stops at `SD init failed`, check the Serial Monitor for the attempted speeds, then verify that:

- The SD card is fully inserted.
- The SD card is formatted as FAT16 or FAT32.
- Your shield's SD chip-select pin matches `SD_CS` in `arduframe.ino`; the default is pin 10 for sockets labeled `SD_SS 10`.

## SD card image layout

Place BMP files in the root of the SD card using these exact names:

```text
slideshow001.bmp
slideshow002.bmp
...
slideshow999.bmp
```

Use uncompressed 24-bit or 32-bit BMP images sized to the TFT resolution: usually 320x240 pixels on a landscape UNO-style ILI9341 2.8-inch shield, 480x320 pixels on a landscape UNO-style ILI9486 3.5-inch shield, or 240x135 pixels on the M5Stack Cardputer built-in ST7789 display. The sketch configures display rotation `1`.

## Behavior

On startup, the Arduino initializes the serial port, TFT, and SD card. Open the Arduino IDE Serial Monitor at `115200` baud to see startup, TFT initialization, SD initialization, image load attempts, image load failures, and slideshow wraparound messages.

It displays `slideshow001.bmp` for 10 seconds, then `slideshow002.bmp` for 10 seconds, continuing through `slideshow999.bmp`. After `slideshow999.bmp` has been displayed for 10 seconds, the slideshow loops back to `slideshow001.bmp`.

If an image cannot be loaded, the sketch shows the missing filename for 10 seconds and then continues to the next slide.
