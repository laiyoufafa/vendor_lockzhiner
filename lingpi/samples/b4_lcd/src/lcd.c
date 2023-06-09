/*
 * Copyright (c) 2022 FuZhou Lockzhiner Electronic Co., Ltd. All rights reserved.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "lz_hardware.h"
#include "lcd_font.h"
#include "lcd.h"

/* 是否启用SPI通信
 * 0 => 禁用SPI，使用gpio模拟SPI通信
 * 1 => 启用SPI
 */
#define LCD_ENABLE_SPI      0
#define LCD_SPI_BUS         0

#define LCD_PIN_CS          GPIO0_PC0
#define LCD_PIN_CLK         GPIO0_PC1
#define LCD_PIN_MOSI        GPIO0_PC2
#define LCD_PIN_RES         GPIO0_PC3
#define LCD_PIN_DC          GPIO0_PC6

#define LCD_CS_Clr()        LzGpioSetVal(LCD_PIN_CS, LZGPIO_LEVEL_LOW)
#define LCD_CS_Set()        LzGpioSetVal(LCD_PIN_CS, LZGPIO_LEVEL_HIGH)

#define LCD_CLK_Clr()       LzGpioSetVal(LCD_PIN_CLK, LZGPIO_LEVEL_LOW)
#define LCD_CLK_Set()       LzGpioSetVal(LCD_PIN_CLK, LZGPIO_LEVEL_HIGH)

#define LCD_MOSI_Clr()      LzGpioSetVal(LCD_PIN_MOSI, LZGPIO_LEVEL_LOW)
#define LCD_MOSI_Set()      LzGpioSetVal(LCD_PIN_MOSI, LZGPIO_LEVEL_HIGH)

#define LCD_RES_Clr()       LzGpioSetVal(LCD_PIN_RES, LZGPIO_LEVEL_LOW)
#define LCD_RES_Set()       LzGpioSetVal(LCD_PIN_RES, LZGPIO_LEVEL_HIGH)

#define LCD_DC_Clr()        LzGpioSetVal(LCD_PIN_DC, LZGPIO_LEVEL_LOW)
#define LCD_DC_Set()        LzGpioSetVal(LCD_PIN_DC, LZGPIO_LEVEL_HIGH)

#if LCD_ENABLE_SPI
static SpiBusIo m_spiBus = {
    .cs =   {
        .gpio = GPIO0_PC0,
        .func = MUX_FUNC4,
        .type = PULL_UP,
        .drv = DRIVE_KEEP,
        .dir = LZGPIO_DIR_KEEP,
        .val = LZGPIO_LEVEL_KEEP
    },
    .clk =  {
        .gpio = GPIO0_PC1,
        .func = MUX_FUNC4,
        .type = PULL_UP,
        .drv = DRIVE_KEEP,
        .dir = LZGPIO_DIR_KEEP,
        .val = LZGPIO_LEVEL_KEEP
    },
    .mosi = {
        .gpio = GPIO0_PC2,
        .func = MUX_FUNC4,
        .type = PULL_UP,
        .drv = DRIVE_KEEP,
        .dir = LZGPIO_DIR_KEEP,
        .val = LZGPIO_LEVEL_KEEP
    },
    .miso = {
        .gpio = INVALID_GPIO,
        .func = MUX_FUNC4,
        .type = PULL_UP,
        .drv = DRIVE_KEEP,
        .dir = LZGPIO_DIR_KEEP,
        .val = LZGPIO_LEVEL_KEEP
    },
    .id = FUNC_ID_SPI0,
    .mode = FUNC_MODE_M1,
};

static LzSpiConfig m_spiConf = {
    .bitsPerWord = SPI_PERWORD_8BITS,
    .firstBit = SPI_MSB,
    .mode = SPI_MODE_3,
    .csm = SPI_CMS_ONE_CYCLES,
    .speed = 50000000,
    .isSlave = false,
};
#endif

/* 寄存器定义 */
#define REG_ADDRESS_COLUMN      0x2a
#define REG_ADDRESS_LINE        0x2B
#define REG_ADDRESS_WRITE       0x2C

/* 寄存器位数 */
#define REG_BITS_MAXSIZE        8

/* 寄存器最高位 */
#define REG_BITS_HIGH           (0x80)

/* sizey的字体单位大小 */
#define SIZEY_UNIT              8

/* uint16取值 */
#define UINT16_TO_H(val)        (((val) & 0xFF00) >> 8)
#define UINT16_TO_L(val)        ((val) & 0x00FF)

/* 字节转化为bits */
#define BYTE_TO_BITS            8

/* LCD配置模式 */
#define LCD_HORIZONTAL_MODE1    1
#define LCD_HORIZONTAL_MODE2    2

