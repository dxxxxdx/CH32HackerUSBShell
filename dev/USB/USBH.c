//
// Created by dxxdx on 2026/7/25.
//

#include "USBH.h"

#include <string.h>

#include "USBH_HW.h"


/* -------------------------------------------------------------------------- */
/*                              私有协议常量                                  */
/* -------------------------------------------------------------------------- */

/*
 * USBH_Process() 使用 20 ms 单调递增 tick。
 * 下列时间常量全部以 tick 为单位，不再使用毫秒。
 */
#define USBH_TICK_PERIOD_MS                 20U
#define USBH_ATTACH_DEBOUNCE_TICKS           1U  /* 20 ms */
#define USBH_BUS_RESET_TICKS                 1U  /* 20 ms */
#define USBH_RESET_SETTLE_TICKS              1U  /* 20 ms */
#define USBH_ADDRESS_SETTLE_TICKS            1U  /* 20 ms */
#define USBH_CONTROL_TIMEOUT_TICKS           10U  /* 200 ms */
#define USBH_KEYBOARD_POLL_TICKS              1U  /* 20 ms */
#define USBH_ERROR_BACKOFF_TICKS             50U  /* 1000 ms */

#define USBH_DEFAULT_DEVICE_ADDRESS       1U
#define USBH_EP0_DEFAULT_PACKET_SIZE      8U
#define USBH_DEVICE_DESCRIPTOR_SIZE      18U
#define USBH_CONFIG_DESCRIPTOR_HEAD_SIZE  9U

#define USBH_REQUEST_DIRECTION_IN       0x80U
#define USBH_REQUEST_DIRECTION_OUT      0x00U
#define USBH_REQUEST_TYPE_STANDARD      0x00U
#define USBH_REQUEST_TYPE_CLASS         0x20U
#define USBH_REQUEST_RECIPIENT_DEVICE   0x00U
#define USBH_REQUEST_RECIPIENT_INTERFACE 0x01U

#define USBH_REQUEST_GET_DESCRIPTOR      0x06U
#define USBH_REQUEST_SET_ADDRESS         0x05U
#define USBH_REQUEST_SET_CONFIGURATION   0x09U
#define USBH_HID_REQUEST_SET_PROTOCOL    0x0BU

#define USBH_DESCRIPTOR_TYPE_DEVICE      0x01U
#define USBH_DESCRIPTOR_TYPE_CONFIG      0x02U
#define USBH_DESCRIPTOR_TYPE_INTERFACE   0x04U
#define USBH_DESCRIPTOR_TYPE_ENDPOINT    0x05U

#define USBH_CLASS_HID                   0x03U
#define USBH_HID_SUBCLASS_BOOT           0x01U
#define USBH_HID_PROTOCOL_KEYBOARD       0x01U
#define USBH_ENDPOINT_DIRECTION_IN       0x80U
#define USBH_ENDPOINT_TRANSFER_INTERRUPT 0x03U

#define USBH_SETUP_PACKET_SIZE            8U

#if defined(__GNUC__) || defined(__clang__)
#define USBH_LIKELY(condition)   __builtin_expect(!!(condition), 1)
#else
#define USBH_LIKELY(condition)   (condition)
#endif

/*
 *                       ~
 *                    ~
 *                 ~
 *               ||
 *               ||
 *          _____||_____
 *       __/      ||     \__
 *      /        \||/       \                */
uint8_t usbHostKeyboardAvailable = 0U;
 /*
 *     |       .------.       |
 *      \_____/________\_____/
 *           /__________\
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



/* -------------------------------------------------------------------------- */
/*                              私有状态定义                                  */
/* -------------------------------------------------------------------------- */

/*
 * 整个根端口设备的生命周期状态。
 *
 * 这些值只在本文件内可见，应用层不依赖具体数值。
 */
typedef enum
{
    /*
     * 对象的零初始化状态。
     * USBH_Process() 第一次调用时完成软件状态清零、DMA 窗口绑定和硬件层初始化。
     */
    USBH_POWER_ON = 0,

    USBH_DISCONNECTED,
    USBH_ATTACH_DEBOUNCE,
    USBH_RESET_BEGIN,
    USBH_RESET_WAIT,
    USBH_RESET_SETTLE,
    USBH_PORT_ENABLE,

    USBH_GET_DEVICE_DESCRIPTOR_HEAD,
    USBH_GET_DEVICE_DESCRIPTOR_FULL,
    USBH_SET_ADDRESS,
    USBH_ADDRESS_SETTLE,
    USBH_GET_CONFIG_DESCRIPTOR_HEAD,
    USBH_GET_CONFIG_DESCRIPTOR_FULL,
    USBH_PARSE_CONFIGURATION,
    USBH_SET_CONFIGURATION,
    USBH_SET_BOOT_PROTOCOL,

    USBH_KEYBOARD_READY,
    USBH_KEYBOARD_POLL,

    USBH_ERROR_BACKOFF
} USBH_STATUS;


/* EP0 控制传输的 SETUP / DATA / STATUS 阶段。 */
typedef enum
{
    USBH_CTRL_IDLE = 0,
    USBH_CTRL_SETUP,
    USBH_CTRL_DATA_IN,
    USBH_CTRL_DATA_OUT,
    USBH_CTRL_STATUS_OUT,
    USBH_CTRL_STATUS_IN
} USBH_CTRL_STATUS;


/* 私有步骤函数的推进结果。 */
typedef enum
{
    USBH_STEP_WAIT = 0,
    USBH_STEP_DONE,
    USBH_STEP_DISCONNECT,
    USBH_STEP_RESTART,
    USBH_STEP_ERROR
} USBH_STEP_RESULT;


/* -------------------------------------------------------------------------- */
/*                              私有函数声明                                  */
/* -------------------------------------------------------------------------- */

static void USBH_PowerOn(USBH_KB *self);

static USBH_STEP_RESULT USBH_PhyDetect(USBH_KB *self, uint32_t nowTick);
static USBH_STEP_RESULT USBH_DeviceConnect(USBH_KB *self, uint32_t nowTick);
static USBH_STEP_RESULT USBH_DeviceScan(USBH_KB *self, uint32_t nowTick);
static USBH_STEP_RESULT USBH_GetReport(USBH_KB *self, uint32_t nowTick);

