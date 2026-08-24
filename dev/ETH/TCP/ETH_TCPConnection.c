#include "ETH_TCPConnection.h"

static uint8_t TCPConnection_IPMatches(
    const uint8_t left[4],
    const uint8_t right[4])
{
    return
        (uint8_t)(
            (left[0] == right[0]) &&
            (left[1] == right[1]) &&
            (left[2] == right[2]) &&
            (left[3] == right[3]));
}

static uint8_t TCPSequence_After(
    uint32_t left,
    uint32_t right)
{
    /* Signed subtraction keeps comparisons valid across sequence wrap. */
    return
        (uint8_t)(
            (int32_t)(left - right) > 0);
}

static uint8_t TCPSequence_Before(
    uint32_t left,
    uint32_t right)
{
    return
        (uint8_t)(
            (int32_t)(left - right) < 0);
}

static void TCPConnection_Copy(
    uint8_t *destination,
    const uint8_t *source,
    uint8_t length)
{
    for (uint8_t index = 0U;
         index < length;
         index++)
    {
        destination[index] = source[index];
    }
}

void TCPConnection_Reset(
    TCPConnection *self)
{
    for (uint8_t index = 0U;
         index < 6U;
         index++)
    {
        self->remoteMAC[index] = 0U;
    }

    for (uint8_t index = 0U;
         index < 4U;
         index++)
    {
        self->remoteIP[index] = 0U;
    }

    self->sendUnacknowledged = 0U;
    self->sendNext = 0U;
    self->receiveNext = 0U;
    self->retransmitDeadline = 0U;
    self->timeWaitDeadline = 0U;
    self->localPort = 0U;
    self->remotePort = 0U;
    self->remoteWindow = 0U;
    self->remoteMSS =
        TCP_DEFAULT_REMOTE_MSS;

    TCPReceiveFIFO_Reset(
        &self->receiveFIFO);
    TCPTransmitFIFO_Reset(
        &self->transmitFIFO);

    self->retransmitCount = 0U;
    self->state =
        TCP_CONNECTION_STATE_CLOSED;
    self->acknowledgmentPending = 0U;
    self->closeRequested = 0U;
    self->remoteFinished = 0U;
    self->resetReceived = 0U;
}

void TCPConnection_BeginConnect(
    TCPConnection *self,
    const uint8_t remoteMAC[6],
    const uint8_t remoteIP[4],
    uint16_t localPort,
    uint16_t remotePort,
    uint32_t initialSequence)
{
    TCPConnection_Reset(self);

    TCPConnection_Copy(
        self->remoteMAC,
        remoteMAC,
        6U);
    TCPConnection_Copy(
        self->remoteIP,
        remoteIP,
        4U);

    self->localPort = localPort;
    self->remotePort = remotePort;
    self->sendUnacknowledged =
        initialSequence;
    self->sendNext = initialSequence;
    self->state =
        TCP_CONNECTION_STATE_SEND_SYN;
}

void TCPConnection_AcceptSYN(
    TCPConnection *self,
    const uint8_t sourceMAC[6],
    const uint8_t sourceIP[4],
    const TCPSegmentView *segment,
    uint32_t initialSequence)
{
    TCPConnection_Reset(self);

    TCPConnection_Copy(
        self->remoteMAC,
        sourceMAC,
        6U);
    TCPConnection_Copy(
        self->remoteIP,
        sourceIP,
        4U);

    self->localPort =
        segment->destinationPort;
    self->remotePort =
        segment->sourcePort;
    self->sendUnacknowledged =
        initialSequence;
    self->sendNext = initialSequence;
    self->receiveNext =
        segment->sequenceNumber + 1U;
    self->remoteWindow =
        segment->windowSize;
    self->state =
        TCP_CONNECTION_STATE_SEND_SYN_ACK;
}

uint8_t TCPConnection_Matches(
    const TCPConnection *self,
    const uint8_t sourceIP[4],
    const TCPSegmentView *segment)
{
    if (self->state ==
        TCP_CONNECTION_STATE_CLOSED)
    {
        return 0U;
    }

    return
        (uint8_t)(
            (TCPConnection_IPMatches(
                 self->remoteIP,
                 sourceIP) != 0U) &&
            (segment->sourcePort ==
             self->remotePort) &&
            (segment->destinationPort ==
             self->localPort));
}

