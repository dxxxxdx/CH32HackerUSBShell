#include "screen.h"
#include "screenFont.h"

#include <string.h>

static const uint16_t screenPalette[16] = {
    0x0000U, 0x8000U, 0x0400U, 0x8400U,
    0x0010U, 0x8010U, 0x0410U, 0xC618U,
    0x8410U, 0xF800U, 0x07E0U, 0xFFE0U,
    0x001FU, 0xF81FU, 0x07FFU, 0xFFFFU,
};

#if defined(__GNUC__)
#define SCREEN_ALWAYS_INLINE static inline __attribute__((always_inline))
#else
#define SCREEN_ALWAYS_INLINE static inline
#endif


Screen screen = {
    .chip = &displayerChip,
    .responseContext = 0,
    .dirtyRows = 0U,
    .logicalTopPhysicalRow = 0U,
    .committedTopPhysicalRow = 0U,
    .pendingScrollLines = 0U,
    .cursor = {
        .column = 0U,
        .row = 0U,
        .attribute = SCREEN_DEFAULT_ATTRIBUTE,
    },
    .savedCursor = {
        .column = 0U,
        .row = 0U,
        .attribute = SCREEN_DEFAULT_ATTRIBUTE,
    },
    .scrollRegion = {
        .top = 0U,
        .bottom = DISPLAYER_CHIP_TEXT_ROWS - 1U,
    },
    .flags = SCREEN_FLAG_AUTO_WRAP | SCREEN_FLAG_CURSOR_VISIBLE,
};

SCREEN_ALWAYS_INLINE uint8_t Screen_WrapPhysicalRow(uint16_t physicalRow)
{
    if (physicalRow >= DISPLAYER_CHIP_TEXT_ROWS)
    {
        physicalRow -= DISPLAYER_CHIP_TEXT_ROWS;
    }

    return (uint8_t)physicalRow;
}

SCREEN_ALWAYS_INLINE uint8_t Screen_NextPhysicalRow(uint8_t physicalRow)
{
    physicalRow++;
    if (physicalRow >= DISPLAYER_CHIP_TEXT_ROWS)
    {
        physicalRow = 0U;
    }

    return physicalRow;
}

SCREEN_ALWAYS_INLINE uint8_t Screen_LogicalToPhysicalRow(
    const Screen *self,
    uint8_t logicalRow)
{
    return Screen_WrapPhysicalRow(
        (uint16_t)self->logicalTopPhysicalRow + logicalRow);
}

SCREEN_ALWAYS_INLINE ScreenLine *Screen_GetLogicalLine(
    Screen *self,
    uint8_t logicalRow)
{
    return &self->lines[Screen_LogicalToPhysicalRow(self, logicalRow)];
}

SCREEN_ALWAYS_INLINE void Screen_MarkPhysicalRowDirty(
    Screen *self,
    uint8_t physicalRow)
{
    self->dirtyRows |= SCREEN_ROW_MASK(physicalRow);
}

SCREEN_ALWAYS_INLINE void Screen_MarkLogicalRowDirty(
    Screen *self,
    uint8_t logicalRow)
{
    Screen_MarkPhysicalRowDirty(
        self,
        Screen_LogicalToPhysicalRow(self, logicalRow));
}

static void Screen_ClearLogicalLine(Screen *self, uint8_t logicalRow)
{
    ScreenLine *line = Screen_GetLogicalLine(self, logicalRow);

    memset(line->character, ' ', DISPLAYER_CHIP_TEXT_COLUMNS);
    memset(line->attributes, self->cursor.attribute, DISPLAYER_CHIP_TEXT_COLUMNS);
    Screen_MarkLogicalRowDirty(self, logicalRow);
}

static void Screen_ClearLineRange(
    Screen *self,
    uint8_t logicalRow,
    uint8_t firstColumn,
    uint8_t endColumn)
{
    ScreenLine *line = Screen_GetLogicalLine(self, logicalRow);
    const uint8_t length = (uint8_t)(endColumn - firstColumn);

    memset(&line->character[firstColumn], ' ', length);
    memset(&line->attributes[firstColumn], self->cursor.attribute, length);
    Screen_MarkLogicalRowDirty(self, logicalRow);
}

