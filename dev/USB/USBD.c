/*
 * CH32V20x USBD CDC-ECM 设备协议入口。
 *
 * PA11/PA12 使用片上 USBD 外设。
 * 数据通过 PMA，而不是 USBFS DMA 缓冲区传输。
 */

#include "USBD.h"
#include "USBD_Internal.h"

#include <stddef.h>
#include <stdint.h>

#include "ch32v20x.h"
#include "ch32v20x_usb.h"
#include "USB_DESC.h"
#include "system_IT.h"
#include "usb_regs.h"


#define USB_DEVICE_CONFIGURATION        1U

#ifndef USB_CDC_SET_ETHERNET_PACKET_FILTER
#define USB_CDC_SET_ETHERNET_PACKET_FILTER  0x43U
#endif


_Static_assert(
    sizeof(USB_SETUP_REQ) == 8U,
    "USB SETUP packet must be 8 bytes");

_Static_assert(
    (USB_PMA_EP3_TX_ADDRESS +
     USB_FULL_SPEED_PACKET_SIZE) <= 0x0200U,
    "USB PMA allocation exceeds 512 bytes");


typedef enum
{
    USB_EP0_STAGE_IDLE = 0,
    USB_EP0_STAGE_DATA_IN,
    USB_EP0_STAGE_STATUS_OUT,
    USB_EP0_STAGE_STATUS_IN
} USBEndpoint0Stage;


typedef struct
{
    USBEndpoint0Stage stage;

    /*
     * 下一段待发送数据。
     */
    const uint8_t *inData;
    uint16_t inRemaining;

    /*
     * DATA IN 末尾是否需要额外 ZLP。
     */
    uint8_t needsZLP;

    /*
     * SET_ADDRESS 必须等 Status IN 完成后生效。
     */
    uint8_t pendingAddress;
    uint8_t pendingAddressValid;

    /*
     * 小型控制请求的返回值。
     *
     * 该缓冲区必须跨越后续 IN transaction 存活。
     */
    uint8_t reply[2];
} USBEndpoint0Context;


typedef struct
{
    USBEndpoint0Context endpoint0;

    /*
     * 由 USB 中断写入，由主循环状态机查询。
     */
    volatile uint8_t configurationValue;
    volatile uint8_t dataAlternateSetting;

    uint16_t ethernetPacketFilter;
} USBDeviceContext;


static USBDeviceContext usbDeviceContext;


/*
 * PMA 在 CPU 地址空间中不是连续的 uint8_t 数组。
 *
 * 每个 16 位 PMA 数据字占一个 32 位 CPU 地址槽。
 * 因此不能把 PMA 地址直接强转成 USB_SETUP_REQ 指针。
 */
void USB_PMA_Write(
    uint16_t pmaAddress,
    const void *source,
    uint16_t length)
{
    const uint8_t *input =
        (const uint8_t *)source;

    volatile uint16_t *output =
        (volatile uint16_t *)(uintptr_t)(
            PMAAddr +
            ((uint32_t)pmaAddress << 1U));

    while (length >= 2U)
    {
        *output =
            (uint16_t)input[0] |
            ((uint16_t)input[1] << 8U);

        /*
         * 下一个 PMA 半字位于下一个 32 位地址槽。
         */
        output += 2U;
        input += 2U;
        length -= 2U;
    }

    if (length != 0U)
    {
        *output = input[0];
    }
}


void USB_PMA_Read(
    void *destination,
    uint16_t pmaAddress,
    uint16_t length)
{
    uint8_t *output =
        (uint8_t *)destination;

    volatile const uint32_t *input =
        (volatile const uint32_t *)(uintptr_t)(
            PMAAddr +
            ((uint32_t)pmaAddress << 1U));

    while (length >= 2U)
    {
        const uint16_t value =
            (uint16_t)*input;

        output[0] =
            (uint8_t)value;

        output[1] =
            (uint8_t)(value >> 8U);

        input++;
        output += 2U;
        length -= 2U;
    }

    if (length != 0U)
    {
        *output =
            (uint8_t)*input;
    }
}


