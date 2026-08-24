#include "UDP.h"


#include <string.h>

#include "DHCP.h"
#include "ETH_Transmitter.h"
#include "IPv4.h"

#define DHCP_FIXED_HEADER_LENGTH       240U
#define DHCP_MINIMUM_MESSAGE_LENGTH    300U

#define DHCP_OFFSET_OPERATION          0U
#define DHCP_OFFSET_HARDWARE_TYPE      1U
#define DHCP_OFFSET_HARDWARE_LENGTH    2U
#define DHCP_OFFSET_TRANSACTION_ID     4U
#define DHCP_OFFSET_YOUR_ADDRESS       16U
#define DHCP_OFFSET_SERVER_ADDRESS     20U
#define DHCP_OFFSET_CLIENT_HARDWARE    28U
#define DHCP_OFFSET_MAGIC_COOKIE       236U
#define DHCP_OFFSET_OPTIONS            240U

#define DHCP_OPERATION_REQUEST         1U
#define DHCP_OPERATION_REPLY           2U

#define DHCP_HARDWARE_ETHERNET         1U
#define DHCP_HARDWARE_LENGTH           6U

#define DHCP_OPTION_SUBNET_MASK        1U
#define DHCP_OPTION_BROADCAST_ADDRESS  28U
#define DHCP_OPTION_REQUESTED_IP       50U
#define DHCP_OPTION_LEASE_TIME         51U
#define DHCP_OPTION_MESSAGE_TYPE       53U
#define DHCP_OPTION_SERVER_IDENTIFIER  54U
#define DHCP_OPTION_RENEWAL_TIME       58U
#define DHCP_OPTION_REBINDING_TIME     59U
#define DHCP_OPTION_PAD                0U
#define DHCP_OPTION_END                255U

#define IPV4_HEADER_LENGTH             20U
#define UDP_HEADER_LENGTH              8U

#define IPV4_PROTOCOL_UDP              17U
#define IPV4_DEFAULT_TTL               64U

#define UDP_DHCP_SERVER_PORT           67U
#define UDP_DHCP_CLIENT_PORT           68U


typedef struct
{
    uint8_t messageType;

    /*
     * DHCP Request 可能通过 Option 50 指定请求地址。
     */
    uint8_t requestedAddress[4];
    uint8_t requestedAddressPresent;
} DHCPRequestInfo;


static const uint8_t dhcpBroadcastMAC[
    ETH_MAC_LENGTH] =
{
    0xFFU, 0xFFU, 0xFFU,
    0xFFU, 0xFFU, 0xFFU
};


static void DHCP_WriteServerAddress(
    uint8_t *destination)
{
    destination[0] = DHCP_SERVER_IP_0;
    destination[1] = DHCP_SERVER_IP_1;
    destination[2] = DHCP_SERVER_IP_2;
    destination[3] = DHCP_SERVER_IP_3;
}


static void DHCP_WriteClientAddress(
    uint8_t *destination)
{
    destination[0] = DHCP_CLIENT_IP_0;
    destination[1] = DHCP_CLIENT_IP_1;
    destination[2] = DHCP_CLIENT_IP_2;
    destination[3] = DHCP_CLIENT_IP_3;
}


static uint8_t DHCP_AddressIsOfferedAddress(
    const uint8_t *address)
{
    return
        (address[0] == DHCP_CLIENT_IP_0) &&
        (address[1] == DHCP_CLIENT_IP_1) &&
        (address[2] == DHCP_CLIENT_IP_2) &&
        (address[3] == DHCP_CLIENT_IP_3);
}


/*
 * 扫描 DHCP options。
 *
 * 当前只提取：
 *   Option 53：DHCP message type
 *   Option 50：Requested IP Address
 */