static void Screen_ScrollUpOne(Screen *self)
{
    if ((self->scrollRegion.top == 0U) &&
        (self->scrollRegion.bottom == (DISPLAYER_CHIP_TEXT_ROWS - 1U)))
    {
        self->logicalTopPhysicalRow =
            Screen_NextPhysicalRow(self->logicalTopPhysicalRow);
        self->pendingScrollLines++;
        Screen_ClearLogicalLine(self, DISPLAYER_CHIP_TEXT_ROWS - 1U);
        return;
    }

    for (uint8_t row = self->scrollRegion.top;
         row < self->scrollRegion.bottom;
         row++)
    {
        *Screen_GetLogicalLine(self, row) =
            *Screen_GetLogicalLine(self, (uint8_t)(row + 1U));
        Screen_MarkLogicalRowDirty(self, row);
    }

    Screen_ClearLogicalLine(self, self->scrollRegion.bottom);
}

static void Screen_ScrollDownOne(Screen *self)
{
    uint8_t row = self->scrollRegion.bottom;

    while (row > self->scrollRegion.top)
    {
        *Screen_GetLogicalLine(self, row) =
            *Screen_GetLogicalLine(self, (uint8_t)(row - 1U));
        Screen_MarkLogicalRowDirty(self, row);
        row--;
    }

    Screen_ClearLogicalLine(self, self->scrollRegion.top);
}

static void Screen_LineFeedInternal(Screen *self)
{
    self->flags &= (uint8_t)~SCREEN_FLAG_WRAP_PENDING;

    if (self->cursor.row < self->scrollRegion.bottom)
    {
        self->cursor.row++;
        return;
    }

    if (self->cursor.row == self->scrollRegion.bottom)
    {
        Screen_ScrollUpOne(self);

        /*
         * cursor.row 不变。
         * 滚屏以后原来的底行变成倒数第二行，
         * 光标仍位于新清空的底行。
         */
    }
}

static void Screen_WriteRun(
    void *context,
    const uint8_t *data,
    uint32_t length)
{
    Screen *self = (Screen *)context;

    while (length != 0U)
    {
        if ((self->flags & SCREEN_FLAG_WRAP_PENDING) != 0U)
        {
            self->flags &= (uint8_t)~SCREEN_FLAG_WRAP_PENDING;

            if ((self->flags & SCREEN_FLAG_AUTO_WRAP) != 0U)
            {
                self->cursor.column = 0U;
                Screen_LineFeedInternal(self);
            }
        }

        ScreenLine *line = Screen_GetLogicalLine(self, self->cursor.row);
        line->character[self->cursor.column] = *data++;
        line->attributes[self->cursor.column] = self->cursor.attribute;
        Screen_MarkLogicalRowDirty(self, self->cursor.row);

        if (self->cursor.column == (DISPLAYER_CHIP_TEXT_COLUMNS - 1U))
        {
            if ((self->flags & SCREEN_FLAG_AUTO_WRAP) != 0U)
            {
                self->flags |= SCREEN_FLAG_WRAP_PENDING;
            }
        }
        else
        {
            self->cursor.column++;
        }

        length--;
    }
}

static void Screen_NopSimple(void *context)
{
    (void)context;
}

static void Screen_Backspace(void *context)
{
    Screen *self = (Screen *)context;

    self->flags &= (uint8_t)~SCREEN_FLAG_WRAP_PENDING;
    if (self->cursor.column != 0U)
    {
        self->cursor.column--;
    }
}

static void Screen_HorizontalTab(void *context)
{
    Screen *self = (Screen *)context;
    uint8_t column = (uint8_t)((self->cursor.column + 8U) & 0xF8U);

    self->flags &= (uint8_t)~SCREEN_FLAG_WRAP_PENDING;
    if (column >= DISPLAYER_CHIP_TEXT_COLUMNS)
    {
        column = DISPLAYER_CHIP_TEXT_COLUMNS - 1U;
    }
    self->cursor.column = column;
}

static void Screen_LineFeed(void *context)
{
    Screen_LineFeedInternal((Screen *)context);
}

static void Screen_CarriageReturn(void *context)
{
    Screen *self = (Screen *)context;
    self->cursor.column = 0U;
    self->flags &= (uint8_t)~SCREEN_FLAG_WRAP_PENDING;
}

static void Screen_SaveCursor(void *context)
{
    Screen *self = (Screen *)context;
    self->savedCursor = self->cursor;
}