/*
 * 配置 PMA RX buffer 的块数。
 *
 * 这里不调用 WCH 原版 _SetEPRxCount()。
 * 原版宏依赖 Device_Property 和 Ep0RxBlks。
 */
void USB_SetEndpointRxCapacity(
    uint8_t endpointNumber,
    uint16_t capacity)
{
    volatile uint32_t *const countRegister =
        (volatile uint32_t *)(uintptr_t)(
            PMAAddr +
            ((uint32_t)(
                _GetBTABLE() +
                ((uint16_t)endpointNumber * 8U) +
                6U) << 1U));

    uint16_t registerValue;

    if (capacity > 62U)
    {
        uint16_t blockCount =
            capacity >> 5U;

        if ((capacity & 0x1FU) == 0U)
        {
            blockCount--;
        }

        registerValue =
            (uint16_t)(
                0x8000U |
                (blockCount << 10U));
    }
    else
    {
        uint16_t blockCount =
            capacity >> 1U;

        if ((capacity & 1U) != 0U)
        {
            blockCount++;
        }

        registerValue =
            (uint16_t)(blockCount << 10U);
    }

    *countRegister = registerValue;
}


static void USB_SetDeviceAddress(
    uint8_t address)
{
    _SetDADDR(
        DADDR_EF |
        (address & DADDR_ADD));
}


static void USB_Endpoint0Stall(void)
{
    _SetEPTxCount(
        USB_EP0_NUMBER,
        0U);

    _SetEPTxStatus(
        USB_EP0_NUMBER,
        EP_TX_STALL);

    _SetEPRxStatus(
        USB_EP0_NUMBER,
        EP_RX_STALL);
}


static void USB_Endpoint0RearmOut(void)
{
    USB_SetEndpointRxCapacity(
        USB_EP0_NUMBER,
        BSP_USB_EP0_PACKET_SIZE);

    _SetEPRxStatus(
        USB_EP0_NUMBER,
        EP_RX_VALID);
}


static void USB_DisableDataEndpoints(void)
{
    _SetEPRxStatus(
        USB_ETHERNET_OUT_ENDPOINT,
        EP_RX_NAK);

    _SetEPTxCount(
        USB_ETHERNET_IN_ENDPOINT,
        0U);

    _SetEPTxStatus(
        USB_ETHERNET_IN_ENDPOINT,
        EP_TX_NAK);

    _ClearDTOG_RX(
        USB_ETHERNET_OUT_ENDPOINT);

    _ClearDTOG_TX(
        USB_ETHERNET_IN_ENDPOINT);

    USBD_HookResetEthernetTransferState();
}


static void USB_EnableDataEndpoints(void)
{
    USBD_HookResetEthernetTransferState();

    /*
     * SET_INTERFACE 后 Bulk 端点从 DATA0 开始。
     */
    _ClearDTOG_RX(
        USB_ETHERNET_OUT_ENDPOINT);

    _ClearDTOG_TX(
        USB_ETHERNET_IN_ENDPOINT);

    USB_SetEndpointRxCapacity(
        USB_ETHERNET_OUT_ENDPOINT,
        USB_FULL_SPEED_PACKET_SIZE);

    _SetEPRxStatus(
        USB_ETHERNET_OUT_ENDPOINT,
        EP_RX_VALID);

    _SetEPTxCount(
        USB_ETHERNET_IN_ENDPOINT,
        0U);

    _SetEPTxStatus(
        USB_ETHERNET_IN_ENDPOINT,
        EP_TX_NAK);
}


