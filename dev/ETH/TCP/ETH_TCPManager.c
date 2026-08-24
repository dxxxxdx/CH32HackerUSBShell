#include "ETH_TCPManager.h"

#include "../DHCP.h"
#include "ETH_TCPSegment.h"

/* The USB peer is the only Ethernet next hop on this point-to-point link. */
static const uint8_t TCPManager_HostMAC[6] =
{
    0x02U, 0x00U, 0x00U,
    0x00U, 0x00U, 0x01U
};

static const uint8_t TCPManager_LocalIP[4] =
{
    DHCP_SERVER_IP_0,
    DHCP_SERVER_IP_1,
    DHCP_SERVER_IP_2,
    DHCP_SERVER_IP_3
};

static uint8_t TCPManager_IPMatches(
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

static void TCPManager_Copy(
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

static uint32_t TCPManager_AllocateSequence(
    TCPManager *self)
{
    const uint32_t sequence =
        self->sequenceCursor;

    /* Avoid reusing the same ISN for consecutive short transactions. */
    self->sequenceCursor +=
        0x00010001UL;

    return sequence;
}

static void TCPManager_QueueReset(
    TCPManager *self,
    const uint8_t sourceMAC[6],
    const uint8_t sourceIP[4],
    const TCPSegmentView *segment)
{
    TCPResetEvent *const event =
        &self->resetEvent;

    if (event->pending != 0U)
    {
        return;
    }

    TCPManager_Copy(
        event->remoteMAC,
        sourceMAC,
        6U);
    TCPManager_Copy(
        event->remoteIP,
        sourceIP,
        4U);

    event->localPort =
        segment->destinationPort;
    event->remotePort =
        segment->sourcePort;

    /* RFC-style reset construction for a segment without a matching TCB. */
    if ((segment->flags & TCP_FLAG_ACK) != 0U)
    {
        event->sequenceNumber =
            segment->acknowledgmentNumber;
        event->acknowledgmentNumber = 0U;
        event->flags = TCP_FLAG_RST;
    }
    else
    {
        uint32_t consumedLength =
            segment->payloadLength;

        if ((segment->flags & TCP_FLAG_SYN) != 0U)
        {
            consumedLength++;
        }

        if ((segment->flags & TCP_FLAG_FIN) != 0U)
        {
            consumedLength++;
        }

        event->sequenceNumber = 0U;
        event->acknowledgmentNumber =
            segment->sequenceNumber +
            consumedLength;
        event->flags =
            TCP_FLAG_RST | TCP_FLAG_ACK;
    }

    event->pending = 1U;
}

uint8_t TCPManager_ReceiveIPv4Segment(
    TCPManager *self,
    const uint8_t sourceMAC[6],
    const uint8_t sourceIP[4],
    const uint8_t destinationIP[4],
    const uint8_t *segment,
    uint16_t segmentLength)
{
    TCPSegmentView view;

    if (TCPManager_IPMatches(
            destinationIP,
            TCPManager_LocalIP) == 0U)
    {
        return 0U;
    }

    if (TCPSegment_Parse(
            &view,
            segment,
            segmentLength) == 0U)
    {
        return 0U;
    }

    if (TCPSegment_VerifyChecksum(
            sourceIP,
            destinationIP,
            segment,
            segmentLength) == 0U)
    {
        return 0U;
    }

    /* The long-lived connection owns its exact four-tuple first. */
    if (TCPConnection_Matches(
            &self->connection,
            sourceIP,
            &view) != 0U)
    {
        TCPManager_Copy(
            self->connection.remoteMAC,
            sourceMAC,
            6U);
        TCPConnection_HandleSegment(
            &self->connection,
            &view);
        return 1U;
    }

    /* The SYN scanner has its own local port and does not disturb Telnet. */
    if (TCPSYNProbe_Matches(
            &self->probe,
            sourceIP,
            &view) != 0U)
    {
        TCPManager_Copy(
            self->probe.remoteMAC,
            sourceMAC,
            6U);
        TCPSYNProbe_HandleSegment(
            &self->probe,
            &view);
        return 1U;
    }

    /* A listener consumes only a bare SYN while the one connection is free. */
    if ((self->listenerEnabled != 0U) &&
        (self->connection.state ==
         TCP_CONNECTION_STATE_CLOSED) &&
        (view.destinationPort ==
         self->listeningPort) &&
        ((view.flags & TCP_FLAG_SYN) != 0U) &&
        ((view.flags & TCP_FLAG_ACK) == 0U))
    {
        TCPConnection_AcceptSYN(
            &self->connection,
            sourceMAC,
            sourceIP,
            &view,
            TCPManager_AllocateSequence(
                self));
        return 1U;
    }

    /* Never answer a reset with another reset. */
    if ((view.flags & TCP_FLAG_RST) != 0U)
    {
        return 0U;
    }

    /*
     * A valid segment for a closed/unmatched port is consumed now and its
     * small reset description is sent later by TCPManager_Process().
     */
    TCPManager_QueueReset(
        self,
        sourceMAC,
        sourceIP,
        &view);

    return 1U;
}

uint8_t TCPManager_StartSYNProbe(
    TCPManager *self,
    const uint8_t remoteIP[4],
    uint16_t remotePort)
{
    if ((self->probe.state !=
         TCP_PROBE_STATE_IDLE) ||
        (remotePort == 0U))
    {
        return 0U;
    }

    TCPManager_Copy(
        self->probe.remoteMAC,
        TCPManager_HostMAC,
        6U);
    TCPManager_Copy(
        self->probe.remoteIP,
        remoteIP,
        4U);

    self->probe.remotePort = remotePort;
    self->probe.sendSequence =
        TCPManager_AllocateSequence(self);
    self->probe.resetSequence = 0U;
    self->probe.deadline = 0U;
    self->probe.retryCount = 0U;
    self->probe.result =
        TCP_PROBE_RESULT_NONE;
    self->probe.state =
        TCP_PROBE_STATE_SEND_SYN;

    return 1U;
}

uint8_t TCPManager_IsProbeIdle(
    const TCPManager *self)
{
    return
        (uint8_t)(
            self->probe.state ==
            TCP_PROBE_STATE_IDLE);
}

uint8_t TCPManager_IsProbeResultReady(
    const TCPManager *self)
{
    return
        (uint8_t)(
            self->probe.state ==
            TCP_PROBE_STATE_RESULT_READY);
}

uint8_t TCPManager_TakeProbeResult(
    TCPManager *self)
{
    const uint8_t result =
        self->probe.result;

    TCPSYNProbe_Reset(
        &self->probe);

    return result;
}

void TCPManager_StopProbe(
    TCPManager *self)
{
    TCPSYNProbe_Reset(
        &self->probe);
}

uint8_t TCPManager_Listen(
    TCPManager *self,
    uint16_t localPort)
{
    if ((self->listenerEnabled != 0U) ||
        (localPort == 0U))
    {
        return 0U;
    }

    self->listeningPort = localPort;
    self->listenerEnabled = 1U;
    return 1U;
}

void TCPManager_StopListening(
    TCPManager *self)
{
    self->listeningPort = 0U;
    self->listenerEnabled = 0U;
}

uint8_t TCPManager_Connect(
    TCPManager *self,
    const uint8_t remoteIP[4],
    uint16_t remotePort)
{
    if ((self->connection.state !=
         TCP_CONNECTION_STATE_CLOSED) ||
        (remotePort == 0U))
    {
        return 0U;
    }

    TCPConnection_BeginConnect(
        &self->connection,
        TCPManager_HostMAC,
        remoteIP,
        TCP_CONNECTION_LOCAL_PORT,
        remotePort,
        TCPManager_AllocateSequence(
            self));

    return 1U;
}

uint8_t TCPManager_IsConnectionEstablished(
    const TCPManager *self)
{
    return TCPConnection_IsEstablished(
        &self->connection);
}

uint16_t TCPManager_Read(
    TCPManager *self,
    uint8_t *destination,
    uint16_t capacity)
{
    const uint16_t readLength =
        TCPConnection_Read(
            &self->connection,
            destination,
            capacity);

    /* Advertise a reopened receive window after the application drains RX. */
    if ((readLength != 0U) &&
        (self->connection.state !=
         TCP_CONNECTION_STATE_CLOSED))
    {
        self->connection.acknowledgmentPending = 1U;
    }

    return readLength;
}

uint16_t TCPManager_Write(
    TCPManager *self,
    const uint8_t *source,
    uint16_t length)
{
    return TCPConnection_Write(
        &self->connection,
        source,
        length);
}

void TCPManager_RequestClose(
    TCPManager *self)
{
    if (self->connection.state !=
        TCP_CONNECTION_STATE_CLOSED)
    {
        self->connection.closeRequested = 1U;
    }
}

void TCPManager_AbortConnection(
    TCPManager *self)
{
    if (self->connection.state !=
        TCP_CONNECTION_STATE_CLOSED)
    {
        self->connection.state =
            TCP_CONNECTION_STATE_SEND_RESET;
    }
}

void TCPManager_NotifyDestinationUnreachable(
    TCPManager *self,
    const uint8_t remoteIP[4],
    uint16_t localPort,
    uint16_t remotePort)
{
    if ((self->probe.state ==
         TCP_PROBE_STATE_WAIT_RESPONSE) &&
        (self->probe.localPort == localPort) &&
        (self->probe.remotePort == remotePort) &&
        (TCPManager_IPMatches(
             self->probe.remoteIP,
             remoteIP) != 0U))
    {
        self->probe.result =
            TCP_PROBE_RESULT_UNREACHABLE;
        self->probe.state =
            TCP_PROBE_STATE_RESULT_READY;
    }

    if ((self->connection.state !=
         TCP_CONNECTION_STATE_CLOSED) &&
        (self->connection.localPort == localPort) &&
        (self->connection.remotePort == remotePort) &&
        (TCPManager_IPMatches(
             self->connection.remoteIP,
             remoteIP) != 0U))
    {
        TCPConnection_Reset(
            &self->connection);
    }
}