static void Screen_RestoreCursor(void *context)
{
    Screen *self = (Screen *)context;
    self->cursor = self->savedCursor;
    self->flags &= (uint8_t)~SCREEN_FLAG_WRAP_PENDING;
}

static void Screen_Index(void *context)
{
    Screen_LineFeedInternal((Screen *)context);
}

static void Screen_NextLine(void *context)
{
    Screen *self = (Screen *)context;
    self->cursor.column = 0U;
    Screen_LineFeedInternal(self);
}

static void Screen_ReverseIndex(void *context)
{
    Screen *self = (Screen *)context;
    self->flags &= (uint8_t)~SCREEN_FLAG_WRAP_PENDING;

    if (self->cursor.row == self->scrollRegion.top)
    {
        Screen_ScrollDownOne(self);
    }
    else if (self->cursor.row != 0U)
    {
        self->cursor.row--;
    }
}

static void Screen_Reset(void *context)
{
    Screen_Init((Screen *)context);
}

static void Screen_NopSelectCharset(
    void *context,
    uint8_t slot,
    uint8_t designator)
{
    (void)context;
    (void)slot;
    (void)designator;
}

static void Screen_CursorUp(void *context, uint16_t count)
{
    Screen *self = (Screen *)context;
    self->flags &= (uint8_t)~SCREEN_FLAG_WRAP_PENDING;
    self->cursor.row = count > self->cursor.row
        ? 0U
        : (uint8_t)(self->cursor.row - count);
}

static void Screen_CursorDown(void *context, uint16_t count)
{
    Screen *self = (Screen *)context;
    uint16_t row = self->cursor.row + count;

    self->flags &= (uint8_t)~SCREEN_FLAG_WRAP_PENDING;
    self->cursor.row = row >= DISPLAYER_CHIP_TEXT_ROWS
        ? DISPLAYER_CHIP_TEXT_ROWS - 1U
        : (uint8_t)row;
}

static void Screen_CursorForward(void *context, uint16_t count)
{
    Screen *self = (Screen *)context;
    uint16_t column = self->cursor.column + count;

    self->flags &= (uint8_t)~SCREEN_FLAG_WRAP_PENDING;
    self->cursor.column = column >= DISPLAYER_CHIP_TEXT_COLUMNS
        ? DISPLAYER_CHIP_TEXT_COLUMNS - 1U
        : (uint8_t)column;
}

static void Screen_CursorBackward(void *context, uint16_t count)
{
    Screen *self = (Screen *)context;
    self->flags &= (uint8_t)~SCREEN_FLAG_WRAP_PENDING;
    self->cursor.column = count > self->cursor.column
        ? 0U
        : (uint8_t)(self->cursor.column - count);
}

static void Screen_CursorNextLine(void *context, uint16_t count)
{
    Screen_CursorDown(context, count);
    Screen_CarriageReturn(context);
}

static void Screen_CursorPreviousLine(void *context, uint16_t count)
{
    Screen_CursorUp(context, count);
    Screen_CarriageReturn(context);
}

static void Screen_CursorHorizontalAbsolute(void *context, uint16_t column)
{
    Screen *self = (Screen *)context;
    self->flags &= (uint8_t)~SCREEN_FLAG_WRAP_PENDING;
    self->cursor.column = column > DISPLAYER_CHIP_TEXT_COLUMNS
        ? DISPLAYER_CHIP_TEXT_COLUMNS - 1U
        : (uint8_t)(column - 1U);
}

static void Screen_CursorVerticalAbsolute(void *context, uint16_t row)
{
    Screen *self = (Screen *)context;
    self->flags &= (uint8_t)~SCREEN_FLAG_WRAP_PENDING;
    self->cursor.row = row > DISPLAYER_CHIP_TEXT_ROWS
        ? DISPLAYER_CHIP_TEXT_ROWS - 1U
        : (uint8_t)(row - 1U);
}

static void Screen_CursorPosition(
    void *context,
    uint16_t row,
    uint16_t column)
{
    Screen_CursorVerticalAbsolute(context, row);
    Screen_CursorHorizontalAbsolute(context, column);
}