static void USB_ResetEndpoints(void)
{
    USBD_HookResetEthernetTransferState();

    _SetBTABLE(
        USB_PMA_BTABLE_ADDRESS);

    /*
     * Bus Reset 后端点寄存器均为复位值。
     * 先清掉可能残留的 CTR 标志。
     */
    for (uint8_t endpointNumber = 0U;
         endpointNumber < USB_ENDPOINT_COUNT;
         endpointNumber++)
    {
        _SetENDPOINT(
            endpointNumber,
            0U);
    }

    /*
     * EP0：Control OUT/IN。
     */
    _SetENDPOINT(
        USB_EP0_NUMBER,
        EP_CONTROL |
        USB_EP0_NUMBER);

    _SetEPRxAddr(
        USB_EP0_NUMBER,
        USB_PMA_EP0_RX_ADDRESS);

    _SetEPTxAddr(
        USB_EP0_NUMBER,
        USB_PMA_EP0_TX_ADDRESS);

    USB_SetEndpointRxCapacity(
        USB_EP0_NUMBER,
        BSP_USB_EP0_PACKET_SIZE);

    _SetEPTxCount(
        USB_EP0_NUMBER,
        0U);

    _ClearDTOG_RX(
        USB_EP0_NUMBER);

    _ClearDTOG_TX(
        USB_EP0_NUMBER);

    _SetEPTxStatus(
        USB_EP0_NUMBER,
        EP_TX_NAK);

    _SetEPRxStatus(
        USB_EP0_NUMBER,
        EP_RX_VALID);

    /*
     * EP1：CDC-ECM Interrupt IN。
     */
    _SetENDPOINT(
        USB_NOTIFICATION_ENDPOINT,
        EP_INTERRUPT |
        USB_NOTIFICATION_ENDPOINT);

    _SetEPTxAddr(
        USB_NOTIFICATION_ENDPOINT,
        USB_PMA_EP1_TX_ADDRESS);

    _SetEPTxCount(
        USB_NOTIFICATION_ENDPOINT,
        0U);

    _ClearDTOG_TX(
        USB_NOTIFICATION_ENDPOINT);

    _SetEPRxStatus(
        USB_NOTIFICATION_ENDPOINT,
        EP_RX_DIS);

    _SetEPTxStatus(
        USB_NOTIFICATION_ENDPOINT,
        EP_TX_NAK);

    /*
     * EP2：CDC-ECM Bulk OUT。
     */
    _SetENDPOINT(
        USB_ETHERNET_OUT_ENDPOINT,
        EP_BULK |
        USB_ETHERNET_OUT_ENDPOINT);

    _SetEPRxAddr(
        USB_ETHERNET_OUT_ENDPOINT,
        USB_PMA_EP2_RX_ADDRESS);

    USB_SetEndpointRxCapacity(
        USB_ETHERNET_OUT_ENDPOINT,
        USB_FULL_SPEED_PACKET_SIZE);

    _ClearDTOG_RX(
        USB_ETHERNET_OUT_ENDPOINT);

    _SetEPTxStatus(
        USB_ETHERNET_OUT_ENDPOINT,
        EP_TX_DIS);

    _SetEPRxStatus(
        USB_ETHERNET_OUT_ENDPOINT,
        EP_RX_NAK);

    /*
     * EP3：CDC-ECM Bulk IN。
     */
    _SetENDPOINT(
        USB_ETHERNET_IN_ENDPOINT,
        EP_BULK |
        USB_ETHERNET_IN_ENDPOINT);

    _SetEPTxAddr(
        USB_ETHERNET_IN_ENDPOINT,
        USB_PMA_EP3_TX_ADDRESS);

    _SetEPTxCount(
        USB_ETHERNET_IN_ENDPOINT,
        0U);

    _ClearDTOG_TX(
        USB_ETHERNET_IN_ENDPOINT);

    _SetEPRxStatus(
        USB_ETHERNET_IN_ENDPOINT,
        EP_RX_DIS);

    _SetEPTxStatus(
        USB_ETHERNET_IN_ENDPOINT,
        EP_TX_NAK);

    /*
     * 地址 0，设备功能开启。
     */
    USB_SetDeviceAddress(0U);
}

