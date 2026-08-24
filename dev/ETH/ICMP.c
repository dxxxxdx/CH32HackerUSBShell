//
// Created by dxxdx on 2026/7/23.
//

#include "ICMP.h"

#include <string.h>

#include "ARP.h"
#include "DHCP.h"
#include "ETH_Transmitter.h"


#define ICMP_TYPE_ECHO_REPLY           0U
#define ICMP_TYPE_ECHO_REQUEST         8U

#define ICMP_CODE_ECHO                 0U

#define ICMP_HEADER_LENGTH             8U

#define ICMP_OFFSET_TYPE               0U
#define ICMP_OFFSET_CODE               1U
#define ICMP_OFFSET_CHECKSUM           2U

#define ICMP_IPV4_HEADER_LENGTH        20U
#define ICMP_IPV4_PROTOCOL             1U
#define ICMP_IPV4_DEFAULT_TTL          64U

#define ICMP_ETHERNET_HEADER_LENGTH    14U


static const uint8_t icmpLocalAddress[4] =
{
    DHCP_SERVER_IP_0,
    DHCP_SERVER_IP_1,
    DHCP_SERVER_IP_2,
    DHCP_SERVER_IP_3,
};


/*
 * 只响应真正发给 MCU 本机地址的 ICMP packet。
 *
 * 广播和组播 Echo Request 当前全部忽略。
 */
static uint8_t ICMP_IsLocalDestination(
    const IPv4PacketView *ipv4Packet)
{
    return
        (uint8_t)(
            memcmp(
                ipv4Packet->destinationAddress,
                icmpLocalAddress,
                4U) == 0);
}


/*
 * 检查整个 ICMP message 的 checksum。
 *
 * checksum 字段本身也参与验证。
 * 正确报文折叠后的结果应为 0xFFFF。
 */
static uint8_t ICMP_IsChecksumValid(
    const uint8_t *icmpPacket,
    uint16_t icmpLength)
{
    uint32_t checksumSum = 0U;

    checksumSum =
        IPv4_ChecksumAdd(
            checksumSum,
            icmpPacket,
            icmpLength);

    return
        IPv4_ChecksumIsValid(
            checksumSum);
}


/*
 * 为已经填好的 ICMP message 生成 checksum。
 *
 * 调用前 checksum 字段必须清零。
 */
static void ICMP_WriteChecksum(
    uint8_t *icmpPacket,
    uint16_t icmpLength)
{
    IPv4_WriteU16(
        &icmpPacket[
            ICMP_OFFSET_CHECKSUM],
        0U);

    uint32_t checksumSum = 0U;

    checksumSum =
        IPv4_ChecksumAdd(
            checksumSum,
            icmpPacket,
            icmpLength);

    IPv4_WriteU16(
        &icmpPacket[
            ICMP_OFFSET_CHECKSUM],
        IPv4_ChecksumGenerate(
            checksumSum));
}


/*
 * 填充无 IPv4 options 的标准 20 字节 IPv4 Header。
 */
static void ICMP_BuildIPv4Header(
    uint8_t *ipv4Reply,
    const IPv4PacketView *request,
    uint16_t ipv4Length)
{
    memset(
        ipv4Reply,
        0,
        ICMP_IPV4_HEADER_LENGTH);

    /*
     * Version = 4
     * IHL     = 5，即 20 字节 Header
     */
    ipv4Reply[0] = 0x45U;

    IPv4_WriteU16(
        &ipv4Reply[2],
        ipv4Length);

    /*
     * 当前不维护 Identification。
     */
    IPv4_WriteU16(
        &ipv4Reply[4],
        0U);

    /*
     * 不分片。
     */
    IPv4_WriteU16(
        &ipv4Reply[6],
        0x4000U);

    ipv4Reply[8] =
        ICMP_IPV4_DEFAULT_TTL;

    ipv4Reply[9] =
        ICMP_IPV4_PROTOCOL;

    /*
     * Source：
     *     MCU 10.24.0.1
     */
    memcpy(
        &ipv4Reply[12],
        icmpLocalAddress,
        4U);

    /*
     * Destination：
     *     Echo Request 的发送方
     */
    memcpy(
        &ipv4Reply[16],
        request->sourceAddress,
        4U);

    /*
     * 计算 IPv4 Header checksum 前必须清零。
     */
    IPv4_WriteU16(
        &ipv4Reply[10],
        0U);

    uint32_t checksumSum = 0U;

    checksumSum =
        IPv4_ChecksumAdd(
            checksumSum,
            ipv4Reply,
            ICMP_IPV4_HEADER_LENGTH);

    IPv4_WriteU16(
        &ipv4Reply[10],
        IPv4_ChecksumGenerate(
            checksumSum));
}


