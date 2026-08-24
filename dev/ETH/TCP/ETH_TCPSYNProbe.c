#include "ETH_TCPSYNProbe.h"

static uint8_t TCPSYNProbe_IPMatches(
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

void TCPSYNProbe_Reset(
    TCPSYNProbe *self)
{
    for (uint8_t index = 0U;
         index < 6U;
         index++)
    {
        self->remoteMAC[index] = 0U;
    }

    self->remoteIP[0] = 0U;
    self->remoteIP[1] = 0U;
    self->remoteIP[2] = 0U;
    self->remoteIP[3] = 0U;

    self->sendSequence = 0U;
    self->resetSequence = 0U;
    self->deadline = 0U;

    self->remotePort = 0U;
    self->retryCount = 0U;
    self->state = TCP_PROBE_STATE_IDLE;
    self->result = TCP_PROBE_RESULT_NONE;
}

uint8_t TCPSYNProbe_Matches(
    const TCPSYNProbe *self,
    const uint8_t sourceIP[4],
    const TCPSegmentView *segment)
{
    if ((self->state !=
         TCP_PROBE_STATE_WAIT_RESPONSE) &&
        (self->state !=
         TCP_PROBE_STATE_SEND_RESET))
    {
        return 0U;
    }

    return
        (uint8_t)(
            (TCPSYNProbe_IPMatches(
                 self->remoteIP,
                 sourceIP) != 0U) &&
            (segment->sourcePort ==
             self->remotePort) &&
            (segment->destinationPort ==
             self->localPort));
}

void TCPSYNProbe_HandleSegment(
    TCPSYNProbe *self,
    const TCPSegmentView *segment)
{
    const uint32_t expectedACK =
        self->sendSequence + 1U;

    if ((segment->flags &
         TCP_FLAG_RST) != 0U)
    {
        if (((segment->flags &
              TCP_FLAG_ACK) != 0U) &&
            (segment->acknowledgmentNumber ==
             expectedACK))
        {
            self->result =
                TCP_PROBE_RESULT_CLOSED;

            self->state =
                TCP_PROBE_STATE_RESULT_READY;
        }

        return;
    }

    if ((segment->flags &
         (TCP_FLAG_SYN | TCP_FLAG_ACK)) ==
        (TCP_FLAG_SYN | TCP_FLAG_ACK))
    {
        if (segment->acknowledgmentNumber !=
            expectedACK)
        {
            return;
        }

        self->resetSequence =
            segment->acknowledgmentNumber;

        self->result =
            TCP_PROBE_RESULT_OPEN;

        self->state =
            TCP_PROBE_STATE_SEND_RESET;
    }
}
