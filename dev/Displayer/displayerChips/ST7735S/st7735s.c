#include "displayerChipConfig.h"

#if (DISPLAYER_CHIP_USE_ST7735S != 0U)

#include "st7735s.h"


#define ST7735S_WIDTH          128U
#define ST7735S_HEIGHT         160U

#define ST7735S_X_OFFSET         0U
#define ST7735S_Y_OFFSET         0U

#define ST7735S_GRAM_HEIGHT    162U
#define ST7735S_CHAR_HEIGHT      8U

#define ST7735S_LINE_BYTES \
(ST7735S_WIDTH * ST7735S_CHAR_HEIGHT * 2U)   /* 2048 B */


extern uint16_t line_buffer[1024] __attribute__((aligned(4)));


static void Delay_Static(uint32_t delay)
{
    for (volatile uint32_t i = 0; i < 400*delay; i++)
    {
        __asm volatile("nop");
    }

}

void ST7735_InitSequence(ST7735S *self)
{
    uint8_t cmd;
    uint8_t data;
    Delay_Static(500);
    /*
     * 建立总线空闲状态：
     * CS=1，DC=1，RST=1
     */
    *(self->io->CTRL_BSHR) = self->io->end_send;
    *(self->io->RST_BSHR)  = self->io->rst_high;

    Delay_Static(500);

    /* 硬件复位 */
    *(self->io->RST_BSHR) = self->io->rst_low;
    Delay_Static(1000);

    *(self->io->RST_BSHR) = self->io->rst_high;
    Delay_Static(5000);
    /* 硬件复位 */


    /* 0x11：Sleep Out */
    cmd = 0x11;
    ST7735_SendCommand(self, &cmd, 1U);
    Delay_Static(500);

    /*
     * 0x3A：Interface Pixel Format
     * 0x05：16-bit RGB565
     */
    cmd = 0x3A;
    ST7735_SendCommand(self, &cmd, 1U);

    data = 0x05;
    ST7735_SendData(self, &data, 1U);

    /*
     * 0x36：Memory Data Access Control
     * 0xC0：沿用原驱动的扫描方向，RGB 顺序。
     */
    cmd = 0x36;
    ST7735_SendCommand(self, &cmd, 1U);

    data = 0xC0;
    ST7735_SendData(self, &data, 1U);

    /* 0x13：Normal Display Mode On */
    cmd = 0x13;
    ST7735_SendCommand(self, &cmd, 1U);
    Delay_Static(500);

    /* 0x29：Display On */
    cmd = 0x29;
    ST7735_SendCommand(self, &cmd, 1U);
    Delay_Static(600);
}



void ST7735S_InitScrollArea(ST7735S *self);

//=================================================================//
//=================================================================//
//=================================================================//
//=================================================================//


void ST7735S_SetPos(
    ST7735S *self,
    uint16_t x0,
    uint16_t y0,
    uint16_t x1,
    uint16_t y1)
{
    if((x0 > x1) || (y0 > y1) || (x1 >= 128U) || (y1 >= 160U))
        while(1) __asm__ volatile("nop");

    const uint16_t ax0 = x0 + ST7735S_X_OFFSET;
    const uint16_t ax1 = x1 + ST7735S_X_OFFSET;
    const uint16_t ay0 = y0 + ST7735S_Y_OFFSET;
    const uint16_t ay1 = y1 + ST7735S_Y_OFFSET;

    if(self->SPIMode == SPI_U8_MODE)
    {
        uint8_t cmd;
        uint8_t args[4];

        /* CASET：1 个命令字节，4 个参数字节。 */
        cmd = 0x2AU;
        ST7735_SendCommand(self, &cmd, 1U);

        args[0] = (uint8_t)(ax0 >> 8U);
        args[1] = (uint8_t)ax0;
        args[2] = (uint8_t)(ax1 >> 8U);
        args[3] = (uint8_t)ax1;
        ST7735_SendData(self, args, 4U);

        /* RASET：1 个命令字节，4 个参数字节。 */
        cmd = 0x2BU;
        ST7735_SendCommand(self, &cmd, 1U);

        args[0] = (uint8_t)(ay0 >> 8U);
        args[1] = (uint8_t)ay0;
        args[2] = (uint8_t)(ay1 >> 8U);
        args[3] = (uint8_t)ay1;
        ST7735_SendData(self, args, 4U);

        /* RAMWR：1 个命令字节。 */
        cmd = 0x2CU;
        ST7735_SendCommand(self, &cmd, 1U);
    }
    else if(self->SPIMode == SPI_U16_MODE)
    {
        uint16_t cmd;
        uint16_t args[2];

        /* 每个命令是 1 个 halfword：线上发送 00 2A。 */
        cmd = 0x002AU;
        ST7735_SendCommand(self, &cmd, 1U);

        /* 2 个 halfword：x0、x1。 */
        args[0] = ax0;
        args[1] = ax1;
        ST7735_SendData(self, args, 2U);

        cmd = 0x002BU;
        ST7735_SendCommand(self, &cmd, 1U);

        /* 2 个 halfword：y0、y1。 */
        args[0] = ay0;
        args[1] = ay1;
        ST7735_SendData(self, args, 2U);

        cmd = 0x002CU;
        ST7735_SendCommand(self, &cmd, 1U);
    }
    else
    {
        while(1) __asm__ volatile("nop");
    }
}
static void ST7735S_SetScrollOffset(
    ST7735S *self,
    uint16_t offset)
{
    if(offset >= 160U)
        while(1) __asm__ volatile("nop");

    const uint16_t ssa = ST7735S_Y_OFFSET + offset;

    if(self->SPIMode == SPI_U8_MODE)
    {
        const uint8_t cmd = 0x37U;
        const uint8_t args[2] = {
            (uint8_t)(ssa >> 8U),
            (uint8_t)ssa
        };

        /* 1 个命令字节，2 个参数字节。 */
        ST7735_SendCommand(self, &cmd, 1U);
        ST7735_SendData(self, args, 2U);
    }
    else if(self->SPIMode == SPI_U16_MODE)
    {
        const uint16_t cmd = 0x0037U;
        const uint16_t args = ssa;

        /* 1 个命令 halfword，1 个参数 halfword。 */
        ST7735_SendCommand(self, &cmd, 1U);
        ST7735_SendData(self, &args, 1U);
    }
    else
    {
        while(1) __asm__ volatile("nop");
    }

    self->scrollOffset = offset;
}


