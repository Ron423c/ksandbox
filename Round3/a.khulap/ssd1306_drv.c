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
#include <linux/slab.h>

#include <linux/i2c.h>
#include <linux/i2c-dev.h>


#define DEVICE_NAME "ssd1306"
#define CLASS_NAME  "oled_display" 
#define BUS_NAME    "i2c_1"

#define SSD1306_WIDTH 128
#define SSD1306_HEIGHT 64

#define WRITECOMMAND  0x00 // 
#define WRITEDATA     0x40 // 


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


#define INITIAL_VALUE   10

static struct workqueue_struct *ssd1306_wq;

struct ssd1306_work_struct {
    struct work_struct work;
    struct ssd1306_data *ssd1306;
    unsigned short initial_value;
};


/* Forward declaration */
static ssize_t ssd1306_attr_store(struct class *class, struct class_attribute *attr, const char *buf, size_t count);

static struct class *class_ssd1306 = 0;
CLASS_ATTR(interval, 0664, NULL, &ssd1306_attr_store);



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


//
int ssd1306_DrawPixel(struct ssd1306_data *drv_data, u16 x, u16 y, ssd1306_COLOR_t color);
int ssd1306_GotoXY(struct ssd1306_data *drv_data, u16 x, u16 y);
char ssd1306_Putc(struct ssd1306_data *drv_data, char ch, TM_FontDef_t* Font, ssd1306_COLOR_t color);
char ssd1306_Puts(struct ssd1306_data *drv_data, char* str, TM_FontDef_t* Font, ssd1306_COLOR_t color);
int ssd1306_UpdateScreen(struct ssd1306_data *drv_data);
int ssd1306_ON(struct ssd1306_data *drv_data);
int ssd1306_OFF(struct ssd1306_data *drv_data);

/* Init sequence taken from the Adafruit SSD1306 Arduino library */
static void ssd1306_init_lcd(struct i2c_client *drv_client) {

    char m;
    char i;

    printk(KERN_ERR "ssd1306: Device init \n");
    	/* Init LCD */
    i2c_smbus_write_byte_data(drv_client, 0x00, 0xAE); //display off
    i2c_smbus_write_byte_data(drv_client, 0x00, 0x20); //Set Memory Addressing Mode
    i2c_smbus_write_byte_data(drv_client, 0x00, 0x10); //00,Horizontal Addressing Mode;01,Vertical Addressing Mode;10,Page Addressing Mode (RESET);11,Invalid
    i2c_smbus_write_byte_data(drv_client, 0x00, 0xB0); //Set Page Start Address for Page Addressing Mode,0-7
    i2c_smbus_write_byte_data(drv_client, 0x00, 0xC8); //Set COM Output Scan Direction
    i2c_smbus_write_byte_data(drv_client, 0x00, 0x00); //---set low column address
    i2c_smbus_write_byte_data(drv_client, 0x00, 0x10); //---set high column address
    i2c_smbus_write_byte_data(drv_client, 0x00, 0x40); //--set start line address
    i2c_smbus_write_byte_data(drv_client, 0x00, 0x81); //--set contrast control register
    i2c_smbus_write_byte_data(drv_client, 0x00, 0x01);
    i2c_smbus_write_byte_data(drv_client, 0x00, 0xA1); //--set segment re-map 0 to 127
    i2c_smbus_write_byte_data(drv_client, 0x00, 0xA6); //--set normal display
    i2c_smbus_write_byte_data(drv_client, 0x00, 0xA8); //--set multiplex ratio(1 to 64)
    i2c_smbus_write_byte_data(drv_client, 0x00, 0x3F); //
    i2c_smbus_write_byte_data(drv_client, 0x00, 0xA4); //0xa4,Output follows RAM content;0xa5,Output ignores RAM content
    i2c_smbus_write_byte_data(drv_client, 0x00, 0xD3); //-set display offset
    i2c_smbus_write_byte_data(drv_client, 0x00, 0x00); //-not offset

    i2c_smbus_write_byte_data(drv_client, 0x00, 0xD5); //--set display clock divide ratio/oscillator frequency
    i2c_smbus_write_byte_data(drv_client, 0x00, 0xa0); //--set divide ratio
    i2c_smbus_write_byte_data(drv_client, 0x00, 0xD9); //--set pre-charge period
    i2c_smbus_write_byte_data(drv_client, 0x00, 0x22); //

    i2c_smbus_write_byte_data(drv_client, 0x00, 0xDA); //--set com pins hardware configuration
    i2c_smbus_write_byte_data(drv_client, 0x00, 0x12);
    i2c_smbus_write_byte_data(drv_client, 0x00, 0xDB); //--set vcomh
    i2c_smbus_write_byte_data(drv_client, 0x00, 0x20); //0x20,0.77xVcc
    i2c_smbus_write_byte_data(drv_client, 0x00, 0x8D); //--set DC-DC enable
    i2c_smbus_write_byte_data(drv_client, 0x00, 0x14); //
    i2c_smbus_write_byte_data(drv_client, 0x00, 0xAF); //--turn on SSD1306 panel

    for (m = 0; m < 8; m++) {
        i2c_smbus_write_byte_data(drv_client, 0x00, 0xB0 + m);
        i2c_smbus_write_byte_data(drv_client, 0x00, 0x00);
        i2c_smbus_write_byte_data(drv_client, 0x00, 0x10);
        /* Write multi data */
        for (i = 0; i < SSD1306_WIDTH; i++) {
            i2c_smbus_write_byte_data(drv_client, 0x40, 0xaa);
        }
    }
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


static void ssd1306_wq_function(struct work_struct *work)
{

    struct ssd1306_data *ssd1306;
    unsigned short initial_value;
    char str[20];
    int value;

    ssd1306 = g_ssd1306;
    initial_value = ((struct ssd1306_work_struct *)work)->initial_value;

    value = initial_value;
    while (value >= 0) {
        sprintf(str, "value = %5d  ", value);

        ssd1306_GotoXY(ssd1306, 8, 40);
        ssd1306_Puts(ssd1306, str, &TM_Font_7x10, SSD1306_COLOR_WHITE);
        ssd1306_UpdateScreen(ssd1306);

        msleep(1000);
        value--;
    }

    ssd1306_GotoXY(ssd1306, 8, 40);
    ssd1306_Puts(ssd1306, "countdown done", &TM_Font_7x10, SSD1306_COLOR_WHITE);
    ssd1306_UpdateScreen(ssd1306);

    kfree(work);

    printk(KERN_ERR "ssd1306_wq_function done!\n");
}

static void ssd1306_wq_wait(void)
{
    if (ssd1306_wq)
        flush_workqueue(ssd1306_wq);    
}

static int ssd1306_wq_addwork(unsigned short initial_value)
{
    bool ret;
    struct ssd1306_work_struct *work;

    work = (struct ssd1306_work_struct *)kmalloc(sizeof(struct ssd1306_work_struct), GFP_KERNEL);
    if (!work)
        return -ENOMEM;

    INIT_WORK((struct work_struct *)work, ssd1306_wq_function);
    work->initial_value = initial_value;

    ret = queue_work(ssd1306_wq, (struct work_struct *)work);
    if (!ret)
        return -EPERM;

    return 0;
}


static int ssd1306_wq_create(struct ssd1306_data *ssd1306data, unsigned short initial_value)
{
    ssd1306_wq = create_workqueue("ssd1306_wq");
    if (!ssd1306_wq)
        return -EBADRQC;

    return 0;
}

static int ssd1306_wq_cleanup(void)
{
    if (ssd1306_wq) {
        flush_workqueue(ssd1306_wq);
        destroy_workqueue(ssd1306_wq);
        ssd1306_wq = 0;
    }
    return 0;
}


static ssize_t ssd1306_attr_store(struct class *class, struct class_attribute *attr, const char *buf, size_t count)
{
    int ret;
    int interval;

    ret = sscanf(buf, "%d", &interval);
    if (ret != 1)
        return 0;

    ssd1306_wq_wait();
    ssd1306_wq_addwork(interval);

    return count;
}

static int ssd1306_class_create(void)
{
    int ret;

    class_ssd1306 = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(class_ssd1306))
        return PTR_ERR(class_ssd1306);
   
    ret = class_create_file(class_ssd1306, &class_attr_interval);
    if (IS_ERR_VALUE(ret))
        return ret;

    return 0;
}