static void Screen_EraseDisplay(void *context, uint8_t mode)
{
    Screen *self = (Screen *)context;

    if (mode == 0U)
    {
        Screen_ClearLineRange(
            self,
            self->cursor.row,
            self->cursor.column,
            DISPLAYER_CHIP_TEXT_COLUMNS);

        for (uint8_t row = (uint8_t)(self->cursor.row + 1U);
             row < DISPLAYER_CHIP_TEXT_ROWS;
             row++)
        {
            Screen_ClearLogicalLine(self, row);
        }
    }
    else if (mode == 1U)
    {
        for (uint8_t row = 0U; row < self->cursor.row; row++)
        {
            Screen_ClearLogicalLine(self, row);
        }

        Screen_ClearLineRange(
            self,
            self->cursor.row,
            0U,
            (uint8_t)(self->cursor.column + 1U));
    }
    else if (mode == 2U)
    {
        for (uint8_t row = 0U; row < DISPLAYER_CHIP_TEXT_ROWS; row++)
        {
            Screen_ClearLogicalLine(self, row);
        }
    }
}

static void Screen_EraseLine(void *context, uint8_t mode)
{
    Screen *self = (Screen *)context;

    if (mode == 0U)
    {
        Screen_ClearLineRange(
            self,
            self->cursor.row,
            self->cursor.column,
            DISPLAYER_CHIP_TEXT_COLUMNS);
    }
    else if (mode == 1U)
    {
        Screen_ClearLineRange(
            self,
            self->cursor.row,
            0U,
            (uint8_t)(self->cursor.column + 1U));
    }
    else if (mode == 2U)
    {
        Screen_ClearLogicalLine(self, self->cursor.row);
    }
}

static void Screen_InsertCharacters(void *context, uint16_t count)
{
    Screen *self = (Screen *)context;
    ScreenLine *line = Screen_GetLogicalLine(self, self->cursor.row);
    uint8_t length = count > (DISPLAYER_CHIP_TEXT_COLUMNS - self->cursor.column)
        ? (uint8_t)(DISPLAYER_CHIP_TEXT_COLUMNS - self->cursor.column)
        : (uint8_t)count;

    memmove(
        &line->character[self->cursor.column + length],
        &line->character[self->cursor.column],
        DISPLAYER_CHIP_TEXT_COLUMNS - self->cursor.column - length);
    memmove(
        &line->attributes[self->cursor.column + length],
        &line->attributes[self->cursor.column],
        DISPLAYER_CHIP_TEXT_COLUMNS - self->cursor.column - length);

    memset(&line->character[self->cursor.column], ' ', length);
    memset(&line->attributes[self->cursor.column], self->cursor.attribute, length);
    Screen_MarkLogicalRowDirty(self, self->cursor.row);
}

static void Screen_DeleteCharacters(void *context, uint16_t count)
{
    Screen *self = (Screen *)context;
    ScreenLine *line = Screen_GetLogicalLine(self, self->cursor.row);
    uint8_t length = count > (DISPLAYER_CHIP_TEXT_COLUMNS - self->cursor.column)
        ? (uint8_t)(DISPLAYER_CHIP_TEXT_COLUMNS - self->cursor.column)
        : (uint8_t)count;
    uint8_t remaining =
        (uint8_t)(DISPLAYER_CHIP_TEXT_COLUMNS - self->cursor.column - length);

    memmove(
        &line->character[self->cursor.column],
        &line->character[self->cursor.column + length],
        remaining);
    memmove(
        &line->attributes[self->cursor.column],
        &line->attributes[self->cursor.column + length],
        remaining);

    memset(&line->character[DISPLAYER_CHIP_TEXT_COLUMNS - length], ' ', length);
    memset(
        &line->attributes[DISPLAYER_CHIP_TEXT_COLUMNS - length],
        self->cursor.attribute,
        length);
    Screen_MarkLogicalRowDirty(self, self->cursor.row);
}

static void Screen_EraseCharacters(void *context, uint16_t count)
{
    Screen *self = (Screen *)context;
    uint16_t end = self->cursor.column + count;

    if (end > DISPLAYER_CHIP_TEXT_COLUMNS)
    {
        end = DISPLAYER_CHIP_TEXT_COLUMNS;
    }

    Screen_ClearLineRange(
        self,
        self->cursor.row,
        self->cursor.column,
        (uint8_t)end);
}

