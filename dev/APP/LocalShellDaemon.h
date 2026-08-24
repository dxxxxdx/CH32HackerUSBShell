//
// Created by dxxdx on 2026/7/31.
//

#ifndef CH32V203C8U_LOCALSHELLDAEMON_H
#define CH32V203C8U_LOCALSHELLDAEMON_H
#include <stdint.h>

/*
 * USB Host键盘当前是否可以产生有效输入。
 *
 * 取值：
 *
 *     0U：键盘不可用。
 *     1U：键盘已经完成枚举，可以产生输入。
 *
 * 该变量刻意采用单向全局状态，而不建立从Shell到USB Host
 * 的反向查询接口。当前项目只有一个固定USB键盘输入源，
 * 为一个状态位建立操作表或回调链会增加不必要的维护成本。
 *
 * 所有权：
 *
 *     USB Host键盘模块是唯一写者。
 *     LocalShellDaemon以及其他模块只能读取。
 *
 * 键盘物理连接不等于可用。只有键盘完成枚举并进入正常
 * 输入状态后才能写为1U。断开、重新枚举或枚举失败时，
 * 必须立即恢复为0U。
 *
 * 该声明不分配存储空间。变量实体只能在USB Host键盘模块
 * 的一个.c文件中定义一次。
 *
 * 当前变量只在主循环上下文中修改，因此不使用volatile。
 * 如果以后改为由中断直接修改，必须重新检查并发和volatile
 * 需求；更推荐中断只提交事件，仍由主循环修改该状态。
 *
 * 禁止在其他文件中手写另一份extern声明，必须包含本头文件，
 * 避免不同翻译单元使用不一致的变量类型。
 */
extern uint8_t usbHostKeyboardAvailable;

/*
 *                       ~
 *                    ~
 *                 ~
 *                ||
 *                ||
 *           _____||_____
 *        __/      ||     \__
 *       /        \||/       \
*       |       .------.      |
*        \_____/________\_____/
*             /__________\
*
*       USB键盘可用状态神位
*
* USB Host键盘模块是唯一写者。
* LocalShellDaemon以及其他模块只能读取。
*
* 0U：键盘不可用。
* 1U：键盘已经完成枚举，可以产生有效输入。
*
* 这是刻意保留的单向状态出口。
* 禁止为了查询该状态，将USBH_KB、描述符缓冲区、
* DMA缓冲区或USB Host内部状态机暴露给上层。
*/



/*
 * 避免LocalShell.h和LocalShellDaemon.h循环包含。
 */
struct LocalShell;


#define LOCAL_SHELL_DAEMON_AVAILABILITY_UNKNOWN \
0xFFU

#define LOCAL_SHELL_DAEMON_AVAILABILITY_WAITING \
0xFEU

typedef struct LocalShellDaemon
{
    /*
     * 0：网络不可用
     * 1：网络可用
     * 0xFF：尚未检查
     */
    uint8_t networkAvailable;

    /*
     * 0：键盘不可用
     * 1：键盘可用
     * 0xFF：尚未检查
     */
    uint8_t keyboardAvailable;


    /*
     * 启动宽限期的起始Tick低16位。
    */
    uint16_t delayBeginTick;



} LocalShellDaemon;

/*
 * 初始化守护状态。
 *
 * 不绑定ETH、输出或屏幕资源，
 * 所有资源都从LocalShell取得。
 */
void LocalShellDaemon_Init(
    LocalShellDaemon *self);


/*
 * 推进守护状态机一次。
 *
 * 网络状态发生变化时，可以要求LocalShell：
 *
 * 1. 抢占前台应用。
 * 2. 清空当前命令。
 * 3. 输出清屏序列和错误信息。
 * 4. 网络恢复后重新进入命令态。
 *
 * Daemon不允许直接操作displayerRx、VT100或Screen。
 */
void LocalShellDaemon_Process(
    LocalShellDaemon *self,
    struct LocalShell *shell);



























#endif //CH32V203C8U_LOCALSHELLDAEMON_H