ETHDispatchResult ICMP_Handle(
    ETHManager *manager,
    const IPv4PacketView *ipv4Packet)
{
    if (ipv4Packet->payloadLength <
        ICMP_HEADER_LENGTH)
    {
        return ETH_DISPATCH_DROPPED;
    }

    if (ICMP_IsLocalDestination(
            ipv4Packet) == 0U)
    {
        return ETH_DISPATCH_DROPPED;
    }

    const uint8_t *const request =
        ipv4Packet->payload;

    /*
     * 当前只实现：
     *
     * Type 8，Code 0
     * ICMP Echo Request
     */
    if ((request[
             ICMP_OFFSET_TYPE] !=
         ICMP_TYPE_ECHO_REQUEST) ||
        (request[
             ICMP_OFFSET_CODE] !=
         ICMP_CODE_ECHO))
    {
        return ETH_DISPATCH_DROPPED;
    }

    if (ICMP_IsChecksumValid(
            request,
            ipv4Packet->payloadLength) == 0U)
    {
        return ETH_DISPATCH_DROPPED;
    }

    /*
     * 防止在复制 ICMP payload 之前越过 TX frame。
     */
    if (ipv4Packet->payloadLength >
        (ETH_FRAME_CAPACITY -
         ICMP_ETHERNET_HEADER_LENGTH -
         ICMP_IPV4_HEADER_LENGTH))
    {
        return ETH_DISPATCH_DROPPED;
    }

    /*
     * Echo Reply 单播返回给请求方 MAC。
     */
    uint8_t *const ipv4Reply =
        ETH_TransmitBegin(
            manager,
            ipv4Packet->sourceMAC,
            ETH_TYPE_IPV4);

    if (ipv4Reply == NULL)
    {
        /*
         * TX frame 正忙。
         * 保留当前 RX frame，稍后重新处理。
         */
        return ETH_DISPATCH_DEFERRED;
    }

    uint8_t *const icmpReply =
        &ipv4Reply[
            ICMP_IPV4_HEADER_LENGTH];

    /*
     * Identifier、Sequence Number 和 Payload
     * 全部原样复制。
     */
    memcpy(
        icmpReply,
        request,
        ipv4Packet->payloadLength);

    /*
     * Echo Request 转换为 Echo Reply。
     */
    icmpReply[
        ICMP_OFFSET_TYPE] =
        ICMP_TYPE_ECHO_REPLY;

    icmpReply[
        ICMP_OFFSET_CODE] =
        ICMP_CODE_ECHO;

    ICMP_WriteChecksum(
        icmpReply,
        ipv4Packet->payloadLength);

    const uint16_t ipv4ReplyLength =
        (uint16_t)(
            ICMP_IPV4_HEADER_LENGTH +
            ipv4Packet->payloadLength);

    ICMP_BuildIPv4Header(
        ipv4Reply,
        ipv4Packet,
        ipv4ReplyLength);

    if (ETH_TransmitCommit(
            manager,
            ipv4ReplyLength) == 0U)
    {
        return ETH_DISPATCH_DEFERRED;
    }

    /*
     * 尝试立即启动 EP3。
     *
     * Commit 成功后 Reply 已经不再依赖 RX frame。
     */
    (void)manager->operations->
        transmit(manager);

    return ETH_DISPATCH_HANDLED;
}