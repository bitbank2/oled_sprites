//
// oled_sprites
//
// Copyright (c) 2018 BitBank Software, Inc.
// Written by Larry Bank
// project started 6/23/2018
// bitbank@pobox.com
//
// This project is meant to create a tile and sprite system for the ATtiny85 similar to classic
// video game consoles and coin-op machines. The SSD1306 OLED is a 128x64 1-bpp display with
// 1K of internal display memory. The challenge is that the ATtiny85 only has 512 bytes of RAM
// and that prevents double-buffering the display to prevent flicker when drawing tiles and
// sprites with transparency. My solution is to draw the background tiles and sprites one
// "page" at a time. A 'page' to the SSD1306 is a 128 byte horizontal strip of pixels. The tiles
// are 8x8 pixels, so the display is divided into 16x8 (128) tiles. In order to support a scrolling
// background that is useful for gaming, 2 additional tiles need to be added on the edges to allow
// for redrawing them off screen. This brings the tile map size to 18x10 (180 bytes). This brings
// the RAM needed by this code to 128+180 = 308 bytes. This should leave enough left over to hold
// a list of sprites (3 bytes each) and other variables.
//
// To use this system, a game writer will manage the tile map and sprite list and then call the
// DrawPlayfield() function to draw everything. On my I2C OLED (chosen for lower cost and fewer
// connections), the code can redraw at > 30FPS and still have time left over for the game logic.
// When using the SPI version of the display, it will update even faster.
//
// Although this code was designed for the limits of the ATtiny85, there's nothing stopping it
// from being used on more powerful Arduinos and with other displays (e.g. Nokia 5110).
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//
// Chars are opaque colors
// Sprites have a transparency mask which is logical AND'd with the background
// image before being logical OR'd with the image data
// The mask is defined first in memory, then the image data
// The bits are oriented the same as the SSD1306 OLED display (LSB on top, vertical bytes)
//
typedef struct tag_gfx_object
{
  byte x;
  byte y;
  byte bType; // type and index (high bit set = 16x16, else 8x8), up to 128 unique sprites
} GFX_OBJECT;

static byte bPlayfield[18 * 10]; // 18x10 (16x8 + edges) scrolling playfield
static int iScrollX, iScrollY;

static GFX_OBJECT object_list[16]; // active objects (can be any number which fits in RAM)
//
// 8x8 sprites
// 8 bytes of mask followed by 8 bytes of pattern
//
const byte ucSprites[] PROGMEM = {0x00, 0x00, 0x3c, 0x3c, 0x3c, 0x3c, 0x00, 0x00,
                                  0x00, 0x7e, 0x42, 0x42, 0x42, 0x42, 0x7e, 0x00};
//
// 16x16 sprites
// 32 bytes of mask followed by 32 bytes of pattern
//
const byte ucBigSprites[] PROGMEM = {
0xff, 0xff, 0xfd, 0xf9, 0xf1, 0xe1, 0xc1, 0x81,
0x81, 0xc1, 0xe1, 0xf1, 0xf9, 0xfd, 0xff, 0xff,
0xff, 0xff, 0xbf, 0x9f, 0x8f, 0x87, 0x83, 0x81,
0x81, 0x83, 0x87, 0x8f, 0x9f, 0xbf, 0xff, 0xff,

0xff, 0x03, 0x05, 0x09, 0x11, 0x21, 0x41, 0x81,
0x81, 0x41, 0x21, 0x11, 0x09, 0x05, 0x03, 0xff,
0xff, 0xc0, 0xa0, 0x90, 0x88, 0x84, 0x82, 0x81,
0x81, 0x82, 0x84, 0x88, 0x90, 0xa0, 0xc0, 0xff};

