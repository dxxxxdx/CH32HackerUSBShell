//
// Created by dxxdx on 2026/7/23.
//

#include "ARP.h"

#include <string.h>

#include "ETH_Transmitter.h"

#include "DHCP.h"

#define ARP_PACKET_LENGTH              28U

#define ARP_HARDWARE_TYPE_ETHERNET     1U
#define ARP_PROTOCOL_TYPE_IPV4         0x0800U

#define ARP_HARDWARE_ADDRESS_LENGTH    6U
#define ARP_PROTOCOL_ADDRESS_LENGTH    4U

#define ARP_OPERATION_REQUEST          1U
#define ARP_OPERATION_REPLY            2U

#define ARP_ETHERTYPE                  0x0806U


/*
 * Ethernet/IPv4 ARP packet 布局。
 */
#define ARP_OFFSET_HARDWARE_TYPE       0U
#define ARP_OFFSET_PROTOCOL_TYPE       2U
#define ARP_OFFSET_HARDWARE_LENGTH     4U
#define ARP_OFFSET_PROTOCOL_LENGTH     5U
#define ARP_OFFSET_OPERATION           6U

#define ARP_OFFSET_SENDER_MAC          8U
#define ARP_OFFSET_SENDER_IP           14U
#define ARP_OFFSET_TARGET_MAC          18U
#define ARP_OFFSET_TARGET_IP           24U


static const uint8_t arpLocalIP[
    ARP_PROTOCOL_ADDRESS_LENGTH] =
{
    DHCP_SERVER_IP_0,
    DHCP_SERVER_IP_1,
    DHCP_SERVER_IP_2,
    DHCP_SERVER_IP_3
};


static const uint8_t arpLocalMAC[
    ARP_HARDWARE_ADDRESS_LENGTH] =
{
    ARP_LOCAL_MAC_0,
    ARP_LOCAL_MAC_1,
    ARP_LOCAL_MAC_2,
    ARP_LOCAL_MAC_3,
    ARP_LOCAL_MAC_4,
    ARP_LOCAL_MAC_5
};


/*
 * 从网络字节序读取 uint16_t。
 *
 * ARP Header 中的多字节字段全部使用网络字节序。
 */
static uint16_t ARP_ReadU16(
    const uint8_t *data)
{
    return
        (uint16_t)(
            ((uint16_t)data[0] << 8U) |
            data[1]);
}


/*
 * 以网络字节序写入 uint16_t。
 */
static void ARP_WriteU16(
    uint8_t *data,
    uint16_t value)
{
    data[0] =
        (uint8_t)(
            value >> 8U);

    data[1] =
        (uint8_t)value;
}


/*
 * 判断 ARP request 查询的是否为 MCU 地址。
 */
static uint8_t ARP_IsLocalAddress(
    const uint8_t *address)
{
    return
        (uint8_t)(
            memcmp(
                address,
                arpLocalIP,
                ARP_PROTOCOL_ADDRESS_LENGTH) == 0);
}


/*
 * 检查当前只支持的 ARP packet 类型：
 *
 *   Hardware Type: Ethernet
 *   Protocol Type: IPv4
 *   Hardware Size: 6
 *   Protocol Size: 4
 *   Operation:     Request
 */
static uint8_t ARP_IsSupportedRequest(
    const uint8_t *arpPacket)
{
    if (ARP_ReadU16(
            &arpPacket[
                ARP_OFFSET_HARDWARE_TYPE]) !=
        ARP_HARDWARE_TYPE_ETHERNET)
    {
        return 0U;
    }

    if (ARP_ReadU16(
            &arpPacket[
                ARP_OFFSET_PROTOCOL_TYPE]) !=
        ARP_PROTOCOL_TYPE_IPV4)
    {
        return 0U;
    }

    if (arpPacket[
            ARP_OFFSET_HARDWARE_LENGTH] !=
        ARP_HARDWARE_ADDRESS_LENGTH)
    {
        return 0U;
    }

    if (arpPacket[
            ARP_OFFSET_PROTOCOL_LENGTH] !=
        ARP_PROTOCOL_ADDRESS_LENGTH)
    {
        return 0U;
    }

    if (ARP_ReadU16(
            &arpPacket[
                ARP_OFFSET_OPERATION]) !=
        ARP_OPERATION_REQUEST)
    {
        return 0U;
    }

    return 1U;
}


/*
 * 填充固定长度的 Ethernet/IPv4 ARP Reply。
 */