static void Screen_InsertLines(void *context, uint16_t count)
{
    Screen *self = (Screen *)context;

    if ((self->cursor.row < self->scrollRegion.top) ||
        (self->cursor.row > self->scrollRegion.bottom))
    {
        return;
    }

    uint8_t length = count >
        (uint16_t)(self->scrollRegion.bottom - self->cursor.row + 1U)
        ? (uint8_t)(self->scrollRegion.bottom - self->cursor.row + 1U)
        : (uint8_t)count;

    while (length-- != 0U)
    {
        uint8_t row = self->scrollRegion.bottom;
        while (row > self->cursor.row)
        {
            *Screen_GetLogicalLine(self, row) =
                *Screen_GetLogicalLine(self, (uint8_t)(row - 1U));
            Screen_MarkLogicalRowDirty(self, row);
            row--;
        }
        Screen_ClearLogicalLine(self, self->cursor.row);
    }
}

static void Screen_DeleteLines(void *context, uint16_t count)
{
    Screen *self = (Screen *)context;

    if ((self->cursor.row < self->scrollRegion.top) ||
        (self->cursor.row > self->scrollRegion.bottom))
    {
        return;
    }

    uint8_t length = count >
        (uint16_t)(self->scrollRegion.bottom - self->cursor.row + 1U)
        ? (uint8_t)(self->scrollRegion.bottom - self->cursor.row + 1U)
        : (uint8_t)count;

    while (length-- != 0U)
    {
        for (uint8_t row = self->cursor.row;
             row < self->scrollRegion.bottom;
             row++)
        {
            *Screen_GetLogicalLine(self, row) =
                *Screen_GetLogicalLine(self, (uint8_t)(row + 1U));
            Screen_MarkLogicalRowDirty(self, row);
        }
        Screen_ClearLogicalLine(self, self->scrollRegion.bottom);
    }
}

static void Screen_ScrollUp(void *context, uint16_t count)
{
    Screen *self = (Screen *)context;
    uint16_t height =
        (uint16_t)(self->scrollRegion.bottom - self->scrollRegion.top + 1U);

    if (count > height)
    {
        count = height;
    }

    while (count-- != 0U)
    {
        Screen_ScrollUpOne(self);
    }
}

static void Screen_ScrollDown(void *context, uint16_t count)
{
    Screen *self = (Screen *)context;
    uint16_t height =
        (uint16_t)(self->scrollRegion.bottom - self->scrollRegion.top + 1U);

    if (count > height)
    {
        count = height;
    }

    while (count-- != 0U)
    {
        Screen_ScrollDownOne(self);
    }
}

static void Screen_ApplyGraphicsCode(Screen *self, uint16_t code)
{
    uint8_t foreground = self->cursor.attribute & 0x0FU;
    uint8_t background = self->cursor.attribute >> 4U;

    if (code == 0U)
    {
        self->cursor.attribute = SCREEN_DEFAULT_ATTRIBUTE;
        return;
    }
    if (code == 1U)
    {
        if (foreground < 8U)
        {
            foreground += 8U;
        }
    }
    else if (code == 22U)
    {
        foreground &= 0x07U;
    }
    else if ((code == 7U) || (code == 27U))
    {
        const uint8_t temporary = foreground;
        foreground = background;
        background = temporary;
    }
    else if (code == 39U)
    {
        foreground = 7U;
    }
    else if (code == 49U)
    {
        background = 0U;
    }
    else if ((code >= 30U) && (code <= 37U))
    {
        foreground = (uint8_t)(code - 30U);
    }
    else if ((code >= 40U) && (code <= 47U))
    {
        background = (uint8_t)(code - 40U);
    }
    else if ((code >= 90U) && (code <= 97U))
    {
        foreground = (uint8_t)(code - 90U + 8U);
    }
    else if ((code >= 100U) && (code <= 107U))
    {
        background = (uint8_t)(code - 100U + 8U);
    }

    self->cursor.attribute = SCREEN_ATTRIBUTE(foreground, background);
}

static void Screen_SetGraphics(
    void *context,
    const VT100CSIParameters *parameters)
{
    Screen *self = (Screen *)context;

    if (parameters->count == 0U)
    {
        self->cursor.attribute = SCREEN_DEFAULT_ATTRIBUTE;
        return;
    }

    for (uint8_t index = 0U; index < parameters->count; index++)
    {
        const uint16_t code =
            (parameters->presentMask & ((uint16_t)1U << index)) != 0U
                ? parameters->value[index]
                : 0U;
        Screen_ApplyGraphicsCode(self, code);
    }
}

