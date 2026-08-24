#ifndef ETH_TCP_SEGMENT_H
#define ETH_TCP_SEGMENT_H

#include "ETH_TCPTypes.h"

uint8_t TCPSegment_Parse(
    TCPSegmentView *view,
    const uint8_t *segment,
    uint16_t segmentLength);

uint8_t TCPSegment_VerifyChecksum(
    const uint8_t sourceIP[4],
    const uint8_t destinationIP[4],
    const uint8_t *segment,
    uint16_t segmentLength);

uint16_t TCPSegment_CalculateChecksum(
    const uint8_t sourceIP[4],
    const uint8_t destinationIP[4],
    const uint8_t *segment,
    uint16_t segmentLength);

#endif
