#ifndef ETH_TCP_SYN_PROBE_H
#define ETH_TCP_SYN_PROBE_H

#include "ETH_TCPTypes.h"

typedef struct TCPSYNProbe
{
    uint8_t remoteMAC[6];
    uint8_t remoteIP[4];

    uint32_t sendSequence;
    uint32_t resetSequence;
    uint32_t deadline;

    uint16_t localPort;
    uint16_t remotePort;

    uint8_t retryCount;
    uint8_t state;
    uint8_t result;
} TCPSYNProbe;

#define TCP_SYN_PROBE_INITIALIZER \
{ \
    .remoteMAC = { 0U, 0U, 0U, 0U, 0U, 0U }, \
    .remoteIP = { 0U, 0U, 0U, 0U }, \
    .sendSequence = 0U, \
    .resetSequence = 0U, \
    .deadline = 0U, \
    .localPort = TCP_PROBE_LOCAL_PORT, \
    .remotePort = 0U, \
    .retryCount = 0U, \
    .state = TCP_PROBE_STATE_IDLE, \
    .result = TCP_PROBE_RESULT_NONE \
}

void TCPSYNProbe_Reset(
    TCPSYNProbe *self);

uint8_t TCPSYNProbe_Matches(
    const TCPSYNProbe *self,
    const uint8_t sourceIP[4],
    const TCPSegmentView *segment);

void TCPSYNProbe_HandleSegment(
    TCPSYNProbe *self,
    const TCPSegmentView *segment);

#endif