static void USBH_ControlStart(
    USBH_KB *self,
    const uint8_t setupPacket[USBH_SETUP_PACKET_SIZE],
    uint8_t *dataBuffer,
    uint16_t dataLength,
    uint32_t nowTick
);

static USBH_STEP_RESULT USBH_ControlProcess(USBH_KB *self, uint32_t nowTick);

static uint8_t USBH_ParseKeyboardConfiguration(
    USBH_KB *self,
    const uint8_t *descriptor,
    uint16_t length
);

static void USBH_ProcessKeyboardReport(
    USBH_KB *self,
    const uint8_t *report,
    uint8_t length
);

static void USBH_ClearDeviceState(USBH_KB *self);
static void USBH_RestartEnumeration(USBH_KB *self, uint32_t nowTick);
static void USBH_ApplyStepResult(USBH_KB *self, USBH_STEP_RESULT result, uint32_t nowTick);

static USBH_STEP_RESULT USBH_ScanDeviceDescriptor(USBH_KB *self, uint32_t nowTick);
static USBH_STEP_RESULT USBH_ScanAddress(USBH_KB *self, uint32_t nowTick);
static USBH_STEP_RESULT USBH_ScanConfiguration(USBH_KB *self, uint32_t nowTick);
static USBH_STEP_RESULT USBH_ScanKeyboardFinalize(USBH_KB *self, uint32_t nowTick);

static uint8_t USBH_TimeReached(uint32_t nowTick, uint32_t deadlineTick);
static uint16_t USBH_ReadU16LE(const uint8_t *data);
static void USBH_WriteU16LE(uint8_t *data, uint16_t value);

static void USBH_BuildGetDescriptorRequest(
    uint8_t setup[USBH_SETUP_PACKET_SIZE],
    uint8_t descriptorType,
    uint8_t descriptorIndex,
    uint16_t length
);

static void USBH_BuildSetAddressRequest(
    uint8_t setup[USBH_SETUP_PACKET_SIZE],
    uint8_t address
);

static void USBH_BuildSetConfigurationRequest(
    uint8_t setup[USBH_SETUP_PACKET_SIZE],
    uint8_t configurationValue
);

static void USBH_BuildSetBootProtocolRequest(
    uint8_t setup[USBH_SETUP_PACKET_SIZE],
    uint8_t interfaceNumber
);


/* -------------------------------------------------------------------------- */
/*                              对外入口                                      */
/* -------------------------------------------------------------------------- */

/*
 * USBH_KB 必须是静态/全局对象，或由调用者显式使用 {0} 初始化。
 * 这样 USB_status 上电后自然为 USBH_POWER_ON，第一次 Process 调用即可自举。
 */
static void USBH_PowerOn(USBH_KB *self)
{
    /*
     * 这里只在 POWER_ON 状态执行一次。对象内的固定 DMA 缓冲区也在此清零，
     * 随后 USBH_HW_Init() 直接把 rxBuffer/txBuffer 地址绑定到 USBFS。
     */
    memset(self, 0, sizeof(*self));

    usbHostKeyboardAvailable = 0U;

    self->CTRL_status = (uint8_t)USBH_CTRL_IDLE;
    self->HW_status = (uint8_t)USBH_HW_IDLE;

    self->deviceAddress = 0U;
    self->deviceSpeed = (uint8_t)USBH_SPEED_FULL;
    self->ep0MaxPacket = USBH_EP0_DEFAULT_PACKET_SIZE;

    USBH_HW_Init(self);

    /* 硬件和软件上下文均已就绪，从下一时间片开始检查设备接入。 */
    self->USB_status = (uint8_t)USBH_DISCONNECTED;
}


/*
 * nowTick 每 20 ms 递增一次。主循环仍应高频调用本函数；
 * Token 完成检查不会被限制为 20 ms 一次。
 */
void USBH_Process(USBH_KB *self, uint32_t nowTick)
{
    USBH_STEP_RESULT result = USBH_STEP_WAIT;
    USBH_STATUS status = (USBH_STATUS)self->USB_status;

    /*
     * 上电自举只占第一次时间片：清对象、绑定 DMA、初始化硬件抽象层。
     * 应用层因此只需反复调用 USBH_Process()，不再单独调用 USBH_Init()。
     */
    if(status == USBH_POWER_ON)
    {
        USBH_PowerOn(self);
        return;
    }

    /*
     * 连接后的常规状态优先检查拔出。
     * 但总线复位期间，主机会主动把 D+/D- 压成 SE0。
     * 这时 USBFS_UMS_DEV_ATTACH 可能暂时撤销，不能据此判定设备拔出。
     *
     * RESET_SETTLE 也跳过顶层拔出检测；等待稳定期结束后，
     * USBH_DeviceConnect() 会在该状态内部重新检查物理连接。
     */
    if((status != USBH_DISCONNECTED) &&
       (status != USBH_RESET_BEGIN) &&
       (status != USBH_RESET_WAIT) &&
       (status != USBH_RESET_SETTLE))
    {
        result = USBH_PhyDetect(self, nowTick);
        if(result == USBH_STEP_DISCONNECT)
        {
            USBH_ApplyStepResult(self, result, nowTick);
            return;
        }
    }

    /*
     * READY/POLL 是设备正常工作后的绝对热路径，绕过下面的枚举 switch。
     * READY 通常只检查 1 tick deadline；POLL 只推进当前 Interrupt IN Token。
     */
    status = (USBH_STATUS)self->USB_status;
    if(USBH_LIKELY((status == USBH_KEYBOARD_READY) ||
                    (status == USBH_KEYBOARD_POLL)))
    {
        result = USBH_GetReport(self, nowTick);

        /* WAIT/DONE 不需要进入冷路径错误分发。 */
        if(USBH_LIKELY(result <= USBH_STEP_DONE))
        {
            return;
        }

        USBH_ApplyStepResult(self, result, nowTick);
        return;
    }

    /*
     * 冷路径只处理插拔、复位和枚举；具体协议步骤继续留在分层函数中。
     */
    switch(status)
    {
        case USBH_DISCONNECTED:
        {
            result = USBH_PhyDetect(self, nowTick);
            break;
        }

        case USBH_ATTACH_DEBOUNCE:
        case USBH_RESET_BEGIN:
        case USBH_RESET_WAIT:
        case USBH_RESET_SETTLE:
        case USBH_PORT_ENABLE:
        {
            result = USBH_DeviceConnect(self, nowTick);
            break;
        }

        case USBH_GET_DEVICE_DESCRIPTOR_HEAD:
        case USBH_GET_DEVICE_DESCRIPTOR_FULL:
        case USBH_SET_ADDRESS:
        case USBH_ADDRESS_SETTLE:
        case USBH_GET_CONFIG_DESCRIPTOR_HEAD:
        case USBH_GET_CONFIG_DESCRIPTOR_FULL:
        case USBH_PARSE_CONFIGURATION:
        case USBH_SET_CONFIGURATION:
        case USBH_SET_BOOT_PROTOCOL:
        {
            result = USBH_DeviceScan(self, nowTick);
            break;
        }

        case USBH_ERROR_BACKOFF:
        {
            if(USBH_TimeReached(nowTick, self->deadline) != 0U)
            {
                USBH_RestartEnumeration(self, nowTick);
            }
            break;
        }

        case USBH_POWER_ON:
        case USBH_KEYBOARD_READY:
        case USBH_KEYBOARD_POLL:
        default:
        {
            /* POWER_ON 和键盘热路径已经在 switch 前处理，落到这里表示状态损坏。 */
            result = USBH_STEP_ERROR;
            break;
        }
    }

    USBH_ApplyStepResult(self, result, nowTick);
}





