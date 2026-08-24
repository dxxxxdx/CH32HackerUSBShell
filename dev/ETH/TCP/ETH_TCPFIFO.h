#ifndef ETH_TCP_FIFO_H
#define ETH_TCP_FIFO_H

#include "ETH_TCPTypes.h"

typedef struct TCPReceiveFIFO
{
    uint8_t data[
        TCP_RECEIVE_BUFFER_SIZE];

    uint16_t readOffset;
    uint16_t writeOffset;
    uint16_t length;
} TCPReceiveFIFO;

typedef struct TCPTransmitFIFO
{
    uint8_t data[
        TCP_TRANSMIT_BUFFER_SIZE];

    uint16_t readOffset;
    uint16_t writeOffset;
    uint16_t length;
    uint16_t sentLength;
} TCPTransmitFIFO;

#define TCP_RECEIVE_FIFO_INITIALIZER \
{ \
    .data = { 0U }, \
    .readOffset = 0U, \
    .writeOffset = 0U, \
    .length = 0U \
}

#define TCP_TRANSMIT_FIFO_INITIALIZER \
{ \
    .data = { 0U }, \
    .readOffset = 0U, \
    .writeOffset = 0U, \
    .length = 0U, \
    .sentLength = 0U \
}

void TCPReceiveFIFO_Reset(
    TCPReceiveFIFO *self);

uint16_t TCPReceiveFIFO_GetFreeSize(
    const TCPReceiveFIFO *self);

uint16_t TCPReceiveFIFO_Write(
    TCPReceiveFIFO *self,
    const uint8_t *source,
    uint16_t length);

uint16_t TCPReceiveFIFO_Read(
    TCPReceiveFIFO *self,
    uint8_t *destination,
    uint16_t capacity);

void TCPTransmitFIFO_Reset(
    TCPTransmitFIFO *self);

uint16_t TCPTransmitFIFO_GetFreeSize(
    const TCPTransmitFIFO *self);

uint16_t TCPTransmitFIFO_GetUnsentLength(
    const TCPTransmitFIFO *self);

uint16_t TCPTransmitFIFO_Write(
    TCPTransmitFIFO *self,
    const uint8_t *source,
    uint16_t length);

uint16_t TCPTransmitFIFO_Copy(
    const TCPTransmitFIFO *self,
    uint16_t relativeOffset,
    uint8_t *destination,
    uint16_t capacity);

void TCPTransmitFIFO_MarkSent(
    TCPTransmitFIFO *self,
    uint16_t length);

void TCPTransmitFIFO_Acknowledge(
    TCPTransmitFIFO *self,
    uint16_t length);

#endif