/* 中文转化为UTF-8的字节数 */
#define CHINESE_TO_BYTES        2

static void lcd_write_bus(uint8_t dat)
{
#if LCD_ENABLE_SPI
    LzSpiWrite(LCD_SPI_BUS, 0, &dat, 1);
#else
    uint8_t i;
    
    LCD_CS_Clr();
    for (i = 0; i < REG_BITS_MAXSIZE; i++) {
        LCD_CLK_Clr();
        if (dat & REG_BITS_HIGH) {
            LCD_MOSI_Set();
        } else {
            LCD_MOSI_Clr();
        }
        LCD_CLK_Set();
        dat <<= 1;
    }
    LCD_CS_Set();
#endif
}

static void lcd_wr_data8(uint8_t dat)
{
    lcd_write_bus(dat);
}

static void lcd_wr_data(uint16_t dat)
{
    lcd_write_bus(UINT16_TO_H(dat));
    lcd_write_bus(UINT16_TO_L(dat));
}

static void lcd_wr_reg(uint8_t dat)
{
    LCD_DC_Clr();
    lcd_write_bus(dat);
    LCD_DC_Set();
}

static void lcd_address_set(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
    /* 列地址设置 */
    lcd_wr_reg(REG_ADDRESS_COLUMN);
    lcd_wr_data(x1);
    lcd_wr_data(x2);
    /* 行地址设置 */
    lcd_wr_reg(REG_ADDRESS_LINE);
    lcd_wr_data(y1);
    lcd_wr_data(y2);
    /* 储存器写 */
    lcd_wr_reg(REG_ADDRESS_WRITE);
}

static uint32_t mypow(uint8_t m, uint8_t n)
{
    uint32_t result = 1;
    
    while (n--) {
        result *= m;
    }
    
    return result;
}


/***************************************************************
 * 函数名称: lcd_show_chinese_12x12
 * 说    明: 显示单个12x12汉字
 * 参    数:
 *       @x：指定汉字串的起始位置X坐标
 *       @y：指定汉字串的起始位置X坐标
 *       @s：指定汉字串（该汉字串为ascii）
 *       @fc: 字的颜色
 *       @bc: 字的背景色
 *       @sizey: 字号，可选：12
 *       @mode: 0为非叠加模式；1为叠加模式
 * 返 回 值: 无
 ***************************************************************/
static void lcd_show_chinese_12x12(uint16_t x,
    uint16_t y,
    uint8_t *s,
    uint16_t fc,
    uint16_t bc,
    uint8_t sizey,
    uint8_t mode)
{
    uint8_t i, j, m = 0;
    uint16_t k;
    uint16_t HZnum;         // 汉字数目
    uint16_t TypefaceNum;   // 一个字符所占字节大小
    uint16_t x0 = x;
    
    TypefaceNum = (sizey / BYTE_TO_BITS + ((sizey % biBYTE_TO_BITSts) ? 1 : 0)) * sizey;
    
    /* 统计汉字数目 */
    HZnum = sizeof(tfont12) / sizeof(typFNT_GB12);
    
    for (k = 0; k < HZnum; k++) {
        if ((tfont12[k].Index[0] == *(s)) && (tfont12[k].Index[1] == *(s + 1))) {
            lcd_address_set(x, y, x + sizey - 1, y + sizey - 1);
            for (i = 0; i < TypefaceNum; i++) {
                for (j = 0; j < BYTE_TO_BITS; j++) {
                    if (!mode) {
                        /* 非叠加方式 */
                        if (tfont12[k].Msk[i] & (0x01 << j)) {
                            lcd_wr_data(fc);
                        } else {
                            lcd_wr_data(bc);
                        }
                        
                        m++;
                        if ((m % sizey) == 0) {
                            m = 0;
                            break;
                        }
                    } else {
                        /* 叠加方式 */
                        if (tfont12[k].Msk[i] & (0x01 << j)) {
                            /* 画一个点 */
                            lcd_draw_point(x, y, fc);
                        }
                        
                        x++;
                        if ((x - x0) == sizey) {
                            x = x0;
                            y++;
                            break;
                        }
                    }
                }
            }
            
            break; /* 查找到对应点阵字库立即退出，防止多个汉字重复取模带来影响 */
        }
    }
}

/***************************************************************
 * 函数名称: lcd_show_chinese_16x16
 * 说    明: 显示单个16x16汉字
 * 参    数:
 *       @x：指定汉字串的起始位置X坐标
 *       @y：指定汉字串的起始位置X坐标
 *       @s：指定汉字串（该汉字串为ascii）
 *       @fc: 字的颜色
 *       @bc: 字的背景色
 *       @sizey: 字号，可选：16
 *       @mode: 0为非叠加模式；1为叠加模式
 * 返 回 值: 无
 ***************************************************************/
