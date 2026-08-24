#ifndef CH32V203C8U_USBD_ETHERNET_H
#define CH32V203C8U_USBD_ETHERNET_H

#include <stdint.h>

#include "ETH.h"


/*
 * 借出一个已经接收完成的 Ethernet frame。
 *
 * 没有完整帧时返回 NULL。
 * 返回的指针在 Release 前保持有效。
 */
const uint8_t *USBD_EthernetGetReceivedFrame(
    const ETHManager *manager);


/*
 * 归还 RX frame，并在 Alt 1 有效时重新开放 EP2。
 */
void USBD_EthernetReleaseReceivedFrame(
    ETHManager *manager);


/*
 * 借出可写的 TX frame。
 *
 * 当前仍有待发送帧时返回 NULL。
 */
uint8_t *USBD_EthernetGetTransmitFrame(
    ETHManager *manager);


/*
 * 提交 manager->txFrameLength 指定的帧，
 * 并主动挂出 EP3 第一包。
 */
uint8_t USBD_EthernetTransmit(
    ETHManager *manager);


extern const ETHOperations
    usbdEthernetOperations;


#endif /* CH32V203C8U_USBD_ETHERNET_H */