// 8x8 font
const byte ucFont[]PROGMEM = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x06,0x5f,0x5f,0x06,0x00,0x00,
  0x00,0x07,0x07,0x00,0x07,0x07,0x00,0x00,0x14,0x7f,0x7f,0x14,0x7f,0x7f,0x14,0x00,
  0x24,0x2e,0x2a,0x6b,0x6b,0x3a,0x12,0x00,0x46,0x66,0x30,0x18,0x0c,0x66,0x62,0x00,
  0x30,0x7a,0x4f,0x5d,0x37,0x7a,0x48,0x00,0x00,0x04,0x07,0x03,0x00,0x00,0x00,0x00,
  0x00,0x1c,0x3e,0x63,0x41,0x00,0x00,0x00,0x00,0x41,0x63,0x3e,0x1c,0x00,0x00,0x00,
  0x08,0x2a,0x3e,0x1c,0x1c,0x3e,0x2a,0x08,0x00,0x08,0x08,0x3e,0x3e,0x08,0x08,0x00,
  0x00,0x00,0x80,0xe0,0x60,0x00,0x00,0x00,0x00,0x08,0x08,0x08,0x08,0x08,0x08,0x00,
  0x00,0x00,0x00,0x60,0x60,0x00,0x00,0x00,0x60,0x30,0x18,0x0c,0x06,0x03,0x01,0x00,
  0x3e,0x7f,0x59,0x4d,0x47,0x7f,0x3e,0x00,0x40,0x42,0x7f,0x7f,0x40,0x40,0x00,0x00,
  0x62,0x73,0x59,0x49,0x6f,0x66,0x00,0x00,0x22,0x63,0x49,0x49,0x7f,0x36,0x00,0x00,
  0x18,0x1c,0x16,0x53,0x7f,0x7f,0x50,0x00,0x27,0x67,0x45,0x45,0x7d,0x39,0x00,0x00,
  0x3c,0x7e,0x4b,0x49,0x79,0x30,0x00,0x00,0x03,0x03,0x71,0x79,0x0f,0x07,0x00,0x00,
  0x36,0x7f,0x49,0x49,0x7f,0x36,0x00,0x00,0x06,0x4f,0x49,0x69,0x3f,0x1e,0x00,0x00,
  0x00,0x00,0x00,0x66,0x66,0x00,0x00,0x00,0x00,0x00,0x80,0xe6,0x66,0x00,0x00,0x00,
  0x08,0x1c,0x36,0x63,0x41,0x00,0x00,0x00,0x00,0x14,0x14,0x14,0x14,0x14,0x14,0x00,
  0x00,0x41,0x63,0x36,0x1c,0x08,0x00,0x00,0x00,0x02,0x03,0x59,0x5d,0x07,0x02,0x00,
  0x3e,0x7f,0x41,0x5d,0x5d,0x5f,0x0e,0x00,0x7c,0x7e,0x13,0x13,0x7e,0x7c,0x00,0x00,
  0x41,0x7f,0x7f,0x49,0x49,0x7f,0x36,0x00,0x1c,0x3e,0x63,0x41,0x41,0x63,0x22,0x00,
  0x41,0x7f,0x7f,0x41,0x63,0x3e,0x1c,0x00,0x41,0x7f,0x7f,0x49,0x5d,0x41,0x63,0x00,
  0x41,0x7f,0x7f,0x49,0x1d,0x01,0x03,0x00,0x1c,0x3e,0x63,0x41,0x51,0x33,0x72,0x00,
  0x7f,0x7f,0x08,0x08,0x7f,0x7f,0x00,0x00,0x00,0x41,0x7f,0x7f,0x41,0x00,0x00,0x00,
  0x30,0x70,0x40,0x41,0x7f,0x3f,0x01,0x00,0x41,0x7f,0x7f,0x08,0x1c,0x77,0x63,0x00,
  0x41,0x7f,0x7f,0x41,0x40,0x60,0x70,0x00,0x7f,0x7f,0x0e,0x1c,0x0e,0x7f,0x7f,0x00,
  0x7f,0x7f,0x06,0x0c,0x18,0x7f,0x7f,0x00,0x1c,0x3e,0x63,0x41,0x63,0x3e,0x1c,0x00,
  0x41,0x7f,0x7f,0x49,0x09,0x0f,0x06,0x00,0x1e,0x3f,0x21,0x31,0x61,0x7f,0x5e,0x00,
  0x41,0x7f,0x7f,0x09,0x19,0x7f,0x66,0x00,0x26,0x6f,0x4d,0x49,0x59,0x73,0x32,0x00,
  0x03,0x41,0x7f,0x7f,0x41,0x03,0x00,0x00,0x7f,0x7f,0x40,0x40,0x7f,0x7f,0x00,0x00,
  0x1f,0x3f,0x60,0x60,0x3f,0x1f,0x00,0x00,0x3f,0x7f,0x60,0x30,0x60,0x7f,0x3f,0x00,
  0x63,0x77,0x1c,0x08,0x1c,0x77,0x63,0x00,0x07,0x4f,0x78,0x78,0x4f,0x07,0x00,0x00,
  0x47,0x63,0x71,0x59,0x4d,0x67,0x73,0x00,0x00,0x7f,0x7f,0x41,0x41,0x00,0x00,0x00,
  0x01,0x03,0x06,0x0c,0x18,0x30,0x60,0x00,0x00,0x41,0x41,0x7f,0x7f,0x00,0x00,0x00,
  0x08,0x0c,0x06,0x03,0x06,0x0c,0x08,0x00,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,
  0x00,0x00,0x03,0x07,0x04,0x00,0x00,0x00,0x20,0x74,0x54,0x54,0x3c,0x78,0x40,0x00,
  0x41,0x7f,0x3f,0x48,0x48,0x78,0x30,0x00,0x38,0x7c,0x44,0x44,0x6c,0x28,0x00,0x00,
  0x30,0x78,0x48,0x49,0x3f,0x7f,0x40,0x00,0x38,0x7c,0x54,0x54,0x5c,0x18,0x00,0x00,
  0x48,0x7e,0x7f,0x49,0x03,0x06,0x00,0x00,0x98,0xbc,0xa4,0xa4,0xf8,0x7c,0x04,0x00,
  0x41,0x7f,0x7f,0x08,0x04,0x7c,0x78,0x00,0x00,0x44,0x7d,0x7d,0x40,0x00,0x00,0x00,
  0x60,0xe0,0x80,0x84,0xfd,0x7d,0x00,0x00,0x41,0x7f,0x7f,0x10,0x38,0x6c,0x44,0x00,
  0x00,0x41,0x7f,0x7f,0x40,0x00,0x00,0x00,0x7c,0x7c,0x18,0x78,0x1c,0x7c,0x78,0x00,
  0x7c,0x78,0x04,0x04,0x7c,0x78,0x00,0x00,0x38,0x7c,0x44,0x44,0x7c,0x38,0x00,0x00,
  0x84,0xfc,0xf8,0xa4,0x24,0x3c,0x18,0x00,0x18,0x3c,0x24,0xa4,0xf8,0xfc,0x84,0x00,
  0x44,0x7c,0x78,0x4c,0x04,0x0c,0x18,0x00,0x48,0x5c,0x54,0x74,0x64,0x24,0x00,0x00,
  0x04,0x04,0x3e,0x7f,0x44,0x24,0x00,0x00,0x3c,0x7c,0x40,0x40,0x3c,0x7c,0x40,0x00,
  0x1c,0x3c,0x60,0x60,0x3c,0x1c,0x00,0x00,0x3c,0x7c,0x60,0x30,0x60,0x7c,0x3c,0x00,
  0x44,0x6c,0x38,0x10,0x38,0x6c,0x44,0x00,0x9c,0xbc,0xa0,0xa0,0xfc,0x7c,0x00,0x00,
  0x4c,0x64,0x74,0x5c,0x4c,0x64,0x00,0x00,0x08,0x08,0x3e,0x77,0x41,0x41,0x00,0x00,
  0x00,0x00,0x00,0x77,0x77,0x00,0x00,0x00,0x41,0x41,0x77,0x3e,0x08,0x08,0x00,0x00,
  0x02,0x03,0x01,0x03,0x02,0x03,0x01,0x00,0x70,0x78,0x4c,0x46,0x4c,0x78,0x70,0x00};


