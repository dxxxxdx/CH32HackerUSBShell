

#include "USB_DESC.h"

#include <stddef.h>
#include <stdint.h>

#define USB_DESC_LOW_BYTE(value) \
    ((uint8_t)((uint16_t)(value) & 0x00FFU))

#define USB_DESC_HIGH_BYTE(value) \
    ((uint8_t)(((uint16_t)(value) >> 8U) & 0x00FFU))

/*
 * USB 2.0 Device Descriptor。
 *
 * 因为配置中使用了 IAD，所以设备级类别使用：
 *
 *   EFh = Miscellaneous Device
 *   02h = Common Class
 *   01h = Interface Association Descriptor
 */
const uint8_t usbDeviceDescriptor[18] =
{
    18U,                               /* bLength */
    USB_DESC_TYPE_DEVICE,              /* bDescriptorType */

    0x00U, 0x02U,                      /* bcdUSB = 2.00 */

    0xEFU,                             /* bDeviceClass */
    0x02U,                             /* bDeviceSubClass */
    0x01U,                             /* bDeviceProtocol */

    USB_DESC_EP0_PACKET_SIZE,          /* bMaxPacketSize0 */

    USB_DESC_LOW_BYTE(USB_DESC_VENDOR_ID),
    USB_DESC_HIGH_BYTE(USB_DESC_VENDOR_ID),

    USB_DESC_LOW_BYTE(USB_DESC_PRODUCT_ID),
    USB_DESC_HIGH_BYTE(USB_DESC_PRODUCT_ID),

    USB_DESC_LOW_BYTE(USB_DESC_DEVICE_RELEASE),
    USB_DESC_HIGH_BYTE(USB_DESC_DEVICE_RELEASE),

    USB_STRING_MANUFACTURER,           /* iManufacturer */
    USB_STRING_PRODUCT,                /* iProduct */
    USB_STRING_SERIAL,                 /* iSerialNumber */

    1U                                 /* bNumConfigurations */
};


/*
 * CDC-ECM Configuration Descriptor。
 *
 * 总长度：
 *
 *   Configuration                    9
 *   Interface Association            8
 *   Control Interface                9
 *   CDC Header                       5
 *   CDC Union                        5
 *   Ethernet Networking             13
 *   Notification Endpoint            7
 *   Data Interface Alt 0              9
 *   Data Interface Alt 1              9
 *   Bulk OUT Endpoint                 7
 *   Bulk IN Endpoint                  7
 *                                  ----
 *                                    88
 */