/* -------------------------------------------------------------------------- */
/*                           根端口物理状态机                                  */
/* -------------------------------------------------------------------------- */

static USBH_STEP_RESULT USBH_PhyDetect(USBH_KB *self, uint32_t nowTick)
{
    uint8_t attached = USBH_HW_IsAttached();

    if((USBH_STATUS)self->USB_status == USBH_DISCONNECTED)
    {
        if(attached != 0U)
        {
            self->deadline = nowTick + USBH_ATTACH_DEBOUNCE_TICKS;
            self->USB_status = (uint8_t)USBH_ATTACH_DEBOUNCE;
            return USBH_STEP_DONE;
        }

        return USBH_STEP_WAIT;
    }

    if(attached == 0U)
    {
        return USBH_STEP_DISCONNECT;
    }

    return USBH_STEP_WAIT;
}


static USBH_STEP_RESULT USBH_DeviceConnect(USBH_KB *self, uint32_t nowTick)
{
    switch((USBH_STATUS)self->USB_status)
    {
        case USBH_ATTACH_DEBOUNCE:
        {
            if(USBH_TimeReached(nowTick, self->deadline) == 0U)
            {
                return USBH_STEP_WAIT;
            }

            if(USBH_HW_IsAttached() == 0U)
            {
                return USBH_STEP_DISCONNECT;
            }

            self->USB_status = (uint8_t)USBH_RESET_BEGIN;
            return USBH_STEP_DONE;
        }

        case USBH_RESET_BEGIN:
        {
            /*
             * 具体寄存器动作全部收进 USBH_HW：
             * 停止旧 Token、关闭端口、恢复地址 0、恢复 FS 默认配置并拉起 RESET。
             */
            USBH_HW_SetBusReset(self, 1U);
            self->deadline = nowTick + USBH_BUS_RESET_TICKS;
            self->USB_status = (uint8_t)USBH_RESET_WAIT;
            return USBH_STEP_DONE;
        }

        case USBH_RESET_WAIT:
        {
            if(USBH_TimeReached(nowTick, self->deadline) == 0U)
            {
                return USBH_STEP_WAIT;
            }

            USBH_HW_SetBusReset(self, 0U);
            self->deadline = nowTick + USBH_RESET_SETTLE_TICKS;
            self->USB_status = (uint8_t)USBH_RESET_SETTLE;
            return USBH_STEP_DONE;
        }

        case USBH_RESET_SETTLE:
        {
            if(USBH_TimeReached(nowTick, self->deadline) == 0U)
            {
                return USBH_STEP_WAIT;
            }

            if(USBH_HW_IsAttached() == 0U)
            {
                return USBH_STEP_DISCONNECT;
            }

            self->USB_status = (uint8_t)USBH_PORT_ENABLE;
            return USBH_STEP_DONE;
        }

        case USBH_PORT_ENABLE:
        {
            if(USBH_HW_IsAttached() == 0U)
            {
                return USBH_STEP_DISCONNECT;
            }

            USBH_HW_EnableAttachedDevice(self);

            self->deviceAddress = 0U;
            self->ep0MaxPacket = USBH_EP0_DEFAULT_PACKET_SIZE;
            self->USB_status = (uint8_t)USBH_GET_DEVICE_DESCRIPTOR_HEAD;
            return USBH_STEP_DONE;
        }

        default:
        {
            return USBH_STEP_ERROR;
        }
    }
}


/* -------------------------------------------------------------------------- */
/*                              设备枚举状态机                                 */
/* -------------------------------------------------------------------------- */

static USBH_STEP_RESULT USBH_DeviceScan(USBH_KB *self, uint32_t nowTick)
{
    switch((USBH_STATUS)self->USB_status)
    {
        case USBH_GET_DEVICE_DESCRIPTOR_HEAD:
        case USBH_GET_DEVICE_DESCRIPTOR_FULL:
        {
            return USBH_ScanDeviceDescriptor(self, nowTick);
        }

        case USBH_SET_ADDRESS:
        case USBH_ADDRESS_SETTLE:
        {
            return USBH_ScanAddress(self, nowTick);
        }

        case USBH_GET_CONFIG_DESCRIPTOR_HEAD:
        case USBH_GET_CONFIG_DESCRIPTOR_FULL:
        case USBH_PARSE_CONFIGURATION:
        {
            return USBH_ScanConfiguration(self, nowTick);
        }

        case USBH_SET_CONFIGURATION:
        case USBH_SET_BOOT_PROTOCOL:
        {
            return USBH_ScanKeyboardFinalize(self, nowTick);
        }

        default:
        {
            return USBH_STEP_ERROR;
        }
    }
}