// some globals
static int iScreenOffset; // current write offset of screen data
static byte oled_addr;
static void oledWriteCommand(unsigned char c);

#define DIRECT_PORT
#define I2CPORT PORTB
#define SAFE_DELAY 0
// A bit set to 1 in the DDR is an output, 0 is an INPUT
#define I2CDDR DDRB

// Pin or port numbers for SDA and SCL
#define BB_SDA 0
#define BB_SCL 2

//
// Transmit a byte and ack bit
//
static inline void i2cByteOut(byte b)
{
byte i;
byte bOld = I2CPORT & ~((1 << BB_SDA) | (1 << BB_SCL));
     for (i=0; i<8; i++)
     {
         bOld &= ~(1 << BB_SDA);
         if (b & 0x80)
            bOld |= (1 << BB_SDA);
         I2CPORT = bOld;
         delayMicroseconds(SAFE_DELAY);
         I2CPORT |= (1 << BB_SCL);
         delayMicroseconds(SAFE_DELAY);
         I2CPORT = bOld;
         b <<= 1;
     } // for i
// ack bit
  I2CPORT = bOld & ~(1 << BB_SDA); // set data low
        delayMicroseconds(SAFE_DELAY);
  I2CPORT |= (1 << BB_SCL); // toggle clock
         delayMicroseconds(SAFE_DELAY);
  I2CPORT = bOld;
} /* i2cByteOut() */

