#include "chipST7735.h"

#if (DISPLAYER_CHIP_USE_ST7735S != 0U)

#include "displayerChip.h"
#include "screen.h"
#include "st7735s.h"

#define CHIP_ST7735_VISIBLE_WIDTH_PIXELS   128U
#define CHIP_ST7735_VISIBLE_HEIGHT_PIXELS  160U
#define CHIP_ST7735_LINE_BUFFER_PIXELS    1024U
#define CHIP_ST7735_TEXT_ROW_PIXELS \
    (DISPLAYER_CHIP_TEXT_COLUMNS * SCREEN_CHAR_WIDTH)

uint16_t line_buffer[1024] __attribute__((aligned(4)));


_Static_assert(
    (DISPLAYER_CHIP_TEXT_COLUMNS * SCREEN_CHAR_WIDTH) <=
        CHIP_ST7735_VISIBLE_WIDTH_PIXELS,
    "Screen text width does not fit ST7735S");

_Static_assert(
    (DISPLAYER_CHIP_TEXT_ROWS * SCREEN_CHAR_HEIGHT) <=
        CHIP_ST7735_VISIBLE_HEIGHT_PIXELS,
    "Screen text height does not fit ST7735S");

_Static_assert(
    SCREEN_CHAR_HEIGHT == 8U,
    "ST7735S adapter currently binds hardware scroll to 8-pixel text rows");

_Static_assert(
    SCREEN_LINE_PIXELS <= CHIP_ST7735_LINE_BUFFER_PIXELS,
    "ST7735S line buffer is too small for Screen text row");

static uint8_t ChipST7735_IsIdle(
    const DisplayerChip *self);

static uint8_t ChipST7735_ScrollUpOneTextRow(
    DisplayerChip *self);

static void ChipST7735_WriteTextRowDMA(
    DisplayerChip *self,
    uint8_t physicalRow,
    uint16_t *pixels);

static const DisplayerChipOperations chipST7735Operations = {
    .isIdle = ChipST7735_IsIdle,
    .scrollUpOneTextRow = ChipST7735_ScrollUpOneTextRow,
    .writeTextRowDMA = ChipST7735_WriteTextRowDMA,
};

DisplayerChip displayerChip = {
    .ops = &chipST7735Operations,
    .lineBuffer = line_buffer,
};

void DisplayerChip_Init(DisplayerChip *self)
{
    (void)self;

    ST7735S_Init(&st7735s);
}

static uint8_t ChipST7735_IsIdle(
    const DisplayerChip *self)
{
    (void)self;

    return (uint8_t)(st7735s.status == ST7735S_IDLE);
}

static uint8_t ChipST7735_ScrollUpOneTextRow(
    DisplayerChip *self)
{
    (void)self;

    if (st7735s.status != ST7735S_IDLE)
    {
        return 0U;
    }

    (void)ST7735S_Scroll8(&st7735s);
    return 1U;
}

static void ChipST7735_WriteTextRowDMA(
    DisplayerChip *self,
    uint8_t physicalRow,
    uint16_t *pixels)
{
    (void)self;

    const uint16_t y =
        (uint16_t)physicalRow * SCREEN_CHAR_HEIGHT;

    ST7735S_SetPos(
        &st7735s,
        0U,
        y,
        CHIP_ST7735_TEXT_ROW_PIXELS - 1U,
        y + SCREEN_CHAR_HEIGHT - 1U);

    ST7735_DMA_SendData(
        &st7735s,
        pixels,
        SCREEN_LINE_PIXELS);
}

#endif /* DISPLAYER_CHIP_USE_ST7735S */
