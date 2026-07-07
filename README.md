# arduframe

A digital photo frame for an Arduino UNO R4 and a 2.8" Arduino TFT touch shield with an SD card reader.

## Hardware

This sketch targets an Arduino-compatible 2.8" UNO-style parallel TFT touch shield with an SD card reader. The LCD is driven with `GFX Library for Arduino` (`Arduino_GFX_Library`) using the shield's fixed 8-bit parallel pin mapping. This avoids the `MCU unsupported` compile error from `MCUFRIEND_kbv` on Arduino UNO R4 boards:

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

The LCD on this shield is not an SPI display, so `TFT_CS` and `TFT_DC` are not configurable sketch constants. The sketch uses `Arduino_UNOPAR8`, whose UNO-shield control/data pins are fixed by the library. The SD card still uses SPI and must use the `SD_CS` value near the top of `arduframe.ino`.

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

Use uncompressed 24-bit BMP images sized to the TFT resolution, usually 320x240 pixels because the sketch configures display rotation `1` in the `Arduino_ILI9341` constructor.

## Behavior

On startup, the Arduino initializes the serial port, TFT, and SD card. Open the Arduino IDE Serial Monitor at `115200` baud to see startup, TFT initialization, SD initialization, image load attempts, image load failures, and slideshow wraparound messages.

It displays `slideshow001.bmp` for 10 seconds, then `slideshow002.bmp` for 10 seconds, continuing through `slideshow999.bmp`. After `slideshow999.bmp` has been displayed for 10 seconds, the slideshow loops back to `slideshow001.bmp`.

If an image cannot be loaded, the sketch shows the missing filename for 10 seconds and then continues to the next slide.