void i2cBegin(byte addr)
{
   I2CPORT |= ((1 << BB_SDA) + (1 << BB_SCL));
   I2CDDR |= ((1 << BB_SDA) + (1 << BB_SCL));
   I2CPORT &= ~(1 << BB_SDA); // data line low first
   delayMicroseconds(SAFE_DELAY);
   I2CPORT &= ~(1 << BB_SCL); // then clock line low is a START signal
   i2cByteOut(addr << 1); // send the slave address
} /* i2cBegin() */

void i2cWrite(byte *pData, byte bLen)
{
byte i, b;
byte bOld = I2CPORT & ~((1 << BB_SDA) | (1 << BB_SCL));

   while (bLen--)
   {
      b = *pData++;
 //     i2cByteOut(b);
 //#ifdef FUTURE
      if (b == 0 || b == 0xff) // special case can save time
      {
         bOld &= ~(1 << BB_SDA);
         if (b & 0x80)
            bOld |= (1 << BB_SDA);
         I2CPORT = bOld;
         for (i=0; i<8; i++)
         {
            I2CPORT |= (1 << BB_SCL); // just toggle SCL, SDA stays the same
            delayMicroseconds(SAFE_DELAY);
            I2CPORT = bOld;
         } // for i    
     }
     else // normal byte needs every bit tested
     {
        for (i=0; i<8; i++)
        {
          
         bOld &= ~(1 << BB_SDA);
         if (b & 0x80)
            bOld |= (1 << BB_SDA);

         I2CPORT = bOld;
         delayMicroseconds(SAFE_DELAY);
         I2CPORT |= (1 << BB_SCL);
         delayMicroseconds(SAFE_DELAY);
         I2CPORT = bOld;
         b <<= 1;
        } // for i
     }
// ACK bit seems to need to be set to 0, but SDA line doesn't need to be tri-state
      I2CPORT &= ~(1 << BB_SDA);
//      delayMicroseconds(SAFE_DELAY);
      I2CPORT |= (1 << BB_SCL); // toggle clock
      delayMicroseconds(SAFE_DELAY);
      I2CPORT &= ~(1 << BB_SCL);
//#endif
   } // for each byte
} /* i2cWrite() */

//
// Send I2C STOP condition
//
void i2cEnd()
{
  I2CPORT &= ~(1 << BB_SDA);
  I2CPORT |= (1 << BB_SCL);
  I2CPORT |= (1 << BB_SDA);
  I2CDDR &= ((1 << BB_SDA) | (1 << BB_SCL)); // let the lines float (tri-state)
} /* i2cEnd() */

// Wrapper function to write I2C data on Arduino
static void I2CWrite(unsigned char *pData, int iLen)
{
  i2cBegin(oled_addr);
  i2cWrite(pData, iLen);
  i2cEnd();
} /* I2CWrite() */