static void lcd_show_chinese_16x16(uint16_t x,
    uint16_t y,
    uint8_t *s,
    uint16_t fc,
    uint16_t bc,
    uint8_t sizey,
    uint8_t mode)
{
    uint8_t i, j, m = 0;
    uint16_t k;
    uint16_t HZnum;         /* 汉字数目 */
    uint16_t TypefaceNum;   /* 一个字符所占字节大小 */
    uint16_t x0 = x;
    
    TypefaceNum = (sizey / SIZEY_UNIT + ((sizey % SIZEY_UNIT) ? 1 : 0)) * sizey;
    /* 统计汉字数目 */
    HZnum = sizeof(tfont16) / sizeof(typFNT_GB16);
    
    for (k = 0; k < HZnum; k++) {
        if ((tfont16[k].Index[0] == *(s)) && (tfont16[k].Index[1] == *(s + 1))) {
            lcd_address_set(x, y, x + sizey - 1, y + sizey - 1);
            for (i = 0; i < TypefaceNum; i++) {
                for (j = 0; j < BYTE_TO_BITS; j++) {
                    if (!mode) {
                        /* 非叠加方式 */
                        if (tfont16[k].Msk[i] & (0x01 << j)) {
                            lcd_wr_data(fc);
                        } else {
                            lcd_wr_data(bc);
                        }
                        
                        m++;
                        if (m % sizey == 0) {
                            m = 0;
                            break;
                        }
                    } else {
                        /* 叠加方式 */
                        if (tfont16[k].Msk[i] & (0x01 << j)) {
                            /* 画一个点 */
                            lcd_draw_point(x, y, fc);
                        }
                        
                        x++;
                        if ((x - x0) == sizey) {
                            x = x0;
                            y++;
                            break;
                        }
                    }
                }
            }
            
            break; /* 查找到对应点阵字库立即退出，防止多个汉字重复取模带来影响 */
        }
    }
}


/***************************************************************
 * 函数名称: lcd_show_chinese_24x24
 * 说    明: 显示单个24x24汉字
 * 参    数:
 *       @x：指定汉字串的起始位置X坐标
 *       @y：指定汉字串的起始位置X坐标
 *       @s：指定汉字串（该汉字串为ascii）
 *       @fc: 字的颜色
 *       @bc: 字的背景色
 *       @sizey: 字号，可选：24
 *       @mode: 0为非叠加模式；1为叠加模式
 * 返 回 值: 无
 ***************************************************************/
static void lcd_show_chinese_24x24(uint16_t x,
    uint16_t y,
    uint8_t *s,
    uint16_t fc,
    uint16_t bc,
    uint8_t sizey,
    uint8_t mode)
{
    uint8_t i, j, m = 0;
    uint16_t k;
    uint16_t HZnum;         /* 汉字数目 */
    uint16_t TypefaceNum;   /* 一个字符所占字节大小 */
    uint16_t x0 = x;
    
    TypefaceNum = (sizey / BYTE_TO_BITS + ((sizey % BYTE_TO_BITS) ? 1 : 0)) * sizey;
    /* 统计汉字数目 */
    HZnum = sizeof(tfont24) / sizeof(typFNT_GB24);
    
    for (k = 0; k < HZnum; k++) {
        if ((tfont24[k].Index[0] == *(s)) && (tfont24[k].Index[1] == *(s + 1))) {
            lcd_address_set(x, y, x + sizey - 1, y + sizey - 1);
            for (i = 0; i < TypefaceNum; i++) {
                for (j = 0; j < BYTE_TO_BITS; j++) {
                    if (!mode) {
                        /* 非叠加方式 */
                        if (tfont24[k].Msk[i] & (0x01 << j)) {
                            lcd_wr_data(fc);
                        } else {
                            lcd_wr_data(bc);
                        }
                        
                        m++;
                        if (m % sizey == 0) {
                            m = 0;
                            break;
                        }
                    } else {
                        /* 叠加方式 */
                        if (tfont24[k].Msk[i] & (0x01 << j)) {
                            /* 画一个点 */
                            lcd_draw_point(x, y, fc);
                        }
                        
                        x++;
                        if ((x - x0) == sizey) {
                            x = x0;
                            y++;
                            break;
                        }
                    }
                }
            }
            
            break; /* 查找到对应点阵字库立即退出，防止多个汉字重复取模带来影响 */
        }
    }
}