const uint8_t
usbConfigurationDescriptor[USB_DESC_CONFIGURATION_LENGTH] =
{
    /*
     * Configuration Descriptor
     */
    9U,                                /* bLength */
    USB_DESC_TYPE_CONFIGURATION,       /* bDescriptorType */

    USB_DESC_LOW_BYTE(
        USB_DESC_CONFIGURATION_LENGTH),
    USB_DESC_HIGH_BYTE(
        USB_DESC_CONFIGURATION_LENGTH),

    USB_INTERFACE_COUNT,               /* bNumInterfaces */
    1U,                                /* bConfigurationValue */
    0U,                                /* iConfiguration */
    0x80U,                             /* Bus powered */
    50U,                               /* 100 mA，单位为 2 mA */


    /*
     * Interface Association Descriptor
     *
     * 声明 Interface 0 和 Interface 1 共同组成一个 ECM 功能。
     */
    8U,                                /* bLength */
    USB_DESC_TYPE_INTERFACE_ASSOCIATION,
    USB_INTERFACE_ECM_CONTROL,         /* bFirstInterface */
    2U,                                /* bInterfaceCount */
    0x02U,                             /* Communications/CDC */
    0x06U,                             /* Ethernet Control Model */
    0x00U,                             /* No class protocol */
    USB_STRING_ECM_FUNCTION,           /* iFunction */


    /*
     * Interface 0：CDC-ECM Control Interface
     */
    9U,                                /* bLength */
    USB_DESC_TYPE_INTERFACE,
    USB_INTERFACE_ECM_CONTROL,         /* bInterfaceNumber */
    0U,                                /* bAlternateSetting */
    1U,                                /* bNumEndpoints */
    0x02U,                             /* CDC Control */
    0x06U,                             /* Ethernet Control Model */
    0x00U,                             /* No class protocol */
    USB_STRING_ECM_FUNCTION,           /* iInterface */


    /*
     * CDC Header Functional Descriptor
     */
    5U,                                /* bFunctionLength */
    USB_DESC_TYPE_CS_INTERFACE,
    USB_CDC_DESC_SUBTYPE_HEADER,
    0x20U, 0x01U,                      /* bcdCDC = 1.20 */


    /*
     * CDC Union Functional Descriptor
     *
     * Interface 0 是控制接口。
     * Interface 1 是对应的数据接口。
     */
    5U,                                /* bFunctionLength */
    USB_DESC_TYPE_CS_INTERFACE,
    USB_CDC_DESC_SUBTYPE_UNION,
    USB_INTERFACE_ECM_CONTROL,         /* bControlInterface */
    USB_INTERFACE_ECM_DATA,            /* bSubordinateInterface0 */


    /*
     * Ethernet Networking Functional Descriptor
     */
    13U,                               /* bFunctionLength */
    USB_DESC_TYPE_CS_INTERFACE,
    USB_CDC_DESC_SUBTYPE_ETHERNET_NETWORKING,

    USB_STRING_MAC_ADDRESS,            /* iMACAddress */

    0x00U, 0x00U, 0x00U, 0x00U,       /* bmEthernetStatistics */

    USB_DESC_LOW_BYTE(
        USB_DESC_ETHERNET_SEGMENT_SIZE),
    USB_DESC_HIGH_BYTE(
        USB_DESC_ETHERNET_SEGMENT_SIZE),

    0x00U, 0x00U,                      /* wNumberMCFilters */
    0U,                                /* bNumberPowerFilters */


    /*
     * EP1 IN：CDC 网络状态通知端点。
     *
     * 后面发送：
     *   NETWORK_CONNECTION
     *   CONNECTION_SPEED_CHANGE
     */
    7U,                                /* bLength */
    USB_DESC_TYPE_ENDPOINT,
    USB_ENDPOINT_NOTIFICATION_IN,      /* bEndpointAddress */
    0x03U,                             /* Interrupt */
    USB_DESC_LOW_BYTE(
        USB_DESC_NOTIFICATION_SIZE),
    USB_DESC_HIGH_BYTE(
        USB_DESC_NOTIFICATION_SIZE),
    16U,                               /* bInterval = 16 ms */


    /*
     * Interface 1，Alternate Setting 0。
     *
     * 默认状态不开放 Bulk 端点。
     */
    9U,                                /* bLength */
    USB_DESC_TYPE_INTERFACE,
    USB_INTERFACE_ECM_DATA,            /* bInterfaceNumber */
    USB_ECM_DATA_INTERFACE_DISABLED,   /* bAlternateSetting */
    0U,                                /* bNumEndpoints */
    0x0AU,                             /* CDC Data */
    0x00U,
    0x00U,
    0U,                                /* iInterface */


    /*
     * Interface 1，Alternate Setting 1。
     *
     * 主机 SET_INTERFACE(1, 1) 后开放 EP2/EP3。
     */
    9U,                                /* bLength */
    USB_DESC_TYPE_INTERFACE,
    USB_INTERFACE_ECM_DATA,            /* bInterfaceNumber */
    USB_ECM_DATA_INTERFACE_ENABLED,    /* bAlternateSetting */
    2U,                                /* bNumEndpoints */
    0x0AU,                             /* CDC Data */
    0x00U,
    0x00U,
    0U,                                /* iInterface */


    /*
     * EP2 OUT：电脑发送以太网帧给单片机。
     */
    7U,                                /* bLength */
    USB_DESC_TYPE_ENDPOINT,
    USB_ENDPOINT_ETHERNET_OUT,
    0x02U,                             /* Bulk */
    USB_DESC_LOW_BYTE(
        USB_DESC_BULK_PACKET_SIZE),
    USB_DESC_HIGH_BYTE(
        USB_DESC_BULK_PACKET_SIZE),
    0U,                                /* Bulk bInterval ignored */


    /*
     * EP3 IN：单片机发送以太网帧给电脑。
     */
    7U,                                /* bLength */
    USB_DESC_TYPE_ENDPOINT,
    USB_ENDPOINT_ETHERNET_IN,
    0x02U,                             /* Bulk */
    USB_DESC_LOW_BYTE(
        USB_DESC_BULK_PACKET_SIZE),
    USB_DESC_HIGH_BYTE(
        USB_DESC_BULK_PACKET_SIZE),
    0U
};