static void I2CWriteData(unsigned char *pData, int iLen)
{
  i2cBegin(oled_addr);
  i2cByteOut(0x40);
  i2cWrite(pData, iLen);
  i2cEnd();
}
//
// Initializes the OLED controller into "page mode"
//
void oledInit(byte bAddr, int bFlip, int bInvert)
{
unsigned char uc[4];
unsigned char oled_initbuf[]={0x00,0xae,0xa8,0x3f,0xd3,0x00,0x40,0xa1,0xc8,
      0xda,0x12,0x81,0xff,0xa4,0xa6,0xd5,0x80,0x8d,0x14,
      0xaf,0x20,0x02};

  oled_addr = bAddr;
  I2CDDR &= ~(1 << BB_SDA);
  I2CDDR &= ~(1 << BB_SCL); // let them float high
  I2CPORT |= (1 << BB_SDA); // set both lines to get pulled up
  I2CPORT |= (1 << BB_SCL);
  
  I2CWrite(oled_initbuf, sizeof(oled_initbuf));
  if (bInvert)
  {
    uc[0] = 0; // command
    uc[1] = 0xa7; // invert command
    I2CWrite(uc, 2);
  }
  if (bFlip) // rotate display 180
  {
    uc[0] = 0; // command
    uc[1] = 0xa0;
    I2CWrite(uc, 2);
    uc[1] = 0xc0;
    I2CWrite(uc, 2);
  }
} /* oledInit() */
//
// Sends a command to turn off the OLED display
//
void oledShutdown()
{
    oledWriteCommand(0xaE); // turn off OLED
}

// Send a single byte command to the OLED controller
static void oledWriteCommand(unsigned char c)
{
unsigned char buf[2];

  buf[0] = 0x00; // command introducer
  buf[1] = c;
  I2CWrite(buf, 2);
} /* oledWriteCommand() */

static void oledWriteCommand2(unsigned char c, unsigned char d)
{
unsigned char buf[3];

  buf[0] = 0x00;
  buf[1] = c;
  buf[2] = d;
  I2CWrite(buf, 3);
} /* oledWriteCommand2() */

//
// Sets the brightness (0=off, 255=brightest)
//
void oledSetContrast(unsigned char ucContrast)
{
  oledWriteCommand2(0x81, ucContrast);
} /* oledSetContrast() */

//
// Send commands to position the "cursor" (aka memory write address)
// to the given row and column
//
static void oledSetPosition(int x, int y)
{
  oledWriteCommand(0xb0 | y); // go to page Y
  oledWriteCommand(0x00 | (x & 0xf)); // // lower col addr
  oledWriteCommand(0x10 | ((x >> 4) & 0xf)); // upper col addr
  iScreenOffset = (y*128)+x;
}

//
// Fill the frame buffer with a byte pattern
// e.g. all off (0x00) or all on (0xff)
//
void oledFill(unsigned char ucData)
{
int x, y;
unsigned char temp[16];

  memset(temp, ucData, 16);
  for (y=0; y<8; y++)
  {
    oledSetPosition(0,y); // set to (0,Y)
    for (x=0; x<8; x++)
    {
      I2CWriteData(temp, 16); 
    } // for x
  } // for y
} /* oledFill() */