static USBH_STEP_RESULT USBH_ScanDeviceDescriptor(USBH_KB *self, uint32_t nowTick)
{
    uint8_t setup[USBH_SETUP_PACKET_SIZE];
    USBH_STEP_RESULT result;

    if((USBH_STATUS)self->USB_status == USBH_GET_DEVICE_DESCRIPTOR_HEAD)
    {
        if((USBH_CTRL_STATUS)self->CTRL_status == USBH_CTRL_IDLE)
        {
            USBH_BuildGetDescriptorRequest(
                setup,
                USBH_DESCRIPTOR_TYPE_DEVICE,
                0U,
                USBH_EP0_DEFAULT_PACKET_SIZE
            );

            USBH_ControlStart(
                self,
                setup,
                self->descriptorBuffer,
                USBH_EP0_DEFAULT_PACKET_SIZE,
                nowTick
            );
        }

        result = USBH_ControlProcess(self, nowTick);
        if(result != USBH_STEP_DONE)
        {
            return result;
        }

        if((self->controlTransferred < USBH_EP0_DEFAULT_PACKET_SIZE) ||
           (self->descriptorBuffer[0] < USBH_EP0_DEFAULT_PACKET_SIZE) ||
           (self->descriptorBuffer[1] != USBH_DESCRIPTOR_TYPE_DEVICE))
        {
            return USBH_STEP_RESTART;
        }

        self->ep0MaxPacket = self->descriptorBuffer[7];
        if((self->ep0MaxPacket != 8U) &&
           (self->ep0MaxPacket != 16U) &&
           (self->ep0MaxPacket != 32U) &&
           (self->ep0MaxPacket != 64U))
        {
            return USBH_STEP_RESTART;
        }

        self->USB_status = (uint8_t)USBH_GET_DEVICE_DESCRIPTOR_FULL;
        return USBH_STEP_DONE;
    }

    if((USBH_CTRL_STATUS)self->CTRL_status == USBH_CTRL_IDLE)
    {
        USBH_BuildGetDescriptorRequest(
            setup,
            USBH_DESCRIPTOR_TYPE_DEVICE,
            0U,
            USBH_DEVICE_DESCRIPTOR_SIZE
        );

        USBH_ControlStart(
            self,
            setup,
            self->descriptorBuffer,
            USBH_DEVICE_DESCRIPTOR_SIZE,
            nowTick
        );
    }

    result = USBH_ControlProcess(self, nowTick);
    if(result != USBH_STEP_DONE)
    {
        return result;
    }

    if((self->controlTransferred < USBH_DEVICE_DESCRIPTOR_SIZE) ||
       (self->descriptorBuffer[0] < USBH_DEVICE_DESCRIPTOR_SIZE) ||
       (self->descriptorBuffer[1] != USBH_DESCRIPTOR_TYPE_DEVICE))
    {
        return USBH_STEP_RESTART;
    }

    self->USB_status = (uint8_t)USBH_SET_ADDRESS;
    return USBH_STEP_DONE;
}


static USBH_STEP_RESULT USBH_ScanAddress(USBH_KB *self, uint32_t nowTick)
{
    uint8_t setup[USBH_SETUP_PACKET_SIZE];
    USBH_STEP_RESULT result;

    if((USBH_STATUS)self->USB_status == USBH_ADDRESS_SETTLE)
    {
        if(USBH_TimeReached(nowTick, self->deadline) == 0U)
        {
            return USBH_STEP_WAIT;
        }

        self->USB_status = (uint8_t)USBH_GET_CONFIG_DESCRIPTOR_HEAD;
        return USBH_STEP_DONE;
    }

    if((USBH_CTRL_STATUS)self->CTRL_status == USBH_CTRL_IDLE)
    {
        USBH_BuildSetAddressRequest(setup, USBH_DEFAULT_DEVICE_ADDRESS);
        USBH_ControlStart(self, setup, NULL, 0U, nowTick);
    }

    result = USBH_ControlProcess(self, nowTick);
    if(result != USBH_STEP_DONE)
    {
        return result;
    }

    self->deviceAddress = USBH_DEFAULT_DEVICE_ADDRESS;
    USBH_HW_SetAddress(self->deviceAddress);

    self->deadline = nowTick + USBH_ADDRESS_SETTLE_TICKS;
    self->USB_status = (uint8_t)USBH_ADDRESS_SETTLE;
    return USBH_STEP_DONE;
}