/***************************************************************
 * 函数名称: lcd_show_chinese_32x32
 * 说    明: 显示单个32x32汉字
 * 参    数:
 *       @x：指定汉字串的起始位置X坐标
 *       @y：指定汉字串的起始位置X坐标
 *       @s：指定汉字串（该汉字串为ascii）
 *       @fc: 字的颜色
 *       @bc: 字的背景色
 *       @sizey: 字号，可选：32
 *       @mode: 0为非叠加模式；1为叠加模式
 * 返 回 值: 无
 ***************************************************************/
static void lcd_show_chinese_32x32(uint16_t x,
    uint16_t y,
    uint8_t *s,
    uint16_t fc,
    uint16_t bc,
    uint8_t sizey,
    uint8_t mode)
{
    uint8_t i, j, m = 0;
    uint16_t k;
    uint16_t HZnum;         /* 汉字数目 */
    uint16_t TypefaceNum;   /* 一个字符所占字节大小 */
    uint16_t x0 = x;
    
    TypefaceNum = (sizey / SIZEY_UNIT + ((sizey % SIZEY_UNIT) ? 1 : 0)) * sizey;
    /* 统计汉字数目 */
    HZnum = sizeof(tfont32) / sizeof(typFNT_GB32);
    
    for (k = 0; k < HZnum; k++) {
        if ((tfont32[k].Index[0] == *(s)) && (tfont32[k].Index[1] == *(s + 1))) {
            lcd_address_set(x, y, x + sizey - 1, y + sizey - 1);
            for (i = 0; i < TypefaceNum; i++) {
                for (j = 0; j < BYTE_TO_BITS; j++) {
                    if (!mode) {
                        /* 非叠加方式 */
                        if (tfont32[k].Msk[i] & (0x01 << j)) {
                            lcd_wr_data(fc);
                        } else {
                            lcd_wr_data(bc);
                        }
                        
                        m++;
                        if (m % sizey == 0) {
                            m = 0;
                            break;
                        }
                    } else {
                        /* 叠加方式 */
                        if (tfont32[k].Msk[i] & (0x01 << j)) {
                            lcd_draw_point(x, y, fc); /* 画一个点 */
                        }
                        
                        x++;
                        if ((x - x0) == sizey) {
                            x = x0;
                            y++;
                            break;
                        }
                    }
                }
            }
            
            break; /* 查找到对应点阵字库立即退出，防止多个汉字重复取模带来影响 */
        }
    }
}

/***************************************************************
 * 函数名称: lcd_init
 * 说    明: Lcd初始化
 * 参    数: 无
 * 返 回 值: 返回0为成功，反之为失败
 ***************************************************************/
