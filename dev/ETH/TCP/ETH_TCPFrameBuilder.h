#ifndef ETH_TCP_FRAME_BUILDER_H
#define ETH_TCP_FRAME_BUILDER_H

#include "../ETH.h"
#include "ETH_TCPTypes.h"

#define TCP_IPV4_HEADER_SIZE                    20U
#define TCP_FRAME_TCP_HEADER_SIZE               20U

typedef struct TCPFrameBuildRequest
{
    const uint8_t *destinationMAC;
    const uint8_t *sourceIP;
    const uint8_t *destinationIP;

    uint32_t sequenceNumber;
    uint32_t acknowledgmentNumber;

    uint16_t sourcePort;
    uint16_t destinationPort;
    uint16_t windowSize;
    uint16_t payloadLength;
    uint16_t identification;

    uint8_t flags;
    uint8_t ttl;
} TCPFrameBuildRequest;

typedef struct TCPFrameBuilder
{
    ETHManager *ethernetManager;

    uint8_t *ipv4Header;
    uint8_t *tcpSegment;
    uint8_t *payload;

    const uint8_t *sourceIP;
    const uint8_t *destinationIP;

    uint16_t ipv4PacketLength;
    uint16_t tcpSegmentLength;
} TCPFrameBuilder;

/*
 * Borrow the shared Ethernet TX frame and write the IPv4/TCP headers.
 * The Ethernet header is deliberately owned by ETH_TransmitBegin().
 * A zero return means that the shared TX frame is still busy; no state
 * machine state may be advanced in that case.
 */
uint8_t TCPFrameBuilder_Begin(
    TCPFrameBuilder *self,
    ETHManager *ethernetManager,
    const TCPFrameBuildRequest *request);

/*
 * Finish both checksums, publish the frame, and kick the link once.
 * ETH_TransmitService() in the main loop remains responsible for retrying
 * the lower-level submission when USB is temporarily busy.
 */
uint8_t TCPFrameBuilder_Commit(
    TCPFrameBuilder *self);

#endif
