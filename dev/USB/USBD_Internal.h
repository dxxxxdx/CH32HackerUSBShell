#ifndef CH32V203C8U_USBD_INTERNAL_H
#define CH32V203C8U_USBD_INTERNAL_H

#include <stdint.h>


/*
 * USBD 各实现文件共享的端点与 PMA 布局。
 *
 * 这是模块内部接口，不提供给应用层使用。
 */
#define USB_PMA_BTABLE_ADDRESS          0x0000U
#define USB_PMA_EP0_RX_ADDRESS          0x0040U
#define USB_PMA_EP0_TX_ADDRESS          0x0080U
#define USB_PMA_EP1_TX_ADDRESS          0x00C0U
#define USB_PMA_EP2_RX_ADDRESS          0x0100U
#define USB_PMA_EP3_TX_ADDRESS          0x0140U

#define USB_EP0_NUMBER                  0U
#define USB_NOTIFICATION_ENDPOINT       1U
#define USB_ETHERNET_OUT_ENDPOINT       2U
#define USB_ETHERNET_IN_ENDPOINT        3U
#define USB_ENDPOINT_COUNT              8U

#define USB_FULL_SPEED_PACKET_SIZE      64U


void USB_PMA_Write(
    uint16_t pmaAddress,
    const void *source,
    uint16_t length);

void USB_PMA_Read(
    void *destination,
    uint16_t pmaAddress,
    uint16_t length);

void USB_SetEndpointRxCapacity(
    uint8_t endpointNumber,
    uint16_t capacity);


/*
 * USB Core 执行到对应位置时调用的 Ethernet 钩子。
 *
 * Core 不认识 ETHManager；manager 的实例绑定和状态修改
 * 全部留在 USBD_Ethernet.c。
 */
void USBD_HookResetEthernetTransferState(void);

void USBD_HookEthernetOutTransaction(void);

void USBD_HookEthernetInTransaction(void);


#endif /* CH32V203C8U_USBD_INTERNAL_H */