unsigned int lcd_init(void)
{
    unsigned int delay_msec = 100;
    unsigned int long_delay_msec = 500;
#if LCD_ENABLE_SPI
    LzSpiDeinit(LCD_SPI_BUS);
    
    if (SpiIoInit(m_spiBus) != LZ_HARDWARE_SUCCESS) {
        printf("%s, %d: SpiIoInit failed!\n", __FILE__, __LINE__);
        return __LINE__;
    }
    if (LzSpiInit(LCD_SPI_BUS, m_spiConf) != LZ_HARDWARE_SUCCESS) {
        printf("%s, %d: LzSpiInit failed!\n", __FILE__, __LINE__);
        return __LINE__;
    }
#else
    /* 初始化GPIO0_C0 */
    LzGpioInit(LCD_PIN_CS);
    LzGpioSetDir(LCD_PIN_CS, LZGPIO_DIR_OUT);
    LzGpioSetVal(LCD_PIN_CS, LZGPIO_LEVEL_HIGH);
    /* 初始化GPIO0_C1 */
    LzGpioInit(LCD_PIN_CLK);
    LzGpioSetDir(LCD_PIN_CLK, LZGPIO_DIR_OUT);
    LzGpioSetVal(LCD_PIN_CLK, LZGPIO_LEVEL_LOW);
    /* 初始化GPIO0_C2 */
    LzGpioInit(LCD_PIN_MOSI);
    LzGpioSetDir(LCD_PIN_MOSI, LZGPIO_DIR_OUT);
    LzGpioSetVal(LCD_PIN_MOSI, LZGPIO_LEVEL_LOW);
#endif
    /* 初始化GPIO0_C3 */
    LzGpioInit(LCD_PIN_RES);
    LzGpioSetDir(LCD_PIN_RES, LZGPIO_DIR_OUT);
    LzGpioSetVal(LCD_PIN_RES, LZGPIO_LEVEL_HIGH);
    
    /* 初始化GPIO0_C6 */
    LzGpioInit(LCD_PIN_DC);
    LzGpioSetDir(LCD_PIN_DC, LZGPIO_DIR_OUT);
    LzGpioSetVal(LCD_PIN_DC, LZGPIO_LEVEL_LOW);
    
    /* 重启lcd */
    LCD_RES_Clr();
    LOS_Msleep(delay_msec);
    LCD_RES_Set();
    LOS_Msleep(delay_msec);
    LOS_Msleep(long_delay_msec);
    lcd_wr_reg(0x11);
    /* 等待LCD 100ms */
    LOS_Msleep(delay_msec);
    /* 启动LCD配置，设置显示和颜色配置 */
    lcd_wr_reg(0X36);
    if (USE_HORIZONTAL == 0) {
        lcd_wr_data8(0x00);
    } else if (USE_HORIZONTAL == LCD_HORIZONTAL_MODE1) {
        lcd_wr_data8(0xC0);
    } else if (USE_HORIZONTAL == LCD_HORIZONTAL_MODE2) {
        lcd_wr_data8(0x70);
    } else {
        lcd_wr_data8(0xA0);
    }
    lcd_wr_reg(0X3A);
    lcd_wr_data8(0X05);
    /* ST7789S帧刷屏率设置 */
    lcd_wr_reg(0xb2);
    lcd_wr_data8(0x0c);
    lcd_wr_data8(0x0c);
    lcd_wr_data8(0x00);
    lcd_wr_data8(0x33);
    lcd_wr_data8(0x33);
    lcd_wr_reg(0xb7);
    lcd_wr_data8(0x35);
    /* ST7789S电源设置 */
    lcd_wr_reg(0xbb);
    lcd_wr_data8(0x35);
    lcd_wr_reg(0xc0);
    lcd_wr_data8(0x2c);
    lcd_wr_reg(0xc2);
    lcd_wr_data8(0x01);
    lcd_wr_reg(0xc3);
    lcd_wr_data8(0x13);
    lcd_wr_reg(0xc4);
    lcd_wr_data8(0x20);
    lcd_wr_reg(0xc6);
    lcd_wr_data8(0x0f);
    lcd_wr_reg(0xca);
    lcd_wr_data8(0x0f);
    lcd_wr_reg(0xc8);
    lcd_wr_data8(0x08);
    lcd_wr_reg(0x55);
    lcd_wr_data8(0x90);
    lcd_wr_reg(0xd0);
    lcd_wr_data8(0xa4);
    lcd_wr_data8(0xa1);
    /* ST7789S gamma设置 */
    lcd_wr_reg(0xe0);
    lcd_wr_data8(0xd0);
    lcd_wr_data8(0x00);
    lcd_wr_data8(0x06);
    lcd_wr_data8(0x09);
    lcd_wr_data8(0x0b);
    lcd_wr_data8(0x2a);
    lcd_wr_data8(0x3c);
    lcd_wr_data8(0x55);
    lcd_wr_data8(0x4b);
    lcd_wr_data8(0x08);
    lcd_wr_data8(0x16);
    lcd_wr_data8(0x14);
    lcd_wr_data8(0x19);
    lcd_wr_data8(0x20);
    lcd_wr_reg(0xe1);
    lcd_wr_data8(0xd0);
    lcd_wr_data8(0x00);
    lcd_wr_data8(0x06);
    lcd_wr_data8(0x09);
    lcd_wr_data8(0x0b);
    lcd_wr_data8(0x29);
    lcd_wr_data8(0x36);
    lcd_wr_data8(0x54);
    lcd_wr_data8(0x4b);
    lcd_wr_data8(0x0d);
    lcd_wr_data8(0x16);
    lcd_wr_data8(0x14);
    lcd_wr_data8(0x21);
    lcd_wr_data8(0x20);
    lcd_wr_reg(0x29);
    
    return 0;
}


/***************************************************************
 * 函数名称: lcd_deinit
 * 说    明: Lcd注销
 * 参    数: 无
 * 返 回 值: 返回0为成功，反之为失败
 ***************************************************************/
unsigned int lcd_deinit(void)
{
#if LCD_ENABLE_SPI
    LzSpiDeinit(LCD_SPI_BUS);
#else
    LzGpioDeinit(LCD_PIN_CS);
    LzGpioDeinit(LCD_PIN_CLK);
    LzGpioDeinit(LCD_PIN_MOSI);
#endif
    LzGpioDeinit(LCD_PIN_RES);
    LzGpioDeinit(LCD_PIN_DC);
    
    return 0;
}


/***************************************************************
 * 函数名称: lcd_fill
 * 说    明: 指定区域填充颜色
 * 参    数:
 *       @xsta：指定区域的起始点X坐标
 *       @ysta：指定区域的起始点Y坐标
 *       @xend：指定区域的结束点X坐标
 *       @yend：指定区域的结束点Y坐标
 *       @color：指定区域的颜色
 * 返 回 值: 无
 ***************************************************************/
