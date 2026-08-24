#ifndef DISPLAYER_CHIP_H
#define DISPLAYER_CHIP_H

#include "displayerChipConfig.h"

#include <stdint.h>

/*
 * DisplayerChip 是 Screen 和具体显示控制器之间的提交接口。
 *
 * 这一层不定义面板像素宽高，也不给终端行列数兜底默认值。
 * 选中的显示芯片配置必须通过条件编译定义 DISPLAYER_CHIP_TEXT_COLUMNS
 * 和 DISPLAYER_CHIP_TEXT_ROWS。Screen 只消费这两个编译期常量来分配静态
 * 字符拓扑，不在运行期接收尺寸字段。
 *
 * Screen 拥有字体渲染；具体芯片适配层拥有真实面板几何、GRAM 偏移、
 * 滚动寄存器和分屏布局。
 *
 * 接入某个芯片时，在适配实现里用 _Static_assert 交叉验证：
 * Screen 渲染出来的完整字符画布必须能完全放进实际可见区域。
 * 典型检查是：
 *   (DISPLAYER_CHIP_TEXT_COLUMNS * SCREEN_CHAR_WIDTH) <= CHIP_VISIBLE_WIDTH_PIXELS
 *   (DISPLAYER_CHIP_TEXT_ROWS * SCREEN_CHAR_HEIGHT) <= CHIP_VISIBLE_HEIGHT_PIXELS
 * 不满足时应该编译期报错，不要在运行期裁剪、取模或偷偷丢像素。
 */

#ifndef DISPLAYER_CHIP_TEXT_COLUMNS
#error "Selected displayer chip must define DISPLAYER_CHIP_TEXT_COLUMNS"
#endif

#ifndef DISPLAYER_CHIP_TEXT_ROWS
#error "Selected displayer chip must define DISPLAYER_CHIP_TEXT_ROWS"
#endif

typedef struct DisplayerChip DisplayerChip;

typedef struct
{
    uint8_t (*const isIdle)(
        const DisplayerChip *self);
    /*
     * 提交一次“整字符行高度”的硬件上滚。
     *
     * 这个函数只改变显示芯片的可见窗口，不负责拷贝 Screen 的字符缓冲，
     * 也不返回需要重绘的物理行。返回 1 表示本次滚动事务已经提交，
     * Screen 可以推进 committedTopPhysicalRow 并补写露出行；返回 0 表示
     * 本次没有触发滚动，Screen 不得修改已提交拓扑，也不得补写露出行。
     *
     * Screen 自己维护已提交的可见顶行：
     * 每收到一次成功返回，Screen 会把 committedTopPhysicalRow 向后推进 1 行，
     * 并认为环形字符画布的最后一行是本次从底部露出的 GRAM 物理文本行，
     * 随后调用 writeTextRowDMA() 重绘它。
     *
     * 调用返回时，芯片层必须已经可以立刻接收紧随其后的 writeTextRowDMA()。
     * 也就是说，本函数可以阻塞发送滚动命令，但不能留下一个会和下一次行 DMA
     * 互相踩踏的异步事务。
     *
     * 多屏幕实现时，适配层负责把这一次终端行滚动分发给所有参与显示的芯片，
     * 并保证它们的可见窗口在同一个逻辑事务内前进一个字体高度。Screen 不知道
     * 分屏布局，也不应该知道某块屏的控制器滚动寄存器、方向位或 GRAM 偏移。
     *
     * 若某个控制器没有硬件滚动能力，不能把这个操作做成简单 NOP。
     * 适配层必须在本函数内部提供等效的“一行上滚”语义，例如使用控制器内存搬移、
     * 自有脏区刷新机制，或另行实现能满足本接口契约的提交路径。
     */
    uint8_t (*const scrollUpOneTextRow)(
        DisplayerChip *self);

    /*
     * pixels 指向 Screen 渲染好的一条 RGB565 文本行。
     * 像素数量由 Screen 的列数、字体宽度和字体高度决定，具体芯片适配层
     * 只能静态校验它能完整写入目标区域，不能在这里重新定义另一套尺寸。
     * DMA 期间具体芯片层必须让 isIdle 返回 0。
     */
    void (*const writeTextRowDMA)(
        DisplayerChip *self,
        uint8_t physicalRow,
        uint16_t *pixels);
} DisplayerChipOperations;

struct DisplayerChip
{
    const DisplayerChipOperations *const ops;

    /*
     * 长期 DMA 行缓冲归芯片层静态拥有，容量必须能容纳 Screen 的一条
     * 完整 RGB565 文本行。Screen 只在芯片空闲时写入。
     */
    uint16_t *const lineBuffer;
};

extern DisplayerChip displayerChip;

void DisplayerChip_Init(DisplayerChip *self);

#endif /* DISPLAYER_CHIP_H */
