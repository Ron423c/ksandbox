/*
 * ssd1306 oled display driver
 *
 * 7-bit I2C slave addresses:
 *  0x3c (ADDR)
  *
 */

#include <linux/init.h>           // Macros used to mark up functions e.g. __init __exit
#include <linux/module.h>         // Core header for loading LKMs into the kernel
#include <linux/device.h>         // Header to support the kernel Driver Model
#include <linux/kernel.h>         // Contains types, macros, functions for the kernel
#include <linux/fs.h>             // Header for the Linux file system support
#include <asm/uaccess.h>          // Required for the copy to user function
#include <linux/timer.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <linux/time.h>

#include <linux/kthread.h>
#include <linux/semaphore.h>
#include <linux/workqueue.h>

#include <linux/i2c.h>
#include <linux/i2c-dev.h>

static int TURN_OFF_DISPLAY = 1;
module_param_named( O, TURN_OFF_DISPLAY, int, 0);

static int offset_number = 600;
module_param_named( on, offset_number, int, 0);

static int mdelay = 10;
module_param_named( md, mdelay, int, 0);


#define DEVICE_NAME "ssd1306"
#define CLASS_NAME  "oled_display" 
#define BUS_NAME    "i2c_1"

#define SSD1306_WIDTH 128
#define SSD1306_HEIGHT 64

#define WRITECOMMAND  0x00 // 
#define WRITEDATA     0x40 // 

#define SYM_WIDTH	18 	//column
#define SYM_HEIGHT	3	//bytes = 3*8 rows

/* register def
#define SSD1306_LCDWIDTH      128
#define SSD1306_LCDHEIGHT      64
#define SSD1306_SETCONTRAST   0x81
#define SSD1306_DISPLAYALLON_RESUME 0xA4
#define SSD1306_DISPLAYALLON 0xA5
#define SSD1306_NORMALDISPLAY 0xA6
#define SSD1306_INVERTDISPLAY 0xA7
#define SSD1306_DISPLAYOFF 0xAE
#define SSD1306_DISPLAYON 0xAF
#define SSD1306_SETDISPLAYOFFSET 0xD3
#define SSD1306_SETCOMPINS 0xDA
#define SSD1306_SETVCOMDETECT 0xDB
#define SSD1306_SETDISPLAYCLOCKDIV 0xD5
#define SSD1306_SETPRECHARGE 0xD9
#define SSD1306_SETMULTIPLEX 0xA8
#define SSD1306_SETLOWCOLUMN 0x00
#define SSD1306_SETHIGHCOLUMN 0x10
#define SSD1306_SETSTARTLINE 0x40
#define SSD1306_MEMORYMODE 0x20
#define SSD1306_COLUMNADDR 0x21
#define SSD1306_PAGEADDR   0x22
#define SSD1306_COMSCANINC 0xC0
#define SSD1306_COMSCANDEC 0xC8
#define SSD1306_SEGREMAP 0xA0
#define SSD1306_CHARGEPUMP 0x8D
#define SSD1306_EXTERNALVCC 0x1
#define SSD1306_SWITCHCAPVCC 0x2
*/



/* Private SSD1306 structure */
struct dpoz{
    u16 CurrentX;
    u16 CurrentY;
    u8  Inverted;
    u8  Initialized;
};


struct ssd1306_data {
    struct i2c_client *client;
    int status;
    struct dpoz poz;
};

/* -------------- hardware description -------------- */
/* device models */
enum {
        ssd1306oled,
};
/* device address */
enum {
        addrssd1306  = 0x3c,
};




/**
* @brief SSD1306 color enumeration
*/
typedef enum {
        SSD1306_COLOR_BLACK = 0x00,   /*!< Black color, no pixel */
        SSD1306_COLOR_WHITE = 0x01    /*!< Pixel is set. Color depends on LCD */
} ssd1306_COLOR_t;

/* i2c device_id structure */
static const struct i2c_device_id ssd1306_idtable [] = {
    { "ssd1306", ssd1306oled },
    { }
};

//extern u16 shared_data;

/* SSD1306 data buffer */
static u8 ssd1306_Buffer[SSD1306_WIDTH * SSD1306_HEIGHT / 8];


/* -------------- font description -------------- */
typedef struct {
    u8 FontWidth;     /*!< Font width in pixels */
    u8 FontHeight;    /*!< Font height in pixels */
    const u16 *data;  /*!< Pointer to data font data array */
} TM_FontDef_t;

