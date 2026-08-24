//
// Created by dxxdx on 2026/7/26.
//

#ifndef LOCAL_SHELL_APPLICATION_H
#define LOCAL_SHELL_APPLICATION_H

#include <stdint.h>

#include "NetworkManager.h"
#include "displayerRx.h"


/*
 * 应用启动函数的返回状态。
 *
 * 使用uint8_t保存，避免枚举类型默认占用四字节。
 */
typedef uint8_t LocalShellApplicationStartStatus;

enum
{
    /*
     * 参数有效，应用已经完成自身状态初始化。
     */
    LOCAL_SHELL_APPLICATION_START_SUCCESS = 0U,

    /*
     * 参数无效，或者应用当前无法启动。
     *
     * Shell不会把键盘控制权交给该应用。
     */
    LOCAL_SHELL_APPLICATION_START_REJECTED
};


/*
 * 应用执行一次时间片后的状态。
 */
typedef uint8_t LocalShellApplicationStatus;

enum
{
    /*
     * 应用尚未结束，下一轮继续调用process()。
     */
    LOCAL_SHELL_APPLICATION_RUNNING = 0U,

    /*
     * 应用正常结束，Shell收回键盘控制权。
     */
    LOCAL_SHELL_APPLICATION_EXIT_SUCCESS,

    /*
     * 应用因内部错误结束，Shell收回键盘控制权。
     */
    LOCAL_SHELL_APPLICATION_EXIT_FAILURE
};


/*
 * 前台应用从LocalShell继承的运行环境。
 *
 * 该结构及其中的绑定关系归LocalShell所有。
 * 应用只能在process()运行期间临时使用这些资源。
 *
 * process()接收到的是const指针，因此应用不能通过
 * 正常接口修改networkManager和standardOutput的绑定。
 *
 * const只保护这里的指针字段，不会把指针所指向的
 * NetworkManager和displayerRx对象变成只读对象。
 */
typedef struct LocalShellInherited
{
    /*
     * 当前Shell继承的网络协议栈聚合实例。
     *
     * 应用需要具体网络能力时，从聚合层取得对应manager。
     */
    NetworkManager *networkManager;

    /*
     * 当前Shell的标准输出。
     *
     * 应用可以向该对象写入数据，但不能决定输出目标。
     * 应用自己的结构体中不得保存displayerRx指针。
     */
    displayerRx *standardOutput;

    const volatile uint32_t* tick;


} LocalShellInherited;


/*
 * 初始化一个静态应用实例。
 *
 * 该函数只负责：
 *
 * 1. 检查命令参数。
 * 2. 复制需要长期保存的参数。
 * 3. 初始化应用内部状态机。
 *
 * 该函数不接收网络聚合对象和标准输出，也不执行网络事务。
 *
 * argumentVector中的字符串位于Shell的命令缓冲区，
 * 只保证在start()返回前有效。
 */
typedef LocalShellApplicationStartStatus
    (*LocalShellApplicationStart)(
        void *application,
        uint8_t argumentCount,
        const char *const *argumentVector);


/*
 * 接收Shell路由给当前前台应用的标准输入。
 *
 * data指向Shell输入缓冲区中的临时数据。
 * input()返回后，对应空间可以立即被覆盖。
 *
 * 应用如果需要延后处理，必须把所需数据复制到
 * 自己拥有的缓冲区。
 *
 * 该函数只负责接收输入和修改应用状态。
 * 实际网络发送由process()逐步推进。
 */
typedef void
    (*LocalShellApplicationInput)(
        void *application,
        const uint8_t *data,
        uint32_t length);


/*
 * 为应用分配一次协作式时间片。
 *
 * 每次调用只能推进有限工作，禁止：
 *
 * 1. 阻塞等待硬件。
 * 2. 阻塞等待网络。
 * 3. 阻塞等待定时器。
 * 4. 执行没有明确上界的循环。
 *
 * 应用在该函数运行期间临时使用inherited中的资源，
 * 但不得把其中的资源指针保存到自己的静态实例中。
 */
typedef LocalShellApplicationStatus
    (*LocalShellApplicationProcess)(
        void *application,
        const LocalShellInherited *inherited);


/*
 * 请求应用停止运行。
 *
 * 该函数只修改应用自身的退出状态，不执行阻塞清理。
 *
 * 需要分阶段完成的资源释放仍然由后续process()
 * 时间片推进。
 *
 * 当清理结束后，process()返回退出状态，
 * Shell才正式收回前台控制权。
 */
typedef void
    (*LocalShellApplicationRequestStop)(
        void *application);


/*
 * 一个静态应用的只读描述符。
 *
 * LocalShell通过静态描述符表查找和启动应用。
 *
 * 描述符本身只保存：
 *
 * 1. 命令名称。
 * 2. 使用说明。
 * 3. 静态应用实例。
 * 4. 应用操作函数。
 *
 * 描述符不保存标准输入、标准输出或者以太网绑定。
 */
typedef struct LocalShellApplication
{
    /*
     * 命令名称，例如"nmap"或者"telnet"。
     */
    const char *name;

    /*
     * 参数使用说明。
     */
    const char *usage;

    /*
     * 应用自身的静态状态机实例。
     */
    void *instance;

    /*
     * 检查参数并初始化应用。
     */
    LocalShellApplicationStart
        start;

    /*
     * 接收Shell转交的标准输入。
     */
    LocalShellApplicationInput
        input;

    /*
     * 推进应用一个时间片。
     */
    LocalShellApplicationProcess
        process;

    /*
     * 请求应用结束。
     */
    LocalShellApplicationRequestStop
        requestStop;
} LocalShellApplication;

#endif