//
// Draw 1 character space that's vertically shifted
void DrawShiftedChar(byte *s1, byte *s2, byte *d, byte bXOff, byte bYOff)
{
byte c, c2, z;

  for (z=0; z< (8-bXOff); z++)
  {
     c = pgm_read_byte(s1++);
     c >>= bYOff; // shift over
     c2 = pgm_read_byte(s2++);
     c2 <<= (8 - bYOff);
     *d++ = (c | c2);            
  } // for z
} /* DrawShiftedChar() */
//
// Draw the sprites visible on the current line
//
void DrawSprites(byte y, byte *pBuf, GFX_OBJECT *pList, byte bCount)
{
byte i, x, bSize, bSprite, *s, *d;
byte cOld, cNew, mask, bYOff, bWidth;

GFX_OBJECT *pObject;
   for (i=0; i<bCount; i++)
   {
      pObject = &pList[i];
      bSprite = pObject->bType; // index
      bSize = (bSprite & 0x80) ? 16 : 8; // big or small sprite
      // see if it's visible
      if (pObject->y >= y+8) // past bottom
         continue;
      if (pObject->y + bSize <= y) // above top
         continue;
      if (pObject->x >= 128) // off right edge
         continue;
      // It's visible on this line; draw it
      bSprite &= 0x7f; // sprite index
      d = &pBuf[pObject->x]; // destination pointer
      if (bSize == 16)
      {
         s = (byte *)&ucBigSprites[bSprite * 64];
         if (pObject->y+8 <= y) // special case - only bottom half drawn
            s += 16;
         bYOff = pObject->y & 7;
         bWidth = 16;
         if (128 - pObject->x < 16)
            bWidth = 128 - pObject->x;
         // 4 possible cases:
         // byte aligned - single source, not shifted
         // single source (shifted, top or bottom row)
         // double source (middle) both need shifting
         if (bYOff == 0) // simplest case
         {
            for (x=0; x<bWidth; x++)
            {
               cOld = d[0];
               mask = pgm_read_byte(s);
               cNew = pgm_read_byte(s+32);
               s++;          
               cOld &= mask;
               cOld |= cNew;
               *d++ = cOld;            
            }
         }
         else if (pObject->y+8 < y) // only bottom half of sprite drawn
         {
            for (x=0; x<bWidth; x++)
            {
               mask = pgm_read_byte(s);
               cNew = pgm_read_byte(s+32);
               s++;
               mask >>= (8-bYOff);
               mask |= (0xff << bYOff);
               cNew >>= (8-bYOff);
               cOld = d[0];
               cOld &= mask;
               cOld |= cNew;
               *d++ = cOld;            
            } // for x
         }
         else if (pObject->y > y) // only top half of sprite drawn
         {
            for (x=0; x<bWidth; x++)
            {
               mask = pgm_read_byte(s);
               cNew = pgm_read_byte(s+32);
               s++;
               mask <<= bYOff;
               mask |= (0xff >> (8-bYOff)); // exposed bits set to 1
               cNew <<= bYOff;
               cOld = d[0];
               cOld &= mask;
               cOld |= cNew;
               *d++ = cOld;            
            } // for x
         }
         else // most difficult - part of top and bottom drawn together
         {
         byte mask2, cNew2;
            for (x=0; x<bWidth; x++)
            {
               mask = pgm_read_byte(s);
               mask2 = pgm_read_byte(s+16);
               cNew = pgm_read_byte(s+32);
               cNew2 = pgm_read_byte(s+48);
               s++;
               mask >>= (8-bYOff);
               cNew >>= (8-bYOff);
               mask2 <<= bYOff;
               cNew2 <<= bYOff;
               mask |= mask2; // combine top and bottom
               cNew |= cNew2;
               cOld = d[0];
               cOld &= mask;
               cOld |= cNew;
               *d++ = cOld;            
            } // for x
         }
      }
      else // 8x8 sprite
      {
         s = (byte *)&ucSprites[bSprite * 16];
         bYOff = pObject->y & 7;
         bWidth = 8;
         if (128 - pObject->x < 8)
            bWidth = 128 - pObject->x;
         for (x=0; x<bWidth; x++)
         {
            mask = pgm_read_byte(s);
            cNew = pgm_read_byte(s+8);
            s++;
            if (bYOff) // needs to be shifted
            {
               if (pObject->y > y)
               {
                  mask <<= bYOff;
                  mask |= (0xff >> (8-bYOff)); // exposed bits set to 1
                  cNew <<= bYOff;
               }
               else
               {
                  mask >>= (8-bYOff);
                  mask |= (0xff << bYOff);
                  cNew >>= (8-bYOff);
               }
            } // needs to be shifted 
            cOld = d[0];
            cOld &= mask;
            cOld |= cNew;
            *d++ = cOld;            
         } // for x
      }
   } // for each sprite
} /* DrawSprites() */
//
// Draw the playfield and sprites
//
void DrawPlayfield(byte bScrollX, byte bScrollY)
{
byte bTemp[128]; // holds data for the current scan line
byte x, y, tx, ty;
byte c, *s, *sNext, *d, bMask, bXOff, bYOff;
int iOffset, iOffset2;

  bXOff = bScrollX & 7;
  bYOff = bScrollY & 7;
  
  ty = bScrollY >> 3;
  for (y=0; y<8; y++) // draw the 8 lines of 128 bytes
  {
     memset(bTemp, 0, sizeof(bTemp));
     tx = bScrollX >> 3;
     if (ty >= 10) ty -= 10; // wrap around
     // Draw the playfield characters at the given scroll position
     d = bTemp;
     if (bYOff) // partial characters vertically means a lot more work :(
     {
        for (x=0; x<16; x++)
        {
           if (tx >= 18) tx -= 18; // wrap around
           iOffset = tx + ty*18;
           iOffset2 = iOffset + 18; // next line
           if (iOffset2 >= 180) // past bottom
              iOffset2 -= 180;
           c = bPlayfield[iOffset];
           s = (byte *)&ucFont[(c * 8) + bXOff];
           c = bPlayfield[iOffset2];
           sNext = (byte *)&ucFont[(c * 8) + bXOff];
           DrawShiftedChar(s, sNext, d, bXOff, bYOff);
           d += (8-bXOff);
           bXOff = 0;
           tx++;
        } // for x
        if (d != &bTemp[128]) // partial character left to draw
        {
           bXOff = (byte)(&bTemp[128] - d);
           if (tx >= 18) tx -= 18;
           iOffset = tx + ty*18;
           iOffset2 = iOffset + 18; // next line
           if (iOffset2 >= 180) // past bottom
              iOffset2 -= 180;
           c = bPlayfield[iOffset];
           s = (byte *)&ucFont[c * 8];
           c = bPlayfield[iOffset2];
           sNext = (byte *)&ucFont[c * 8];
           DrawShiftedChar(s, sNext, d, 8-bXOff, bYOff);
        }      
     } // complex case
     else // simpler case of vertical offset of 0 for each character
     {
        for (x=0; x<16; x++)
        {
           if (tx >= 18) tx -= 18; // wrap around
           iOffset = tx + ty*18;
           c = bPlayfield[iOffset];
           s = (byte *)&ucFont[(c * 8) + bXOff];
           memcpy_P(d, s, 8 - bXOff);
           d += (8 - bXOff);
           bXOff = 0;
           tx++;
        } // for x
        if (d != &bTemp[128]) // partial character left to draw
        {
           bXOff = (byte)(&bTemp[128] - d);
           if (tx >= 18) tx -= 18;
           iOffset = tx + ty*18;
           c = bPlayfield[iOffset];
           s = (byte *)&ucFont[c * 8];
           memcpy_P(d, s, bXOff);
        }
     } // simpler case
     DrawSprites(y*8, bTemp, object_list, 16);
     // Send it to the display
     oledSetPosition(0, y);
     I2CWriteData(bTemp, 128);
     ty++;
  } // for y

} /* DrawPlayfield() */

