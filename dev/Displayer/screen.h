#ifndef SCREEN_H
#define SCREEN_H

#include "displayerChip.h"
#include "VT100.h"

#include <stdint.h>

/*
 * Screen 只定义当前字体尺寸。
 * 字符画布行列数由选中的 DisplayerChip 配置在编译期给出。
 */
#define SCREEN_CHAR_WIDTH         8U
#define SCREEN_CHAR_HEIGHT        8U

#define SCREEN_LINE_PIXELS \
    (DISPLAYER_CHIP_TEXT_COLUMNS * SCREEN_CHAR_WIDTH * SCREEN_CHAR_HEIGHT)

#if (DISPLAYER_CHIP_TEXT_ROWS > 64U)
#error "dirtyRows uses one uint64_t and supports at most 64 rows"
#endif

#define SCREEN_ROW_MASK(row) \
    ((uint64_t)1U << (row))

#if (DISPLAYER_CHIP_TEXT_ROWS == 64U)
#define SCREEN_ALL_ROWS_MASK UINT64_MAX
#else
#define SCREEN_ALL_ROWS_MASK \
    (SCREEN_ROW_MASK(DISPLAYER_CHIP_TEXT_ROWS) - (uint64_t)1U)
#endif

#define SCREEN_FLAG_WRAP_PENDING  0x01U
#define SCREEN_FLAG_AUTO_WRAP     0x02U
#define SCREEN_FLAG_CURSOR_VISIBLE 0x04U

/* attributes：低4位前景色索引，高4位背景色索引。 */
#define SCREEN_ATTRIBUTE(foreground, background) \
    ((uint8_t)((((uint8_t)(background) & 0x0FU) << 4U) | \
               ((uint8_t)(foreground) & 0x0FU)))

#define SCREEN_DEFAULT_ATTRIBUTE \
    SCREEN_ATTRIBUTE(7U, 0U)

typedef struct
{
    uint8_t character[DISPLAYER_CHIP_TEXT_COLUMNS];
    uint8_t attributes[DISPLAYER_CHIP_TEXT_COLUMNS];
} ScreenLine;

_Static_assert(
    sizeof(ScreenLine) == (DISPLAYER_CHIP_TEXT_COLUMNS * 2U),
    "Screen line must not contain padding");

typedef struct
{
    uint8_t column;
    uint8_t row;
    uint8_t attribute;
} ScreenCursorState;

typedef struct
{
    uint8_t top;
    uint8_t bottom;
} ScreenScrollRegion;

typedef void (*ScreenResponseWrite)(
    void *context,
    const uint8_t *data,
    uint32_t length);

typedef struct Screen
{
    DisplayerChip *chip;

    void *responseContext;

    /*
     * lines 按显示芯片 GRAM 的物理字符行编号。
     * 逻辑行到物理行的映射由 Screen 的 logicalTopPhysicalRow 负责，
     * Screen 不保存芯片滚动寄存器影子状态。
     */
    ScreenLine lines[DISPLAYER_CHIP_TEXT_ROWS];

    /* 每一位对应 lines 中的一条物理存储行。 */
    uint64_t dirtyRows;

    /*
     * Screen 自己拥有字符拓扑。
     * logicalTopPhysicalRow 是终端逻辑顶行对应的 lines[] 物理行。
     * committedTopPhysicalRow 是已经提交给显示芯片的可见顶行。
     */
    uint8_t logicalTopPhysicalRow;
    uint8_t committedTopPhysicalRow;

    /* 已经更新字符拓扑、但尚未提交给显示芯片的整字符行滚动次数。 */
    uint16_t pendingScrollLines;

    ScreenCursorState cursor;
    ScreenCursorState savedCursor;
    ScreenScrollRegion scrollRegion;

    /* SCREEN_FLAG_* 的组合。 */
    uint8_t flags;
} Screen;

extern Screen screen;
extern const VT100Operations screenOperations;

/* 外部依赖由 screen 的静态初始化器绑定；这里只复位运行期状态。 */
void Screen_Init(Screen *self);

/* LCD 空闲时最多提交一项滚动或一条脏行 DMA；返回1表示有提交。 */
uint8_t Screen_Service(Screen *self);

/* 将全部字符行标脏，供初始化、复位或强制重绘使用。 */
void Screen_InvalidateAll(Screen *self);
uint8_t Screen_IsIdle(
    const Screen *self);
#endif /* SCREEN_H */