const u16 TM_Font7x10 [] = {
0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, // sp
0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x0000, 0x1000, 0x0000, 0x0000, // !
0x2800, 0x2800, 0x2800, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, // "
0x2400, 0x2400, 0x7C00, 0x2400, 0x4800, 0x7C00, 0x4800, 0x4800, 0x0000, 0x0000, // #
0x3800, 0x5400, 0x5000, 0x3800, 0x1400, 0x5400, 0x5400, 0x3800, 0x1000, 0x0000, // $
0x2000, 0x5400, 0x5800, 0x3000, 0x2800, 0x5400, 0x1400, 0x0800, 0x0000, 0x0000, // %
0x1000, 0x2800, 0x2800, 0x1000, 0x3400, 0x4800, 0x4800, 0x3400, 0x0000, 0x0000, // &
0x1000, 0x1000, 0x1000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, // '
0x0800, 0x1000, 0x2000, 0x2000, 0x2000, 0x2000, 0x2000, 0x2000, 0x1000, 0x0800, // (
0x2000, 0x1000, 0x0800, 0x0800, 0x0800, 0x0800, 0x0800, 0x0800, 0x1000, 0x2000, // )
0x1000, 0x3800, 0x1000, 0x2800, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, // *
0x0000, 0x0000, 0x1000, 0x1000, 0x7C00, 0x1000, 0x1000, 0x0000, 0x0000, 0x0000, // +
0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x1000, 0x1000, 0x1000, // ,
0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x3800, 0x0000, 0x0000, 0x0000, 0x0000, // -
0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x1000, 0x0000, 0x0000, // .
0x0800, 0x0800, 0x1000, 0x1000, 0x1000, 0x1000, 0x2000, 0x2000, 0x0000, 0x0000, // /
0x3800, 0x4400, 0x4400, 0x5400, 0x4400, 0x4400, 0x4400, 0x3800, 0x0000, 0x0000, // 0
0x1000, 0x3000, 0x5000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x0000, 0x0000, // 1
0x3800, 0x4400, 0x4400, 0x0400, 0x0800, 0x1000, 0x2000, 0x7C00, 0x0000, 0x0000, // 2
0x3800, 0x4400, 0x0400, 0x1800, 0x0400, 0x0400, 0x4400, 0x3800, 0x0000, 0x0000, // 3
0x0800, 0x1800, 0x2800, 0x2800, 0x4800, 0x7C00, 0x0800, 0x0800, 0x0000, 0x0000, // 4
0x7C00, 0x4000, 0x4000, 0x7800, 0x0400, 0x0400, 0x4400, 0x3800, 0x0000, 0x0000, // 5
0x3800, 0x4400, 0x4000, 0x7800, 0x4400, 0x4400, 0x4400, 0x3800, 0x0000, 0x0000, // 6
0x7C00, 0x0400, 0x0800, 0x1000, 0x1000, 0x2000, 0x2000, 0x2000, 0x0000, 0x0000, // 7
0x3800, 0x4400, 0x4400, 0x3800, 0x4400, 0x4400, 0x4400, 0x3800, 0x0000, 0x0000, // 8
0x3800, 0x4400, 0x4400, 0x4400, 0x3C00, 0x0400, 0x4400, 0x3800, 0x0000, 0x0000, // 9
0x0000, 0x0000, 0x1000, 0x0000, 0x0000, 0x0000, 0x0000, 0x1000, 0x0000, 0x0000, // :
0x0000, 0x0000, 0x0000, 0x1000, 0x0000, 0x0000, 0x0000, 0x1000, 0x1000, 0x1000, // ;
0x0000, 0x0000, 0x0C00, 0x3000, 0x4000, 0x3000, 0x0C00, 0x0000, 0x0000, 0x0000, // <
0x0000, 0x0000, 0x0000, 0x7C00, 0x0000, 0x7C00, 0x0000, 0x0000, 0x0000, 0x0000, // =
0x0000, 0x0000, 0x6000, 0x1800, 0x0400, 0x1800, 0x6000, 0x0000, 0x0000, 0x0000, // >
0x3800, 0x4400, 0x0400, 0x0800, 0x1000, 0x1000, 0x0000, 0x1000, 0x0000, 0x0000, // ?
0x3800, 0x4400, 0x4C00, 0x5400, 0x5C00, 0x4000, 0x4000, 0x3800, 0x0000, 0x0000, // @
0x1000, 0x2800, 0x2800, 0x2800, 0x2800, 0x7C00, 0x4400, 0x4400, 0x0000, 0x0000, // A
0x7800, 0x4400, 0x4400, 0x7800, 0x4400, 0x4400, 0x4400, 0x7800, 0x0000, 0x0000, // B
0x3800, 0x4400, 0x4000, 0x4000, 0x4000, 0x4000, 0x4400, 0x3800, 0x0000, 0x0000, // C
0x7000, 0x4800, 0x4400, 0x4400, 0x4400, 0x4400, 0x4800, 0x7000, 0x0000, 0x0000, // D
0x7C00, 0x4000, 0x4000, 0x7C00, 0x4000, 0x4000, 0x4000, 0x7C00, 0x0000, 0x0000, // E
0x7C00, 0x4000, 0x4000, 0x7800, 0x4000, 0x4000, 0x4000, 0x4000, 0x0000, 0x0000, // F
0x3800, 0x4400, 0x4000, 0x4000, 0x5C00, 0x4400, 0x4400, 0x3800, 0x0000, 0x0000, // G
0x4400, 0x4400, 0x4400, 0x7C00, 0x4400, 0x4400, 0x4400, 0x4400, 0x0000, 0x0000, // H
0x3800, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x3800, 0x0000, 0x0000, // I
0x0400, 0x0400, 0x0400, 0x0400, 0x0400, 0x0400, 0x4400, 0x3800, 0x0000, 0x0000, // J
0x4400, 0x4800, 0x5000, 0x6000, 0x5000, 0x4800, 0x4800, 0x4400, 0x0000, 0x0000, // K
0x4000, 0x4000, 0x4000, 0x4000, 0x4000, 0x4000, 0x4000, 0x7C00, 0x0000, 0x0000, // L
0x4400, 0x6C00, 0x6C00, 0x5400, 0x4400, 0x4400, 0x4400, 0x4400, 0x0000, 0x0000, // M
0x4400, 0x6400, 0x6400, 0x5400, 0x5400, 0x4C00, 0x4C00, 0x4400, 0x0000, 0x0000, // N
0x3800, 0x4400, 0x4400, 0x4400, 0x4400, 0x4400, 0x4400, 0x3800, 0x0000, 0x0000, // O
0x7800, 0x4400, 0x4400, 0x4400, 0x7800, 0x4000, 0x4000, 0x4000, 0x0000, 0x0000, // P
0x3800, 0x4400, 0x4400, 0x4400, 0x4400, 0x4400, 0x5400, 0x3800, 0x0400, 0x0000, // Q
0x7800, 0x4400, 0x4400, 0x4400, 0x7800, 0x4800, 0x4800, 0x4400, 0x0000, 0x0000, // R
0x3800, 0x4400, 0x4000, 0x3000, 0x0800, 0x0400, 0x4400, 0x3800, 0x0000, 0x0000, // S
0x7C00, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x0000, 0x0000, // T
0x4400, 0x4400, 0x4400, 0x4400, 0x4400, 0x4400, 0x4400, 0x3800, 0x0000, 0x0000, // U
0x4400, 0x4400, 0x4400, 0x2800, 0x2800, 0x2800, 0x1000, 0x1000, 0x0000, 0x0000, // V
0x4400, 0x4400, 0x5400, 0x5400, 0x5400, 0x6C00, 0x2800, 0x2800, 0x0000, 0x0000, // W
0x4400, 0x2800, 0x2800, 0x1000, 0x1000, 0x2800, 0x2800, 0x4400, 0x0000, 0x0000, // X
0x4400, 0x4400, 0x2800, 0x2800, 0x1000, 0x1000, 0x1000, 0x1000, 0x0000, 0x0000, // Y
0x7C00, 0x0400, 0x0800, 0x1000, 0x1000, 0x2000, 0x4000, 0x7C00, 0x0000, 0x0000, // Z
0x1800, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1800, // [
0x2000, 0x2000, 0x1000, 0x1000, 0x1000, 0x1000, 0x0800, 0x0800, 0x0000, 0x0000, /* \ */
0x3000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x3000, // ]
0x1000, 0x2800, 0x2800, 0x4400, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, // ^
0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xFE00, // _
0x2000, 0x1000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, // `
0x0000, 0x0000, 0x3800, 0x4400, 0x3C00, 0x4400, 0x4C00, 0x3400, 0x0000, 0x0000, // a
0x4000, 0x4000, 0x5800, 0x6400, 0x4400, 0x4400, 0x6400, 0x5800, 0x0000, 0x0000, // b
0x0000, 0x0000, 0x3800, 0x4400, 0x4000, 0x4000, 0x4400, 0x3800, 0x0000, 0x0000, // c
0x0400, 0x0400, 0x3400, 0x4C00, 0x4400, 0x4400, 0x4C00, 0x3400, 0x0000, 0x0000, // d
0x0000, 0x0000, 0x3800, 0x4400, 0x7C00, 0x4000, 0x4400, 0x3800, 0x0000, 0x0000, // e
0x0C00, 0x1000, 0x7C00, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x0000, 0x0000, // f
0x0000, 0x0000, 0x3400, 0x4C00, 0x4400, 0x4400, 0x4C00, 0x3400, 0x0400, 0x7800, // g
0x4000, 0x4000, 0x5800, 0x6400, 0x4400, 0x4400, 0x4400, 0x4400, 0x0000, 0x0000, // h
0x1000, 0x0000, 0x7000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x0000, 0x0000, // i
0x1000, 0x0000, 0x7000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0xE000, // j
0x4000, 0x4000, 0x4800, 0x5000, 0x6000, 0x5000, 0x4800, 0x4400, 0x0000, 0x0000, // k
0x7000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x0000, 0x0000, // l
0x0000, 0x0000, 0x7800, 0x5400, 0x5400, 0x5400, 0x5400, 0x5400, 0x0000, 0x0000, // m
0x0000, 0x0000, 0x5800, 0x6400, 0x4400, 0x4400, 0x4400, 0x4400, 0x0000, 0x0000, // n
0x0000, 0x0000, 0x3800, 0x4400, 0x4400, 0x4400, 0x4400, 0x3800, 0x0000, 0x0000, // o
0x0000, 0x0000, 0x5800, 0x6400, 0x4400, 0x4400, 0x6400, 0x5800, 0x4000, 0x4000, // p
0x0000, 0x0000, 0x3400, 0x4C00, 0x4400, 0x4400, 0x4C00, 0x3400, 0x0400, 0x0400, // q
0x0000, 0x0000, 0x5800, 0x6400, 0x4000, 0x4000, 0x4000, 0x4000, 0x0000, 0x0000, // r
0x0000, 0x0000, 0x3800, 0x4400, 0x3000, 0x0800, 0x4400, 0x3800, 0x0000, 0x0000, // s
0x2000, 0x2000, 0x7800, 0x2000, 0x2000, 0x2000, 0x2000, 0x1800, 0x0000, 0x0000, // t
0x0000, 0x0000, 0x4400, 0x4400, 0x4400, 0x4400, 0x4C00, 0x3400, 0x0000, 0x0000, // u
0x0000, 0x0000, 0x4400, 0x4400, 0x2800, 0x2800, 0x2800, 0x1000, 0x0000, 0x0000, // v
0x0000, 0x0000, 0x5400, 0x5400, 0x5400, 0x6C00, 0x2800, 0x2800, 0x0000, 0x0000, // w
0x0000, 0x0000, 0x4400, 0x2800, 0x1000, 0x1000, 0x2800, 0x4400, 0x0000, 0x0000, // x
0x0000, 0x0000, 0x4400, 0x4400, 0x2800, 0x2800, 0x1000, 0x1000, 0x1000, 0x6000, // y
0x0000, 0x0000, 0x7C00, 0x0800, 0x1000, 0x2000, 0x4000, 0x7C00, 0x0000, 0x0000, // z
0x1800, 0x1000, 0x1000, 0x1000, 0x2000, 0x2000, 0x1000, 0x1000, 0x1000, 0x1800, // {
0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, // |
0x3000, 0x1000, 0x1000, 0x1000, 0x0800, 0x0800, 0x1000, 0x1000, 0x1000, 0x3000, // }
0x0000, 0x0000, 0x0000, 0x7400, 0x4C00, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, // ~
};