static void TCPConnection_EnterTimeWait(
    TCPConnection *self)
{
    self->state =
        TCP_CONNECTION_STATE_TIME_WAIT;
    self->timeWaitDeadline = 0U;
    self->retransmitDeadline = 0U;
    self->retransmitCount = 0U;
}

static void TCPConnection_HandleAcknowledgment(
    TCPConnection *self,
    uint32_t acknowledgmentNumber)
{
    /* Ignore old ACKs and ACKs for bytes that have never been sent. */
    if ((TCPSequence_After(
             acknowledgmentNumber,
             self->sendUnacknowledged) == 0U) ||
        (TCPSequence_After(
             acknowledgmentNumber,
             self->sendNext) != 0U))
    {
        return;
    }

    const uint32_t acknowledgedLength =
        acknowledgmentNumber -
        self->sendUnacknowledged;

    uint16_t acknowledgedPayload =
        self->transmitFIFO.sentLength;

    if ((uint32_t)acknowledgedPayload >
        acknowledgedLength)
    {
        acknowledgedPayload =
            (uint16_t)acknowledgedLength;
    }

    TCPTransmitFIFO_Acknowledge(
        &self->transmitFIFO,
        acknowledgedPayload);

    self->sendUnacknowledged =
        acknowledgmentNumber;
    self->retransmitDeadline = 0U;
    self->retransmitCount = 0U;
}

static void TCPConnection_HandlePayloadAndFIN(
    TCPConnection *self,
    const TCPSegmentView *segment)
{
    const uint8_t *payload =
        segment->payload;
    uint16_t payloadLength =
        segment->payloadLength;
    uint32_t sequenceNumber =
        segment->sequenceNumber;

    /*
     * Trim a retransmitted prefix.  This permits a segment which begins
     * before receiveNext but also carries new contiguous bytes.
     */
    if (TCPSequence_Before(
            sequenceNumber,
            self->receiveNext) != 0U)
    {
        /* Even a fully duplicate segment should receive the current ACK. */
        self->acknowledgmentPending = 1U;

        const uint32_t duplicateLength =
            self->receiveNext -
            sequenceNumber;

        if (duplicateLength >=
            payloadLength)
        {
            payload += payloadLength;
            payloadLength = 0U;
            sequenceNumber =
                self->receiveNext;
        }
        else
        {
            payload += duplicateLength;
            payloadLength -=
                (uint16_t)duplicateLength;
            sequenceNumber +=
                duplicateLength;
        }
    }

    /*
     * Stall-on-OOO policy: future data is not copied or remembered.
     * Sending the current ACK makes the peer retransmit the missing range.
     */
    if (TCPSequence_After(
            sequenceNumber,
            self->receiveNext) != 0U)
    {
        self->acknowledgmentPending = 1U;
        return;
    }

    /*
     * Never acknowledge bytes that could not enter the receive FIFO.
     * Advertising the reduced receive window naturally applies backpressure.
     */
    if (payloadLength >
        TCPReceiveFIFO_GetFreeSize(
            &self->receiveFIFO))
    {
        self->acknowledgmentPending = 1U;
        return;
    }

    if (payloadLength != 0U)
    {
        (void)TCPReceiveFIFO_Write(
            &self->receiveFIFO,
            payload,
            payloadLength);
        self->receiveNext +=
            payloadLength;
        self->acknowledgmentPending = 1U;
    }

    const uint32_t finSequence =
        segment->sequenceNumber +
        segment->payloadLength;

    if ((segment->flags & TCP_FLAG_FIN) == 0U)
    {
        return;
    }

    if (TCPSequence_Before(
            finSequence,
            self->receiveNext) != 0U)
    {
        /* The peer retransmitted a FIN which was already consumed. */
        self->acknowledgmentPending = 1U;

        if (self->state ==
            TCP_CONNECTION_STATE_TIME_WAIT)
        {
            self->timeWaitDeadline = 0U;
        }

        return;
    }

    if (finSequence != self->receiveNext)
    {
        return;
    }

    self->receiveNext++;
    self->remoteFinished = 1U;
    self->acknowledgmentPending = 1U;

    if (self->state ==
        TCP_CONNECTION_STATE_ESTABLISHED)
    {
        self->state =
            TCP_CONNECTION_STATE_CLOSE_WAIT;
    }
    else if (self->state ==
             TCP_CONNECTION_STATE_FIN_WAIT_1)
    {
        self->state =
            TCP_CONNECTION_STATE_CLOSING;
    }
    else if (self->state ==
             TCP_CONNECTION_STATE_FIN_WAIT_2)
    {
        TCPConnection_EnterTimeWait(self);
    }
    else if (self->state ==
             TCP_CONNECTION_STATE_TIME_WAIT)
    {
        /* A duplicate FIN restarts TIME_WAIT after its ACK is sent. */
        self->timeWaitDeadline = 0U;
    }
}