static uint8_t DHCP_ParseOptions(
    const uint8_t *options,
    uint16_t optionsLength,
    DHCPRequestInfo *request)
{
    uint16_t offset = 0U;

    request->messageType = 0U;
    request->requestedAddressPresent = 0U;

    while (offset < optionsLength)
    {
        const uint8_t option = options[offset++];

        if (option == DHCP_OPTION_PAD)
        {
            continue;
        }

        if (option == DHCP_OPTION_END)
        {
            break;
        }

        /*
         * 普通 option 后面至少还要有一个长度字节。
         */
        if (offset >= optionsLength)
        {
            return 0U;
        }

        const uint8_t optionLength =
            options[offset++];

        if ((uint16_t)(
                offset +
                optionLength) >
            optionsLength)
        {
            return 0U;
        }

        switch (option)
        {
        case DHCP_OPTION_MESSAGE_TYPE:
            if (optionLength == 1U)
            {
                request->messageType =
                    options[offset];
            }
            break;

        case DHCP_OPTION_REQUESTED_IP:
            if (optionLength == 4U)
            {
                memcpy(
                    request->requestedAddress,
                    &options[offset],
                    4U);

                request->requestedAddressPresent =
                    1U;
            }
            break;

        default:
            /*
             * Parameter Request List、Host Name、
             * Client Identifier 等暂时不需要处理。
             */
            break;
        }

        offset =
            (uint16_t)(
                offset +
                optionLength);
    }

    return
        request->messageType != 0U;
}


static uint8_t *DHCP_AppendOptionU8(
    uint8_t *option,
    uint8_t type,
    uint8_t value)
{
    option[0] = type;
    option[1] = 1U;
    option[2] = value;

    return &option[3];
}


static uint8_t *DHCP_AppendOptionU32(
    uint8_t *option,
    uint8_t type,
    uint32_t value)
{
    option[0] = type;
    option[1] = 4U;

    IPv4_WriteU32(
        &option[2],
        value);

    return &option[6];
}


static uint8_t *DHCP_AppendOptionAddress(
    uint8_t *option,
    uint8_t type,
    const uint8_t address[4])
{
    option[0] = type;
    option[1] = 4U;

    memcpy(
        &option[2],
        address,
        4U);

    return &option[6];
}


/*
 * 创建 DHCP Offer 或 ACK。
 *
 * 返回完整 IPv4 packet 的长度。
 */