static void USB_HandleBusReset(void)
{
    usbDeviceContext =
        (USBDeviceContext){0};

    USB_ResetEndpoints();

    /*
     * 端点初始化完成后，允许 CTR_RX/CTR_TX
     * 通过 USB IRQ 上报。
     */
    _SetCNTR(
        (uint16_t)(
            _GetCNTR() |
            CNTR_CTRM));
}
static void USB_Endpoint0StartDataIn(
    const uint8_t *data,
    uint16_t dataLength,
    uint16_t requestedLength)
{
    uint16_t transferLength =
        dataLength;

    if (transferLength > requestedLength)
    {
        transferLength =
            requestedLength;
    }

    usbDeviceContext.endpoint0.stage =
        USB_EP0_STAGE_DATA_IN;

    usbDeviceContext.endpoint0.inData =
        data;

    usbDeviceContext.endpoint0.inRemaining =
        transferLength;

    usbDeviceContext.endpoint0.needsZLP =
        (
            (transferLength != 0U) &&
            (transferLength < requestedLength) &&
            ((transferLength %
              BSP_USB_EP0_PACKET_SIZE) == 0U)
        );

    uint16_t packetLength =
        transferLength;

    if (packetLength >
        BSP_USB_EP0_PACKET_SIZE)
    {
        packetLength =
            BSP_USB_EP0_PACKET_SIZE;
    }

    if (packetLength != 0U)
    {
        USB_PMA_Write(
            USB_PMA_EP0_TX_ADDRESS,
            data,
            packetLength);

        usbDeviceContext.endpoint0.inData +=
            packetLength;

        usbDeviceContext.endpoint0.inRemaining -=
            packetLength;
    }

    _SetEPRxStatus(
        USB_EP0_NUMBER,
        EP_RX_NAK);

    _SetEPTxCount(
        USB_EP0_NUMBER,
        packetLength);

    _SetEPTxStatus(
        USB_EP0_NUMBER,
        EP_TX_VALID);
}


static void USB_Endpoint0ArmStatusIn(void)
{
    usbDeviceContext.endpoint0.stage =
        USB_EP0_STAGE_STATUS_IN;

    _SetEPRxStatus(
        USB_EP0_NUMBER,
        EP_RX_NAK);

    _SetEPTxCount(
        USB_EP0_NUMBER,
        0U);

    _SetEPTxStatus(
        USB_EP0_NUMBER,
        EP_TX_VALID);
}


static void USB_HandleGetDescriptor(
    const USB_SETUP_REQ *setup)
{
    if ((setup->bRequestType &
         USB_REQ_TYP_READ) == 0U)
    {
        USB_Endpoint0Stall();
        return;
    }

    const uint8_t descriptorType =
        (uint8_t)(setup->wValue >> 8U);

    const uint8_t descriptorIndex =
        (uint8_t)setup->wValue;

    uint16_t descriptorLength = 0U;

    const uint8_t *const descriptor =
        USBDesc_GetDescriptor(
            descriptorType,
            descriptorIndex,
            &descriptorLength);

    if (descriptor == NULL)
    {
        USB_Endpoint0Stall();
        return;
    }

    USB_Endpoint0StartDataIn(
        descriptor,
        descriptorLength,
        setup->wLength);
}