static USBH_STEP_RESULT USBH_ScanConfiguration(USBH_KB *self, uint32_t nowTick)
{
    uint8_t setup[USBH_SETUP_PACKET_SIZE];
    USBH_STEP_RESULT result;

    if((USBH_STATUS)self->USB_status == USBH_GET_CONFIG_DESCRIPTOR_HEAD)
    {
        if((USBH_CTRL_STATUS)self->CTRL_status == USBH_CTRL_IDLE)
        {
            USBH_BuildGetDescriptorRequest(
                setup,
                USBH_DESCRIPTOR_TYPE_CONFIG,
                0U,
                USBH_CONFIG_DESCRIPTOR_HEAD_SIZE
            );

            USBH_ControlStart(
                self,
                setup,
                self->descriptorBuffer,
                USBH_CONFIG_DESCRIPTOR_HEAD_SIZE,
                nowTick
            );
        }

        result = USBH_ControlProcess(self, nowTick);
        if(result != USBH_STEP_DONE)
        {
            return result;
        }

        if((self->controlTransferred < USBH_CONFIG_DESCRIPTOR_HEAD_SIZE) ||
           (self->descriptorBuffer[0] < USBH_CONFIG_DESCRIPTOR_HEAD_SIZE) ||
           (self->descriptorBuffer[1] != USBH_DESCRIPTOR_TYPE_CONFIG))
        {
            return USBH_STEP_RESTART;
        }

        self->configurationDescriptorLength =
            USBH_ReadU16LE(&self->descriptorBuffer[2]);

        if((self->configurationDescriptorLength < USBH_CONFIG_DESCRIPTOR_HEAD_SIZE) ||
           (self->configurationDescriptorLength > USBH_DESCRIPTOR_BUFFER_SIZE))
        {
            /* 当前简化实现不截断描述符，容量不足就明确拒绝。 */
            return USBH_STEP_ERROR;
        }

        self->USB_status = (uint8_t)USBH_GET_CONFIG_DESCRIPTOR_FULL;
        return USBH_STEP_DONE;
    }

    if((USBH_STATUS)self->USB_status == USBH_GET_CONFIG_DESCRIPTOR_FULL)
    {
        if((USBH_CTRL_STATUS)self->CTRL_status == USBH_CTRL_IDLE)
        {
            USBH_BuildGetDescriptorRequest(
                setup,
                USBH_DESCRIPTOR_TYPE_CONFIG,
                0U,
                self->configurationDescriptorLength
            );

            USBH_ControlStart(
                self,
                setup,
                self->descriptorBuffer,
                self->configurationDescriptorLength,
                nowTick
            );
        }

        result = USBH_ControlProcess(self, nowTick);
        if(result != USBH_STEP_DONE)
        {
            return result;
        }

        if(self->controlTransferred < self->configurationDescriptorLength)
        {
            return USBH_STEP_RESTART;
        }

        self->USB_status = (uint8_t)USBH_PARSE_CONFIGURATION;
        return USBH_STEP_DONE;
    }

    if(USBH_ParseKeyboardConfiguration(
           self,
           self->descriptorBuffer,
           self->configurationDescriptorLength) == 0U)
    {
        return USBH_STEP_ERROR;
    }

    self->USB_status = (uint8_t)USBH_SET_CONFIGURATION;
    return USBH_STEP_DONE;
}


static USBH_STEP_RESULT USBH_ScanKeyboardFinalize(USBH_KB *self, uint32_t nowTick)
{
    uint8_t setup[USBH_SETUP_PACKET_SIZE];
    USBH_STEP_RESULT result;

    if((USBH_STATUS)self->USB_status == USBH_SET_CONFIGURATION)
    {
        if((USBH_CTRL_STATUS)self->CTRL_status == USBH_CTRL_IDLE)
        {
            USBH_BuildSetConfigurationRequest(setup, self->configurationValue);
            USBH_ControlStart(self, setup, NULL, 0U, nowTick);
        }

        result = USBH_ControlProcess(self, nowTick);
        if(result != USBH_STEP_DONE)
        {
            return result;
        }

        self->USB_status = (uint8_t)USBH_SET_BOOT_PROTOCOL;
        return USBH_STEP_DONE;
    }

    if((USBH_CTRL_STATUS)self->CTRL_status == USBH_CTRL_IDLE)
    {
        USBH_BuildSetBootProtocolRequest(setup, self->keyboardInterface);
        USBH_ControlStart(self, setup, NULL, 0U, nowTick);
    }

    result = USBH_ControlProcess(self, nowTick);
    if(result != USBH_STEP_DONE)
    {
        return result;
    }

    self->keyboardInToggle = 0U;
    memset(self->previousReport, 0, sizeof(self->previousReport));

    self->deadline = nowTick + USBH_KEYBOARD_POLL_TICKS;
    self->USB_status = (uint8_t)USBH_KEYBOARD_READY;
    usbHostKeyboardAvailable = 1U;
    return USBH_STEP_DONE;
}


/* -------------------------------------------------------------------------- */
/*                           EP0 控制传输状态机                                */
/* -------------------------------------------------------------------------- */

static void USBH_ControlStart(
    USBH_KB *self,
    const uint8_t setupPacket[USBH_SETUP_PACKET_SIZE],
    uint8_t *dataBuffer,
    uint16_t dataLength,
    uint32_t nowTick)
{
    memcpy(self->txBuffer.bytes, setupPacket, USBH_SETUP_PACKET_SIZE);

    self->controlBuffer = dataBuffer;
    self->controlRemaining = dataLength;
    self->controlTransferred = 0U;
    self->controlDirection = setupPacket[0] & USBH_REQUEST_DIRECTION_IN;
    self->controlToggle = 1U;
    self->controlDeadline = nowTick + USBH_CONTROL_TIMEOUT_TICKS;

    self->CTRL_status = (uint8_t)USBH_CTRL_SETUP;
}


