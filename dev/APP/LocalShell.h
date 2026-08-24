//
// Created by dxxdx on 2026/7/26.
//

#ifndef LOCAL_SHELL_H
#define LOCAL_SHELL_H

#include <stdint.h>

#include "App.h"
#include "LocalShellCommandDispatch.h"
#include "LocalShellDaemon.h"

/*
 * 本地键盘输入缓冲区容量。
 *
 * 键盘驱动通过LocalShell_Input()写入数据，
 * LocalShell_Process()逐步取出并进行路由。
 */
#define LOCAL_SHELL_INPUT_BUFFER_SIZE      32U

/*
 * LocalShell_Process()每轮最多处理的输入字节数。
 *
 * 该限制用于避免连续输入长期占用主循环。
 * 前台应用的process()每轮仍然只调用一次。
 */
#define LOCAL_SHELL_INPUT_BUDGET            8U


/*
 * 当前键盘输入的归属。
 *
 * 使用uint8_t保存，避免枚举在结构体中占用四字节。
 */
typedef uint8_t LocalShellState;

enum
{
    /*
     * Shell自己接收键盘数据，维护命令行。
     */
    LOCAL_SHELL_STATE_COMMAND = 0U,

    /*
     * 键盘数据被路由到当前前台应用。
     */
    LOCAL_SHELL_STATE_APPLICATION,
    LOCAL_SHELL_STATE_DAEMON
};


/*
 * LocalShell_Process()本轮的运行结果。
 *
 * 该状态只供上层调度器统计，不表示前台应用的退出状态。
 */
typedef uint8_t LocalShellProcessStatus;

enum
{
    LOCAL_SHELL_PROCESS_IDLE = 0U,
    LOCAL_SHELL_PROCESS_ACTIVE
};


/*
 * LocalShell_Input()的写入结果。
 */
typedef uint8_t LocalShellInputStatus;

enum
{
    LOCAL_SHELL_INPUT_ACCEPTED = 0U,

    /*
     * 输入缓冲区剩余空间不足。
     *
     * 本次数据整体拒绝，不覆盖尚未处理的数据，
     * 也不从中间截断一个输入序列。
     */
    LOCAL_SHELL_INPUT_FULL
};


/*
 * 本地Shell实例。
 *
 * LocalShell负责：
 *
 * 1. 保存键盘输入。
 * 2. 维护命令行状态。
 * 3. 查找并启动静态应用。
 * 4. 决定键盘输入的路由目标。
 * 5. 为当前前台应用分配时间片。
 * 6. 持有应用继承的网络和输出环境。
 *
 * LocalShell不负责实现具体应用业务。
 */
typedef struct LocalShell
{
    /*
     * Shell拥有的继承环境。
     *
     * 应用只能在被调度时临时获得该结构的只读视图。
     */
    LocalShellInherited inherited;


    /*
     * Shell自己的后台监督状态机。
     */
    LocalShellDaemon daemon;

    /*
     * 当前获得键盘输入和运行时间片的应用。
     *
     * 命令态时为0。
     * 应用态时指向静态应用表中的某一项。
     */
    const LocalShellApplication
        *foregroundApplication;

    /*
     * 键盘输入先进先出缓冲区。
     *
     * LocalShell_Input()负责写入，
     * LocalShell_Process()负责读取。
     */
    uint8_t inputBuffer[
        LOCAL_SHELL_INPUT_BUFFER_SIZE];

    /*
     * 下一个待读取字节的位置。读的就是上面那个inputbuffer
     */
    volatile uint8_t inputReadIndex;

    /*
     * 下一个待写入字节的位置。
     */
    volatile uint8_t inputWriteIndex;

    /*
     * 当前已经保存的输入字节数。
     */
    volatile uint8_t inputLength;

    /*
    * 当前键盘输入路由状态。
    */
    volatile LocalShellState state;

    /*
     * Shell命令行编辑、解析和分发状态。
     */
    LocalShellCommandDispatchStage commandDispatch;



} LocalShell;


/*
 * 初始化LocalShell的运行期状态，并绑定继承资源。
 *
 * 静态应用表和应用实例由LocalShell.c直接定义。
 * 输入缓冲区和命令缓冲区由LocalShell自身持有。
 *
 * 因此初始化函数只接收运行期必须绑定的：
 *
 * 1. 网络协议栈聚合实例。
 * 2. 标准输出实例。
 */
void LocalShell_Init(
    LocalShell *self,
    NetworkManager *networkManager);


/*
 * 向LocalShell写入键盘数据。
 *
 * 当输入缓冲区剩余空间不足时，本次数据整体拒绝，
 * 不覆盖任何尚未消费的数据。
 */
LocalShellInputStatus LocalShell_Input(
    LocalShell *self,
    const uint8_t *source,
    uint32_t length);


/*
 * 推进LocalShell一次。
 *
 * 每轮最多处理LOCAL_SHELL_INPUT_BUDGET个输入字节。
 *
 * 命令态：
 *
 *     输入交给Shell自己的命令行状态机。
 *
 * 应用态：
 *
 *     输入交给foregroundApplication->input()。
 *
 * 输入处理完成后，如果存在前台应用，则调用一次
 * foregroundApplication->process()。
 *
 * 应用退出后，Shell清除foregroundApplication，
 * 收回键盘控制权，切换回命令态并输出新提示符。
 */
LocalShellProcessStatus LocalShell_Process(
    LocalShell *self);




extern LocalShell Local_Shell;









#endif