static void Screen_SetMode(
    void *context,
    uint8_t privateMarker,
    const VT100CSIParameters *parameters,
    uint8_t enabled)
{
    Screen *self = (Screen *)context;

    if (privateMarker != (uint8_t)'?')
    {
        return;
    }

    for (uint8_t index = 0U; index < parameters->count; index++)
    {
        if ((parameters->presentMask & ((uint16_t)1U << index)) == 0U)
        {
            continue;
        }

        if (parameters->value[index] == 7U)
        {
            if (enabled != 0U)
            {
                self->flags |= SCREEN_FLAG_AUTO_WRAP;
            }
            else
            {
                self->flags &= (uint8_t)~SCREEN_FLAG_AUTO_WRAP;
            }
        }
        else if (parameters->value[index] == 25U)
        {
            if (enabled != 0U)
            {
                self->flags |= SCREEN_FLAG_CURSOR_VISIBLE;
            }
            else
            {
                self->flags &= (uint8_t)~SCREEN_FLAG_CURSOR_VISIBLE;
            }
        }
    }
}

static void Screen_SetScrollRegion(
    void *context,
    uint16_t top,
    uint16_t bottom)
{
    Screen *self = (Screen *)context;

    if (bottom == 0U)
    {
        bottom = DISPLAYER_CHIP_TEXT_ROWS;
    }

    if ((top == 0U) ||
        (top >= bottom) ||
        (bottom > DISPLAYER_CHIP_TEXT_ROWS))
    {
        return;
    }

    self->scrollRegion.top = (uint8_t)(top - 1U);
    self->scrollRegion.bottom = (uint8_t)(bottom - 1U);
    self->cursor.column = 0U;
    self->cursor.row = 0U;
    self->flags &= (uint8_t)~SCREEN_FLAG_WRAP_PENDING;
}

static uint32_t Screen_AppendUnsigned(uint8_t *destination, uint16_t value)
{
    uint8_t reverse[5];
    uint32_t count = 0U;

    do
    {
        reverse[count++] = (uint8_t)('0' + (value % 10U));
        value /= 10U;
    }
    while (value != 0U);

    for (uint32_t index = 0U; index < count; index++)
    {
        destination[index] = reverse[count - index - 1U];
    }

    return count;
}



const VT100Operations screenOperations = {
    .ground = {
        .writeRun = Screen_WriteRun,
        .bell = Screen_NopSimple,
        .backspace = Screen_Backspace,
        .horizontalTab = Screen_HorizontalTab,
        .lineFeed = Screen_LineFeed,
        .carriageReturn = Screen_CarriageReturn,
    },
    .escape = {
        .saveCursor = Screen_SaveCursor,
        .restoreCursor = Screen_RestoreCursor,
        .index = Screen_Index,
        .nextLine = Screen_NextLine,
        .reverseIndex = Screen_ReverseIndex,
        .reset = Screen_Reset,
        .selectCharset = Screen_NopSelectCharset,
    },
    .csi = {
        .cursorUp = Screen_CursorUp,
        .cursorDown = Screen_CursorDown,
        .cursorForward = Screen_CursorForward,
        .cursorBackward = Screen_CursorBackward,
        .cursorNextLine = Screen_CursorNextLine,
        .cursorPreviousLine = Screen_CursorPreviousLine,
        .cursorHorizontalAbsolute = Screen_CursorHorizontalAbsolute,
        .cursorVerticalAbsolute = Screen_CursorVerticalAbsolute,
        .cursorPosition = Screen_CursorPosition,
        .eraseDisplay = Screen_EraseDisplay,
        .eraseLine = Screen_EraseLine,
        .insertCharacters = Screen_InsertCharacters,
        .deleteCharacters = Screen_DeleteCharacters,
        .eraseCharacters = Screen_EraseCharacters,
        .insertLines = Screen_InsertLines,
        .deleteLines = Screen_DeleteLines,
        .scrollUp = Screen_ScrollUp,
        .scrollDown = Screen_ScrollDown,
        .setGraphics = Screen_SetGraphics,
        .setMode = Screen_SetMode,
        .setScrollRegion = Screen_SetScrollRegion,
        .saveCursor = Screen_SaveCursor,
        .restoreCursor = Screen_RestoreCursor,
    },
};

