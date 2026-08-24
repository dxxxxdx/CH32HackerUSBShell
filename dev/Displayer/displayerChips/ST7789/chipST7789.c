//
// Created by dxxdx on 2026/8/5.
//

#include "chipST7789.h"


#if (DISPLAYER_CHIP_USE_ST7789 != 0U)

#include "displayerChip.h"
#include "screen.h"
#include "st7789.h"

#define CHIP_ST7789_VISIBLE_WIDTH_PIXELS   240U
#define CHIP_ST7789_VISIBLE_HEIGHT_PIXELS  320U

#define CHIP_ST7789_LINE_BUFFER_PIXELS \
    (CHIP_ST7789_VISIBLE_WIDTH_PIXELS * SCREEN_CHAR_HEIGHT)

uint16_t line_buffer[CHIP_ST7789_LINE_BUFFER_PIXELS]
    __attribute__((aligned(4)));

_Static_assert(
    (DISPLAYER_CHIP_TEXT_COLUMNS * SCREEN_CHAR_WIDTH) <=
        CHIP_ST7789_VISIBLE_WIDTH_PIXELS,
    "Screen text width does not fit ST7789");

_Static_assert(
    (DISPLAYER_CHIP_TEXT_ROWS * SCREEN_CHAR_HEIGHT) <=
        CHIP_ST7789_VISIBLE_HEIGHT_PIXELS,
    "Screen text height does not fit ST7789");

_Static_assert(
    SCREEN_LINE_PIXELS <= CHIP_ST7789_LINE_BUFFER_PIXELS,
    "ST7789 line buffer is too small for Screen text row");

_Static_assert(
    SCREEN_CHAR_HEIGHT == ST7789_TEXT_ROW_HEIGHT_PIXELS,
    "ST7789 adapter currently binds hardware scroll to 8-pixel text rows");


static uint8_t ChipST7789_IsIdle(
    const DisplayerChip *self);

static uint8_t ChipST7789_ScrollUpOneTextRow(
    DisplayerChip *self);

static void ChipST7789_WriteTextRowDMA(
    DisplayerChip *self,
    uint8_t physicalRow,
    uint16_t *pixels);

static const DisplayerChipOperations chipST7789Operations = {
    .isIdle = ChipST7789_IsIdle,
    .scrollUpOneTextRow = ChipST7789_ScrollUpOneTextRow,
    .writeTextRowDMA = ChipST7789_WriteTextRowDMA,
};

DisplayerChip displayerChip = {
    .ops = &chipST7789Operations,
    .lineBuffer = line_buffer,
};

void DisplayerChip_Init(DisplayerChip *self)
{
    (void)self;

    ST7789_Init(&st7789);
}

static uint8_t ChipST7789_IsIdle(
    const DisplayerChip *self)
{
    (void)self;

    return (uint8_t)(st7789.status == ST7789_IDLE);
}

static uint8_t ChipST7789_ScrollUpOneTextRow(
    DisplayerChip *self)
{
    (void)self;

    if (st7789.status != ST7789_IDLE)
    {
        return 0U;
    }

    (void)ST7789_Scroll8(&st7789);
    return 1U;
}

static void ChipST7789_WriteTextRowDMA(
    DisplayerChip *self,
    uint8_t physicalRow,
    uint16_t *pixels)
{
    (void)self;

    const uint16_t y =
        (uint16_t)physicalRow * SCREEN_CHAR_HEIGHT;

    ST7789_SetPos(
        &st7789,
        0U,
        y,
        (DISPLAYER_CHIP_TEXT_COLUMNS * SCREEN_CHAR_WIDTH) - 1U,
        y + SCREEN_CHAR_HEIGHT - 1U);

    ST7789_DMA_SendData(
        &st7789,
        pixels,
        SCREEN_LINE_PIXELS);
}























#endif /* DISPLAYER_CHIP_USE_ST7789 */