TM_FontDef_t TM_Font_7x10 = {
    7,
    10,
    TM_Font7x10
};


#define ARRAY_COL 5
#define ARRAY_ROW SYM_WIDTH

u64 symArray[ARRAY_ROW][ARRAY_COL] = {
	//		1		0			3		2			5		4			7		6			9		8
	{ 0x00000000000ffff0, 0x000c000700f00070, 0x000c000000000000, 0x00000000000ff800, 0x000007f0000fe000 },
	{ 0x00000000001ffff8, 0x001c000700fc0078, 0x001c0fff0001c000, 0x00000000001ffe00, 0x00000ff8001ff000 },
	{ 0x00000000003ffffc, 0x003c000700fc007c, 0x003c0fff0001f000, 0x00000007003fff80, 0x00001ffc003ff878 },
	{ 0x000000000078001e, 0x0078000700fe001e, 0x00780fff0001f800, 0x0000000700783fc0, 0x00003c1e00783cfc },
	{ 0x00e0007000f0000f, 0x00f0000700ee000f, 0x00f00e070001fe00, 0x00c0000700f01fe0, 0x0000780f00f01dfe },
	{ 0x00e0007800e00007, 0x00e0000700e70007, 0x00e00e070001ff00, 0x00f0000700e00ff0, 0x0000700700e00fcf },
	{ 0x00e0003c00e00007, 0x00e0000700e70007, 0x00e00e070001cfc0, 0x00fc000700e00e78, 0x00c0700700e00f87 },
	{ 0x00e0001e00e00007, 0x00e00e0700e38007, 0x00e00e070001c7e0, 0x00ff000700e00e3c, 0x00e0700700e00f07 },
	{ 0x00ffffff00e00007, 0x00e00f0700e38007, 0x00e00e070001c1f8, 0x003fc00700e00e1e, 0x00f0700700e00f07 },
	{ 0x00ffffff00e00007, 0x00e00f8700e1c007, 0x00e00e070001c0fe, 0x000ff00700e00e0f, 0x0078700700e00f07 },
	{ 0x00ffffff00e00007, 0x00e00fc700e1c007, 0x00e00e070001c03f, 0x0003fc0700e00e07, 0x003c700700e00f07 },
	{ 0x00ffffff00e00007, 0x00e00fe700e0e007, 0x00e00e070001c01f, 0x0000ff0700e00e03, 0x001e700700e00f87 },
	{ 0x00e0000000e00007, 0x00e00ef700e0e007, 0x00e00e0700ffffff, 0x00003fc700e00e00, 0x000ff00700e00fcf },
	{ 0x00e0000000f0000f, 0x00f01e7f00e0700f, 0x00f01e0700ffffff, 0x00000ff700f01e00, 0x0007f80f00f01dfe },
	{ 0x00e000000078001e, 0x00783c3f00e0781e, 0x00783c0700ffffff, 0x000003ff00783c00, 0x0003fc1e00783cfc },
	{ 0x00e00000003ffffc, 0x003ff81f00e03ffc, 0x003ff80700ffffff, 0x000000ff003ff800, 0x0001fffc003ff878 },
	{ 0x00000000001ffff8, 0x001ff00f00e01ff8, 0x001ff0070001c000, 0x0000003f001ff000, 0x00007ff8001ff000 },
	{ 0x00000000000ffff0, 0x000fe00700e00ff0, 0x000fe0000001c000, 0x0000000f000fe000, 0x00001ff0000fe000 },
};