static USBH_STEP_RESULT USBH_ControlProcess(USBH_KB *self, uint32_t nowTick)
{
    USBH_IO_RESULT ioResult;
    USBH_TRANSACTION transaction;
    uint16_t packetLength;

    if((USBH_CTRL_STATUS)self->CTRL_status == USBH_CTRL_IDLE)
    {
        return USBH_STEP_ERROR;
    }

    if(USBH_TimeReached(nowTick, self->controlDeadline) != 0U)
    {
        return USBH_STEP_RESTART;
    }

    switch((USBH_CTRL_STATUS)self->CTRL_status)
    {
        case USBH_CTRL_SETUP:
        {
            transaction = (USBH_TRANSACTION)
            {
                .endpoint = 0U,
                .tokenType = USBH_TOKEN_SETUP,
                .toggle = 0U,
                .length = USBH_SETUP_PACKET_SIZE
            };

            ioResult = USBH_HW_Transact(self, transaction);
            if(ioResult == USBH_IO_BUSY)
            {
                return USBH_STEP_WAIT;
            }
            if(ioResult == USBH_IO_NAK)
            {
                return USBH_STEP_WAIT;
            }
            if(ioResult == USBH_IO_DISCONNECT)
            {
                return USBH_STEP_DISCONNECT;
            }
            if(ioResult != USBH_IO_SUCCESS)
            {
                return USBH_STEP_RESTART;
            }

            if(self->controlRemaining == 0U)
            {
                self->CTRL_status =
                    (self->controlDirection != 0U) ?
                    (uint8_t)USBH_CTRL_STATUS_OUT :
                    (uint8_t)USBH_CTRL_STATUS_IN;
            }
            else if(self->controlDirection != 0U)
            {
                self->CTRL_status = (uint8_t)USBH_CTRL_DATA_IN;
            }
            else
            {
                self->CTRL_status = (uint8_t)USBH_CTRL_DATA_OUT;
            }

            return USBH_STEP_WAIT;
        }

        case USBH_CTRL_DATA_IN:
        {
            transaction = (USBH_TRANSACTION)
            {
                .endpoint = 0U,
                .tokenType = USBH_TOKEN_IN,
                .toggle = self->controlToggle,
                .length = 0U
            };

            ioResult = USBH_HW_Transact(self, transaction);
            if(ioResult == USBH_IO_BUSY)
            {
                return USBH_STEP_WAIT;
            }
            if(ioResult == USBH_IO_NAK)
            {
                return USBH_STEP_WAIT;
            }
            if(ioResult == USBH_IO_DISCONNECT)
            {
                return USBH_STEP_DISCONNECT;
            }
            if(ioResult != USBH_IO_SUCCESS)
            {
                return USBH_STEP_RESTART;
            }

            packetLength = self->rxLength;
            if(packetLength > self->controlRemaining)
            {
                packetLength = self->controlRemaining;
            }

            if(packetLength != 0U)
            {
                memcpy(
                    &self->controlBuffer[self->controlTransferred],
                    self->rxBuffer.bytes,
                    packetLength
                );
            }

            self->controlTransferred += packetLength;
            self->controlRemaining -= packetLength;

            /* 只有成功 DATA 包才翻转 toggle。 */
            self->controlToggle ^= 1U;

            /*
             * 短包或请求长度已经收满，都表示 DATA IN 阶段结束。
             * 下一阶段由主机发送零长度 DATA1 OUT Status。
             */
            if((self->rxLength < self->ep0MaxPacket) ||
               (self->controlRemaining == 0U))
            {
                self->CTRL_status = (uint8_t)USBH_CTRL_STATUS_OUT;
            }

            return USBH_STEP_WAIT;
        }

        case USBH_CTRL_DATA_OUT:
        {
            packetLength = self->controlRemaining;
            if(packetLength > self->ep0MaxPacket)
            {
                packetLength = self->ep0MaxPacket;
            }

            /*
             * 只有当前没有挂起 Token 时才装载下一包，避免 WAIT 阶段反复覆盖 DMA 窗口。
             */
            if((USBH_HW_STATUS)self->HW_status == USBH_HW_IDLE)
            {
                if(packetLength != 0U)
                {
                    memcpy(
                        self->txBuffer.bytes,
                        &self->controlBuffer[self->controlTransferred],
                        packetLength
                    );
                }
            }

            transaction = (USBH_TRANSACTION)
            {
                .endpoint = 0U,
                .tokenType = USBH_TOKEN_OUT,
                .toggle = self->controlToggle,
                .length = (uint8_t)packetLength
            };

            ioResult = USBH_HW_Transact(self, transaction);
            if(ioResult == USBH_IO_BUSY)
            {
                return USBH_STEP_WAIT;
            }
            if(ioResult == USBH_IO_NAK)
            {
                return USBH_STEP_WAIT;
            }
            if(ioResult == USBH_IO_DISCONNECT)
            {
                return USBH_STEP_DISCONNECT;
            }
            if(ioResult != USBH_IO_SUCCESS)
            {
                return USBH_STEP_RESTART;
            }

            self->controlTransferred += packetLength;
            self->controlRemaining -= packetLength;
            self->controlToggle ^= 1U;

            if(self->controlRemaining == 0U)
            {
                self->CTRL_status = (uint8_t)USBH_CTRL_STATUS_IN;
            }

            return USBH_STEP_WAIT;
        }

        case USBH_CTRL_STATUS_OUT:
        {
            transaction = (USBH_TRANSACTION)
            {
                .endpoint = 0U,
                .tokenType = USBH_TOKEN_OUT,
                .toggle = 1U,
                .length = 0U
            };

            ioResult = USBH_HW_Transact(self, transaction);
            break;
        }

        case USBH_CTRL_STATUS_IN:
        {
            transaction = (USBH_TRANSACTION)
            {
                .endpoint = 0U,
                .tokenType = USBH_TOKEN_IN,
                .toggle = 1U,
                .length = 0U
            };

            ioResult = USBH_HW_Transact(self, transaction);
            break;
        }

        default:
        {
            return USBH_STEP_ERROR;
        }
    }

    /* STATUS IN / OUT 共用这一段收尾。 */
    if(ioResult == USBH_IO_BUSY)
    {
        return USBH_STEP_WAIT;
    }
    if(ioResult == USBH_IO_NAK)
    {
        return USBH_STEP_WAIT;
    }
    if(ioResult == USBH_IO_DISCONNECT)
    {
        return USBH_STEP_DISCONNECT;
    }
    if(ioResult != USBH_IO_SUCCESS)
    {
        return USBH_STEP_RESTART;
    }

    if(((USBH_CTRL_STATUS)self->CTRL_status == USBH_CTRL_STATUS_IN) &&
       (self->rxLength != 0U))
    {
        return USBH_STEP_RESTART;
    }

    self->CTRL_status = (uint8_t)USBH_CTRL_IDLE;
    self->controlBuffer = NULL;
    self->controlRemaining = 0U;
    return USBH_STEP_DONE;
}


/* -------------------------------------------------------------------------- */
/*                           配置描述符与键盘轮询                              */
/* -------------------------------------------------------------------------- */