void TCPConnection_HandleSegment(
    TCPConnection *self,
    const TCPSegmentView *segment)
{
    if ((segment->flags & TCP_FLAG_RST) != 0U)
    {
        TCPConnection_Reset(self);
        self->resetReceived = 1U;
        return;
    }

    self->remoteWindow =
        segment->windowSize;

    if (self->state ==
        TCP_CONNECTION_STATE_SYN_SENT)
    {
        if (((segment->flags &
              (TCP_FLAG_SYN | TCP_FLAG_ACK)) ==
             (TCP_FLAG_SYN | TCP_FLAG_ACK)) &&
            (segment->acknowledgmentNumber ==
             self->sendNext))
        {
            self->sendUnacknowledged =
                segment->acknowledgmentNumber;
            self->receiveNext =
                segment->sequenceNumber + 1U;
            self->retransmitDeadline = 0U;
            self->retransmitCount = 0U;
            self->state =
                TCP_CONNECTION_STATE_SEND_ACK;
        }

        return;
    }

    if ((segment->flags & TCP_FLAG_ACK) != 0U)
    {
        TCPConnection_HandleAcknowledgment(
            self,
            segment->acknowledgmentNumber);

        if ((self->state ==
             TCP_CONNECTION_STATE_SYN_RECEIVED) &&
            (segment->acknowledgmentNumber ==
             self->sendNext))
        {
            self->state =
                TCP_CONNECTION_STATE_ESTABLISHED;
        }
        else if ((self->state ==
                  TCP_CONNECTION_STATE_FIN_WAIT_1) &&
                 (segment->acknowledgmentNumber ==
                  self->sendNext))
        {
            self->state =
                TCP_CONNECTION_STATE_FIN_WAIT_2;
        }
        else if ((self->state ==
                  TCP_CONNECTION_STATE_CLOSING) &&
                 (segment->acknowledgmentNumber ==
                  self->sendNext))
        {
            TCPConnection_EnterTimeWait(self);
        }
        else if ((self->state ==
                  TCP_CONNECTION_STATE_LAST_ACK) &&
                 (segment->acknowledgmentNumber ==
                  self->sendNext))
        {
            TCPConnection_Reset(self);
            return;
        }
    }

    /* Control-only handshake states do not accept application bytes. */
    if ((self->state == TCP_CONNECTION_STATE_ESTABLISHED) ||
        (self->state == TCP_CONNECTION_STATE_FIN_WAIT_1) ||
        (self->state == TCP_CONNECTION_STATE_FIN_WAIT_2) ||
        (self->state == TCP_CONNECTION_STATE_CLOSE_WAIT) ||
        (self->state == TCP_CONNECTION_STATE_CLOSING) ||
        (self->state == TCP_CONNECTION_STATE_TIME_WAIT))
    {
        TCPConnection_HandlePayloadAndFIN(
            self,
            segment);
    }
}

uint8_t TCPConnection_IsEstablished(
    const TCPConnection *self)
{
    return
        (uint8_t)(
            self->state ==
            TCP_CONNECTION_STATE_ESTABLISHED);
}

uint16_t TCPConnection_Read(
    TCPConnection *self,
    uint8_t *destination,
    uint16_t capacity)
{
    return TCPReceiveFIFO_Read(
        &self->receiveFIFO,
        destination,
        capacity);
}

uint16_t TCPConnection_Write(
    TCPConnection *self,
    const uint8_t *source,
    uint16_t length)
{
    /* TCP remains writable after the peer half-closes its send direction. */
    if ((self->state !=
         TCP_CONNECTION_STATE_ESTABLISHED) &&
        (self->state !=
         TCP_CONNECTION_STATE_CLOSE_WAIT))
    {
        return 0U;
    }

    return TCPTransmitFIFO_Write(
        &self->transmitFIFO,
        source,
        length);
}
