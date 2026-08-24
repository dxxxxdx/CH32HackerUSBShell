//
// Created by dxxdx on 2026/7/23.
//

#ifndef CH32V203C8U_DHCP_H
#define CH32V203C8U_DHCP_H

#include <stdint.h>

/*
 * USB 网卡使用的 IPv4 子网：
 *
 * Network: 10.24.0.0/24
 * MCU:     10.24.0.1
 * Host:    10.24.0.2
 * Mask:    255.255.255.0
 */
#define DHCP_SERVER_IP_0       10U
#define DHCP_SERVER_IP_1       24U
#define DHCP_SERVER_IP_2       0U
#define DHCP_SERVER_IP_3       1U

#define DHCP_CLIENT_IP_0       10U
#define DHCP_CLIENT_IP_1       24U
#define DHCP_CLIENT_IP_2       0U
#define DHCP_CLIENT_IP_3       2U

#define DHCP_SUBNET_MASK_0     255U
#define DHCP_SUBNET_MASK_1     255U
#define DHCP_SUBNET_MASK_2     255U
#define DHCP_SUBNET_MASK_3     0U

/*
 * DHCP 租约时间，单位为秒。
 * 当前只有一个固定客户端地址，不需要维护地址池。
 */
#define DHCP_LEASE_TIME        86400UL

/*
 * BOOTP/DHCP 固定端口。
 */
#define DHCP_SERVER_PORT       67U
#define DHCP_CLIENT_PORT       68U

/*
 * DHCP magic cookie。
 *
 * 在线上按照 99、130、83、99 四个字节保存。
 */
#define DHCP_MAGIC_COOKIE      0x63825363UL

/*
 * DHCP message type。
 */
#define DHCP_MESSAGE_DISCOVER  1U
#define DHCP_MESSAGE_OFFER     2U
#define DHCP_MESSAGE_REQUEST   3U
#define DHCP_MESSAGE_DECLINE   4U
#define DHCP_MESSAGE_ACK       5U
#define DHCP_MESSAGE_NAK       6U
#define DHCP_MESSAGE_RELEASE   7U
#define DHCP_MESSAGE_INFORM    8U

#endif /* CH32V203C8U_DHCP_H */