_Static_assert(
    sizeof(usbConfigurationDescriptor) ==
    USB_DESC_CONFIGURATION_LENGTH,
    "USB configuration descriptor length error");


/*
 * String 0：English (United States)，语言 ID 0x0409。
 */
const uint8_t usbStringLanguage[] =
{
    4U,
    USB_DESC_TYPE_STRING,
    0x09U, 0x04U
};


/*
 * String 1："dxxdx"
 */
const uint8_t usbStringManufacturer[] =
{
    12U,
    USB_DESC_TYPE_STRING,

    'd', 0U,
    'x', 0U,
    'x', 0U,
    'd', 0U,
    'x', 0U
};


/*
 * String 2："USB ECM Shell"
 */
const uint8_t usbStringProduct[] =
{
    28U,
    USB_DESC_TYPE_STRING,

    'U', 0U,
    'S', 0U,
    'B', 0U,
    ' ', 0U,
    'E', 0U,
    'C', 0U,
    'M', 0U,
    ' ', 0U,
    'S', 0U,
    'h', 0U,
    'e', 0U,
    'l', 0U,
    'l', 0U
};


/*
 * String 3："00000001"
 *
 * 后续可以用芯片 96-bit Unique ID 动态生成。
 */
const uint8_t usbStringSerial[] =
{
    18U,
    USB_DESC_TYPE_STRING,

    '0', 0U,
    '0', 0U,
    '0', 0U,
    '0', 0U,
    '0', 0U,
    '0', 0U,
    '0', 0U,
    '1', 0U
};


/*
 * String 4：ECM MAC 地址。
 *
 * 必须是 12 个十六进制字符，不能带冒号。
 *
 * 02 表示：
 *   bit0 = 0：单播地址
 *   bit1 = 1：本地管理地址
 *
 * 实际以太网协议层使用的 MAC 字节为：
 *
 *   02:00:00:00:00:01
 */
const uint8_t usbStringMACAddress[] =
{
    26U,
    USB_DESC_TYPE_STRING,

    '0', 0U,
    '2', 0U,
    '0', 0U,
    '0', 0U,
    '0', 0U,
    '0', 0U,
    '0', 0U,
    '0', 0U,
    '0', 0U,
    '0', 0U,
    '0', 0U,
    '1', 0U
};


/*
 * String 5："CDC ECM"
 */
const uint8_t usbStringECMFunction[] =
{
    16U,
    USB_DESC_TYPE_STRING,

    'C', 0U,
    'D', 0U,
    'C', 0U,
    ' ', 0U,
    'E', 0U,
    'C', 0U,
    'M', 0U
};


const uint8_t *USBDesc_GetDescriptor(
    uint8_t descriptorType,
    uint8_t descriptorIndex,
    uint16_t *length)
{
    switch (descriptorType)
    {
    case USB_DESC_TYPE_DEVICE:
        if (descriptorIndex == 0U)
        {
            *length = sizeof(usbDeviceDescriptor);
            return usbDeviceDescriptor;
        }
        break;

    case USB_DESC_TYPE_CONFIGURATION:
        if (descriptorIndex == 0U)
        {
            *length =
                sizeof(usbConfigurationDescriptor);

            return usbConfigurationDescriptor;
        }
        break;

    case USB_DESC_TYPE_STRING:
        switch (descriptorIndex)
        {
        case USB_STRING_LANGUAGE:
            *length = sizeof(usbStringLanguage);
            return usbStringLanguage;

        case USB_STRING_MANUFACTURER:
            *length = sizeof(usbStringManufacturer);
            return usbStringManufacturer;

        case USB_STRING_PRODUCT:
            *length = sizeof(usbStringProduct);
            return usbStringProduct;

        case USB_STRING_SERIAL:
            *length = sizeof(usbStringSerial);
            return usbStringSerial;

        case USB_STRING_MAC_ADDRESS:
            *length = sizeof(usbStringMACAddress);
            return usbStringMACAddress;

        case USB_STRING_ECM_FUNCTION:
            *length = sizeof(usbStringECMFunction);
            return usbStringECMFunction;

        default:
            break;
        }
        break;

    default:
        break;
    }

    *length = 0U;
    return NULL;
}