void lcd_fill(uint16_t xsta, uint16_t ysta, uint16_t xend, uint16_t yend, uint16_t color)
{
    uint16_t i, j;
    
    /* 设置显示范围 */
    lcd_address_set(xsta, ysta, xend - 1, yend - 1);
    /* 填充颜色 */
    for (i = ysta; i < yend; i++) {
        for (j = xsta; j < xend; j++) {
            lcd_wr_data(color);
        }
    }
}


/***************************************************************
 * 函数名称: lcd_draw_point
 * 说    明: 指定位置画一个点
 * 参    数:
 *       @x：指定点的X坐标
 *       @y：指定点的Y坐标
 *       @color：指定点的颜色
 * 返 回 值: 无
 ***************************************************************/
void lcd_draw_point(uint16_t x, uint16_t y, uint16_t color)
{
    /* 设置光标位置 */
    lcd_address_set(x, y, x, y);
    lcd_wr_data(color);
}


/***************************************************************
 * 函数名称: lcd_draw_line
 * 说    明: 指定位置画一条线
 * 参    数:
 *       @x1：指定线的起始点X坐标
 *       @y1：指定线的起始点Y坐标
 *       @x2：指定线的结束点X坐标
 *       @y2：指定线的结束点Y坐标
 *       @color：指定点的颜色
 * 返 回 值: 无
 ***************************************************************/
void lcd_draw_line(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color)
{
    uint16_t t;
    int xerr = 0, yerr = 0, delta_x, delta_y, distance;
    int incx, incy, uRow, uCol;
    
    /* 计算坐标增量 */
    delta_x = x2 - x1;
    delta_y = y2 - y1;
    /* 画线起点坐标 */
    uRow = x1;
    uCol = y1;
    
    if (delta_x > 0) {
        /* 设置单步方向 */
        incx = 1;
    } else if (delta_x == 0) {
        /* 垂直线 */
        incx = 0;
    } else {
        incx = -1;
        delta_x = -delta_x;
    }
    
    if (delta_y > 0) {
        incy = 1;
    } else if (delta_y == 0) {
        /* 水平线 */
        incy = 0;
    } else {
        incy = -1;
        delta_y = -delta_y;
    }
    
    if (delta_x > delta_y) {
        /* 选取基本增量坐标轴 */
        distance = delta_x;
    } else {
        distance = delta_y;
    }
    
    for (t = 0; t < distance + 1; t++) {
        /* 画点 */
        lcd_draw_point(uRow, uCol, color);
        xerr += delta_x;
        yerr += delta_y;
        if (xerr > distance) {
            xerr -= distance;
            uRow += incx;
        }
        if (yerr > distance) {
            yerr -= distance;
            uCol += incy;
        }
    }
}


/***************************************************************
 * 函数名称: lcd_draw_rectangle
 * 说    明: 指定位置画矩形
 * 参    数:
 *       @x1：指定矩形的起始点X坐标
 *       @y1：指定矩形的起始点Y坐标
 *       @x2：指定矩形的结束点X坐标
 *       @y2：指定矩形的结束点Y坐标
 *       @color：指定点的颜色
 * 返 回 值: 无
 ***************************************************************/
void lcd_draw_rectangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color)
{
    lcd_draw_line(x1, y1, x2, y1, color);
    lcd_draw_line(x1, y1, x1, y2, color);
    lcd_draw_line(x1, y2, x2, y2, color);
    lcd_draw_line(x2, y1, x2, y2, color);
}


/***************************************************************
 * 函数名称: lcd_draw_circle
 * 说    明: 指定位置画圆
 * 参    数:
 *       @x0：指定圆的中心点X坐标
 *       @y0：指定圆的中心点Y坐标
 *       @r：指定圆的半径
 *       @color：指定点的颜色
 * 返 回 值: 无
 ***************************************************************/
void lcd_draw_circle(uint16_t x0, uint16_t y0, uint8_t r, uint16_t color)
{
    int a, b;
    
    a = 0;
    b = r;
    
    while (a <= b) {
        lcd_draw_point(x0 - b, y0 - a, color);
        lcd_draw_point(x0 + b, y0 - a, color);
        lcd_draw_point(x0 - a, y0 + b, color);
        lcd_draw_point(x0 - a, y0 - b, color);
        lcd_draw_point(x0 + b, y0 + a, color);
        lcd_draw_point(x0 + a, y0 - b, color);
        lcd_draw_point(x0 + a, y0 + b, color);
        lcd_draw_point(x0 - b, y0 + a, color);
        a++;
        /* 判断要画的点是否过远 */
        if ((a * a + b * b) > (r * r)) {
            b--;
        }
    }
}


