#ifndef CH32V203C8U_UDP_H
#define CH32V203C8U_UDP_H

#include "IPv4.h"


#define UDP_HEADER_LENGTH 8U


typedef struct
{
    uint16_t sourcePort;
    uint16_t destinationPort;

    const uint8_t *header;

    const uint8_t *payload;
    uint16_t payloadLength;

    uint16_t length;
} UDPPacketView;


ETHDispatchResult UDP_Handle(
    ETHManager *manager,
    const IPv4PacketView *ipv4Packet);


#endif /* CH32V203C8U_UDP_H */