//
int ssd1306_DrawPixel(struct ssd1306_data *drv_data, u16 x, u16 y, ssd1306_COLOR_t color);
int ssd1306_GotoXY(struct ssd1306_data *drv_data, u16 x, u16 y);
char ssd1306_Putc(struct ssd1306_data *drv_data, char ch, TM_FontDef_t* Font, ssd1306_COLOR_t color);
char ssd1306_Puts(struct ssd1306_data *drv_data, char* str, TM_FontDef_t* Font, ssd1306_COLOR_t color);
int ssd1306_UpdateScreen(struct ssd1306_data *drv_data);
int ssd1306_ON(struct ssd1306_data *drv_data);
int ssd1306_OFF(struct ssd1306_data *drv_data);
int ssd1306_clean(struct ssd1306_data *drv_data, int clean_ssd1306_Buffer);

/* Init sequence taken from the Adafruit SSD1306 Arduino library */
static void ssd1306_init_lcd(struct i2c_client *drv_client) {

    char m;
    char i;
    u8 d;

    printk(KERN_ERR "ssd1306: Device init \n");
    	/* Init LCD */
    if (TURN_OFF_DISPLAY)
    	i2c_smbus_write_byte_data(drv_client, 0x00, 0xAE); //display off

    i2c_smbus_write_byte_data(drv_client, 0x00, 0x20); //Set Memory Addressing Mode
    // San Bobro: the bug is detected in the next line.
    // Modes are described by "00b", "01b", "10b" in binary format, not hex!
    // So to set "Page Addressing Mode" we should sent 0x02, not 0x10.
    // 0x10 - enables "Vertical Addressing Mode" as and 0x00
    // because the lower 2 bits are taken into account only. (See advanced datasheet).
    i2c_smbus_write_byte_data(drv_client, 0x00, 0x02);	// 0x00 - Horizontal Addressing Mode;
    													// 0x01 - Vertical Addressing Mode;
    													// 0x02 - Page Addressing Mode (RESET);
    													// 0x03 - Invalid.

    i2c_smbus_write_byte_data(drv_client, 0x00, 0xB0); //Set Page Start Address for Page Addressing Mode,0-7
    i2c_smbus_write_byte_data(drv_client, 0x00, 0xC8); //Set COM Output Scan Direction
    i2c_smbus_write_byte_data(drv_client, 0x00, 0x00); //---set low column address
    i2c_smbus_write_byte_data(drv_client, 0x00, 0x10); //---set high column address
    i2c_smbus_write_byte_data(drv_client, 0x00, 0x40); //--set start line address
    i2c_smbus_write_byte_data(drv_client, 0x00, 0x81); //--set contrast control register
    i2c_smbus_write_byte_data(drv_client, 0x00, 0xFE);
    i2c_smbus_write_byte_data(drv_client, 0x00, 0xA1); //--set segment re-map 0 to 127
    i2c_smbus_write_byte_data(drv_client, 0x00, 0xA6); //--set normal display
    i2c_smbus_write_byte_data(drv_client, 0x00, 0xA8); //--set multiplex ratio(1 to 64)
    i2c_smbus_write_byte_data(drv_client, 0x00, 0x3F); //ox3f
    i2c_smbus_write_byte_data(drv_client, 0x00, 0xA4); //0xa4,Output follows RAM content;0xa5,Output ignores RAM content
    i2c_smbus_write_byte_data(drv_client, 0x00, 0xD3); //-set display offset
    i2c_smbus_write_byte_data(drv_client, 0x00, 0x00); //-not offset

    i2c_smbus_write_byte_data(drv_client, 0x00, 0xD5); //--set display clock divide ratio/oscillator frequency
    i2c_smbus_write_byte_data(drv_client, 0x00, 0xF0); //--set divide ratio //0xa0
    i2c_smbus_write_byte_data(drv_client, 0x00, 0xD9); //--set pre-charge period
    i2c_smbus_write_byte_data(drv_client, 0x00, 0x22); //

    i2c_smbus_write_byte_data(drv_client, 0x00, 0xDA); //--set com pins hardware configuration
    i2c_smbus_write_byte_data(drv_client, 0x00, 0x12);
    i2c_smbus_write_byte_data(drv_client, 0x00, 0xDB); //--set vcomh
    i2c_smbus_write_byte_data(drv_client, 0x00, 0x20); //0x20,0.77xVcc
    i2c_smbus_write_byte_data(drv_client, 0x00, 0x8D); //--set DC-DC enable
    i2c_smbus_write_byte_data(drv_client, 0x00, 0x14); //
    i2c_smbus_write_byte_data(drv_client, 0x00, 0xAF); //--turn on SSD1306 panel

    d = jiffies % 0xff;
    if (!d)
    	d = 0x18;

//    printk(KERN_ERR "%s: ms before data writing to LCD = %u\n", __func__, jiffies_to_msecs(jiffies));
    for (m = 0; m < 8; m++) {
        i2c_smbus_write_byte_data(drv_client, 0x00, 0xB0 + m);
        i2c_smbus_write_byte_data(drv_client, 0x00, 0x00);
        i2c_smbus_write_byte_data(drv_client, 0x00, 0x10);
        /* Write multi data */
        for (i = 0; i < SSD1306_WIDTH; i++) {
            i2c_smbus_write_byte_data(drv_client, 0x40, d);
        }
    }
//    printk(KERN_ERR "%s: ms after data writing to LCD =  %u\n", __func__, jiffies_to_msecs(jiffies));
//    msleep(400000);
//    printk(KERN_ERR "%s: ms after sleep = %u\n", __func__, jiffies_to_msecs(jiffies));
}
/**/
int ssd1306_UpdateScreen(struct ssd1306_data *drv_data) {

    struct i2c_client *drv_client;
    char m;
    char i;

    drv_client = drv_data->client;

    for (m = 0; m < 8; m++) {
        i2c_smbus_write_byte_data(drv_client, 0x00, 0xB0 + m);
        i2c_smbus_write_byte_data(drv_client, 0x00, 0x00);
        i2c_smbus_write_byte_data(drv_client, 0x00, 0x10);
        /* Write multi data */
        for (i = 0; i < SSD1306_WIDTH; i++) {
            i2c_smbus_write_byte_data(drv_client, 0x40, ssd1306_Buffer[SSD1306_WIDTH*m +i]);
        }   
    }

    return 0;
}
/**/
int ssd1306_DrawPixel(struct ssd1306_data *drv_data, u16 x, u16 y, ssd1306_COLOR_t color) {
    struct dpoz *poz;
    poz = &drv_data->poz;
    

    if ( x >= SSD1306_WIDTH || y >= SSD1306_HEIGHT ) {
        return -1;
    }
    /* Check if pixels are inverted */
    if (poz->Inverted) {
        color = (ssd1306_COLOR_t)!color;
    }
    /* Set color */
    if (color == SSD1306_COLOR_WHITE) {
        ssd1306_Buffer[x + (y / 8) * SSD1306_WIDTH] |= 1 << (y % 8);
    }
    else {
        ssd1306_Buffer[x + (y / 8) * SSD1306_WIDTH] &= ~(1 << (y % 8));
    }

    return 0;
}

