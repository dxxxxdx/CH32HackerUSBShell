//
// Created by dxxdx on 2026/7/23.
//
#ifndef CH32V203C8U_ICMP_H
#define CH32V203C8U_ICMP_H

#include "IPv4.h"


ETHDispatchResult ICMP_Handle(
    ETHManager *manager,
    const IPv4PacketView *ipv4Packet);


#endif /* CH32V203C8U_ICMP_H */