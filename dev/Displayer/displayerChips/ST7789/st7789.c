//
// Created by dxxdx on 2026/8/6.
//

#include "st7789.h"
#include "st7789.h"

static void Delay_Static(uint32_t delay)
{
    for (volatile uint32_t i = 0U; i < (400U * delay); i++)
    {
        __asm volatile("nop");
    }
}

static void ST7789_SendCommandDataU8(
    ST7789 *self,
    uint8_t command,
    const uint8_t *data,
    uint32_t dataLength)
{
    ST7789_SendCommand(self, &command, 1U);
    ST7789_SendData(self, data, dataLength);
}

static void ST7789_InitSequence(ST7789 *self)
{
    static const uint8_t memoryDataAccessControl[] = {
        ST7789_MADCTL_VALUE
    };
    static const uint8_t interfacePixelFormat[] = {
        0x55U
    };
    static const uint8_t porchControl[] = {
        0x0CU, 0x0CU, 0x00U, 0x33U, 0x33U
    };
    static const uint8_t gateControl[] = {
        0x74U
    };
    static const uint8_t vcomSetting[] = {
        0x1FU
    };
    static const uint8_t lcmControl[] = {
        0x2CU
    };
    static const uint8_t vdvVrhCommandEnable[] = {
        0x01U
    };
    static const uint8_t vrhSet[] = {
        0x10U
    };
    static const uint8_t vdvSetting[] = {
        0x20U
    };
    static const uint8_t frameRateControl[] = {
        0x0FU
    };
    static const uint8_t powerControl[] = {
        0xA4U, 0xA1U
    };
    static const uint8_t gateOutputSelection[] = {
        0xA1U
    };
    static const uint8_t positiveGamma[] = {
        0xD0U, 0x07U, 0x0EU, 0x0BU,
        0x0AU, 0x14U, 0x38U, 0x33U,
        0x4FU, 0x37U, 0x16U, 0x16U,
        0x2AU, 0x2EU
    };
    static const uint8_t negativeGamma[] = {
        0xD0U, 0x0BU, 0x10U, 0x08U,
        0x08U, 0x06U, 0x35U, 0x54U,
        0x4DU, 0x0AU, 0x14U, 0x14U,
        0x2CU, 0x2FU
    };
    static const uint8_t equalizeTimeControl[] = {
        0x11U, 0x11U, 0x03U
    };

    uint8_t command;

    /* CS=1, DC=1 and RESET=1 before applying reset. */
    *(self->io->CTRL_BSHR) = self->io->end_send;
    *(self->io->RST_BSHR) = self->io->rst_high;
    Delay_Static(500U);

    *(self->io->RST_BSHR) = self->io->rst_low;
    Delay_Static(100U);

    *(self->io->RST_BSHR) = self->io->rst_high;

    /*
     * The datasheet requires at least 5 ms before ordinary commands and
     * forbids SLPOUT during the first 120 ms following hardware reset.
     */
    Delay_Static(6000U);

    command = 0x11U;
    ST7789_SendCommand(self, &command, 1U);

    /* SLPOUT requires another 120 ms before display configuration. */
    Delay_Static(6000U);

    ST7789_SendCommandDataU8(
        self, 0x36U,
        memoryDataAccessControl,
        sizeof(memoryDataAccessControl));

    ST7789_SendCommandDataU8(
        self, 0x3AU,
        interfacePixelFormat,
        sizeof(interfacePixelFormat));

    ST7789_SendCommandDataU8(
        self, 0xB2U,
        porchControl,
        sizeof(porchControl));

    ST7789_SendCommandDataU8(
        self, 0xB7U,
        gateControl,
        sizeof(gateControl));

    ST7789_SendCommandDataU8(
        self, 0xBBU,
        vcomSetting,
        sizeof(vcomSetting));

    ST7789_SendCommandDataU8(
        self, 0xC0U,
        lcmControl,
        sizeof(lcmControl));

    ST7789_SendCommandDataU8(
        self, 0xC2U,
        vdvVrhCommandEnable,
        sizeof(vdvVrhCommandEnable));

    ST7789_SendCommandDataU8(
        self, 0xC3U,
        vrhSet,
        sizeof(vrhSet));

    ST7789_SendCommandDataU8(
        self, 0xC4U,
        vdvSetting,
        sizeof(vdvSetting));

    ST7789_SendCommandDataU8(
        self, 0xC6U,
        frameRateControl,
        sizeof(frameRateControl));

    ST7789_SendCommandDataU8(
        self, 0xD0U,
        powerControl,
        sizeof(powerControl));

    ST7789_SendCommandDataU8(
        self, 0xD6U,
        gateOutputSelection,
        sizeof(gateOutputSelection));

    ST7789_SendCommandDataU8(
        self, 0xE0U,
        positiveGamma,
        sizeof(positiveGamma));

    ST7789_SendCommandDataU8(
        self, 0xE1U,
        negativeGamma,
        sizeof(negativeGamma));

    ST7789_SendCommandDataU8(
        self, 0xE9U,
        equalizeTimeControl,
        sizeof(equalizeTimeControl));

    command = 0x21U;
    ST7789_SendCommand(self, &command, 1U);

    command = 0x29U;
    ST7789_SendCommand(self, &command, 1U);
    Delay_Static(1000U);
}