static uint16_t DHCP_BuildReply(
    uint8_t *ipv4Reply,
    const uint8_t *request,
    uint8_t replyMessageType)
{
    uint8_t *const udpReply =
        &ipv4Reply[IPV4_HEADER_LENGTH];

    uint8_t *const dhcpReply =
        &udpReply[UDP_HEADER_LENGTH];

    /*
     * BOOTP/DHCP 固定头部全部清零。
     *
     * 这样 ciaddr、giaddr、sname、file 等当前
     * 不使用的字段自然保持为零。
     */
    memset(
        dhcpReply,
        0,
        DHCP_FIXED_HEADER_LENGTH);

    dhcpReply[DHCP_OFFSET_OPERATION] =
        DHCP_OPERATION_REPLY;

    dhcpReply[DHCP_OFFSET_HARDWARE_TYPE] =
        DHCP_HARDWARE_ETHERNET;

    dhcpReply[DHCP_OFFSET_HARDWARE_LENGTH] =
        DHCP_HARDWARE_LENGTH;

    /*
     * xid 必须原样返回，主机依靠它匹配请求和响应。
     */
    memcpy(
        &dhcpReply[
            DHCP_OFFSET_TRANSACTION_ID],
        &request[
            DHCP_OFFSET_TRANSACTION_ID],
        4U);

    /*
     * 固定租给主机 10.24.0.2。
     */
    DHCP_WriteClientAddress(
        &dhcpReply[
            DHCP_OFFSET_YOUR_ADDRESS]);

    /*
     * DHCP server 是 MCU 自己：10.24.0.1。
     */
    DHCP_WriteServerAddress(
        &dhcpReply[
            DHCP_OFFSET_SERVER_ADDRESS]);

    /*
     * 原样返回客户端 MAC。
     */
    memcpy(
        &dhcpReply[
            DHCP_OFFSET_CLIENT_HARDWARE],
        &request[
            DHCP_OFFSET_CLIENT_HARDWARE],
        DHCP_HARDWARE_LENGTH);

    dhcpReply[
        DHCP_OFFSET_MAGIC_COOKIE + 0U] =
        0x63U;

    dhcpReply[
        DHCP_OFFSET_MAGIC_COOKIE + 1U] =
        0x82U;

    dhcpReply[
        DHCP_OFFSET_MAGIC_COOKIE + 2U] =
        0x53U;

    dhcpReply[
        DHCP_OFFSET_MAGIC_COOKIE + 3U] =
        0x63U;

    uint8_t *option =
        &dhcpReply[
            DHCP_OFFSET_OPTIONS];

    /*
     * DHCP Message Type。
     */
    option =
        DHCP_AppendOptionU8(
            option,
            DHCP_OPTION_MESSAGE_TYPE,
            replyMessageType);

    static const uint8_t serverAddress[4] =
    {
        DHCP_SERVER_IP_0,
        DHCP_SERVER_IP_1,
        DHCP_SERVER_IP_2,
        DHCP_SERVER_IP_3
    };

    static const uint8_t subnetMask[4] =
    {
        DHCP_SUBNET_MASK_0,
        DHCP_SUBNET_MASK_1,
        DHCP_SUBNET_MASK_2,
        DHCP_SUBNET_MASK_3
    };

    static const uint8_t broadcastAddress[4] =
    {
        DHCP_SERVER_IP_0,
        DHCP_SERVER_IP_1,
        DHCP_SERVER_IP_2,
        255U
    };

    /*
     * 告诉主机这份租约由哪个 DHCP server 发出。
     */
    option =
        DHCP_AppendOptionAddress(
            option,
            DHCP_OPTION_SERVER_IDENTIFIER,
            serverAddress);

    option =
        DHCP_AppendOptionAddress(
            option,
            DHCP_OPTION_SUBNET_MASK,
            subnetMask);

    option =
        DHCP_AppendOptionAddress(
            option,
            DHCP_OPTION_BROADCAST_ADDRESS,
            broadcastAddress);

    option =
        DHCP_AppendOptionU32(
            option,
            DHCP_OPTION_LEASE_TIME,
            DHCP_LEASE_TIME);

    /*
     * T1 = 12 小时。
     */
    option =
        DHCP_AppendOptionU32(
            option,
            DHCP_OPTION_RENEWAL_TIME,
            DHCP_LEASE_TIME / 2U);

    /*
     * T2 = 21 小时。
     */
    option =
        DHCP_AppendOptionU32(
            option,
            DHCP_OPTION_REBINDING_TIME,
            (DHCP_LEASE_TIME * 7U) / 8U);

    *option++ = DHCP_OPTION_END;

    uint16_t dhcpLength =
        (uint16_t)(
            option -
            dhcpReply);

    /*
     * BOOTP/DHCP 客户端通常按最小 300 字节报文准备接收。
     *
     * END option 后补 PAD，不改变 options 的含义。
     */
    if (dhcpLength <
        DHCP_MINIMUM_MESSAGE_LENGTH)
    {
        memset(
            &dhcpReply[dhcpLength],
            DHCP_OPTION_PAD,
            DHCP_MINIMUM_MESSAGE_LENGTH -
            dhcpLength);

        dhcpLength =
            DHCP_MINIMUM_MESSAGE_LENGTH;
    }

    const uint16_t udpLength =
        (uint16_t)(
            UDP_HEADER_LENGTH +
            dhcpLength);

    const uint16_t ipv4Length =
        (uint16_t)(
            IPV4_HEADER_LENGTH +
            udpLength);

    /*
     * UDP header。
     */
    IPv4_WriteU16(
        &udpReply[0],
        UDP_DHCP_SERVER_PORT);

    IPv4_WriteU16(
        &udpReply[2],
        UDP_DHCP_CLIENT_PORT);

    IPv4_WriteU16(
        &udpReply[4],
        udpLength);

    /*
     * IPv4 允许 UDP checksum 为零。
     * 第一版先不计算 UDP checksum。
     */
    IPv4_WriteU16(
        &udpReply[6],
        0U);

    /*
     * IPv4 header。
     */
    memset(
        ipv4Reply,
        0,
        IPV4_HEADER_LENGTH);

    ipv4Reply[0] = 0x45U;
    ipv4Reply[1] = 0U;

    IPv4_WriteU16(
        &ipv4Reply[2],
        ipv4Length);

    /*
     * Identification 暂时为零。
     */
    IPv4_WriteU16(
        &ipv4Reply[4],
        0U);

    /*
     * 设置 Don't Fragment。
     */
    IPv4_WriteU16(
        &ipv4Reply[6],
        0x4000U);

    ipv4Reply[8] =
        IPV4_DEFAULT_TTL;

    ipv4Reply[9] =
        IPV4_PROTOCOL_UDP;

    DHCP_WriteServerAddress(
        &ipv4Reply[12]);

    /*
     * 客户端此时可能还没有正式配置地址，
     * 因此 Offer 和 ACK 都广播到 255.255.255.255。
     */
    ipv4Reply[16] = 255U;
    ipv4Reply[17] = 255U;
    ipv4Reply[18] = 255U;
    ipv4Reply[19] = 255U;

    /*
     * 计算校验和之前，checksum 字段必须清零。
     */
    IPv4_WriteU16(
        &ipv4Reply[10],
        0U);

    uint32_t checksumSum = 0U;

    checksumSum =
        IPv4_ChecksumAdd(
            checksumSum,
            ipv4Reply,
            IPV4_HEADER_LENGTH);

    IPv4_WriteU16(
        &ipv4Reply[10],
        IPv4_ChecksumGenerate(
            checksumSum));

    return ipv4Length;
}


