//
// Created by dxxdx on 2026/7/25.
//

#ifndef CH32V203C8U_USBH_HW_H
#define CH32V203C8U_USBH_HW_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 前向声明，硬件层只通过对象保存单笔事务上下文。 */
typedef struct USBH_KB USBH_KB;
/*
 * 单笔 Token 参数。
 *
 * 四个 uint8_t 正好占一个 RV32 字，按值传递即可。
 * toggle 使用逻辑值 0/1，而不是 WCH 寄存器位掩码。
 */
typedef struct
{
    uint8_t endpoint;
    uint8_t tokenType;
    uint8_t toggle;
    uint8_t length;
} USBH_TRANSACTION;
/*
 * 单笔 USB Token 的执行结果。
 *
 * 该结果只描述一个 SETUP / IN / OUT Token，不表示整个控制传输完成。
 */
typedef enum
{
    USBH_IO_BUSY = 0,
    USBH_IO_SUCCESS,
    USBH_IO_NAK,
    USBH_IO_STALL,
    USBH_IO_DISCONNECT,
    USBH_IO_TIMEOUT,
    USBH_IO_ERROR
} USBH_IO_RESULT;

/* USBFS 当前是否已经挂起一笔 Token。 */
typedef enum
{
    USBH_HW_IDLE = 0,
    USBH_HW_WAIT
} USBH_HW_STATUS;

/*
 * 协议层使用的抽象 Token 类型。
 * USBH.c 不再接触 WCH 的 USB_PID_* 寄存器编码。
 */
typedef enum
{
    USBH_TOKEN_SETUP = 0,
    USBH_TOKEN_OUT,
    USBH_TOKEN_IN
} USBH_TOKEN_TYPE;

/* 协议层使用的抽象设备速率。 */
typedef enum
{
    USBH_SPEED_FULL = 0,
    USBH_SPEED_LOW
} USBH_DEVICE_SPEED;



/*
 * 固定 64 字节 USBFS DMA 窗口。
 * union 通过 uint32_t 成员自然保证至少 4 字节对齐。
 */
typedef union
{
    uint32_t words[16];
    uint8_t bytes[64];
} USBH_PACKET_BUFFER;

_Static_assert(
    sizeof(USBH_PACKET_BUFFER) == 64U,
    "USBH_PACKET_BUFFER must be 64 bytes"
);

/* 绑定对象内的固定 DMA 缓冲区并打开 USBFS DMA。 */
void USBH_HW_Init(USBH_KB *self);

/* 查询根端口当前是否存在设备；内部顺手清除 DETECT 锁存标志。 */
uint8_t USBH_HW_IsAttached(void);

/*
 * 控制总线复位。
 *
 * enable != 0：停止旧事务、关闭端口、恢复地址 0 和 FS 默认配置，拉起 RESET。
 * enable == 0：仅释放 RESET。
 */
void USBH_HW_SetBusReset(USBH_KB *self, uint8_t enable);

/* 检测 LS/FS、配置控制器、使能根端口并开启自动 SOF。 */
void USBH_HW_EnableAttachedDevice(USBH_KB *self);

/* 修改 USBFS 当前目标设备地址。 */
void USBH_HW_SetAddress(uint8_t address);

/* 停止事务、释放复位、关闭 SOF/端口并恢复地址 0。 */
void USBH_HW_Disable(USBH_KB *self);

/*
 * 单笔 Token 的伪阻塞执行器。
 *
 * 第一次调用启动 Token 并返回 BUSY；后续调用只检查完成标志。
 */
USBH_IO_RESULT USBH_HW_Transact(
    USBH_KB *self,
    USBH_TRANSACTION transaction
);

#ifdef __cplusplus
}
#endif

#endif /* CH32V203C8U_USBH_HW_H */