static uint8_t USBH_ParseKeyboardConfiguration(
    USBH_KB *self,
    const uint8_t *descriptor,
    uint16_t length)
{
    uint16_t offset = 0U;
    uint8_t inKeyboardInterface = 0U;
    uint8_t foundConfiguration = 0U;
    uint8_t foundEndpoint = 0U;

    while((offset + 2U) <= length)
    {
        uint8_t descriptorLength = descriptor[offset];
        uint8_t descriptorType = descriptor[offset + 1U];

        if((descriptorLength < 2U) ||
           ((uint32_t)offset + descriptorLength > length))
        {
            return 0U;
        }

        switch(descriptorType)
        {
            case USBH_DESCRIPTOR_TYPE_CONFIG:
            {
                if(descriptorLength < USBH_CONFIG_DESCRIPTOR_HEAD_SIZE)
                {
                    return 0U;
                }

                self->configurationValue = descriptor[offset + 5U];
                foundConfiguration = 1U;
                inKeyboardInterface = 0U;
                break;
            }

            case USBH_DESCRIPTOR_TYPE_INTERFACE:
            {
                if(descriptorLength < 9U)
                {
                    return 0U;
                }

                inKeyboardInterface =
                    (descriptor[offset + 5U] == USBH_CLASS_HID) &&
                    (descriptor[offset + 6U] == USBH_HID_SUBCLASS_BOOT) &&
                    (descriptor[offset + 7U] == USBH_HID_PROTOCOL_KEYBOARD);

                if(inKeyboardInterface != 0U)
                {
                    self->keyboardInterface = descriptor[offset + 2U];
                }
                break;
            }

            case USBH_DESCRIPTOR_TYPE_ENDPOINT:
            {
                uint8_t endpointAddress;
                uint8_t endpointAttributes;
                uint16_t endpointMaxPacket;

                if((inKeyboardInterface == 0U) || (descriptorLength < 7U))
                {
                    break;
                }

                endpointAddress = descriptor[offset + 2U];
                endpointAttributes = descriptor[offset + 3U] & 0x03U;
                endpointMaxPacket = USBH_ReadU16LE(&descriptor[offset + 4U]);

                if(((endpointAddress & USBH_ENDPOINT_DIRECTION_IN) != 0U) &&
                   (endpointAttributes == USBH_ENDPOINT_TRANSFER_INTERRUPT) &&
                   (endpointMaxPacket >= USBH_BOOT_REPORT_SIZE) &&
                   (endpointMaxPacket <= USBH_PACKET_BUFFER_SIZE))
                {
                    self->keyboardInEndpoint = endpointAddress & 0x0FU;
                    foundEndpoint = 1U;
                }
                break;
            }

            default:
            {
                break;
            }
        }

        offset += descriptorLength;
    }

    return (foundConfiguration != 0U) && (foundEndpoint != 0U);
}


static USBH_STEP_RESULT USBH_GetReport(USBH_KB *self, uint32_t nowTick)
{
    USBH_IO_RESULT ioResult;
    USBH_TRANSACTION transaction;

    if((USBH_STATUS)self->USB_status == USBH_KEYBOARD_READY)
    {
        if(USBH_TimeReached(nowTick, self->deadline) == 0U)
        {
            return USBH_STEP_WAIT;
        }

        /* 本时间片只切换状态，下一次调用再真正踢 IN Token。 */
        self->USB_status = (uint8_t)USBH_KEYBOARD_POLL;
        return USBH_STEP_DONE;
    }

    transaction = (USBH_TRANSACTION)
    {
        .endpoint = self->keyboardInEndpoint,
        .tokenType = USBH_TOKEN_IN,
        .toggle = self->keyboardInToggle,
        .length = 0U
    };

    ioResult = USBH_HW_Transact(self, transaction);

    if(ioResult == USBH_IO_BUSY)
    {
        return USBH_STEP_WAIT;
    }

    if(ioResult == USBH_IO_SUCCESS)
    {
        if(self->rxLength < USBH_BOOT_REPORT_SIZE)
        {
            return USBH_STEP_RESTART;
        }

        self->keyboardInToggle ^= 1U;
        USBH_ProcessKeyboardReport(self, self->rxBuffer.bytes, self->rxLength);

        self->deadline = nowTick + USBH_KEYBOARD_POLL_TICKS;
        self->USB_status = (uint8_t)USBH_KEYBOARD_READY;
        return USBH_STEP_DONE;
    }

    if(ioResult == USBH_IO_NAK)
    {
        self->deadline = nowTick + USBH_KEYBOARD_POLL_TICKS;
        self->USB_status = (uint8_t)USBH_KEYBOARD_READY;
        return USBH_STEP_DONE;
    }

    if(ioResult == USBH_IO_DISCONNECT)
    {
        return USBH_STEP_DISCONNECT;
    }

    /* STALL、超时和其他协议错误统一暴力复位并重新枚举。 */
    return USBH_STEP_RESTART;
}



static void USBH_ProcessKeyboardReport(
    USBH_KB *self,
    const uint8_t *report,
    uint8_t length)
{
    uint8_t currentIndex;
    uint8_t previousIndex;

    if(length < USBH_BOOT_REPORT_SIZE)
    {
        return;
    }

    /*
     * byte 2..7 是当前仍处于按下状态的最多六个普通按键。
     * 只对 previousReport 中不存在的新 Usage ID 触发 KeyDown。
     */
    for(currentIndex = 2U; currentIndex < USBH_BOOT_REPORT_SIZE; currentIndex++)
    {
        uint8_t usageId = report[currentIndex];
        uint8_t alreadyPressed = 0U;

        /* 0 为空槽；1..3 是 HID 错误/rollover 码，不作为正常按键输出。 */
        if(usageId <= 3U)
        {
            continue;
        }

        for(previousIndex = 2U;
            previousIndex < USBH_BOOT_REPORT_SIZE;
            previousIndex++)
        {
            if(self->previousReport[previousIndex] == usageId)
            {
                alreadyPressed = 1U;
                break;
            }
        }

        if(alreadyPressed == 0U)
        {
            USBH_OnKeyDown(
                self,
                report[0],
                usageId
            );
        }
    }

    memcpy(self->previousReport, report, USBH_BOOT_REPORT_SIZE);
}


/* -------------------------------------------------------------------------- */
/*                             错误与状态清理                                  */
/* -------------------------------------------------------------------------- */