/*
 * DHCP 使用 UDP 67/68。
 *
 * 当前实现只有一个固定租约：
 *
 *   MCU:  10.24.0.1
 *   Host: 10.24.0.2
 */
static ETHDispatchResult UDP_HandleDHCP(
    ETHManager *manager,
    const IPv4PacketView *ipv4Packet,
    const UDPPacketView *udpPacket)
{
    (void)ipv4Packet;

    if (udpPacket->payloadLength <
        DHCP_FIXED_HEADER_LENGTH)
    {
        return ETH_DISPATCH_DROPPED;
    }

    const uint8_t *const request =
        udpPacket->payload;

    /*
     * 这里只接受 Ethernet 上的 BOOTP request。
     */
    if ((request[
             DHCP_OFFSET_OPERATION] !=
         DHCP_OPERATION_REQUEST) ||
        (request[
             DHCP_OFFSET_HARDWARE_TYPE] !=
         DHCP_HARDWARE_ETHERNET) ||
        (request[
             DHCP_OFFSET_HARDWARE_LENGTH] !=
         DHCP_HARDWARE_LENGTH))
    {
        return ETH_DISPATCH_DROPPED;
    }

    /*
     * 检查 DHCP magic cookie：
     *
     * 63 82 53 63
     */
    if ((request[
             DHCP_OFFSET_MAGIC_COOKIE + 0U] !=
         0x63U) ||
        (request[
             DHCP_OFFSET_MAGIC_COOKIE + 1U] !=
         0x82U) ||
        (request[
             DHCP_OFFSET_MAGIC_COOKIE + 2U] !=
         0x53U) ||
        (request[
             DHCP_OFFSET_MAGIC_COOKIE + 3U] !=
         0x63U))
    {
        return ETH_DISPATCH_DROPPED;
    }

    DHCPRequestInfo requestInfo;

    if (DHCP_ParseOptions(
            &request[
                DHCP_OFFSET_OPTIONS],
            (uint16_t)(
                udpPacket->payloadLength -
                DHCP_OFFSET_OPTIONS),
            &requestInfo) == 0U)
    {
        return ETH_DISPATCH_DROPPED;
    }

    uint8_t replyMessageType;

    switch (requestInfo.messageType)
    {
    case DHCP_MESSAGE_DISCOVER:
        replyMessageType =
            DHCP_MESSAGE_OFFER;
        break;

    case DHCP_MESSAGE_REQUEST:
        /*
         * 如果主机明确请求了另一个地址，
         * 当前固定地址 DHCP server 不处理它。
         */
        if ((requestInfo
                 .requestedAddressPresent != 0U) &&
            (DHCP_AddressIsOfferedAddress(
                 requestInfo.requestedAddress) == 0U))
        {
            return ETH_DISPATCH_DROPPED;
        }

        replyMessageType =
            DHCP_MESSAGE_ACK;
        break;

    default:
        /*
         * Decline、Release、Inform 暂时不处理。
         */
        return ETH_DISPATCH_DROPPED;
    }

    /*
     * TX 正忙时不释放当前 RX frame。
     *
     * 等主循环再次调用分发器以后，
     * 会重新处理同一个 DHCP request。
     */
    uint8_t *const ipv4Reply =
        ETH_TransmitBegin(
            manager,
            dhcpBroadcastMAC,
            ETH_TYPE_IPV4);

    if (ipv4Reply == NULL)
    {
        return ETH_DISPATCH_DEFERRED;
    }

    const uint16_t ipv4ReplyLength =
        DHCP_BuildReply(
            ipv4Reply,
            request,
            replyMessageType);

    if (ETH_TransmitCommit(
            manager,
            ipv4ReplyLength) == 0U)
    {
        /*
         * 理论上 Begin 成功以后 Commit 不应失败。
         * 若失败，不能声称已经处理完成。
         */
        return ETH_DISPATCH_DEFERRED;
    }

    /*
     * 尝试立即踢出第一包。
     *
     * 如果 USB 暂时忙，完整回复已经保存在 TX frame，
     * 主循环后续继续调用 transmit() 即可。
     *
     * RX request 此时可以释放，因为回复不再依赖它。
     */
    (void)manager->operations->
        transmit(manager);

    return ETH_DISPATCH_HANDLED;
}