/***************************************************************
 * 函数名称: lcd_show_chinese
 * 说    明: 显示汉字串
 * 参    数:
 *       @x：指定汉字串的起始位置X坐标
 *       @y：指定汉字串的起始位置X坐标
 *       @s：指定汉字串（该汉字串为utf-8）
 *       @fc: 字的颜色
 *       @bc: 字的背景色
 *       @sizey: 字号，可选：12、16、24、32
 *       @mode: 0为非叠加模式；1为叠加模式
 * 返 回 值: 无
 ***************************************************************/
void lcd_show_chinese(uint16_t x, uint16_t y, uint8_t *s, uint16_t fc, uint16_t bc, uint8_t sizey, uint8_t mode)
{
    uint8_t buffer[128];
    uint32_t buffer_len = 0;
    uint32_t len = strlen(s);
    
    memset(buffer, 0, sizeof(buffer));
    /* utf8格式汉字转化为ascii格式 */
    chinese_utf8_to_ascii(s, strlen(s), buffer, &buffer_len);
    
    for (uint32_t i = 0; i < buffer_len; i += CHINESE_TO_BYTES, x += sizey) {
        if (sizey == LCD_FONT_SIZE12) {
            lcd_show_chinese_12x12(x, y, &buffer[i], fc, bc, sizey, mode);
        } else if (sizey == LCD_FONT_SIZE16) {
            lcd_show_chinese_16x16(x, y, &buffer[i], fc, bc, sizey, mode);
        } else if (sizey == LCD_FONT_SIZE24) {
            lcd_show_chinese_24x24(x, y, &buffer[i], fc, bc, sizey, mode);
        } else if (sizey == LCD_FONT_SIZE32) {
            lcd_show_chinese_32x32(x, y, &buffer[i], fc, bc, sizey, mode);
        } else {
            return;
        }
    }
}


/***************************************************************
 * 函数名称: lcd_show_char
 * 说    明: 显示一个字符
 * 参    数:
 *       @x：指定字符的起始位置X坐标
 *       @y：指定字串的起始位置X坐标
 *       @s：指定字串
 *       @fc: 字的颜色
 *       @bc: 字的背景色
 *       @sizey: 字号，可选：16、24、32
 *       @mode: 0为非叠加模式；1为叠加模式
 * 返 回 值: 无
 ***************************************************************/
void lcd_show_char(uint16_t x, uint16_t y, uint8_t num, uint16_t fc, uint16_t bc, uint8_t sizey, uint8_t mode)
{
    uint8_t temp, sizex, t, m = 0;
    uint16_t i;
    uint16_t TypefaceNum; /* 一个字符所占字节大小 */
    uint16_t x0 = x;
    uint16_t size_y2x = 2;
    
    sizex = sizey / size_y2x;
    TypefaceNum = (sizex / BYTE_TO_BITS + ((sizex % BYTE_TO_BITS) ? 1 : 0)) * sizey;
    
    /* 得到偏移后的值 */
    num = num - ' ';
    /* 设置光标位置 */
    lcd_address_set(x, y, x + sizex - 1, y + sizey - 1);
    
    for (i = 0; i < TypefaceNum; i++) {
        if (sizey == LCD_FONT_SIZE12) {
            /* 调用6x12字体 */
            temp = ascii_1206[num][i];
        } else if (sizey == LCD_FONT_SIZE16) {
            /* 调用8x16字体 */
            temp = ascii_1608[num][i];
        } else if (sizey == LCD_FONT_SIZE24) {
            /* 调用12x24字体 */
            temp = ascii_2412[num][i];
        } else if (sizey == LCD_FONT_SIZE32) {
            /* 调用16x32字体 */
            temp = ascii_3216[num][i];
        } else {
            return;
        }
        
        for (t = 0; t < BYTE_TO_BITS; t++) {
            if (!mode) {
                /* 非叠加模式 */
                if (temp & (0x01 << t)) {
                    lcd_wr_data(fc);
                } else {
                    lcd_wr_data(bc);
                }
                
                m++;
                if (m % sizex == 0) {
                    m = 0;
                    break;
                }
            } else {
                /* 叠加模式 */
                if (temp & (0x01 << t)) {
                    /* 画一个点 */
                    lcd_draw_point(x, y, fc);
                }
                
                x++;
                if ((x - x0) == sizex) {
                    x = x0;
                    y++;
                    break;
                }
            }
        }
    }
}