static void USB_HandleGetStatus(
    const USB_SETUP_REQ *setup)
{
    if (((setup->bRequestType &
          USB_REQ_TYP_READ) == 0U) ||
        (setup->wValue != 0U) ||
        (setup->wLength != 2U))
    {
        USB_Endpoint0Stall();
        return;
    }

    usbDeviceContext.endpoint0.reply[0] = 0U;
    usbDeviceContext.endpoint0.reply[1] = 0U;

    switch (setup->bRequestType &
            USB_REQ_RECIP_MASK)
    {
    case USB_REQ_RECIP_DEVICE:
        {
            if (setup->wIndex != 0U)
            {
                USB_Endpoint0Stall();
                return;
            }

            break;
        }

    case USB_REQ_RECIP_INTERF:
        {
            const uint8_t interfaceNumber =
                (uint8_t)setup->wIndex;

            if (((setup->wIndex & 0xFF00U) != 0U) ||
                (interfaceNumber >= USB_INTERFACE_COUNT))
            {
                USB_Endpoint0Stall();
                return;
            }

            break;
        }

    case USB_REQ_RECIP_ENDP:
        {
            const uint8_t endpointAddress =
                (uint8_t)setup->wIndex;

            const uint8_t endpointNumber =
                endpointAddress & 0x0FU;

            if (((setup->wIndex & 0xFF00U) != 0U) ||
                (endpointNumber >
                 USB_ETHERNET_IN_ENDPOINT))
            {
                USB_Endpoint0Stall();
                return;
            }

            if ((endpointAddress & 0x80U) != 0U)
            {
                usbDeviceContext.endpoint0.reply[0] =
                    (uint8_t)_GetTxStallStatus(
                        endpointNumber);
            }
            else
            {
                usbDeviceContext.endpoint0.reply[0] =
                    (uint8_t)_GetRxStallStatus(
                        endpointNumber);
            }

            break;
        }

    default:
        USB_Endpoint0Stall();
        return;
    }

    USB_Endpoint0StartDataIn(
        usbDeviceContext.endpoint0.reply,
        2U,
        setup->wLength);
}


static void USB_HandleGetConfiguration(
    const USB_SETUP_REQ *setup)
{
    if ((setup->bRequestType != 0x80U) ||
        (setup->wValue != 0U) ||
        (setup->wIndex != 0U) ||
        (setup->wLength != 1U))
    {
        USB_Endpoint0Stall();
        return;
    }

    usbDeviceContext.endpoint0.reply[0] =
        usbDeviceContext.configurationValue;

    USB_Endpoint0StartDataIn(
        usbDeviceContext.endpoint0.reply,
        1U,
        setup->wLength);
}


static void USB_HandleSetConfiguration(
    const USB_SETUP_REQ *setup)
{
    /*
     * SET_CONFIGURATION 的合法格式：
     *
     * bmRequestType = 0x00
     * wValue        = 0 或 1
     * wIndex        = 0
     * wLength       = 0
     */
    if ((setup->bRequestType != 0x00U) ||
        (setup->wIndex != 0U) ||
        (setup->wLength != 0U) ||
        ((setup->wValue != 0U) &&
         (setup->wValue !=
          USB_DEVICE_CONFIGURATION)))
    {
        USB_Endpoint0Stall();
        return;
    }

    /*
     * 每次重新选择配置，都从 Data Interface Alt 0 开始。
     */
    usbDeviceContext.configurationValue =
        (uint8_t)setup->wValue;

    usbDeviceContext.dataAlternateSetting = 0U;
    usbDeviceContext.ethernetPacketFilter = 0U;

    _ClearDTOG_TX(
        USB_NOTIFICATION_ENDPOINT);

    _SetEPTxCount(
        USB_NOTIFICATION_ENDPOINT,
        0U);

    _SetEPTxStatus(
        USB_NOTIFICATION_ENDPOINT,
        EP_TX_NAK);

    USB_DisableDataEndpoints();

    /*
     * SETUP transaction 已经由硬件 ACK。
     *
     * 这里继续准备 DATA1 零长度 Status IN。
     * 主机 ACK 该状态包后，本次控制传输才真正结束。
     */
    USB_Endpoint0ArmStatusIn();
}


static void USB_HandleGetInterface(
    const USB_SETUP_REQ *setup)
{
    if ((setup->bRequestType != 0x81U) ||
        (setup->wValue != 0U) ||
        (setup->wLength != 1U) ||
        (usbDeviceContext.configurationValue == 0U) ||
        ((setup->wIndex & 0xFF00U) != 0U))
    {
        USB_Endpoint0Stall();
        return;
    }

    const uint8_t interfaceNumber =
        (uint8_t)setup->wIndex;

    if (interfaceNumber ==
        USB_INTERFACE_ECM_CONTROL)
    {
        usbDeviceContext.endpoint0.reply[0] = 0U;
    }
    else if (interfaceNumber ==
             USB_INTERFACE_ECM_DATA)
    {
        usbDeviceContext.endpoint0.reply[0] =
            usbDeviceContext.dataAlternateSetting;
    }
    else
    {
        USB_Endpoint0Stall();
        return;
    }

    USB_Endpoint0StartDataIn(
        usbDeviceContext.endpoint0.reply,
        1U,
        setup->wLength);
}