static void ARP_BuildReply(
    uint8_t *reply,
    const uint8_t *request)
{
    ARP_WriteU16(
        &reply[
            ARP_OFFSET_HARDWARE_TYPE],
        ARP_HARDWARE_TYPE_ETHERNET);

    ARP_WriteU16(
        &reply[
            ARP_OFFSET_PROTOCOL_TYPE],
        ARP_PROTOCOL_TYPE_IPV4);

    reply[
        ARP_OFFSET_HARDWARE_LENGTH] =
        ARP_HARDWARE_ADDRESS_LENGTH;

    reply[
        ARP_OFFSET_PROTOCOL_LENGTH] =
        ARP_PROTOCOL_ADDRESS_LENGTH;

    ARP_WriteU16(
        &reply[
            ARP_OFFSET_OPERATION],
        ARP_OPERATION_REPLY);

    /*
     * Sender：
     *
     *   MCU_MAC
     *   10.24.0.1
     */
    memcpy(
        &reply[
            ARP_OFFSET_SENDER_MAC],
        arpLocalMAC,
        ARP_HARDWARE_ADDRESS_LENGTH);

    memcpy(
        &reply[
            ARP_OFFSET_SENDER_IP],
        arpLocalIP,
        ARP_PROTOCOL_ADDRESS_LENGTH);

    /*
     * Target：
     *
     *   request 中的发送方 MAC
     *   request 中的发送方 IPv4
     */
    memcpy(
        &reply[
            ARP_OFFSET_TARGET_MAC],
        &request[
            ARP_OFFSET_SENDER_MAC],
        ARP_HARDWARE_ADDRESS_LENGTH);

    memcpy(
        &reply[
            ARP_OFFSET_TARGET_IP],
        &request[
            ARP_OFFSET_SENDER_IP],
        ARP_PROTOCOL_ADDRESS_LENGTH);
}


ETHDispatchResult ARP_Handle(
    ETHManager *manager,
    const ETHFrameView *ethernetFrame)
{
    /*
     * Ethernet/IPv4 ARP packet 固定为 28 字节。
     *
     * Ethernet padding 不属于 ARP packet，
     * 因此这里只要求至少存在 28 字节。
     */
    if (ethernetFrame->packetLength <
        ARP_PACKET_LENGTH)
    {
        return ETH_DISPATCH_DROPPED;
    }

    const uint8_t *const request =
        ethernetFrame->packet;

    if (ARP_IsSupportedRequest(
            request) == 0U)
    {
        return ETH_DISPATCH_DROPPED;
    }

    /*
     * ARP Header 中的 Sender MAC 应该和
     * Ethernet Header 中的 Source MAC 一致。
     *
     * 不一致的畸形 frame 直接丢弃。
     */
    if (memcmp(
            &request[
                ARP_OFFSET_SENDER_MAC],
            ethernetFrame->sourceMAC,
            ARP_HARDWARE_ADDRESS_LENGTH) != 0)
    {
        return ETH_DISPATCH_DROPPED;
    }

    /*
     * 只响应：
     *
     *     Who has 10.24.0.1?
     *
     * 对其他 IPv4 地址保持沉默。
     */
    if (ARP_IsLocalAddress(
            &request[
                ARP_OFFSET_TARGET_IP]) == 0U)
    {
        return ETH_DISPATCH_DROPPED;
    }

    /*
     * ARP Reply 是单播回复。
     *
     * Ethernet destination MAC 使用 request
     * 中的 Sender Hardware Address。
     */
    uint8_t *const reply =
        ETH_TransmitBegin(
            manager,
            &request[
                ARP_OFFSET_SENDER_MAC],
            ARP_ETHERTYPE);

    if (reply == NULL)
    {
        /*
         * TX frame 正忙。
         *
         * 保留当前 RX frame，稍后重新处理。
         */
        return ETH_DISPATCH_DEFERRED;
    }

    ARP_BuildReply(
        reply,
        request);

    if (ETH_TransmitCommit(
            manager,
            ARP_PACKET_LENGTH) == 0U)
    {
        /*
         * 28 字节不可能超过 TX frame capacity。
         * 若仍然提交失败，暂时保留 RX frame。
         */
        return ETH_DISPATCH_DEFERRED;
    }

    /*
     * 尝试立即启动 EP3。
     *
     * Commit 成功后 Reply 已经完全独立于 RX frame，
     * 即使 USB 当前未能启动，主循环也可以继续重试。
     */
    (void)manager->operations->
        transmit(manager);

    return ETH_DISPATCH_HANDLED;
}

