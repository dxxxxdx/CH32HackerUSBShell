#include "ETH_TCPFrameBuilder.h"

#include "../ETH_Dispatcher.h"
#include "../ETH_Transmitter.h"
#include "../IPv4.h"
#include "ETH_TCPSegment.h"

#include <stddef.h>

static void TCPFrameBuilder_Copy(
    uint8_t *destination,
    const uint8_t *source,
    uint16_t length)
{
    for (uint16_t index = 0U;
         index < length;
         index++)
    {
        destination[index] = source[index];
    }
}

uint8_t TCPFrameBuilder_Begin(
    TCPFrameBuilder *self,
    ETHManager *ethernetManager,
    const TCPFrameBuildRequest *request)
{
    const uint16_t tcpSegmentLength =
        TCP_FRAME_TCP_HEADER_SIZE +
        request->payloadLength;

    const uint16_t ipv4PacketLength =
        TCP_IPV4_HEADER_SIZE +
        tcpSegmentLength;

    /*
     * Validate all lengths before borrowing the one shared TX frame.
     * Once Begin succeeds, Commit must happen in this same call path.
     */
    if (ipv4PacketLength >
        (ETH_FRAME_CAPACITY -
         ETH_HEADER_LENGTH))
    {
        return 0U;
    }

    uint8_t *const ipv4Header =
        ETH_TransmitBegin(
            ethernetManager,
            request->destinationMAC,
            ETH_TYPE_IPV4);

    if (ipv4Header == NULL)
    {
        return 0U;
    }

    uint8_t *const tcpHeader =
        &ipv4Header[TCP_IPV4_HEADER_SIZE];

    /* A minimal, unfragmented IPv4 header. */
    ipv4Header[0] = 0x45U;
    ipv4Header[1] = 0U;
    IPv4_WriteU16(
        &ipv4Header[2],
        ipv4PacketLength);
    IPv4_WriteU16(
        &ipv4Header[4],
        request->identification);
    IPv4_WriteU16(
        &ipv4Header[6],
        0x4000U); /* DF: this stack does not reassemble fragments. */
    ipv4Header[8] = request->ttl;
    ipv4Header[9] = TCP_PROTOCOL_NUMBER;
    ipv4Header[10] = 0U;
    ipv4Header[11] = 0U;

    TCPFrameBuilder_Copy(
        &ipv4Header[12],
        request->sourceIP,
        4U);
    TCPFrameBuilder_Copy(
        &ipv4Header[16],
        request->destinationIP,
        4U);

    /* A minimal TCP header.  Incoming TCP options are accepted but ignored. */
    IPv4_WriteU16(
        &tcpHeader[0],
        request->sourcePort);
    IPv4_WriteU16(
        &tcpHeader[2],
        request->destinationPort);
    IPv4_WriteU32(
        &tcpHeader[4],
        request->sequenceNumber);
    IPv4_WriteU32(
        &tcpHeader[8],
        request->acknowledgmentNumber);
    tcpHeader[12] = 0x50U; /* Five 32-bit words, no options. */
    tcpHeader[13] = request->flags;
    IPv4_WriteU16(
        &tcpHeader[14],
        request->windowSize);
    tcpHeader[16] = 0U;
    tcpHeader[17] = 0U;
    tcpHeader[18] = 0U;
    tcpHeader[19] = 0U;

    self->ethernetManager =
        ethernetManager;
    self->ipv4Header = ipv4Header;
    self->tcpSegment = tcpHeader;
    self->payload =
        &tcpHeader[TCP_FRAME_TCP_HEADER_SIZE];
    self->sourceIP = request->sourceIP;
    self->destinationIP =
        request->destinationIP;
    self->ipv4PacketLength =
        ipv4PacketLength;
    self->tcpSegmentLength =
        tcpSegmentLength;

    return 1U;
}

uint8_t TCPFrameBuilder_Commit(
    TCPFrameBuilder *self)
{
    const uint16_t tcpChecksum =
        TCPSegment_CalculateChecksum(
            self->sourceIP,
            self->destinationIP,
            self->tcpSegment,
            self->tcpSegmentLength);

    IPv4_WriteU16(
        &self->tcpSegment[16],
        tcpChecksum);

    uint32_t ipv4Checksum = 0U;

    ipv4Checksum = IPv4_ChecksumAdd(
        ipv4Checksum,
        self->ipv4Header,
        TCP_IPV4_HEADER_SIZE);

    IPv4_WriteU16(
        &self->ipv4Header[10],
        IPv4_ChecksumGenerate(
            ipv4Checksum));

    if (ETH_TransmitCommit(
            self->ethernetManager,
            self->ipv4PacketLength) == 0U)
    {
        return 0U;
    }

    /*
     * Match ARP/ICMP/UDP behavior: kick once immediately.  A committed
     * frame remains pending and ETH_TransmitService() retries it later.
     */
    (void)self->ethernetManager->operations->
        transmit(self->ethernetManager);

    return 1U;
}
