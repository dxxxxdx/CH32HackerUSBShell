#ifndef ETH_TCP_CONNECTION_H
#define ETH_TCP_CONNECTION_H

#include "ETH_TCPFIFO.h"

typedef struct TCPConnection
{
    uint8_t remoteMAC[6];
    uint8_t remoteIP[4];

    uint32_t sendUnacknowledged;
    uint32_t sendNext;
    uint32_t receiveNext;
    uint32_t retransmitDeadline;
    uint32_t timeWaitDeadline;

    uint16_t localPort;
    uint16_t remotePort;
    uint16_t remoteWindow;
    uint16_t remoteMSS;

    TCPReceiveFIFO receiveFIFO;
    TCPTransmitFIFO transmitFIFO;

    uint8_t retransmitCount;
    uint8_t state;
    uint8_t acknowledgmentPending;
    uint8_t closeRequested;
    uint8_t remoteFinished;
    uint8_t resetReceived;
} TCPConnection;

#define TCP_CONNECTION_INITIALIZER \
{ \
    .remoteMAC = { 0U, 0U, 0U, 0U, 0U, 0U }, \
    .remoteIP = { 0U, 0U, 0U, 0U }, \
    .sendUnacknowledged = 0U, \
    .sendNext = 0U, \
    .receiveNext = 0U, \
    .retransmitDeadline = 0U, \
    .timeWaitDeadline = 0U, \
    .localPort = 0U, \
    .remotePort = 0U, \
    .remoteWindow = 0U, \
    .remoteMSS = TCP_DEFAULT_REMOTE_MSS, \
    .receiveFIFO = TCP_RECEIVE_FIFO_INITIALIZER, \
    .transmitFIFO = TCP_TRANSMIT_FIFO_INITIALIZER, \
    .retransmitCount = 0U, \
    .state = TCP_CONNECTION_STATE_CLOSED, \
    .acknowledgmentPending = 0U, \
    .closeRequested = 0U, \
    .remoteFinished = 0U, \
    .resetReceived = 0U \
}

void TCPConnection_Reset(
    TCPConnection *self);

void TCPConnection_BeginConnect(
    TCPConnection *self,
    const uint8_t remoteMAC[6],
    const uint8_t remoteIP[4],
    uint16_t localPort,
    uint16_t remotePort,
    uint32_t initialSequence);

void TCPConnection_AcceptSYN(
    TCPConnection *self,
    const uint8_t sourceMAC[6],
    const uint8_t sourceIP[4],
    const TCPSegmentView *segment,
    uint32_t initialSequence);

uint8_t TCPConnection_Matches(
    const TCPConnection *self,
    const uint8_t sourceIP[4],
    const TCPSegmentView *segment);

void TCPConnection_HandleSegment(
    TCPConnection *self,
    const TCPSegmentView *segment);

uint8_t TCPConnection_IsEstablished(
    const TCPConnection *self);

uint16_t TCPConnection_Read(
    TCPConnection *self,
    uint8_t *destination,
    uint16_t capacity);

uint16_t TCPConnection_Write(
    TCPConnection *self,
    const uint8_t *source,
    uint16_t length);

#endif