void ST7735S_InitScrollArea(ST7735S *self)
{
    const uint16_t topFixed = ST7735S_Y_OFFSET;
    const uint16_t scrollArea = 160U;
    const uint16_t bottomFixed = 162U - topFixed - 160U;

    if(self->SPIMode == SPI_U8_MODE)
    {
        const uint8_t cmd = 0x33U;
        const uint8_t args[6] = {
            (uint8_t)(topFixed >> 8U),
            (uint8_t)topFixed,
            (uint8_t)(scrollArea >> 8U),
            (uint8_t)scrollArea,
            (uint8_t)(bottomFixed >> 8U),
            (uint8_t)bottomFixed
        };

        /* 1 个命令字节，6 个参数字节。 */
        ST7735_SendCommand(self, &cmd, 1U);
        ST7735_SendData(self, args, 6U);
    }
    else if(self->SPIMode == SPI_U16_MODE)
    {
        const uint16_t cmd = 0x0033U;
        const uint16_t args[3] = {
            topFixed,
            scrollArea,
            bottomFixed
        };

        /* 1 个命令 halfword，3 个参数 halfword。 */
        ST7735_SendCommand(self, &cmd, 1U);
        ST7735_SendData(self, args, 3U);
    }
    else
    {
        while(1) __asm__ volatile("nop");
    }

    ST7735S_SetScrollOffset(self, 0U);
}
uint16_t ST7735S_Scroll8(ST7735S *self)
{
    uint16_t nextOffset;

    /*
     * MADCTL = 0xC0，MY 使垂直地址方向反转。
     * 为了让画面视觉上向上滚8像素，VSCSAD 必须减8。
     */
    if (self->scrollOffset >= 8U)
    {
        nextOffset = self->scrollOffset - 8U;
    }
    else
    {
        nextOffset = self->scrollOffset + 152U;
    }

    ST7735S_SetScrollOffset(self, nextOffset);

    /*
     * raw scrollOffset 经过 MY 反转后对应的可见顶端。
     */
    uint16_t topY;

    if (nextOffset == 0U)
    {
        topY = 0U;
    }
    else
    {
        topY = 160U - nextOffset;
    }

    uint16_t exposedY = topY + 152U;

    if (exposedY >= 160U)
    {
        exposedY -= 160U;
    }

    return exposedY;
}

void ST7735S_PrepareColorLine(uint16_t color)
{
    uint32_t *dst = (uint32_t *)(void *)line_buffer;
    uint32_t * const end = dst + 512U;

    /* 一个 word 装两个相同的 RGB565 像素。 */
    const uint32_t packed =
        (uint32_t)color | ((uint32_t)color << 16U);

    while(dst != end)
        *dst++ = packed;
}

void ST7735S_FillColor(ST7735S *self, uint16_t color)
{
    if(self->SPIMode != SPI_U16_MODE)
        while(1) __asm__ volatile("nop");

    ST7735S_PrepareColorLine(color);
    ST7735S_SetScrollOffset(self, 0U);
    ST7735S_SetPos(self, 0U, 0U, 127U, 159U);

    /*
     * 每块：
     *   128 × 8 = 1024 个 halfword
     *   1024 × 2 = 2048 字节
     *
     * 整屏：
     *   160 / 8 = 20 块
     */
    for(uint32_t block = 0U; block < 20U; block++)
        ST7735_SendData(self, line_buffer, 1024U);
}


extern void st7735s_CH32_HalfWord_Switch(ST7735S *self);

void ST7735S_Init(ST7735S *self)
{
    ST7735_InitSequence(self);

    /* 此时还是 U8，先完成一次性滚动区配置。 */
    ST7735S_InitScrollArea(self);

    /* 运行期切换到 U16。 */
     st7735s_CH32_HalfWord_Switch(self);

     ST7735S_FillColor(self, 0x0000U);

     ST7735S_PrepareColorLine(0xf3BfU);

     ST7735S_SetPos(self, 0U, 0U, 127U, 159U);

     ST7735_DMA_SendData(self, (uint16_t*)line_buffer, ST7735S_LINE_BYTES / 2U);
}

#endif /* DISPLAYER_CHIP_USE_ST7735S */