void ST7789_SetPos(
    ST7789 *self,
    uint16_t x0,
    uint16_t y0,
    uint16_t x1,
    uint16_t y1)
{
    if ((x0 > x1) ||
        (y0 > y1) ||
        (x1 >= ST7789_WIDTH_PIXELS) ||
        (y1 >= ST7789_HEIGHT_PIXELS))
    {
        while (1) __asm__ volatile("nop");
    }

    if (self->SPIMode == ST7789_SPI_U8_MODE)
    {
        uint8_t command;
        uint8_t arguments[4];

        command = 0x2AU;
        ST7789_SendCommand(self, &command, 1U);

        arguments[0] = (uint8_t)(x0 >> 8U);
        arguments[1] = (uint8_t)x0;
        arguments[2] = (uint8_t)(x1 >> 8U);
        arguments[3] = (uint8_t)x1;
        ST7789_SendData(self, arguments, 4U);

        command = 0x2BU;
        ST7789_SendCommand(self, &command, 1U);

        arguments[0] = (uint8_t)(y0 >> 8U);
        arguments[1] = (uint8_t)y0;
        arguments[2] = (uint8_t)(y1 >> 8U);
        arguments[3] = (uint8_t)y1;
        ST7789_SendData(self, arguments, 4U);

        command = 0x2CU;
        ST7789_SendCommand(self, &command, 1U);
    }
    else if (self->SPIMode == ST7789_SPI_U16_MODE)
    {
        uint16_t command;
        uint16_t arguments[2];

        command = 0x002AU;
        ST7789_SendCommand(self, &command, 1U);

        arguments[0] = x0;
        arguments[1] = x1;
        ST7789_SendData(self, arguments, 2U);

        command = 0x002BU;
        ST7789_SendCommand(self, &command, 1U);

        arguments[0] = y0;
        arguments[1] = y1;
        ST7789_SendData(self, arguments, 2U);

        command = 0x002CU;
        ST7789_SendCommand(self, &command, 1U);
    }
    else
    {
        while (1) __asm__ volatile("nop");
    }
}

static void ST7789_SetScrollOffset(
    ST7789 *self,
    uint16_t offset)
{
    if (offset >= ST7789_HEIGHT_PIXELS)
    {
        while (1) __asm__ volatile("nop");
    }

    if (self->SPIMode == ST7789_SPI_U8_MODE)
    {
        const uint8_t command = 0x37U;
        const uint8_t arguments[2] = {
            (uint8_t)(offset >> 8U),
            (uint8_t)offset
        };

        ST7789_SendCommand(self, &command, 1U);
        ST7789_SendData(self, arguments, 2U);
    }
    else if (self->SPIMode == ST7789_SPI_U16_MODE)
    {
        const uint16_t command = 0x0037U;
        const uint16_t argument = offset;

        ST7789_SendCommand(self, &command, 1U);
        ST7789_SendData(self, &argument, 1U);
    }
    else
    {
        while (1) __asm__ volatile("nop");
    }

    self->scrollOffset = offset;
}

