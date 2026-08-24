#ifndef VT100_H
#define VT100_H

#include <stdint.h>

#ifndef VT100_MAX_PARAMS
#define VT100_MAX_PARAMS 16U
#endif

#ifndef VT100_MAX_INTERMEDIATES
#define VT100_MAX_INTERMEDIATES 2U
#endif

#if (VT100_MAX_PARAMS < 1U) || (VT100_MAX_PARAMS > 16U)
#error "VT100_MAX_PARAMS must be in range 1..16"
#endif

typedef enum
{
    VT100_STATE_GROUND = 0,
    VT100_STATE_ESCAPE,
    VT100_STATE_ESCAPE_INTERMEDIATE,
    VT100_STATE_CSI_ENTRY,
    VT100_STATE_CSI_PARAM,
    VT100_STATE_CSI_INTERMEDIATE,
    VT100_STATE_CSI_IGNORE,
    VT100_STATE_STRING_IGNORE,
    VT100_STATE_STRING_ESCAPE
} VT100State;

typedef enum
{
    VT100_STRING_NONE = 0,
    VT100_STRING_OSC,
    VT100_STRING_DCS,
    VT100_STRING_SOS,
    VT100_STRING_PM,
    VT100_STRING_APC
} VT100StringKind;

/* VT100 前置声明，供结构体内部的异步回调函数指针使用。 */
typedef struct VT100 VT100;

/*
 * presentMask distinguishes an omitted parameter from an explicit zero.
 * Example: ESC[m has no present parameters; ESC[0m has parameter 0 present.
 */
typedef struct
{
    /* CSI 参数值，例如 ESC[12;40H 会得到 value[0]=12、value[1]=40。 */
    uint16_t value[VT100_MAX_PARAMS];

    /*
     * 参数存在位图：第 n 位为 1 表示第 n 个参数被显式写出。
     * 用于区分参数省略和显式传入 0，例如 ESC[m 与 ESC[0m。
     */
    uint16_t presentMask;

    /* 当前已经建立的参数槽数量，包括被分号分隔出的空参数槽。 */
    uint8_t count;

    /*
     * CSI 私有标记，常见为 '?'、'>'、'='、'<'；不存在时为 0。
     * 例如 ESC[?25l 中 privateMarker 为 '?'。
     */
    uint8_t privateMarker;

    /*
     * 参数之后、final byte 之前的 intermediate byte。
     * 合法范围为 0x20～0x2F，第一版可记录后再选择忽略命令。
     */
    uint8_t intermediate[VT100_MAX_INTERMEDIATES];

    /* 当前已经保存的 intermediate byte 数量。 */
    uint8_t intermediateCount;
} VT100CSIParameters;



/* 无附加参数的简单操作，例如退格、换行、保存光标。 */
typedef void (*VT100SimpleOperation)(void *context);

/* 携带次数的操作，例如光标上移 count 行、删除 count 个字符。 */
typedef void (*VT100CountOperation)(void *context, uint16_t count);

/* 携带单个模式值的操作，例如按 mode=0/1/2 擦除行或屏幕。 */
typedef void (*VT100ModeOperation)(void *context, uint8_t mode);

/*
 * 批量写入连续普通字符。
 * data 指向本次普通文本起点，length 表示可连续处理的字节数。
 */
typedef void (*VT100WriteRunOperation)(
    void *context,
    const uint8_t *data,
    uint32_t length);

/*
 * 光标绝对定位操作。
 * row、column 是解析并应用默认值后的终端坐标参数。
 */
typedef void (*VT100CursorPositionOperation)(
    void *context,
    uint16_t row,
    uint16_t column);

/*
 * SGR 图形属性操作，对应 CSI ... m。
 * parameters 中保留颜色、粗体、下划线、反色等全部原始参数。
 */
typedef void (*VT100SetGraphicsOperation)(
    void *context,
    const VT100CSIParameters *parameters);

/*
 * 设置或清除终端模式，对应 CSI ... h 和 CSI ... l。
 * privateMarker 用于区分标准模式与 DEC 私有模式；enabled 为 1 表示
 * 设置模式，为 0 表示清除模式。
 */
typedef void (*VT100SetModeOperation)(
    void *context,
    uint8_t privateMarker,
    const VT100CSIParameters *parameters,
    uint8_t enabled);

/* 设置上下滚动边界，对应 CSI top;bottom r。 */
typedef void (*VT100SetScrollRegionOperation)(
    void *context,
    uint16_t top,
    uint16_t bottom);

/*
 * 处理终端状态查询，对应 CSI ... n。
 * request 为查询编号，例如 5 表示设备状态，6 表示报告光标位置。
 */
typedef void (*VT100DeviceStatusOperation)(
    void *context,
    uint16_t request);