void Screen_Init(Screen *self)
{
    self->dirtyRows = 0U;
    self->logicalTopPhysicalRow = 0U;
    self->committedTopPhysicalRow = 0U;
    self->pendingScrollLines = 0U;

    self->cursor.column = 0U;
    self->cursor.row = 0U;
    self->cursor.attribute = SCREEN_DEFAULT_ATTRIBUTE;
    self->savedCursor = self->cursor;

    self->scrollRegion.top = 0U;
    self->scrollRegion.bottom = DISPLAYER_CHIP_TEXT_ROWS - 1U;
    self->flags = SCREEN_FLAG_AUTO_WRAP | SCREEN_FLAG_CURSOR_VISIBLE;

    for (uint8_t row = 0U; row < DISPLAYER_CHIP_TEXT_ROWS; row++)
    {
        memset(
            self->lines[row].character,
            ' ',
            DISPLAYER_CHIP_TEXT_COLUMNS);
        memset(
            self->lines[row].attributes,
            SCREEN_DEFAULT_ATTRIBUTE,
            DISPLAYER_CHIP_TEXT_COLUMNS);
    }

    Screen_InvalidateAll(self);
}

void Screen_InvalidateAll(Screen *self)
{
    self->dirtyRows = SCREEN_ALL_ROWS_MASK;
}

static void Screen_RenderPhysicalLine(Screen *self, uint8_t physicalRow)
{
    const ScreenLine *line = &self->lines[physicalRow];
    uint16_t *destination = self->chip->lineBuffer;

    for (uint8_t pixelRow = 0U;
         pixelRow < SCREEN_CHAR_HEIGHT;
         pixelRow++)
    {
        for (uint8_t column = 0U;
             column < DISPLAYER_CHIP_TEXT_COLUMNS;
             column++)
        {
            uint8_t character = line->character[column];
            const uint8_t attribute = line->attributes[column];
            const uint16_t foreground = screenPalette[attribute & 0x0FU];
            const uint16_t background = screenPalette[attribute >> 4U];

            if ((character < 0x20U) || (character > 0x7FU))
            {
                character = (uint8_t)'?';
            }

            const uint8_t pixels =
                screenFont8x8[character - 0x20U][pixelRow];

            for (uint8_t pixelColumn = 0U;
                 pixelColumn < SCREEN_CHAR_WIDTH;
                 pixelColumn++)
            {
                *destination++ =
                    (pixels & ((uint8_t)1U << pixelColumn)) != 0U
                        ? foreground
                        : background;
            }
        }
    }
}

uint8_t Screen_Service(Screen *self)
{

    if (self->chip->ops->isIdle(self->chip) == 0U)
    {
        return 0U;
    }
    uint8_t physicalRow;

    if (self->pendingScrollLines > 0)
    {
        if (self->chip->ops->scrollUpOneTextRow(self->chip) == 0U)
        {
            return 0U;
        }

        self->committedTopPhysicalRow =
            Screen_NextPhysicalRow(self->committedTopPhysicalRow);
        self->pendingScrollLines--;

        /*
         * 硬件滚动和本次露出的物理行是一笔事务。
         * 多次滚动排队后，脏位的数值顺序不再等于 GRAM 露出顺序，
         * 所以这里必须重绘本次刚露出的底部物理行。
         */
        physicalRow = Screen_WrapPhysicalRow(
            (uint16_t)self->committedTopPhysicalRow +
            (DISPLAYER_CHIP_TEXT_ROWS - 1U));
    }
    else
    {
        if (self->dirtyRows == 0U)
        {
            return 0U;
        }

        physicalRow = 0U;
        while ((self->dirtyRows & SCREEN_ROW_MASK(physicalRow)) == 0U)
        {
            physicalRow++;
        }
    }

    self->dirtyRows &= ~SCREEN_ROW_MASK(physicalRow);
    Screen_RenderPhysicalLine(self, physicalRow);

    self->chip->ops->writeTextRowDMA(
        self->chip,
        physicalRow,
        self->chip->lineBuffer);

    return 1U;
}
uint8_t Screen_IsIdle(
    const Screen *self)
{
    return
        (uint8_t)(
            (self->chip->ops->isIdle(self->chip) != 0U) &&
            (self->pendingScrollLines == 0U) &&
            (self->dirtyRows == 0U)
        );
}
#undef SCREEN_ALWAYS_INLINE
