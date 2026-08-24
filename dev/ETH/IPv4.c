#include "IPv4.h"

#include "ICMP.h"
#include "NetworkManager.h"
#include "TCP/ETH_TCPManager.h"
#include "UDP.h"


/*
 * 除 DF 以外的 Flags 和 Fragment Offset。
 *
 * 当前不支持 IPv4 分片。
 */
#define IPV4_FRAGMENT_REJECT_MASK 0xBFFFU


ETHDispatchResult IPv4_Handle(
    ETHManager *manager,
    const ETHFrameView *ethernetFrame)
{
    const uint8_t *const header =
        ethernetFrame->packet;

    const uint16_t receivedLength =
        ethernetFrame->packetLength;

    if (receivedLength <
        IPV4_MIN_HEADER_LENGTH)
    {
        return ETH_DISPATCH_DROPPED;
    }

    const uint8_t version =
        header[0] >> 4U;

    const uint8_t headerWords =
        header[0] & 0x0FU;

    if ((version != IPV4_VERSION) ||
        (headerWords < 5U))
    {
        return ETH_DISPATCH_DROPPED;
    }

    const uint16_t headerLength =
        (uint16_t)headerWords * 4U;

    if (headerLength >
        receivedLength)
    {
        return ETH_DISPATCH_DROPPED;
    }

    const uint16_t totalLength =
        IPv4_ReadU16(
            &header[2]);

    if ((totalLength < headerLength) ||
        (totalLength > receivedLength))
    {
        return ETH_DISPATCH_DROPPED;
    }

    /*
     * 当前只接受完整、未分片的 IPv4 packet。
     */
    const uint16_t fragmentField =
        IPv4_ReadU16(
            &header[6]);

    if ((fragmentField &
         IPV4_FRAGMENT_REJECT_MASK) != 0U)
    {
        return ETH_DISPATCH_DROPPED;
    }

    if (header[8] == 0U)
    {
        return ETH_DISPATCH_DROPPED;
    }

    uint32_t checksum = 0U;

    checksum = IPv4_ChecksumAdd(
        checksum,
        header,
        headerLength);

    if (IPv4_ChecksumIsValid(
            checksum) == 0U)
    {
        return ETH_DISPATCH_DROPPED;
    }

    const IPv4PacketView packet =
    {
        .sourceMAC =
            ethernetFrame->sourceMAC,

        .destinationMAC =
            ethernetFrame->destinationMAC,

        .sourceAddress =
            &header[12],

        .destinationAddress =
            &header[16],

        .header =
            header,

        .headerLength =
            headerLength,

        .payload =
            &header[headerLength],

        .payloadLength =
            totalLength -
            headerLength,

        .protocol =
            header[9],

        .ttl =
            header[8]
    };

    switch (packet.protocol)
    {
    case IPV4_PROTOCOL_ICMP:
        return ICMP_Handle(
            manager,
            &packet);

    case IPV4_PROTOCOL_TCP:
        /*
         * TCP copies accepted payload into its own FIFO and records any
         * required ACK/RST.  The Ethernet RX frame can therefore be released
         * immediately; TCPManager_Process() sends replies when TX is free.
         */
        if (TCPManager_ReceiveIPv4Segment(
                &networkManager.tcp,
                packet.sourceMAC,
                packet.sourceAddress,
                packet.destinationAddress,
                packet.payload,
                packet.payloadLength) != 0U)
        {
            return ETH_DISPATCH_HANDLED;
        }

        return ETH_DISPATCH_DROPPED;

    case IPV4_PROTOCOL_UDP:
        return UDP_Handle(
            manager,
            &packet);

    default:
        return ETH_DISPATCH_DROPPED;
    }
}