static void ST7789_InitScrollArea(ST7789 *self)
{
    if (self->SPIMode == ST7789_SPI_U8_MODE)
    {
        const uint8_t command = 0x33U;
        const uint8_t arguments[6] = {
            0x00U, 0x00U,
            0x01U, 0x40U,
            0x00U, 0x00U
        };

        ST7789_SendCommand(self, &command, 1U);
        ST7789_SendData(self, arguments, 6U);
    }
    else if (self->SPIMode == ST7789_SPI_U16_MODE)
    {
        const uint16_t command = 0x0033U;
        const uint16_t arguments[3] = {
            0U,
            ST7789_HEIGHT_PIXELS,
            0U
        };

        ST7789_SendCommand(self, &command, 1U);
        ST7789_SendData(self, arguments, 3U);
    }
    else
    {
        while (1) __asm__ volatile("nop");
    }

    ST7789_SetScrollOffset(self, 0U);
}

uint16_t ST7789_Scroll8(ST7789 *self)
{
    uint16_t nextOffset;

#if ((ST7789_MADCTL_VALUE & 0x80U) != 0U)
    if (self->scrollOffset >= ST7789_TEXT_ROW_HEIGHT_PIXELS)
    {
        nextOffset =
            self->scrollOffset - ST7789_TEXT_ROW_HEIGHT_PIXELS;
    }
    else
    {
        nextOffset =
            self->scrollOffset +
            ST7789_HEIGHT_PIXELS -
            ST7789_TEXT_ROW_HEIGHT_PIXELS;
    }
#else
    nextOffset =
        self->scrollOffset + ST7789_TEXT_ROW_HEIGHT_PIXELS;

    if (nextOffset >= ST7789_HEIGHT_PIXELS)
    {
        nextOffset -= ST7789_HEIGHT_PIXELS;
    }
#endif

    ST7789_SetScrollOffset(self, nextOffset);

    uint16_t exposedY =
        nextOffset +
        ST7789_HEIGHT_PIXELS -
        ST7789_TEXT_ROW_HEIGHT_PIXELS;

    if (exposedY >= ST7789_HEIGHT_PIXELS)
    {
        exposedY -= ST7789_HEIGHT_PIXELS;
    }

    return exposedY;
}

void ST7789_PrepareColorLine(uint16_t color)
{
    uint32_t *destination =
        (uint32_t *)(void *)line_buffer;

    uint32_t * const end =
        destination + (ST7789_LINE_BUFFER_PIXELS / 2U);

    const uint32_t packed =
        (uint32_t)color |
        ((uint32_t)color << 16U);

    while (destination != end)
    {
        *destination++ = packed;
    }
}

void ST7789_FillColor(
    ST7789 *self,
    uint16_t color)
{
    if (self->SPIMode != ST7789_SPI_U16_MODE)
    {
        while (1) __asm__ volatile("nop");
    }

    ST7789_PrepareColorLine(color);
    ST7789_SetScrollOffset(self, 0U);

    ST7789_SetPos(
        self,
        0U,
        0U,
        ST7789_WIDTH_PIXELS - 1U,
        ST7789_HEIGHT_PIXELS - 1U);

    for (uint32_t block = 0U;
         block < (ST7789_HEIGHT_PIXELS /
                  ST7789_TEXT_ROW_HEIGHT_PIXELS);
         block++)
    {
        ST7789_SendData(
            self,
            line_buffer,
            ST7789_LINE_BUFFER_PIXELS);
    }
}

extern void st7789_CH32_HalfWord_Switch(ST7789 *self);

void ST7789_Init(ST7789 *self)
{
    ST7789_InitSequence(self);
    ST7789_InitScrollArea(self);

    st7789_CH32_HalfWord_Switch(self);

    ST7789_FillColor(self, 0x0000U);
}