static void USB_HandleSetInterface(
    const USB_SETUP_REQ *setup)
{
    if ((setup->bRequestType != 0x01U) ||
        (setup->wLength != 0U) ||
        (usbDeviceContext.configurationValue == 0U) ||
        ((setup->wIndex & 0xFF00U) != 0U) ||
        ((setup->wValue & 0xFF00U) != 0U))
    {
        USB_Endpoint0Stall();
        return;
    }

    const uint8_t interfaceNumber =
        (uint8_t)setup->wIndex;

    const uint8_t alternateSetting =
        (uint8_t)setup->wValue;

    if (interfaceNumber ==
        USB_INTERFACE_ECM_CONTROL)
    {
        if (alternateSetting != 0U)
        {
            USB_Endpoint0Stall();
            return;
        }
    }
    else if (interfaceNumber ==
             USB_INTERFACE_ECM_DATA)
    {
        if (alternateSetting ==
            USB_ECM_DATA_INTERFACE_DISABLED)
        {
            USB_DisableDataEndpoints();
        }
        else if (alternateSetting ==
                 USB_ECM_DATA_INTERFACE_ENABLED)
        {
            USB_EnableDataEndpoints();
        }
        else
        {
            USB_Endpoint0Stall();
            return;
        }

        usbDeviceContext.dataAlternateSetting =
            alternateSetting;
    }
    else
    {
        USB_Endpoint0Stall();
        return;
    }

    USB_Endpoint0ArmStatusIn();
}


static void USB_HandleStandardRequest(
    const USB_SETUP_REQ *setup)
{
    switch (setup->bRequest)
    {
    case USB_GET_DESCRIPTOR:
        USB_HandleGetDescriptor(setup);
        break;

    case USB_SET_ADDRESS:
        {
            if ((setup->bRequestType != 0x00U) ||
                (setup->wValue > DADDR_ADD) ||
                (setup->wIndex != 0U) ||
                (setup->wLength != 0U))
            {
                USB_Endpoint0Stall();
                break;
            }

            usbDeviceContext.endpoint0.pendingAddress =
                (uint8_t)setup->wValue;

            usbDeviceContext.endpoint0.pendingAddressValid =
                1U;

            USB_Endpoint0ArmStatusIn();
            break;
        }

    case USB_GET_STATUS:
        USB_HandleGetStatus(setup);
        break;

    case USB_GET_CONFIGURATION:
        USB_HandleGetConfiguration(setup);
        break;

    case USB_SET_CONFIGURATION:
        USB_HandleSetConfiguration(
            setup);
        break;

    case USB_GET_INTERFACE:
        USB_HandleGetInterface(setup);
        break;

    case USB_SET_INTERFACE:
        USB_HandleSetInterface(
            setup);
        break;

    default:
        USB_Endpoint0Stall();
        break;
    }
}


static void USB_HandleClassRequest(
    const USB_SETUP_REQ *setup)
{
    switch (setup->bRequest)
    {
    case USB_CDC_SET_ETHERNET_PACKET_FILTER:
        {
            if ((setup->bRequestType != 0x21U) ||
                (setup->wIndex !=
                 USB_INTERFACE_ECM_CONTROL) ||
                (setup->wLength != 0U) ||
                (usbDeviceContext.configurationValue == 0U))
            {
                USB_Endpoint0Stall();
                break;
            }

            usbDeviceContext.ethernetPacketFilter =
                setup->wValue;

            USB_Endpoint0ArmStatusIn();
            break;
        }

    default:
        USB_Endpoint0Stall();
        break;
    }
}