/*
 * 选择字符集，对应 ESC ( final、ESC ) final 等序列。
 * slot 表示 G0/G1 等字符集槽位，designator 表示字符集代号。
 */
typedef void (*VT100SelectCharsetOperation)(
    void *context,
    uint8_t slot,
    uint8_t designator);

/* GROUND state: printable bytes and single-byte C0 controls. */
typedef struct
{
    VT100WriteRunOperation writeRun;

    VT100SimpleOperation bell;
    VT100SimpleOperation backspace;
    VT100SimpleOperation horizontalTab;
    VT100SimpleOperation lineFeed;
    VT100SimpleOperation carriageReturn;
} VT100GroundOperations;

/* ESC state: short commands completed directly by an ESC final byte. */
typedef struct
{
    VT100SimpleOperation saveCursor;
    VT100SimpleOperation restoreCursor;
    VT100SimpleOperation index;
    VT100SimpleOperation nextLine;
    VT100SimpleOperation reverseIndex;
    VT100SimpleOperation reset;

    VT100SelectCharsetOperation selectCharset;
} VT100EscapeOperations;

/* CSI state: parameterized cursor, editing, style and mode operations. */
typedef struct
{
    VT100CountOperation cursorUp;
    VT100CountOperation cursorDown;
    VT100CountOperation cursorForward;
    VT100CountOperation cursorBackward;
    VT100CountOperation cursorNextLine;
    VT100CountOperation cursorPreviousLine;

    VT100CountOperation cursorHorizontalAbsolute;
    VT100CountOperation cursorVerticalAbsolute;
    VT100CursorPositionOperation cursorPosition;

    VT100ModeOperation eraseDisplay;
    VT100ModeOperation eraseLine;
    VT100CountOperation insertCharacters;
    VT100CountOperation deleteCharacters;
    VT100CountOperation eraseCharacters;

    VT100CountOperation insertLines;
    VT100CountOperation deleteLines;
    VT100CountOperation scrollUp;
    VT100CountOperation scrollDown;

    VT100SetGraphicsOperation setGraphics;
    VT100SetModeOperation setMode;
    VT100SetScrollRegionOperation setScrollRegion;

    VT100SimpleOperation saveCursor;
    VT100SimpleOperation restoreCursor;
    VT100DeviceStatusOperation deviceStatus;
} VT100CSIOperations;























/*
 * Store this table as static const. VT100 keeps only a pointer to it, so the
 * function table does not consume mutable RAM inside every parser object.
 * Every operation pointer must be bound. Use VT100_NopOperations while the
 * real display callbacks are not implemented.
 */
typedef struct
{
    VT100GroundOperations ground;
    VT100EscapeOperations escape;
    VT100CSIOperations csi;
} VT100Operations;

struct VT100
{
    const VT100Operations *operations;
    void *operationContext;

    /* 处理结束通知：上游通过 self->consumedLength 获得实际消费量。 */
    uint32_t (*const processCallBack)(struct VT100 *self);

    /*
     * 向上游申请当前可连续处理的数据。
     * 回调负责设置 self->byteStreamPtr，并返回可用字节数。
     */
    uint32_t (*const getAvailableLength)(struct VT100 *self);

    /* 当前借入的连续字节流起点，由 getAvailableLength 回调设置。 */
    uint8_t *byteStreamPtr;

    /* 当前借入的连续可用字节数。 */
    uint32_t availableLength;

    /* 本次 VT100_Process 实际消费量，每次调用都会重新刷新。 */
    uint32_t consumedLength;

    VT100State state;
    VT100StringKind stringKind;

    VT100CSIParameters csi;

    uint8_t escapeIntermediate[VT100_MAX_INTERMEDIATES];
    uint8_t escapeIntermediateCount;
};

/* 所有操作槽均绑定空函数，便于先运行解析器并在 GDB 中打断点。 */

/* 默认全局解析器，已静态绑定 displayerRx 输入和 NOP 终端操作表。 */
extern VT100 VT100_Instance;

/*
 * Initialize parser state and bind a static operation table.
 */
void VT100_Init(
    VT100 *self,
    const VT100Operations *operations,
    void *operationContext);

/*
 * Reset only protocol parsing state. Screen contents are not changed and the
 * terminal reset operation is not called.
 */
void VT100_ResetParser(VT100 *self);

/*
 * 主动调用 self->getAvailableLength() 获取数据，并处理至多 length 字节。
 * 每次调用开始时重新计算 self->consumedLength，并返回实际消费量。
 * 未完成的 ESC/CSI 序列仍会被消费，其解析现场保存在 self 中。
 */
uint32_t VT100_Process(
    VT100 *self,
    uint32_t length);

/* 调用 self->processCallBack()，通知上游释放 consumedLength 字节。 */
uint32_t VT100_ProcessCallBack(VT100 *self);

#endif /* VT100_H */
