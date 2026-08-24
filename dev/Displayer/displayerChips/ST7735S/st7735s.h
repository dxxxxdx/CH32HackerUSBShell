//
// Created by zys on 26-5-2.
//

#ifndef ST7735S_H
#define ST7735S_H

#include "displayerChipConfig.h"

#if (DISPLAYER_CHIP_USE_ST7735S != 0U)

#include <stdint.h>

typedef struct ST7735_IO
{
    volatile uint32_t * const CTRL_BSHR;  /* CS + DC 同一端口 */
    volatile uint32_t * const RST_BSHR;

    const uint32_t begin_data;  /* CS=0，DC 保持 1 */
    const uint32_t begin_cmd;   /* CS=0，DC=0 */
    const uint32_t end_send;    /* CS=1，DC=1 */

    const uint32_t rst_low;
    const uint32_t rst_high;
} ST7735_IO;

typedef enum ST7735S_Status
{
    ST7735S_IDLE = 0,
    ST7735S_SENDING_DATA = 1,
    ST7735S_SENDING_CMD = 2,
    ST7735S_DMA_SENDING_DATA = 3,
}ST7735S_Status;
typedef enum  ST7735S_SPIMode
{
    SPI_U8_MODE = 0,
    SPI_U16_MODE = 1,
}ST7735S_SPIMode;

typedef struct ST7735S {

    const ST7735_IO* io;
    void (*const transmitBlock)(struct ST7735S *self,const void* bitstream,uint32_t LengthByMode);
    //这玩意是给数据走的通道，如果你不想要或者单片机压根没这个功能，直接自己改签名，之后也别换16位模式了
    //丢给ai让他裁就行
    void (*const  transmitDMA_U16)(struct ST7735S *self,uint16_t* bitstream,uint32_t u16length);
    void (*const  transmit_callback)(struct ST7735S *self);
    volatile uint8_t status ;
    uint8_t SPIMode;
    volatile uint16_t scrollOffset;
} ST7735S;



__attribute__((always_inline))
static inline void ST7735_DMA_SendData(ST7735S *self,uint16_t* bitstream,uint32_t u16length)
{
    if (self->status != ST7735S_IDLE) {
        while (self->status != ST7735S_IDLE) ;
    }
    self->status = ST7735S_DMA_SENDING_DATA;
    /* 一次 MMIO：只把 CS 拉低，DC 保持高 */
    *(self->io->CTRL_BSHR) = self->io->begin_data;

    self->transmitDMA_U16(self, bitstream, u16length);

}

__attribute__((always_inline))
static inline void ST7735_SendData(
    ST7735S *self,
    const void *data,
    uint32_t lengthByMode)
{
    while(self->status != ST7735S_IDLE);

    self->status = ST7735S_SENDING_DATA;
    *(self->io->CTRL_BSHR) = self->io->begin_data;
    self->transmitBlock(self, data, lengthByMode);
}

__attribute__((always_inline))
static inline void ST7735_SendCommand(
    ST7735S *self,
    const void *cmd,
    uint32_t lengthByMode)
{
    while(self->status != ST7735S_IDLE);

    self->status = ST7735S_SENDING_CMD;
    *(self->io->CTRL_BSHR) = self->io->begin_cmd;
    self->transmitBlock(self, cmd, lengthByMode);
}

__attribute__((always_inline))
static inline void ST7735_TransmitComplete(ST7735S *self)
{
    /* 一次 MMIO：CS、DC 无脑全部拉高 */
    *(self->io->CTRL_BSHR) = self->io->end_send;
    self->status = ST7735S_IDLE;
}


void ST7735S_Init(ST7735S *self);

void ST7735S_SetPos(
    ST7735S *self,
    uint16_t x0,
    uint16_t y0,
    uint16_t x1,
    uint16_t y1
);

/*
 * 向上滚动 8 像素。
 * 返回刚刚从屏幕底部露出来的 8 像素区域在 GRAM 中的起始 Y。
 */
uint16_t ST7735S_Scroll8(ST7735S *self);
void ST7735S_PrepareColorLine(uint16_t color);
void ST7735S_FillColor(ST7735S *self, uint16_t color);
extern ST7735S st7735s;






























#endif /* DISPLAYER_CHIP_USE_ST7735S */

#endif /* ST7735S_H */
