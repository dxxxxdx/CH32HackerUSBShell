#include "ETH_TCPSegment.h"

static uint16_t TCPSegment_ReadU16(
    const uint8_t *source)
{
    return
        ((uint16_t)source[0] << 8U) |
        (uint16_t)source[1];
}

static uint32_t TCPSegment_ReadU32(
    const uint8_t *source)
{
    return
        ((uint32_t)source[0] << 24U) |
        ((uint32_t)source[1] << 16U) |
        ((uint32_t)source[2] << 8U) |
        (uint32_t)source[3];
}

static uint32_t TCPSegment_ChecksumAdd(
    uint32_t checksum,
    const uint8_t *data,
    uint16_t length)
{
    while (length >= 2U)
    {
        checksum +=
            ((uint16_t)data[0] << 8U) |
            (uint16_t)data[1];

        data += 2U;
        length -= 2U;
    }

    if (length != 0U)
    {
        checksum +=
            (uint16_t)data[0] << 8U;
    }

    return checksum;
}

static uint16_t TCPSegment_ChecksumFold(
    uint32_t checksum)
{
    while ((checksum >> 16U) != 0U)
    {
        checksum =
            (checksum & 0xFFFFU) +
            (checksum >> 16U);
    }

    return (uint16_t)checksum;
}

static uint32_t TCPSegment_AddPseudoHeader(
    uint32_t checksum,
    const uint8_t sourceIP[4],
    const uint8_t destinationIP[4],
    uint16_t segmentLength)
{
    checksum = TCPSegment_ChecksumAdd(
        checksum,
        sourceIP,
        4U);

    checksum = TCPSegment_ChecksumAdd(
        checksum,
        destinationIP,
        4U);

    checksum += TCP_PROTOCOL_NUMBER;
    checksum += segmentLength;

    return checksum;
}

uint8_t TCPSegment_Parse(
    TCPSegmentView *view,
    const uint8_t *segment,
    uint16_t segmentLength)
{
    if (segmentLength <
        TCP_HEADER_MINIMUM_SIZE)
    {
        return 0U;
    }

    const uint8_t headerWords =
        segment[12] >> 4U;

    if (headerWords < 5U)
    {
        return 0U;
    }

    const uint16_t headerLength =
        (uint16_t)headerWords * 4U;

    if ((headerLength >
         TCP_HEADER_MAXIMUM_SIZE) ||
        (headerLength > segmentLength))
    {
        return 0U;
    }

    view->sourcePort =
        TCPSegment_ReadU16(
            &segment[0]);

    view->destinationPort =
        TCPSegment_ReadU16(
            &segment[2]);

    view->sequenceNumber =
        TCPSegment_ReadU32(
            &segment[4]);

    view->acknowledgmentNumber =
        TCPSegment_ReadU32(
            &segment[8]);

    view->headerLength =
        (uint8_t)headerLength;

    view->flags = segment[13];

    view->windowSize =
        TCPSegment_ReadU16(
            &segment[14]);

    view->urgentPointer =
        TCPSegment_ReadU16(
            &segment[18]);

    view->payload =
        &segment[headerLength];

    view->payloadLength =
        segmentLength - headerLength;

    return 1U;
}

uint8_t TCPSegment_VerifyChecksum(
    const uint8_t sourceIP[4],
    const uint8_t destinationIP[4],
    const uint8_t *segment,
    uint16_t segmentLength)
{
    uint32_t checksum = 0U;

    checksum = TCPSegment_AddPseudoHeader(
        checksum,
        sourceIP,
        destinationIP,
        segmentLength);

    checksum = TCPSegment_ChecksumAdd(
        checksum,
        segment,
        segmentLength);

    return
        (uint8_t)(
            TCPSegment_ChecksumFold(
                checksum) == 0xFFFFU);
}

uint16_t TCPSegment_CalculateChecksum(
    const uint8_t sourceIP[4],
    const uint8_t destinationIP[4],
    const uint8_t *segment,
    uint16_t segmentLength)
{
    uint32_t checksum = 0U;

    checksum = TCPSegment_AddPseudoHeader(
        checksum,
        sourceIP,
        destinationIP,
        segmentLength);

    checksum = TCPSegment_ChecksumAdd(
        checksum,
        segment,
        segmentLength);

    return
        (uint16_t)~TCPSegment_ChecksumFold(
            checksum);
}