/***************************************************************
 * 函数名称: lcd_show_string
 * 说    明: 显示字符串
 * 参    数:
 *       @x：指定字符的起始位置X坐标
 *       @y：指定字串的起始位置Y坐标
 *       @s：指定字串
 *       @fc: 字的颜色
 *       @bc: 字的背景色
 *       @sizey: 字号，可选：16、24、32
 *       @mode: 0为非叠加模式；1为叠加模式
 * 返 回 值: 无
 ***************************************************************/
void lcd_show_string(uint16_t x, uint16_t y, const uint8_t *p, uint16_t fc, uint16_t bc, uint8_t sizey, uint8_t mode)
{
#define X_OFFSET(sizey)         ((sizey) / 2) /* 根据字体大小偏移X轴坐标 */
    while (*p != '\0') {
        lcd_show_char(x, y, *p, fc, bc, sizey, mode);
        x += X_OFFSET(sizey);
        p++;
    }
}


/***************************************************************
 * 函数名称: lcd_show_int_num
 * 说    明: 显示整数变量
 * 参    数:
 *       @x：指定整数变量的起始位置X坐标
 *       @y：指定整数变量的起始位置X坐标
 *       @num：指定整数变量
 *       @fc: 整数变量的颜色
 *       @bc: 整数变量的背景色
 *       @sizey: 字号，可选：16、24、32
 * 返 回 值: 无
 ***************************************************************/
void lcd_show_int_num(uint16_t x, uint16_t y, uint16_t num, uint8_t len, uint16_t fc, uint16_t bc, uint8_t sizey)
{
    uint8_t base = 48; /* ASCII字符'0' */
    uint8_t power_base = 10;
    uint8_t power_remainder = 10;
    uint8_t t, temp;
    uint8_t enshow = 0;
    uint8_t sizex = sizey / 2;
    
    for (t = 0; t < len; t++) {
        temp = (num / mypow(power_base, len - t - 1)) % power_remainder;
        if (enshow == 0 && t < (len - 1)) {
            if (temp == 0) {
                lcd_show_char(x + t * sizex, y, ' ', fc, bc, sizey, 0);
                continue;
            } else {
                enshow = 1;
            }
        }
        lcd_show_char(x + t * sizex, y, temp + base, fc, bc, sizey, 0);
    }
}


/***************************************************************
 * 函数名称: lcd_show_float_num1
 * 说    明: 显示两位小数变量
 * 参    数:
 *       @x：指定浮点变量的起始位置X坐标
 *       @y：指定浮点变量的起始位置X坐标
 *       @num：指定浮点变量
 *       @fc: 浮点变量的颜色
 *       @bc: 浮点变量的背景色
 *       @sizey: 字号，可选：16、24、32
 * 返 回 值: 无
 ***************************************************************/
void lcd_show_float_num1(uint16_t x, uint16_t y, float num, uint8_t len, uint16_t fc, uint16_t bc, uint8_t sizey)
{
#define X_OFFSET(sizey)         ((sizey) / 2)   /* 根据字体大小偏移X坐标移动点数 */
#define FLOAT_TO_INT(num)       ((num) * 100)   /* 将float数值转化为int数值 */
#define DECIMAL_TOW             2               /* 保留小数点后2个数值 */
#define CHAR_0                  ((uint8_t)('0'))
    uint8_t t, temp, sizex;
    uint16_t num1;
    
    sizex = X_OFFSET(sizey);
    num1 = FLOAT_TO_INT(num);
    for (t = 0; t < len; t++) {
        temp = (num1 / mypow(10, len - t - 1)) % 10;
        if (t == (len - DECIMAL_TOW)) {
            lcd_show_char(x + (len - DECIMAL_TOW)*sizex, y, '.', fc, bc, sizey, 0);
            t++;
            len += 1;
        }
        lcd_show_char(x + t * sizex, y, temp + CHAR_0, fc, bc, sizey, 0);
    }
}

/***************************************************************
 * 函数名称: lcd_show_picture
 * 说    明: 显示图片
 * 参    数:
 *       @x：指定图片的起始位置X坐标
 *       @y：指定图片的起始位置X坐标
 *       @length：指定图片的长度
 *       @width：指定图片的宽度
 *       @pic：指定图片的内容
 * 返 回 值: 无
 ***************************************************************/
void lcd_show_picture(uint16_t x, uint16_t y, uint16_t length, uint16_t width, const uint8_t *pic)
{
    uint16_t i, j;
    uint32_t k = 0;
    uint32_t unit = 2;
    
    lcd_address_set(x, y, x + length - 1, y + width - 1);
    for (i = 0; i < length; i++) {
        for (j = 0; j < width; j++) {
            lcd_wr_data8(pic[k * unit]);
            lcd_wr_data8(pic[k * unit + 1]);
            k++;
        }
    }
}
