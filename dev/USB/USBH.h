//
// Created by dxxdx on 2026/7/25.
//

#ifndef CH32V203C8U_USBH_H
#define CH32V203C8U_USBH_H

#include <stdint.h>

#include "USBH_HW.h"

#ifdef __cplusplus
extern "C" {
#endif

#define USBH_PACKET_BUFFER_SIZE       64U
#define USBH_DESCRIPTOR_BUFFER_SIZE  128U
#define USBH_BOOT_REPORT_SIZE          8U






/*
 * 根端口直连、单设备、Boot Keyboard 专用 USB Host 对象。
 *
 * 顶层状态枚举、EP0 控制传输状态和步骤函数全部封装在 USBH.c。
 * WCH USBFS 寄存器操作全部封装在 USBH_HW.c。
 *
 * 对象必须静态/全局或显式使用 {0} 初始化。
 * 主循环中只需高频调用：
 *
 *     USBH_Process(&usbKeyboard, systemTick20ms);
 */
struct USBH_KB
{
    /* USB Host 顶层状态，具体枚举仅在 USBH.c 内部可见。 */
    uint8_t USB_status;

    /* 当前 EP0 控制传输阶段，具体枚举仅在 USBH.c 内部可见。 */
    uint8_t CTRL_status;

    /* 当前单笔 Token 的底层状态，使用 USBH_HW_STATUS。 */
    uint8_t HW_status;

    /* 当前 USB 设备地址；总线复位后恢复为 0。 */
    uint8_t deviceAddress;

    /* 当前设备速率，保存 USBH_SPEED_FULL 或 USBH_SPEED_LOW。 */
    uint8_t deviceSpeed;

    /* EP0 最大包长；枚举开始为 8，随后由 bMaxPacketSize0 更新。 */
    uint8_t ep0MaxPacket;

    /* 配置描述符中的 bConfigurationValue。 */
    uint8_t configurationValue;

    /* Boot Keyboard 接口的 bInterfaceNumber。 */
    uint8_t keyboardInterface;

    /* Keyboard Interrupt IN 端点号，只保存低四位。 */
    uint8_t keyboardInEndpoint;

    /* Keyboard Interrupt IN 端点的 DATA0 / DATA1 逻辑状态。 */
    uint8_t keyboardInToggle;

    /* 最近一次成功 IN Token 的实际接收长度。 */
    uint8_t rxLength;

    /* 上一次标准 8 字节 Boot Keyboard Report。 */
    uint8_t previousReport[USBH_BOOT_REPORT_SIZE];

    /* 顶层状态截止 tick：消抖、复位、地址稳定和轮询共用；1 tick = 20 ms。 */
    uint32_t deadline;

    /* 当前 EP0 控制传输的总超时截止 tick；1 tick = 20 ms。 */
    uint32_t controlDeadline;

    /* 当前已经启动的单笔 Token；由 USBH_HW_Transact() 保存。 */
    USBH_TRANSACTION currentTransaction;

    /* 当前控制传输 DATA 阶段的缓冲区；无 DATA 阶段时为 0。 */
    uint8_t *controlBuffer;

    /* 当前控制传输还剩多少 DATA 字节。 */
    uint16_t controlRemaining;

    /* 当前控制传输已经完成多少 DATA 字节。 */
    uint16_t controlTransferred;

    /* 当前控制传输方向，保存 Setup Packet 的 bmRequestType bit7。 */
    uint8_t controlDirection;

    /* 控制传输 DATA 阶段的逻辑 DATA0 / DATA1 状态。 */
    uint8_t controlToggle;

    /* 配置描述符声明的 wTotalLength。 */
    uint16_t configurationDescriptorLength;

    /* USBFS 唯一的 64 字节发送 DMA 窗口。 */
    USBH_PACKET_BUFFER txBuffer;

    /* USBFS 唯一的 64 字节接收 DMA 窗口。 */
    USBH_PACKET_BUFFER rxBuffer;

    /* 设备描述符和配置描述符的固定累计缓冲区。 */
    uint8_t descriptorBuffer[USBH_DESCRIPTOR_BUFFER_SIZE];

};



/*
 * usbHostKeyboardAvailable的唯一存储实体。
 *
 * 上电默认没有可用键盘。该变量只能由本模块的枚举、
 * 断开和错误处理路径修改。
 */
extern uint8_t usbHostKeyboardAvailable;




/*
 * USB Host 总调度函数。
 *
 * 应在主循环中高频调用。每次调用最多推进一个顶层状态或一笔 Token，
 * 不会在内部等待完整枚举或完整控制传输结束。
 *
 * nowTick 每 20 ms 递增一次，并允许 uint32_t 自然回绕。
 */
void USBH_Process(
    USBH_KB *self,
    uint32_t nowTick
);

void USBH_OnKeyDown(
    USBH_KB *self,
    uint8_t modifier,
    uint8_t usageId
);

extern USBH_KB usbKeyboard;

#ifdef __cplusplus
}
#endif

#endif /* CH32V203C8U_USBH_H */