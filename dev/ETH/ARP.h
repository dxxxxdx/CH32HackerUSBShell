//
// Created by dxxdx on 2026/7/23.
//

#ifndef CH32V203C8U_ARP_H
#define CH32V203C8U_ARP_H

#include "ETH_Dispatcher.h"


/*
 * MCU 协议栈自己的 MAC。
 *
 * PC 侧 USB 网卡是：
 *     02:00:00:00:00:01
 *
 * MCU 使用：
 *     02:00:00:00:00:02
 *
 * 这个地址必须与 ETH_TransmitBegin() 写入
 * Ethernet Header 的 source MAC 保持一致。
 */
#define ARP_LOCAL_MAC_0     0x02U
#define ARP_LOCAL_MAC_1     0x00U
#define ARP_LOCAL_MAC_2     0x00U
#define ARP_LOCAL_MAC_3     0x00U
#define ARP_LOCAL_MAC_4     0x00U
#define ARP_LOCAL_MAC_5     0x02U


/*
 * 处理一个 Ethernet ARP frame。
 *
 * 当前只响应：
 *
 *   Ethernet + IPv4
 *   ARP Request
 *   Target IPv4 = ARP_LOCAL_IP
 */
ETHDispatchResult ARP_Handle(
    ETHManager *manager,
    const ETHFrameView *ethernetFrame);


#endif /* CH32V203C8U_ARP_H */