static int ssd1306_class_cleanup(void)
{
    if (class_ssd1306) {
        class_remove_file(class_ssd1306, &class_attr_interval);
        class_destroy(class_ssd1306);
    }
    return 0;
}


static int
ssd1306_probe(struct i2c_client *drv_client, const struct i2c_device_id *id)
{
    int ret;
    struct ssd1306_data *ssd1306;
    struct i2c_adapter *adapter;

    printk(KERN_ERR "init I2C driver\n");


    ssd1306 = devm_kzalloc(&drv_client->dev, sizeof(struct ssd1306_data),
                        GFP_KERNEL);
    if (!ssd1306)
        return -ENOMEM;

    g_ssd1306 = ssd1306;

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
    ssd1306_init_lcd( drv_client);
    ssd1306_GotoXY(ssd1306, 8, 25);
    ssd1306_Puts(ssd1306, "HELLO GLOBALLOGIC", &TM_Font_7x10, SSD1306_COLOR_WHITE);
    ssd1306_UpdateScreen(ssd1306);

    dev_err(&drv_client->dev, "ssd1306 driver successfully loaded\n");

    ret = ssd1306_wq_create(ssd1306, INITIAL_VALUE);
    if (ret < 0)
        dev_err(&drv_client->dev, "workqueue initialization failed!\n");

    ssd1306_wq_addwork(INITIAL_VALUE);

    ssd1306_class_create();

    return 0;
}

static int
ssd1306_remove(struct i2c_client *client)
{
    ssd1306_class_cleanup();
    return ssd1306_wq_cleanup();
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
    printk(KERN_ERR "ssd1306 mod init\n");
    ret = i2c_add_driver ( &ssd1306_driver);
    if(ret) 
    {
        printk(KERN_ERR "failed to add new i2c driver");
        return ret;
    }

    return ret;
}

/* Module exit */
static void __exit ssd1306_exit(void)
{
    i2c_del_driver(&ssd1306_driver);
    printk(KERN_ERR "ssd1306: cleanup\n");  
}

/* -------------- kernel thread description -------------- */
module_init(ssd1306_init);
module_exit(ssd1306_exit);


MODULE_DESCRIPTION("SSD1306 Display Driver");
MODULE_AUTHOR("GL team");
MODULE_LICENSE("GPL");
