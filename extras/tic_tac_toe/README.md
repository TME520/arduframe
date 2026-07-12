# Tic-tac-toe touchscreen demo

This is an archived standalone MCUFRIEND/TouchScreen demo that previously lived in the repository root.

Arduino builds compile every `.ino`, `.cpp`, and `.c` file in the sketch root together. Keeping this demo's `main.cpp` in the root caused duplicate `setup()`, `loop()`, and `tft` definitions when building `arduframe.ino`. Leave these files in this subdirectory unless you are opening the tic-tac-toe demo as its own sketch.
