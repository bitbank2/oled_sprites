oled_sprites

Copyright (c) 2018 BitBank Software, Inc.
Written by Larry Bank
project started 6/23/2018
bitbank@pobox.com

This project is meant to create a tile and sprite system for the ATtiny85 similar to classic
video game consoles and coin-op machines. The SSD1306 OLED is a 128x64 1-bpp display with
1K of internal display memory. The challenge is that the ATtiny85 only has 512 bytes of RAM
and that prevents double-buffering the display to prevent flicker when drawing tiles and
sprites with transparency. My solution is to draw the background tiles and sprites one
"page" at a time. A 'page' to the SSD1306 is a 128 byte horizontal strip of pixels. The tiles
are 8x8 pixels, so the display is divided into 16x8 (128) tiles. In order to support a scrolling
background that is useful for gaming, 2 additional tiles need to be added on the edges to allow
for redrawing them off screen. This brings the tile map size to 18x10 (180 bytes). This brings
the RAM needed by this code to 128+180 = 308 bytes. This should leave enough left over to hold
a list of sprites (3 bytes each) and other variables.

To use this system, a game writer will manage the tile map and sprite list and then call the
DrawPlayfield() function to draw everything. On my I2C OLED (chosen for lower cost and fewer
connections), the code can redraw at > 30FPS and still have time left over for the game logic.
When using the SPI version of the display, it will update even faster.

Although this code was designed for the limits of the ATtiny85, there's nothing stopping it
from being used on more powerful Arduinos and with other displays (e.g. Nokia 5110).

