# arduframe

A digital photo frame for an Arduino UNO R4 and a 2.8" Arduino TFT touch shield with an SD card reader.

## Hardware

This sketch targets an Arduino-compatible 2.8" TFT touch shield using an ILI9341 display controller and an SD card reader. The default pin mapping matches the Adafruit 2.8" TFT Touch Shield for Arduino:

- TFT chip select: pin 10
- TFT data/command: pin 9
- SD chip select: pin 4

If your shield uses different pins, update `TFT_CS`, `TFT_DC`, and `SD_CS` near the top of `arduframe.ino`.

## Arduino libraries

Install these libraries with the Arduino IDE Library Manager:

- `Adafruit GFX Library`
- `Adafruit ILI9341`
- `Adafruit ImageReader Library`
- `SdFat - Adafruit Fork`

The sketch also uses the standard Arduino `SPI` library. The Adafruit ImageReader library requires the SdFat API, so the sketch intentionally uses `SdFat` instead of Arduino's built-in `SD` library.

## SD card image layout

Place BMP files in the root of the SD card using these exact names:

```text
slideshow001.bmp
slideshow002.bmp
...
slideshow999.bmp
```

For best results, use uncompressed BMP images sized to the TFT resolution, usually 320x240 pixels when the sketch uses `tft.setRotation(1)`.

## Behavior

On startup, the Arduino initializes the TFT and SD card. It displays `slideshow001.bmp` for 10 seconds, then `slideshow002.bmp` for 10 seconds, continuing through `slideshow999.bmp`. After `slideshow999.bmp` has been displayed for 10 seconds, the slideshow loops back to `slideshow001.bmp`.

If an image cannot be loaded, the sketch shows the missing filename for 10 seconds and then continues to the next slide.
