# DisplayerChip 接入说明

`DisplayerChip` 是 `Screen` 向具体显示芯片提交 RGB565 文本行的边界。`Screen` 负责 VT100 后端状态、字符拓扑、字体渲染和脏行选择；显示芯片适配层负责真实面板几何、GRAM 偏移、滚动寄存器、DMA 提交和多屏分发。

## 编译期配置

选中的显示芯片配置必须在 `displayerChipConfig.h` 中定义：

```c
#define DISPLAYER_CHIP_ID_ST7735S 1U
#define DISPLAYER_CHIP_SELECTED DISPLAYER_CHIP_ID_ST7735S
#define DISPLAYER_CHIP_USE_ST7735S 1U
#define DISPLAYER_CHIP_TEXT_COLUMNS 16U
#define DISPLAYER_CHIP_TEXT_ROWS    20U
```

这两个宏是当前显示芯片给 `Screen` 暴露的终端字符网格，不是面板像素宽高。`Screen` 用它们分配静态字符缓冲，不允许运行期传行列数，也不使用动态内存。
`DISPLAYER_CHIP_USE_xxx` 宏用于给未选中的芯片源码做整文件门控，避免 gcc 编译无关驱动实现。

`Screen` 自己定义当前字体尺寸：

```c
#define SCREEN_CHAR_WIDTH   8U
#define SCREEN_CHAR_HEIGHT  8U
```

适配层接入具体芯片时必须做编译期校验，确保字符画布完整放进真实可见区域：

```c
_Static_assert(
    (DISPLAYER_CHIP_TEXT_COLUMNS * SCREEN_CHAR_WIDTH) <= CHIP_VISIBLE_WIDTH_PIXELS,
    "Screen text width does not fit selected displayer chip");

_Static_assert(
    (DISPLAYER_CHIP_TEXT_ROWS * SCREEN_CHAR_HEIGHT) <= CHIP_VISIBLE_HEIGHT_PIXELS,
    "Screen text height does not fit selected displayer chip");
```

不要在运行期裁剪、取模或丢像素。真实面板像素宽高、窗口偏移和分屏布局只应该出现在具体芯片适配实现里。

## 必要对象

适配层必须提供全局静态对象：

```c
DisplayerChip displayerChip = {
    .ops = &xxxOperations,
    .lineBuffer = xxxLineBuffer,
};
```

`lineBuffer` 必须是长期静态缓冲，容量至少为 `SCREEN_LINE_PIXELS` 个 `uint16_t` RGB565 像素，并满足具体 DMA 的对齐要求。`Screen` 只在 `isIdle()` 返回 1 后写入该缓冲。

显示芯片初始化走抽象入口：

```c
void DisplayerChip_Init(DisplayerChip *self);
```

当前选中的芯片适配层提供这个符号。上层只调用 `DisplayerChip_Init(&displayerChip)`，不直接 include 具体芯片头。

## DisplayerChipOperations

`isIdle`

```c
uint8_t (*const isIdle)(const DisplayerChip *self);
```

返回 1 表示芯片层当前可以接收新的滚动命令或文本行 DMA；返回 0 表示仍忙。DMA 期间必须返回 0。

`scrollUpOneTextRow`

```c
uint8_t (*const scrollUpOneTextRow)(DisplayerChip *self);
```

提交一次整字符行高度的上滚。返回 1 表示滚动事务已经稳定触发，`Screen` 会推进已提交拓扑并补写本次露出的底部物理文本行；返回 0 表示本次没有触发滚动，`Screen` 不推进状态、不补写露出行。

调用返回后，芯片层必须已经能立刻接收紧随其后的 `writeTextRowDMA()`。如果控制器没有硬件滚动能力，适配层也必须提供等效的一行上滚语义，不能简单 NOP。

`writeTextRowDMA`

```c
void (*const writeTextRowDMA)(
    DisplayerChip *self,
    uint8_t physicalRow,
    uint16_t *pixels);
```

提交 `physicalRow` 对应的 GRAM 物理文本行。`pixels` 指向 `Screen` 已渲染好的完整 RGB565 文本行，长度是 `SCREEN_LINE_PIXELS` 个 `uint16_t`。函数启动提交后，直到传输真正完成前，`isIdle()` 必须返回 0。

## 多屏扩展

多屏时仍然只暴露一个 `DisplayerChip displayerChip` 给 `Screen`。适配层内部负责把 `physicalRow` 和像素行切分到各个屏幕，或把一次 `scrollUpOneTextRow()` 分发给所有参与显示的芯片。

`Screen` 不应该知道每块屏幕的控制器型号、GRAM 偏移、MADCTL/方向位、滚动寄存器或左右/上下拼接方式。