static void USBH_ApplyStepResult(
    USBH_KB *self,
    USBH_STEP_RESULT result,
    uint32_t nowTick)
{
    switch(result)
    {
        case USBH_STEP_WAIT:
        case USBH_STEP_DONE:
        {
            break;
        }

        case USBH_STEP_DISCONNECT:
        {

            USBH_ClearDeviceState(self);
            self->USB_status = (uint8_t)USBH_DISCONNECTED;
            break;
        }

        case USBH_STEP_RESTART:
        {
            USBH_RestartEnumeration(self, nowTick);
            break;
        }

        case USBH_STEP_ERROR:
        default:
        {
            USBH_ClearDeviceState(self);
            self->deadline = nowTick + USBH_ERROR_BACKOFF_TICKS;
            self->USB_status = (uint8_t)USBH_ERROR_BACKOFF;
            break;
        }
    }
}


static void USBH_ClearDeviceState(USBH_KB *self)
{
    usbHostKeyboardAvailable = 0U;
    USBH_HW_Disable(self);

    self->CTRL_status = (uint8_t)USBH_CTRL_IDLE;
    self->HW_status = (uint8_t)USBH_HW_IDLE;

    self->deviceAddress = 0U;
    self->deviceSpeed = (uint8_t)USBH_SPEED_FULL;
    self->ep0MaxPacket = USBH_EP0_DEFAULT_PACKET_SIZE;

    self->configurationValue = 0U;
    self->keyboardInterface = 0U;
    self->keyboardInEndpoint = 0U;
    self->keyboardInToggle = 0U;
    self->rxLength = 0U;

    memset(self->previousReport, 0, sizeof(self->previousReport));
    memset(&self->currentTransaction, 0, sizeof(self->currentTransaction));

    self->controlBuffer = NULL;
    self->controlRemaining = 0U;
    self->controlTransferred = 0U;
    self->controlDirection = 0U;
    self->controlToggle = 0U;
    self->configurationDescriptorLength = 0U;
}


static void USBH_RestartEnumeration(USBH_KB *self, uint32_t nowTick)
{
    USBH_ClearDeviceState(self);

    if(USBH_HW_IsAttached() != 0U)
    {
        self->deadline = nowTick;
        self->USB_status = (uint8_t)USBH_RESET_BEGIN;
    }
    else
    {
        self->USB_status = (uint8_t)USBH_DISCONNECTED;
    }
}


/* -------------------------------------------------------------------------- */
/*                             通用小工具                                      */
/* -------------------------------------------------------------------------- */

static uint8_t USBH_TimeReached(uint32_t nowTick, uint32_t deadlineTick)
{
    return ((int32_t)(nowTick - deadlineTick) >= 0) ? 1U : 0U;
}


static uint16_t USBH_ReadU16LE(const uint8_t *data)
{
    return (uint16_t)data[0] |
           ((uint16_t)data[1] << 8U);
}


static void USBH_WriteU16LE(uint8_t *data, uint16_t value)
{
    data[0] = (uint8_t)(value & 0xFFU);
    data[1] = (uint8_t)(value >> 8U);
}


/* -------------------------------------------------------------------------- */
/*                             Setup Packet 构造                               */
/* -------------------------------------------------------------------------- */

static void USBH_BuildGetDescriptorRequest(
    uint8_t setup[USBH_SETUP_PACKET_SIZE],
    uint8_t descriptorType,
    uint8_t descriptorIndex,
    uint16_t length)
{
    setup[0] = USBH_REQUEST_DIRECTION_IN |
               USBH_REQUEST_TYPE_STANDARD |
               USBH_REQUEST_RECIPIENT_DEVICE;
    setup[1] = USBH_REQUEST_GET_DESCRIPTOR;
    setup[2] = descriptorIndex;
    setup[3] = descriptorType;
    setup[4] = 0U;
    setup[5] = 0U;
    USBH_WriteU16LE(&setup[6], length);
}


static void USBH_BuildSetAddressRequest(
    uint8_t setup[USBH_SETUP_PACKET_SIZE],
    uint8_t address)
{
    setup[0] = USBH_REQUEST_DIRECTION_OUT |
               USBH_REQUEST_TYPE_STANDARD |
               USBH_REQUEST_RECIPIENT_DEVICE;
    setup[1] = USBH_REQUEST_SET_ADDRESS;
    USBH_WriteU16LE(&setup[2], address);
    USBH_WriteU16LE(&setup[4], 0U);
    USBH_WriteU16LE(&setup[6], 0U);
}


static void USBH_BuildSetConfigurationRequest(
    uint8_t setup[USBH_SETUP_PACKET_SIZE],
    uint8_t configurationValue)
{
    setup[0] = USBH_REQUEST_DIRECTION_OUT |
               USBH_REQUEST_TYPE_STANDARD |
               USBH_REQUEST_RECIPIENT_DEVICE;
    setup[1] = USBH_REQUEST_SET_CONFIGURATION;
    USBH_WriteU16LE(&setup[2], configurationValue);
    USBH_WriteU16LE(&setup[4], 0U);
    USBH_WriteU16LE(&setup[6], 0U);
}


static void USBH_BuildSetBootProtocolRequest(
    uint8_t setup[USBH_SETUP_PACKET_SIZE],
    uint8_t interfaceNumber)
{
    setup[0] = USBH_REQUEST_DIRECTION_OUT |
               USBH_REQUEST_TYPE_CLASS |
               USBH_REQUEST_RECIPIENT_INTERFACE;
    setup[1] = USBH_HID_REQUEST_SET_PROTOCOL;

    /* wValue == 0 选择 Boot Protocol；1 为 Report Protocol。 */
    USBH_WriteU16LE(&setup[2], 0U);
    USBH_WriteU16LE(&setup[4], interfaceNumber);
    USBH_WriteU16LE(&setup[6], 0U);
}


USBH_KB usbKeyboard =
{
    .USB_status  = USBH_POWER_ON,
    .deviceSpeed = USBH_SPEED_FULL,
    .ep0MaxPacket = USBH_EP0_DEFAULT_PACKET_SIZE
};


