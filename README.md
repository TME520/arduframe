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

At every UNO-style startup, the sketch now runs a visible controller trial set before settling on the selected compile-time driver. Watch the LCD while Serial prints `TFT controller trial: ILI9341`, `ILI9486`, `ILI9488`, and `HX8357`: the first trial that shows red/green/blue stripes, diagonal lines, a center circle, and the driver label is the best controller candidate. Set `ARDUFRAME_TFT_CONTROLLER_TRIAL_SECONDS` to `0` to skip this startup trial once the display is identified, or increase it if you need more time to watch each candidate. If all trials stay white, the evidence points away from BMP or SD-card decoding and toward shield pin mapping, reset/control wiring, level compatibility, or a non-UNO-parallel shield.

If the controller is still unknown, the sketch also supports a compile-and-upload trial matrix for likely UNO parallel shield controllers. Change `ARDUFRAME_TFT_DRIVER` to one of these values, upload, and watch both the Serial log and the startup color sequence:

```cpp
#define ARDUFRAME_TFT_DRIVER ARDUFRAME_TFT_ILI9341  // common 2.8-inch 240x320 shields
#define ARDUFRAME_TFT_DRIVER ARDUFRAME_TFT_ILI9486  // common 3.5-inch 320x480 shields
#define ARDUFRAME_TFT_DRIVER ARDUFRAME_TFT_ILI9488  // alternate 320x480 controller
#define ARDUFRAME_TFT_DRIVER ARDUFRAME_TFT_HX8357   // alternate 320x480 controller
```


### MCUFRIEND_kbv fallback probe

If the main sketch reaches the SD/BMP log messages but the TFT stays white through every visible controller trial, try the standalone MCUFRIEND probe in `extras/mcufriend_probe/mcufriend_probe.ino`. Upload it with a classic AVR UNO/Mega-compatible board and the `MCUFRIEND_kbv` library installed. The probe prints `tft.readID()` and then forces common controller IDs while drawing a large red/green/blue test pattern for each ID. The first forced ID that changes the LCD from white is the controller family to use for further bring-up.

`MCUFRIEND_kbv` is useful for identifying many inexpensive UNO-style parallel shields, but it does not support every modern Arduino core. If it will not compile for an Arduino UNO R4, temporarily move the shield to an AVR UNO/Mega for this identification test, then bring the discovered controller/pin information back to the main sketch.

For hands-on testing, set `ARDUFRAME_TFT_DIAGNOSTIC_TRIAL_SECONDS` above `0` so the color pattern remains visible before the sketch moves on to SD-card and slideshow work:

```cpp
#define ARDUFRAME_TFT_DIAGNOSTIC_TRIAL_SECONDS 20
```

A good trial is one where the screen visibly changes through black, white, red, green, blue, and bars with the expected orientation and size. A white-only screen for one driver but visible colors for another strongly identifies the working controller family.

At startup, the sketch draws one second of red/green/blue/white color bars before initializing the SD card. If those bars remain white or invisible, the failure is in the TFT controller/pin initialization rather than BMP decoding or SD reading. The startup log prints the selected controller and drawable size so you can confirm the build, for example `UNO TFT driver: ILI9341 240x320`.

Before the normal graphics-library initialization, UNO-style builds also run a low-level TFT readback probe against the shield's 8-bit parallel bus. The log includes a reset pulse, the expected pin mapping, and a sweep of common display-controller registers such as `0x04`, `0x09`, `0xBF`, `0xD3`, `0xDA`, `0xDB`, `0xDC`, and `0xEF`. The probe runs twice, once with internal pullups and once with floating inputs, to separate likely controller replies from floating-bus noise. When the controller answers, the sketch prints direct, byte-swapped, and bit-reversed candidate 16-bit IDs with labels for common chips such as `ILI9341`, `ILI9486`, `ILI9488`, `ILI9325`, `ILI9328`, `HX8357`, `SSD1289`, and related clone IDs. If every read is `00` or `FF`, the LCD read line may be unavailable on that shield, the controller may not support those reads, or the shield may not actually match the UNO control/data pin mapping printed by the sketch.

After initialization, the visible TFT diagnostic is now longer than a single color-bar flash: the sketch fills the whole display black, white, red, green, and blue, then draws red/green/blue/white bars. It also draws a high-contrast diagnostic overlay with borders, diagonals, crosshairs, and a center circle over the startup pattern and over each successfully loaded BMP image. Set `ARDUFRAME_DRAW_DIAGNOSTIC_OVERLAY` to `0` after hardware bring-up if you want clean slideshow images. If the Serial Monitor continues into SD and BMP messages but none of these colors or overlay shapes appear, focus on the LCD controller type, reset/control pins, or shield compatibility rather than the SD card or BMP files.

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

## White-screen TFT identification

If the screen remains white through the startup color sequence, first identify the exact shield before changing SD-card or BMP settings. Power the Arduino off, remove the shield if needed, and record:

- Any model text printed on the shield PCB, such as `2.8 TFT LCD Shield`, `mcufriend.com`, or a version number.
- Any text on the LCD flex cable, especially controller-like names such as `ILI9341`, `ILI9325`, `ILI9328`, `ILI9486`, `HX8347`, `ST7781`, or `R61505`.
- Any pin labels on the shield edge, especially whether they match `RD=A0`, `WR=A1`, `RS=A2`, `CS=A3`, `RST=A4`, and `SD_SS=10`.
- The full Serial Monitor startup log, including the physical-ID checklist and read-register sweep.

The sketch prints this same checklist at startup on UNO-style builds so the Serial Monitor log captures exactly what markings are needed when diagnosing a white screen.

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

Use 24-bit BMP images, uncompressed 32-bit BMP images, or 32-bit BMP images with `BI_BITFIELDS` color masks sized to the TFT resolution: usually 320x240 pixels on a landscape UNO-style ILI9341 2.8-inch shield, 480x320 pixels on a landscape UNO-style ILI9486 3.5-inch shield, or 240x135 pixels on the M5Stack Cardputer built-in ST7789 display. The sketch configures display rotation `1`. If the Serial Monitor reports a 32-bit BMP with `header=124` and `compression=3`, that is a common BITMAPV5/`BI_BITFIELDS` export and is supported by reading the BMP color masks before drawing the pixels.

## Behavior

On startup, the Arduino initializes the serial port, TFT, and SD card. Open the Arduino IDE Serial Monitor at `115200` baud to see startup, TFT initialization, SD initialization, image load attempts, BMP open/header/draw progress, image load failures, and slideshow wraparound messages. The sketch tries conservative SD SPI speeds first (`4 MHz`, then `1 MHz`, then `10 MHz`) because some shield/card combinations can initialize at a high speed but stall during file reads.

It displays `slideshow001.bmp` for 10 seconds, then `slideshow002.bmp` for 10 seconds, continuing through `slideshow999.bmp`. After `slideshow999.bmp` has been displayed for 10 seconds, the slideshow loops back to `slideshow001.bmp`.

If an image cannot be loaded, the sketch shows the missing filename for 10 seconds and then continues to the next slide.