/**/
int ssd1306_GotoXY(struct ssd1306_data *drv_data, u16 x, u16 y) {
    /* Set write pointers */
    struct dpoz *poz;
   
    poz = &drv_data->poz;

    poz->CurrentX = x;
    poz->CurrentY = y;

    return 0;
}


/**/
char ssd1306_Putc(struct ssd1306_data *drv_data, char ch, TM_FontDef_t* Font, ssd1306_COLOR_t color) {
    u32 i, b, j;
    struct dpoz *poz;
    poz = &drv_data->poz;

    /* Check available space in LCD */
    if ( SSD1306_WIDTH <= (poz->CurrentX + Font->FontWidth) || SSD1306_HEIGHT <= (poz->CurrentY + Font->FontHeight)) {
        return 0;
    }
    /* Go through font */
    for (i = 0; i < Font->FontHeight; i++) {
        b = Font->data[(ch - 32) * Font->FontHeight + i];
        for (j = 0; j < Font->FontWidth; j++) {
            if ((b << j) & 0x8000) {
                ssd1306_DrawPixel(drv_data, poz->CurrentX + j, (poz->CurrentY + i), (ssd1306_COLOR_t) color);
            } else {
                ssd1306_DrawPixel(drv_data, poz->CurrentX + j, (poz->CurrentY + i), (ssd1306_COLOR_t)!color);
            }
        }
    }
    /* Increase pointer */ 
    poz->CurrentX += Font->FontWidth;
    /* Return character written */
    return ch;
}
/**/
char ssd1306_Puts(struct ssd1306_data *drv_data, char* str, TM_FontDef_t* Font, ssd1306_COLOR_t color) {
    while (*str) { /* Write character by character */
        if (ssd1306_Putc(drv_data, *str, Font, color) != *str) {
            return *str; /* Return error */
        }
        str++; /* Increase string pointer */
    }
    return *str; /* Everything OK, zero should be returned */
}