static void USB_HandleSetup(void)
{
    /*
     * 新 SETUP 会终止上一笔 EP0 控制传输。
     */
    usbDeviceContext.endpoint0 =
        (USBEndpoint0Context){0};

    _SetEPTxCount(
        USB_EP0_NUMBER,
        0U);

    _SetEPTxStatus(
        USB_EP0_NUMBER,
        EP_TX_NAK);

    _SetEPRxStatus(
        USB_EP0_NUMBER,
        EP_RX_NAK);

    /*
     * PMA 不是连续字节内存。
     * 固定复制 8 字节 SETUP 到栈上。
     */
    USB_SETUP_REQ setup;

    USB_PMA_Read(
        &setup,
        USB_PMA_EP0_RX_ADDRESS,
        sizeof(setup));

    switch (setup.bRequestType &
            USB_REQ_TYP_MASK)
    {
    case USB_REQ_TYP_STANDARD:
        USB_HandleStandardRequest(
            &setup);
        break;

    case USB_REQ_TYP_CLASS:
        USB_HandleClassRequest(
            &setup);
        break;

    case USB_REQ_TYP_VENDOR:
    default:
        USB_Endpoint0Stall();
        break;
    }
}


static void USB_HandleEndpoint0Out(void)
{
    if (usbDeviceContext.endpoint0.stage !=
        USB_EP0_STAGE_STATUS_OUT)
    {
        USB_Endpoint0RearmOut();
        return;
    }

    if (_GetEPRxCount(
            USB_EP0_NUMBER) != 0U)
    {
        USB_Endpoint0Stall();
        return;
    }

    usbDeviceContext.endpoint0 =
        (USBEndpoint0Context){0};

    _SetEPTxCount(
        USB_EP0_NUMBER,
        0U);

    _SetEPTxStatus(
        USB_EP0_NUMBER,
        EP_TX_NAK);

    USB_Endpoint0RearmOut();
}


static void USB_HandleOut(
    uint8_t endpointNumber)
{
    if (endpointNumber == USB_EP0_NUMBER)
    {
        USB_HandleEndpoint0Out();
        return;
    }

    if (endpointNumber ==
        USB_ETHERNET_OUT_ENDPOINT)
    {
        USBD_HookEthernetOutTransaction();
        return;
    }
}


static void USB_HandleEndpoint0In(void)
{
    switch (usbDeviceContext.endpoint0.stage)
    {
    case USB_EP0_STAGE_STATUS_IN:
        {
            if (usbDeviceContext.endpoint0
                    .pendingAddressValid != 0U)
            {
                USB_SetDeviceAddress(
                    usbDeviceContext.endpoint0
                        .pendingAddress);
            }

            usbDeviceContext.endpoint0 =
                (USBEndpoint0Context){0};

            _SetEPTxCount(
                USB_EP0_NUMBER,
                0U);

            _SetEPTxStatus(
                USB_EP0_NUMBER,
                EP_TX_NAK);

            USB_Endpoint0RearmOut();
            break;
        }

    case USB_EP0_STAGE_DATA_IN:
        {
            if (usbDeviceContext.endpoint0
                    .inRemaining != 0U)
            {
                uint16_t packetLength =
                    usbDeviceContext.endpoint0
                        .inRemaining;

                if (packetLength >
                    BSP_USB_EP0_PACKET_SIZE)
                {
                    packetLength =
                        BSP_USB_EP0_PACKET_SIZE;
                }

                USB_PMA_Write(
                    USB_PMA_EP0_TX_ADDRESS,
                    usbDeviceContext.endpoint0
                        .inData,
                    packetLength);

                usbDeviceContext.endpoint0.inData +=
                    packetLength;

                usbDeviceContext.endpoint0.inRemaining -=
                    packetLength;

                _SetEPTxCount(
                    USB_EP0_NUMBER,
                    packetLength);

                _SetEPTxStatus(
                    USB_EP0_NUMBER,
                    EP_TX_VALID);

                break;
            }

            if (usbDeviceContext.endpoint0
                    .needsZLP != 0U)
            {
                usbDeviceContext.endpoint0.needsZLP = 0U;

                _SetEPTxCount(
                    USB_EP0_NUMBER,
                    0U);

                _SetEPTxStatus(
                    USB_EP0_NUMBER,
                    EP_TX_VALID);

                break;
            }

            usbDeviceContext.endpoint0.stage =
                USB_EP0_STAGE_STATUS_OUT;

            _SetEPTxStatus(
                USB_EP0_NUMBER,
                EP_TX_NAK);

            USB_Endpoint0RearmOut();
            break;
        }

    default:
        _SetEPTxCount(
            USB_EP0_NUMBER,
            0U);

        _SetEPTxStatus(
            USB_EP0_NUMBER,
            EP_TX_NAK);

        break;
    }
}


