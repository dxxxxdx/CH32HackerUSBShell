#include "ETH_TCPFIFO.h"

static uint16_t TCPFIFO_Advance(
    uint16_t offset,
    uint16_t amount,
    uint16_t capacity)
{
    offset += amount;

    while (offset >= capacity)
    {
        offset -= capacity;
    }

    return offset;
}

static uint16_t TCPFIFO_Minimum(
    uint16_t left,
    uint16_t right)
{
    if (left < right)
    {
        return left;
    }

    return right;
}

void TCPReceiveFIFO_Reset(
    TCPReceiveFIFO *self)
{
    self->readOffset = 0U;
    self->writeOffset = 0U;
    self->length = 0U;
}

uint16_t TCPReceiveFIFO_GetFreeSize(
    const TCPReceiveFIFO *self)
{
    return
        TCP_RECEIVE_BUFFER_SIZE -
        self->length;
}

uint16_t TCPReceiveFIFO_Write(
    TCPReceiveFIFO *self,
    const uint8_t *source,
    uint16_t length)
{
    const uint16_t writeLength =
        TCPFIFO_Minimum(
            length,
            TCPReceiveFIFO_GetFreeSize(
                self));

    for (uint16_t index = 0U;
         index < writeLength;
         index++)
    {
        self->data[self->writeOffset] =
            source[index];

        self->writeOffset =
            TCPFIFO_Advance(
                self->writeOffset,
                1U,
                TCP_RECEIVE_BUFFER_SIZE);
    }

    self->length += writeLength;

    return writeLength;
}

uint16_t TCPReceiveFIFO_Read(
    TCPReceiveFIFO *self,
    uint8_t *destination,
    uint16_t capacity)
{
    const uint16_t readLength =
        TCPFIFO_Minimum(
            capacity,
            self->length);

    for (uint16_t index = 0U;
         index < readLength;
         index++)
    {
        destination[index] =
            self->data[self->readOffset];

        self->readOffset =
            TCPFIFO_Advance(
                self->readOffset,
                1U,
                TCP_RECEIVE_BUFFER_SIZE);
    }

    self->length -= readLength;

    return readLength;
}

void TCPTransmitFIFO_Reset(
    TCPTransmitFIFO *self)
{
    self->readOffset = 0U;
    self->writeOffset = 0U;
    self->length = 0U;
    self->sentLength = 0U;
}

uint16_t TCPTransmitFIFO_GetFreeSize(
    const TCPTransmitFIFO *self)
{
    return
        TCP_TRANSMIT_BUFFER_SIZE -
        self->length;
}

uint16_t TCPTransmitFIFO_GetUnsentLength(
    const TCPTransmitFIFO *self)
{
    return
        self->length -
        self->sentLength;
}

uint16_t TCPTransmitFIFO_Write(
    TCPTransmitFIFO *self,
    const uint8_t *source,
    uint16_t length)
{
    const uint16_t writeLength =
        TCPFIFO_Minimum(
            length,
            TCPTransmitFIFO_GetFreeSize(
                self));

    for (uint16_t index = 0U;
         index < writeLength;
         index++)
    {
        self->data[self->writeOffset] =
            source[index];

        self->writeOffset =
            TCPFIFO_Advance(
                self->writeOffset,
                1U,
                TCP_TRANSMIT_BUFFER_SIZE);
    }

    self->length += writeLength;

    return writeLength;
}

uint16_t TCPTransmitFIFO_Copy(
    const TCPTransmitFIFO *self,
    uint16_t relativeOffset,
    uint8_t *destination,
    uint16_t capacity)
{
    if (relativeOffset >= self->length)
    {
        return 0U;
    }

    const uint16_t available =
        self->length - relativeOffset;

    const uint16_t copyLength =
        TCPFIFO_Minimum(
            capacity,
            available);

    uint16_t sourceOffset =
        TCPFIFO_Advance(
            self->readOffset,
            relativeOffset,
            TCP_TRANSMIT_BUFFER_SIZE);

    for (uint16_t index = 0U;
         index < copyLength;
         index++)
    {
        destination[index] =
            self->data[sourceOffset];

        sourceOffset =
            TCPFIFO_Advance(
                sourceOffset,
                1U,
                TCP_TRANSMIT_BUFFER_SIZE);
    }

    return copyLength;
}

void TCPTransmitFIFO_MarkSent(
    TCPTransmitFIFO *self,
    uint16_t length)
{
    const uint16_t unsentLength =
        TCPTransmitFIFO_GetUnsentLength(
            self);

    if (length > unsentLength)
    {
        length = unsentLength;
    }

    self->sentLength += length;
}

void TCPTransmitFIFO_Acknowledge(
    TCPTransmitFIFO *self,
    uint16_t length)
{
    if (length > self->sentLength)
    {
        length = self->sentLength;
    }

    self->readOffset =
        TCPFIFO_Advance(
            self->readOffset,
            length,
            TCP_TRANSMIT_BUFFER_SIZE);

    self->length -= length;
    self->sentLength -= length;
}