/**/
int ssd1306_ON(struct ssd1306_data *drv_data) {
    struct i2c_client *drv_client;

    drv_client = drv_data->client;

    i2c_smbus_write_byte_data(drv_client, 0x00, 0x8D);
    i2c_smbus_write_byte_data(drv_client, 0x00, 0x14);
    i2c_smbus_write_byte_data(drv_client, 0x00, 0xAF);
    return 0;
}

/**/
int ssd1306_OFF(struct ssd1306_data *drv_data) {
    struct i2c_client *drv_client;

    drv_client = drv_data->client;

    i2c_smbus_write_byte_data(drv_client, 0x00, 0x8D);
    i2c_smbus_write_byte_data(drv_client, 0x00, 0x10);
    i2c_smbus_write_byte_data(drv_client, 0x00, 0xAE);
    return 0;
}

int ssd1306_clean(struct ssd1306_data *drv_data, int clean_ssd1306_Buffer) {
    struct i2c_client *drv_client;
    char m;
    char i;

    drv_client = drv_data->client;

    for (m = 0; m < 8; m++) {
        i2c_smbus_write_byte_data(drv_client, 0x00, 0xB0 + m);
        i2c_smbus_write_byte_data(drv_client, 0x00, 0x00);
        i2c_smbus_write_byte_data(drv_client, 0x00, 0x10);
        /* Write multi data */
        for (i = 0; i < SSD1306_WIDTH; i++) {
            i2c_smbus_write_byte_data(drv_client, 0x40, 0x00);
        }
    }

    if (clean_ssd1306_Buffer)
    	memset(ssd1306_Buffer, 0, sizeof(ssd1306_Buffer));

    return 0;
}