void setup() {
  byte x, y, c, *d;
  // put your setup code here, to run once:
  delay(50); // wait for the OLED to fully power up
  oledInit(0x3c, 0, 0);
  oledFill(0);
  for (y=0; y<10; y++)
  {
    d = &bPlayfield[y * 18];
    c = 'A'-32;
    for (x=0; x<18; x++)
    {
       *d++ = c++;
    }
  }
  memset(object_list, 0, sizeof(object_list));
  for (y=0; y<16; y++) // starting positions of the objects
  {
     object_list[y].x = (y & 3) * 24;
     object_list[y].y = (y & 0xc) * 3;
  }
  object_list[15].bType = 0x80; // big sprite
  object_list[15].y = 36;
  iScrollX = iScrollY = 0;
}

void loop() {
  // put your main code here, to run repeatedly:
int dx, dy;

   dx = dy = 1;
   while (1)
   {
     DrawPlayfield(iScrollX, iScrollY);
     iScrollX++; iScrollY++;
     if (iScrollX >= 144) iScrollX = 0;
     if (iScrollY >= 80) iScrollY = 0;
     object_list[15].x+= dx;
     object_list[15].y+= dy;
     if (object_list[15].x == 0 || object_list[15].x == 111) // bounce off L/R sides
        dx = -dx;
     if (object_list[15].y == 0 || object_list[15].y == 47) // bounce off T/B sides
        dy = -dy;
  }   
//     delay(2000);
}