static uint8_t UDP_IsChecksumValid(
    const IPv4PacketView *ipv4Packet,
    const uint8_t *udp,
    uint16_t udpLength)
{
    const uint16_t receivedChecksum =
        IPv4_ReadU16(
            &udp[6]);

    /*
     * IPv4 允许 UDP checksum 为 0。
     */
    if (receivedChecksum == 0U)
    {
        return 1U;
    }

    uint32_t sum = 0U;

    sum = IPv4_ChecksumAdd(
        sum,
        ipv4Packet->sourceAddress,
        4U);

    sum = IPv4_ChecksumAdd(
        sum,
        ipv4Packet->destinationAddress,
        4U);

    /*
     * Pseudo Header：
     *
     * Zero + Protocol
     * UDP Length
     */
    sum += IPV4_PROTOCOL_UDP;
    sum += udpLength;

    sum = IPv4_ChecksumAdd(
        sum,
        udp,
        udpLength);

    return
        IPv4_ChecksumIsValid(sum);
}




/*
 * 其他 UDP 服务预留入口。
 */
static ETHDispatchResult UDP_HandleOther(
    ETHManager *manager,
    const IPv4PacketView *ipv4Packet,
    const UDPPacketView *udpPacket)
{
    (void)manager;
    (void)ipv4Packet;
    (void)udpPacket;

    return ETH_DISPATCH_DROPPED;
}


ETHDispatchResult UDP_Handle(
    ETHManager *manager,
    const IPv4PacketView *ipv4Packet)
{
    const uint8_t *const udp =
        ipv4Packet->payload;

    if (ipv4Packet->payloadLength <
        UDP_HEADER_LENGTH)
    {
        return ETH_DISPATCH_DROPPED;
    }

    const uint16_t udpLength =
        IPv4_ReadU16(
            &udp[4]);

    if ((udpLength <
         UDP_HEADER_LENGTH) ||
        (udpLength !=
         ipv4Packet->payloadLength))
    {
        return ETH_DISPATCH_DROPPED;
    }

    if (UDP_IsChecksumValid(
            ipv4Packet,
            udp,
            udpLength) == 0U)
    {
        return ETH_DISPATCH_DROPPED;
    }

    const UDPPacketView packet =
    {
        .sourcePort =
            IPv4_ReadU16(
                &udp[0]),

        .destinationPort =
            IPv4_ReadU16(
                &udp[2]),

        .header =
            udp,

        .payload =
            &udp[
                UDP_HEADER_LENGTH],

        .payloadLength =
            (uint16_t)(
                udpLength -
                UDP_HEADER_LENGTH),

        .length =
            udpLength
    };

    if ((packet.sourcePort ==
         UDP_DHCP_CLIENT_PORT) &&
        (packet.destinationPort ==
         UDP_DHCP_SERVER_PORT))
    {
        return UDP_HandleDHCP(
            manager,
            ipv4Packet,
            &packet);
    }

    return UDP_HandleOther(
        manager,
        ipv4Packet,
        &packet);
}