static int
ssd1306_probe(struct i2c_client *drv_client, const struct i2c_device_id *id)
{
    struct ssd1306_data *ssd1306;
    struct i2c_adapter *adapter;
    int i, j;
    int k = 0;

    printk(KERN_ERR "init I2C driver\n");


    ssd1306 = devm_kzalloc(&drv_client->dev, sizeof(struct ssd1306_data),
                        GFP_KERNEL);
    if (!ssd1306)
        return -ENOMEM;

    ssd1306->client = drv_client;
    ssd1306->status = 0xABCD;

    i2c_set_clientdata(drv_client, ssd1306);

    adapter = drv_client->adapter;

    if (!adapter)
    {
        dev_err(&drv_client->dev, "adapter indentification error\n");
        return -ENODEV;
    }

    printk(KERN_ERR "I2C client address %d \n", drv_client->addr);

    if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
            dev_err(&drv_client->dev, "operation not supported\n");
            return -ENODEV;
    }

    //cra[]
    ssd1306_clean(ssd1306, 1);
    ssd1306_init_lcd( drv_client);
    ssd1306_GotoXY(ssd1306, 5, 4);
    ssd1306_Puts(ssd1306, "HELLO GLOBALLOGIC", &TM_Font_7x10, SSD1306_COLOR_WHITE);
    ssd1306_UpdateScreen(ssd1306);
//    msleep(2000);
//    ssd1306_clean(ssd1306, 0);
//    msleep(3000);
//    ssd1306_GotoXY(ssd1306, 5, 25);
//    ssd1306_Puts(ssd1306, "HELLO GLOBALLOGIC", &TM_Font_7x10, SSD1306_COLOR_WHITE);
//    ssd1306_UpdateScreen(ssd1306);
//
//    ssd1306_GotoXY(ssd1306, 5, 50);
//    ssd1306_Puts(ssd1306, "HELLO GLOBALLOGIC", &TM_Font_7x10, SSD1306_COLOR_WHITE);
//    ssd1306_UpdateScreen(ssd1306);

//    msleep(100);

//    for (i=0; i<100; i++)
//    {
//    i2c_smbus_write_byte_data(drv_client, 0x00, 0x2E); //Stop scrolling
//
//    i2c_smbus_write_byte_data(drv_client, 0x00, 0x29);
//    i2c_smbus_write_byte_data(drv_client, 0x00, 0x00);
//    i2c_smbus_write_byte_data(drv_client, 0x00, 0x00);
//    i2c_smbus_write_byte_data(drv_client, 0x00, 0x11);
//    i2c_smbus_write_byte_data(drv_client, 0x00, 0x07);
//    i2c_smbus_write_byte_data(drv_client, 0x00, 0x01);
//
//    i2c_smbus_write_byte_data(drv_client, 0x00, 0x2F); //Start scrolling
//    msleep(3);
//    i2c_smbus_write_byte_data(drv_client, 0x00, 0x2E); //Stop scrolling
//
//    i2c_smbus_write_byte_data(drv_client, 0x00, 0x2A);
//    i2c_smbus_write_byte_data(drv_client, 0x00, 0x00);
//    i2c_smbus_write_byte_data(drv_client, 0x00, 0x00);
//    i2c_smbus_write_byte_data(drv_client, 0x00, 0x11);
//    i2c_smbus_write_byte_data(drv_client, 0x00, 0x07);
//    i2c_smbus_write_byte_data(drv_client, 0x00, 0x01);
//
//    i2c_smbus_write_byte_data(drv_client, 0x00, 0x2F); //Start scrolling
//    msleep(3);
//    }
//    msleep(3);
//    i2c_smbus_write_byte_data(drv_client, 0x00, 0x2E); //Stop scrolling

//    msleep(1000);
    ssd1306_clean(ssd1306, 1);

    i2c_smbus_write_byte_data(drv_client, 0x00, 0x20); //Set Memory Addressing Mode
    i2c_smbus_write_byte_data(drv_client, 0x00, 0x01);	// 0x00 - Horizontal Addressing Mode;
    													// 0x01 - Vertical Addressing Mode;
    													// 0x02 - Page Addressing Mode (RESET);
    													// 0x03 - Invalid.
    i2c_smbus_write_byte_data(drv_client, 0x00, 0x21); //Set Column Address 21h
    i2c_smbus_write_byte_data(drv_client, 0x00, 0x0A);
    i2c_smbus_write_byte_data(drv_client, 0x00, 0x7F-0x0A);

    i2c_smbus_write_byte_data(drv_client, 0x00, 0x22); // Set Page Address 22h
    i2c_smbus_write_byte_data(drv_client, 0x00, 0x00);
    i2c_smbus_write_byte_data(drv_client, 0x00, 0x07);

    for (i = 0; i < 8*30; i++) {
    	i2c_smbus_write_byte_data(drv_client, 0x40, 0x03);
    }

    u8 *pc;
