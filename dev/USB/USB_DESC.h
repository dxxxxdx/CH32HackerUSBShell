//
// Created by dxxdx on 2026/7/22.
//

#ifndef CH32V203C8U_USB_DESC_H
#define CH32V203C8U_USB_DESC_H

#include <stdint.h>

/*
 * 当前 VID/PID 只适合开发调试。
 *
 * 0x1A86 是 WCH 的 VID，正式产品不能自行占用未知 PID。
 * 后续正式使用时应换成自己获得授权的 VID/PID。
 */
#ifndef USB_DESC_VENDOR_ID
#define USB_DESC_VENDOR_ID              0x1A86U
#endif

#ifndef USB_DESC_PRODUCT_ID
#define USB_DESC_PRODUCT_ID             0xE0C0U
#endif

#define USB_DESC_DEVICE_RELEASE         0x0100U
#define USB_DESC_EP0_PACKET_SIZE        64U

#define USB_DESC_CONFIGURATION_LENGTH   88U
#define USB_DESC_ETHERNET_MTU           1500U
#define USB_DESC_ETHERNET_SEGMENT_SIZE  1514U

#define USB_DESC_NOTIFICATION_SIZE      16U
#define USB_DESC_BULK_PACKET_SIZE       64U

/*
 * 标准描述符类型。
 */
typedef enum
{
    USB_DESC_TYPE_DEVICE                 = 0x01U,
    USB_DESC_TYPE_CONFIGURATION          = 0x02U,
    USB_DESC_TYPE_STRING                 = 0x03U,
    USB_DESC_TYPE_INTERFACE              = 0x04U,
    USB_DESC_TYPE_ENDPOINT               = 0x05U,
    USB_DESC_TYPE_DEVICE_QUALIFIER       = 0x06U,
    USB_DESC_TYPE_INTERFACE_ASSOCIATION  = 0x0BU,

    USB_DESC_TYPE_CS_INTERFACE           = 0x24U,
    USB_DESC_TYPE_CS_ENDPOINT            = 0x25U
} USBDescriptorType;

/*
 * CDC 功能描述符子类型。
 */
typedef enum
{
    USB_CDC_DESC_SUBTYPE_HEADER               = 0x00U,
    USB_CDC_DESC_SUBTYPE_UNION                = 0x06U,
    USB_CDC_DESC_SUBTYPE_ETHERNET_NETWORKING  = 0x0FU
} USBCDCDescriptorSubtype;

/*
 * CDC-ECM 使用两个接口。
 */
typedef enum
{
    USB_INTERFACE_ECM_CONTROL = 0U,
    USB_INTERFACE_ECM_DATA    = 1U,

    USB_INTERFACE_COUNT       = 2U
} USBInterfaceNumber;

/*
 * ECM Data Interface 的 Alternate Setting。
 */
typedef enum
{
    USB_ECM_DATA_INTERFACE_DISABLED = 0U,
    USB_ECM_DATA_INTERFACE_ENABLED  = 1U
} USBEthernetDataAlternateSetting;

/*
 * 描述符字符串索引。
 */
typedef enum
{
    USB_STRING_LANGUAGE     = 0U,
    USB_STRING_MANUFACTURER = 1U,
    USB_STRING_PRODUCT      = 2U,
    USB_STRING_SERIAL       = 3U,
    USB_STRING_MAC_ADDRESS  = 4U,
    USB_STRING_ECM_FUNCTION = 5U,

    USB_STRING_COUNT
} USBStringIndex;

/*
 * 端点地址。
 *
 * bit7:
 *   0 = OUT，主机发给设备
 *   1 = IN，设备发给主机
 */
typedef enum
{
    USB_ENDPOINT_CONTROL_OUT      = 0x00U,
    USB_ENDPOINT_CONTROL_IN       = 0x80U,

    USB_ENDPOINT_NOTIFICATION_IN  = 0x81U,
    USB_ENDPOINT_ETHERNET_OUT     = 0x02U,
    USB_ENDPOINT_ETHERNET_IN      = 0x83U
} USBEndpointAddress;

/*
 * 设备描述符。
 */
extern const uint8_t usbDeviceDescriptor[18];

/*
 * 完整配置描述符，包含：
 *
 *   Configuration
 *   Interface Association
 *   ECM Control Interface
 *   CDC Header
 *   CDC Union
 *   Ethernet Networking
 *   EP1 Interrupt IN
 *   Data Interface Alt 0
 *   Data Interface Alt 1
 *   EP2 Bulk OUT
 *   EP3 Bulk IN
 */
extern const uint8_t
    usbConfigurationDescriptor[USB_DESC_CONFIGURATION_LENGTH];

/*
 * USB 字符串描述符。
 */
extern const uint8_t usbStringLanguage[];
extern const uint8_t usbStringManufacturer[];
extern const uint8_t usbStringProduct[];
extern const uint8_t usbStringSerial[];
extern const uint8_t usbStringMACAddress[];
extern const uint8_t usbStringECMFunction[];

/*
 * 根据 GET_DESCRIPTOR 的 descriptorType 和 descriptorIndex
 * 查找描述符。
 *
 * 返回：
 *   描述符起始地址；
 *   不支持的描述符返回 NULL。
 *
 * length：
 *   写入完整描述符长度。
 *
 * 注意：
 *   描述符位于 Flash，不能直接把返回地址塞进 USBFS DMA。
 *   EP0 发送时要把每一段复制到 bspUSBEndpoint0Buffer。
 */
const uint8_t *USBDesc_GetDescriptor(
    uint8_t descriptorType,
    uint8_t descriptorIndex,
    uint16_t *length);

#endif /* CH32V203C8U_USB_DESC_H */