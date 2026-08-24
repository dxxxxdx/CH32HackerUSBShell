#ifndef ST7789_H
#define ST7789_H

#include <stdint.h>

#define ST7789_WIDTH_PIXELS              240U
#define ST7789_HEIGHT_PIXELS             320U
#define ST7789_TEXT_ROW_HEIGHT_PIXELS      8U
#define ST7789_LINE_BUFFER_PIXELS       1920U

/*
 * BOE 3.2-inch initialization code uses the native portrait direction.
 *
 * MY = 0, MX = 0, MV = 0, ML = 0, RGB = 0.
 * MV must remain 0 when the vertical scroll registers are used.
 */
#define ST7789_MADCTL_VALUE              0x00U

typedef struct ST7789_IO
{
    volatile uint32_t * const CTRL_BSHR;
    volatile uint32_t * const RST_BSHR;

    const uint32_t begin_data;
    const uint32_t begin_cmd;
    const uint32_t end_send;

    const uint32_t rst_low;
    const uint32_t rst_high;
} ST7789_IO;

typedef enum ST7789_Status
{
    ST7789_IDLE = 0,
    ST7789_SENDING_DATA = 1,
    ST7789_SENDING_CMD = 2,
    ST7789_DMA_SENDING_DATA = 3,
} ST7789_Status;

typedef enum ST7789_SPIMode
{
    ST7789_SPI_U8_MODE = 0,
    ST7789_SPI_U16_MODE = 1,
} ST7789_SPIMode;

typedef struct ST7789
{
    const ST7789_IO *io;

    void (*const transmitBlock)(
        struct ST7789 *self,
        const void *bitstream,
        uint32_t lengthByMode);

    void (*const transmitDMA_U16)(
        struct ST7789 *self,
        uint16_t *bitstream,
        uint32_t u16Length);

    void (*const transmit_callback)(
        struct ST7789 *self);

    volatile uint8_t status;
    uint8_t SPIMode;
    volatile uint16_t scrollOffset;
} ST7789;

__attribute__((always_inline))
static inline void ST7789_DMA_SendData(
    ST7789 *self,
    uint16_t *bitstream,
    uint32_t u16Length)
{
    while (self->status != ST7789_IDLE);

    self->status = ST7789_DMA_SENDING_DATA;
    *(self->io->CTRL_BSHR) = self->io->begin_data;
    self->transmitDMA_U16(self, bitstream, u16Length);
}

__attribute__((always_inline))
static inline void ST7789_SendData(
    ST7789 *self,
    const void *data,
    uint32_t lengthByMode)
{
    while (self->status != ST7789_IDLE);

    self->status = ST7789_SENDING_DATA;
    *(self->io->CTRL_BSHR) = self->io->begin_data;
    self->transmitBlock(self, data, lengthByMode);
}

__attribute__((always_inline))
static inline void ST7789_SendCommand(
    ST7789 *self,
    const void *command,
    uint32_t lengthByMode)
{
    while (self->status != ST7789_IDLE);

    self->status = ST7789_SENDING_CMD;
    *(self->io->CTRL_BSHR) = self->io->begin_cmd;
    self->transmitBlock(self, command, lengthByMode);
}

__attribute__((always_inline))
static inline void ST7789_TransmitComplete(
    ST7789 *self)
{
    *(self->io->CTRL_BSHR) = self->io->end_send;
    self->status = ST7789_IDLE;
}

void ST7789_Init(ST7789 *self);

void ST7789_SetPos(
    ST7789 *self,
    uint16_t x0,
    uint16_t y0,
    uint16_t x1,
    uint16_t y1);

uint16_t ST7789_Scroll8(ST7789 *self);

void ST7789_PrepareColorLine(uint16_t color);

void ST7789_FillColor(
    ST7789 *self,
    uint16_t color);

extern uint16_t line_buffer[ST7789_LINE_BUFFER_PIXELS]
    __attribute__((aligned(4)));

extern ST7789 st7789;

#endif