//    for (i=0; i<8; i++, pc++)
//    {
//    	printk(KERN_ERR "%.2x\n", *pc);
//
//    }

	i2c_smbus_write_byte_data(drv_client, 0x00, 0x21); //Set Column Address 21h
	i2c_smbus_write_byte_data(drv_client, 0x00, 0x0A+40);
	i2c_smbus_write_byte_data(drv_client, 0x00, 0x0A+40+SYM_WIDTH-1);

	i2c_smbus_write_byte_data(drv_client, 0x00, 0x22); // Set Page Address 22h
	i2c_smbus_write_byte_data(drv_client, 0x00, 0x01);
	i2c_smbus_write_byte_data(drv_client, 0x00, 0x07);



    u64 mask = 0;
    u64 mask0 = 0;
    unsigned int ms;

//	msleep(2000);
	ms = jiffies_to_msecs(jiffies);
    while (1)
    {
    	pc = (u8*)&(symArray[0][0]);
    	for (i = 0; i < 8*SYM_WIDTH; i++, pc++) {
    		if (i%8 == 7)
    		{
    			pc += 8*(ARRAY_COL-1);
    			continue;
    		}

    		i2c_smbus_write_byte_data(drv_client, 0x40, *pc);
//    		printk(KERN_ERR "%.2x\n", *pc);

    	}

#define OFFSET 1

#if  (OFFSET==1)
    	for (i = 0; i < ARRAY_ROW; i++)
    	{
    		mask0 = symArray[i][0] & 0x0000000000000001;
    		for (j=ARRAY_COL-1; j >= 0; j--)
    		{
    			mask = symArray[i][j] & 0x0000000000000001;
    			symArray[i][j] = symArray[i][j] >> 1;
        		if (mask0)
        			symArray[i][j] = symArray[i][j] | 0x8000000000000000;
        		mask0 = mask;
    		}
    	}
    	if (mdelay)
    		msleep( mdelay );

#elif (OFFSET==2)
    	for (i = 0; i < ARRAY_ROW; i++)
    	{
    		mask0 = symArray[i][0] & 0x0000000000000003;
    		for (j=ARRAY_COL-1; j >= 0; j--)
    		{
    			mask = symArray[i][j] & 0x0000000000000003;
    			symArray[i][j] = symArray[i][j] >> 2;
        		if (mask0)
        			symArray[i][j] = symArray[i][j] | 0xC000000000000000;
        		mask0 = mask;
    		}
    	}
    	if (mdelay)
    		msleep( mdelay );
#endif

    	k++;
    	if (k>=offset_number) break;
    	if (k%32==0)
    		msleep(530);
    }
	ms = jiffies_to_msecs(jiffies) - ms;
	dev_err(&drv_client->dev, "@@@@@ time for shift count %d, OFFSET %u and mdelay %d is %u. @@@@@\n",
			offset_number, OFFSET, mdelay, ms);


    dev_set_drvdata(&drv_client->dev, ssd1306);
    dev_err(&drv_client->dev, "ssd1306 driver successfully loaded\n\n");


//    dev_err(&drv_client->dev, "ssd1306 driver successfully loaded**********************\n\n");
//    return -1;

    return 0;
}

static int
ssd1306_remove(struct i2c_client *drv_client)
{
	struct ssd1306_data *ssd1306;

	ssd1306 = dev_get_drvdata(&drv_client->dev);
	if (TURN_OFF_DISPLAY)
		ssd1306_OFF(ssd1306);
	dev_err(&drv_client->dev, "ssd1306 driver successfully unloaded\n");
    return 0;
}


static struct i2c_driver ssd1306_driver = {
    .driver = {
        .name = DEVICE_NAME,
     },

    .probe       = ssd1306_probe ,
    .remove      = ssd1306_remove,
    .id_table    = ssd1306_idtable,
};


/* Module init */
static int __init ssd1306_init ( void )
{
    int ret;
    /*
    * An i2c_driver is used with one or more i2c_client (device) nodes to access
    * i2c slave chips, on a bus instance associated with some i2c_adapter.
    */
    printk(KERN_ERR "********************************\n");
    printk(KERN_ERR "%s(): ssd1306 mod init\n", __func__);
    ret = i2c_add_driver ( &ssd1306_driver);
    if(ret) 
    {
        printk(KERN_ERR "%s(): failed to add new i2c driver\n", __func__);
        return ret;
    }

    return ret;
}

/* Module exit */
static void __exit ssd1306_exit(void)
{
    i2c_del_driver(&ssd1306_driver);
    printk(KERN_ERR "%s(): ssd1306: cleanup\n\n", __func__);
}

/* -------------- kernel thread description -------------- */
module_init(ssd1306_init);
module_exit(ssd1306_exit);


MODULE_DESCRIPTION("SSD1306 Display Driver");
MODULE_AUTHOR("GL team");
MODULE_LICENSE("GPL");