static void USB_HandleIn(
    uint8_t endpointNumber)
{
    if (endpointNumber == USB_EP0_NUMBER)
    {
        USB_HandleEndpoint0In();
        return;
    }

    if (endpointNumber ==
        USB_NOTIFICATION_ENDPOINT)
    {
        /*
         * TODO:
         * 完成一条 CDC notification。
         */
        _SetEPTxCount(
            USB_NOTIFICATION_ENDPOINT,
            0U);

        _SetEPTxStatus(
            USB_NOTIFICATION_ENDPOINT,
            EP_TX_NAK);

        return;
    }

    if (endpointNumber ==
        USB_ETHERNET_IN_ENDPOINT)
    {
        USBD_HookEthernetInTransaction();
        return;
    }
}


__attribute__((always_inline))
static inline void USB_DispatchTransfer(
    uint16_t interruptStatus)
{
    const uint8_t endpointNumber =
        (uint8_t)(
            interruptStatus &
            ISTR_EP_ID);

    const uint16_t endpointStatus =
        _GetENDPOINT(
            endpointNumber);

    if ((endpointStatus &
         EP_CTR_RX) != 0U)
    {
        const uint8_t isSetup =
            (uint8_t)(
                (endpointStatus &
                 EP_SETUP) != 0U);

        /*
         * 先清 CTR，再修改端点状态。
         */
        _ClearEP_CTR_RX(
            endpointNumber);

        if (isSetup != 0U)
        {
            /*
             * 新 SETUP 会取消旧的 IN 完成事件。
             */
            if ((endpointStatus &
                 EP_CTR_TX) != 0U)
            {
                _ClearEP_CTR_TX(
                    endpointNumber);
            }

            if (endpointNumber ==
                USB_EP0_NUMBER)
            {
                USB_HandleSetup();
            }

            return;
        }

        USB_HandleOut(
            endpointNumber);
    }

    if ((endpointStatus &
         EP_CTR_TX) != 0U)
    {
        _ClearEP_CTR_TX(
            endpointNumber);

        USB_HandleIn(
            endpointNumber);
    }
}


uint8_t USBD_IsHostConfigured(void)
{
    return
        usbDeviceContext.configurationValue ==
        USB_DEVICE_CONFIGURATION;
}


uint8_t USBD_IsDataInterfaceEnabled(void)
{
    return
        (
            (usbDeviceContext.configurationValue ==
             USB_DEVICE_CONFIGURATION) &&
            (usbDeviceContext.dataAlternateSetting ==
             USB_ECM_DATA_INTERFACE_ENABLED)
        );
}

void USB_LP_CAN1_RX0_IRQHandler(void)
{
    uint16_t interruptStatus =
        _GetISTR();

    /*
     * Bus Reset 使旧 transaction 全部失效。
     */
    if ((interruptStatus &
         ISTR_RESET) != 0U)
    {
        _SetISTR(
            (uint16_t)~ISTR_RESET);

        USB_HandleBusReset();
    }

    /*
     * ISTR_CTR 由各端点的 CTR_RX/CTR_TX 汇总产生。
     * 必须一直处理到没有端点事件为止。
     */
    while (((interruptStatus =
             _GetISTR()) &
            ISTR_CTR) != 0U)
    {
        USB_DispatchTransfer(
            interruptStatus);
    }
}
