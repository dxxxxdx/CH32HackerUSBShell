#include "ETH_TCPManager.h"

#include "../DHCP.h"
#include "ETH_TCPFrameBuilder.h"


static const uint8_t TCPManager_LocalIP[4] =
{
    DHCP_SERVER_IP_0,
    DHCP_SERVER_IP_1,
    DHCP_SERVER_IP_2,
    DHCP_SERVER_IP_3
};

static uint16_t TCPManager_MinimumU16(
    uint16_t left,
    uint16_t right)
{
    if (left < right)
    {
        return left;
    }

    return right;
}

static uint8_t TCPManager_DeadlineReached(
    uint32_t currentTick,
    uint32_t deadline)
{
    /* Signed subtraction remains correct when the 32-bit tick wraps. */
    return
        (uint8_t)(
            (int32_t)(
                currentTick - deadline) >= 0);
}

/*
 * 在共享 TX 缓冲区里构造一帧完整的 Ethernet + IPv4 + TCP。
 *
 * 返回 0 表示没有发布 frame。Process() 路径里的常见原因是共享 TX
 * frame 仍忙；构包长度非法也会失败，调用者同样不得推进 TCP 状态。
 */
static uint8_t TCPManager_TryTransmit(
    TCPManager *self,
    ETHManager *ethernetManager,
    const TCPFrameBuildRequest *request,
    const TCPTransmitFIFO *transmitFIFO,
    uint16_t fifoOffset)
{
    TCPFrameBuilder builder;

    if (
            TCPFrameBuilder_Begin(
                &builder,
                ethernetManager,
                request) == 0U)
    {
        return 0U;
    }

    if (request->payloadLength != 0U)
    {
        /*
         * The caller derives payloadLength from this FIFO, so this copy is
         * exact.  Keeping the copy here means RX/TX FIFOs never alias the
         * USB-owned Ethernet frame.
         */
        (void)TCPTransmitFIFO_Copy(
            transmitFIFO,
            fifoOffset,
            builder.payload,
            request->payloadLength);
    }

    if (TCPFrameBuilder_Commit(
                &builder) == 0U)
    {
        return 0U;
    }

    self->nextIPv4Identification++;
    return 1U;
}

static uint16_t TCPManager_ConnectionWindow(
    const TCPConnection *connection)
{
    return TCPReceiveFIFO_GetFreeSize(
        &connection->receiveFIFO);
}

/*
 * 处理连接自己拥有的控制发送状态。
 *
 * 包含首次 SYN/ACK/FIN/RST 发送，以及 closeRequested 到 FIN 的入口判断。
 */
static uint8_t TCPManager_ProcessConnectionControl(
    TCPManager *self,
    ETHManager *ethernetManager,
    TCPConnection *connection,
    uint16_t localWindow,
    uint32_t currentTick)
{
    if (connection->state ==
        TCP_CONNECTION_STATE_SEND_RESET)
    {
        uint8_t flags = TCP_FLAG_RST;

        if (connection->receiveNext != 0U)
        {
            flags |= TCP_FLAG_ACK;
        }

        const TCPFrameBuildRequest request =
        {
            .destinationMAC =
                connection->remoteMAC,
            .sourceIP = TCPManager_LocalIP,
            .destinationIP =
                connection->remoteIP,
            .sequenceNumber =
                connection->sendNext,
            .acknowledgmentNumber =
                connection->receiveNext,
            .sourcePort =
                connection->localPort,
            .destinationPort =
                connection->remotePort,
            .windowSize = 0U,
            .payloadLength = 0U,
            .identification =
                self->nextIPv4Identification,
            .flags = flags,
            .ttl = TCP_IPV4_DEFAULT_TTL
        };

        if (TCPManager_TryTransmit(
                self,
                ethernetManager,
                &request,
                &connection->transmitFIFO,
                0U) != 0U)
        {
            TCPConnection_Reset(connection);
        }

        return 1U;
    }

    if (connection->state ==
        TCP_CONNECTION_STATE_SEND_SYN)
    {
        const TCPFrameBuildRequest request =
        {
            .destinationMAC =
                connection->remoteMAC,
            .sourceIP = TCPManager_LocalIP,
            .destinationIP =
                connection->remoteIP,
            .sequenceNumber =
                connection->sendNext,
            .acknowledgmentNumber = 0U,
            .sourcePort =
                connection->localPort,
            .destinationPort =
                connection->remotePort,
            .windowSize = localWindow,
            .payloadLength = 0U,
            .identification =
                self->nextIPv4Identification,
            .flags = TCP_FLAG_SYN,
            .ttl = TCP_IPV4_DEFAULT_TTL
        };

        if (TCPManager_TryTransmit(
                self,
                ethernetManager,
                &request,
                &connection->transmitFIFO,
                0U) != 0U)
        {
            connection->sendNext++;
            connection->retransmitDeadline =
                currentTick +
                TCP_CONNECTION_RETRANSMIT_TICKS;
            connection->retransmitCount = 0U;
            connection->state =
                TCP_CONNECTION_STATE_SYN_SENT;
        }

        return 1U;
    }

    if (connection->state ==
        TCP_CONNECTION_STATE_SEND_SYN_ACK)
    {
        const TCPFrameBuildRequest request =
        {
            .destinationMAC =
                connection->remoteMAC,
            .sourceIP = TCPManager_LocalIP,
            .destinationIP =
                connection->remoteIP,
            .sequenceNumber =
                connection->sendNext,
            .acknowledgmentNumber =
                connection->receiveNext,
            .sourcePort =
                connection->localPort,
            .destinationPort =
                connection->remotePort,
            .windowSize = localWindow,
            .payloadLength = 0U,
            .identification =
                self->nextIPv4Identification,
            .flags =
                TCP_FLAG_SYN | TCP_FLAG_ACK,
            .ttl = TCP_IPV4_DEFAULT_TTL
        };

        if (TCPManager_TryTransmit(
                self,
                ethernetManager,
                &request,
                &connection->transmitFIFO,
                0U) != 0U)
        {
            connection->sendNext++;
            connection->retransmitDeadline =
                currentTick +
                TCP_CONNECTION_RETRANSMIT_TICKS;
            connection->retransmitCount = 0U;
            connection->state =
                TCP_CONNECTION_STATE_SYN_RECEIVED;
        }

        return 1U;
    }

    if (connection->state ==
        TCP_CONNECTION_STATE_SEND_ACK)
    {
        const TCPFrameBuildRequest request =
        {
            .destinationMAC =
                connection->remoteMAC,
            .sourceIP = TCPManager_LocalIP,
            .destinationIP =
                connection->remoteIP,
            .sequenceNumber =
                connection->sendNext,
            .acknowledgmentNumber =
                connection->receiveNext,
            .sourcePort =
                connection->localPort,
            .destinationPort =
                connection->remotePort,
            .windowSize = localWindow,
            .payloadLength = 0U,
            .identification =
                self->nextIPv4Identification,
            .flags = TCP_FLAG_ACK,
            .ttl = TCP_IPV4_DEFAULT_TTL
        };

        if (TCPManager_TryTransmit(
                self,
                ethernetManager,
                &request,
                &connection->transmitFIFO,
                0U) != 0U)
        {
            connection->acknowledgmentPending = 0U;
            connection->state =
                TCP_CONNECTION_STATE_ESTABLISHED;
        }

        return 1U;
    }

    /*
     * Closing is eligible only after all application data is acknowledged.
     * Moving the state here lets this same Process() pass attempt the FIN.
     */
    if ((connection->closeRequested != 0U) &&
        (connection->transmitFIFO.length == 0U) &&
        (connection->sendUnacknowledged ==
         connection->sendNext))
    {
        if (connection->state ==
            TCP_CONNECTION_STATE_ESTABLISHED)
        {
            connection->state =
                TCP_CONNECTION_STATE_SEND_FIN;
        }
        else if (connection->state ==
                 TCP_CONNECTION_STATE_CLOSE_WAIT)
        {
            connection->state =
                TCP_CONNECTION_STATE_SEND_LAST_ACK;
        }
    }

    if ((connection->state ==
         TCP_CONNECTION_STATE_SEND_FIN) ||
        (connection->state ==
         TCP_CONNECTION_STATE_SEND_LAST_ACK))
    {
        const uint8_t passiveClose =
            (uint8_t)(
                connection->state ==
                TCP_CONNECTION_STATE_SEND_LAST_ACK);

        const TCPFrameBuildRequest request =
        {
            .destinationMAC =
                connection->remoteMAC,
            .sourceIP = TCPManager_LocalIP,
            .destinationIP =
                connection->remoteIP,
            .sequenceNumber =
                connection->sendNext,
            .acknowledgmentNumber =
                connection->receiveNext,
            .sourcePort =
                connection->localPort,
            .destinationPort =
                connection->remotePort,
            .windowSize = localWindow,
            .payloadLength = 0U,
            .identification =
                self->nextIPv4Identification,
            .flags =
                TCP_FLAG_FIN | TCP_FLAG_ACK,
            .ttl = TCP_IPV4_DEFAULT_TTL
        };

        if (TCPManager_TryTransmit(
                self,
                ethernetManager,
                &request,
                &connection->transmitFIFO,
                0U) != 0U)
        {
            connection->sendNext++;
            connection->acknowledgmentPending = 0U;
            connection->retransmitDeadline =
                currentTick +
                TCP_CONNECTION_RETRANSMIT_TICKS;
            connection->retransmitCount = 0U;
            connection->state =
                (passiveClose != 0U) ?
                TCP_CONNECTION_STATE_LAST_ACK :
                TCP_CONNECTION_STATE_FIN_WAIT_1;
        }

        return 1U;
    }

    return 0U;
}

/*
 * 处理连接的数据面发送、ACK、重传和超时。
 *
 * 这里保留原来的判断顺序，避免拆函数时改变状态机节奏。
 */
static uint8_t TCPManager_ProcessConnectionData(
    TCPManager *self,
    ETHManager *ethernetManager,
    TCPConnection *connection,
    uint16_t localWindow,
    uint32_t currentTick)
{
    /* Retransmit active and passive handshakes with the original sequence. */
    if (((connection->state ==
          TCP_CONNECTION_STATE_SYN_SENT) ||
         (connection->state ==
          TCP_CONNECTION_STATE_SYN_RECEIVED)) &&
        (connection->retransmitDeadline != 0U) &&
        (TCPManager_DeadlineReached(
             currentTick,
             connection->retransmitDeadline) != 0U))
    {
        if (connection->retransmitCount >=
            TCP_CONNECTION_RETRY_LIMIT)
        {
            TCPConnection_Reset(connection);
            return 0U;
        }

        const uint8_t passiveHandshake =
            (uint8_t)(
                connection->state ==
                TCP_CONNECTION_STATE_SYN_RECEIVED);

        const TCPFrameBuildRequest request =
        {
            .destinationMAC =
                connection->remoteMAC,
            .sourceIP = TCPManager_LocalIP,
            .destinationIP =
                connection->remoteIP,
            .sequenceNumber =
                connection->sendUnacknowledged,
            .acknowledgmentNumber =
                connection->receiveNext,
            .sourcePort =
                connection->localPort,
            .destinationPort =
                connection->remotePort,
            .windowSize = localWindow,
            .payloadLength = 0U,
            .identification =
                self->nextIPv4Identification,
            .flags =
                (passiveHandshake != 0U) ?
                    (TCP_FLAG_SYN |
                     TCP_FLAG_ACK) :
                    TCP_FLAG_SYN,
            .ttl = TCP_IPV4_DEFAULT_TTL
        };

        if (TCPManager_TryTransmit(
                self,
                ethernetManager,
                &request,
                &connection->transmitFIFO,
                0U) != 0U)
        {
            connection->retransmitCount++;
            connection->retransmitDeadline =
                currentTick +
                TCP_CONNECTION_RETRANSMIT_TICKS;
        }

        return 1U;
    }

    /* Immediate ACKs also implement duplicate ACKs for the OOO stall policy. */
    if (connection->acknowledgmentPending != 0U)
    {
        const TCPFrameBuildRequest request =
        {
            .destinationMAC =
                connection->remoteMAC,
            .sourceIP = TCPManager_LocalIP,
            .destinationIP =
                connection->remoteIP,
            .sequenceNumber =
                connection->sendNext,
            .acknowledgmentNumber =
                connection->receiveNext,
            .sourcePort =
                connection->localPort,
            .destinationPort =
                connection->remotePort,
            .windowSize = localWindow,
            .payloadLength = 0U,
            .identification =
                self->nextIPv4Identification,
            .flags = TCP_FLAG_ACK,
            .ttl = TCP_IPV4_DEFAULT_TTL
        };

        if (TCPManager_TryTransmit(
                self,
                ethernetManager,
                &request,
                &connection->transmitFIFO,
                0U) != 0U)
        {
            connection->acknowledgmentPending = 0U;

            if (connection->state ==
                TCP_CONNECTION_STATE_TIME_WAIT)
            {
                connection->timeWaitDeadline =
                    currentTick +
                    TCP_CONNECTION_TIME_WAIT_TICKS;
            }
        }

        return 1U;
    }

    /* A partial ACK restarts the timer for the remaining in-flight bytes. */
    if ((connection->transmitFIFO.sentLength != 0U) &&
        (connection->retransmitDeadline == 0U))
    {
        connection->retransmitDeadline =
            currentTick +
            TCP_CONNECTION_RETRANSMIT_TICKS;
    }

    /* Retransmit the single cumulative in-flight data range. */
    if ((connection->transmitFIFO.sentLength != 0U) &&
        (connection->retransmitDeadline != 0U) &&
        (TCPManager_DeadlineReached(
             currentTick,
             connection->retransmitDeadline) != 0U))
    {
        if (connection->retransmitCount >=
            TCP_CONNECTION_RETRY_LIMIT)
        {
            connection->state =
                TCP_CONNECTION_STATE_SEND_RESET;
            return 1U;
        }

        const TCPFrameBuildRequest request =
        {
            .destinationMAC =
                connection->remoteMAC,
            .sourceIP = TCPManager_LocalIP,
            .destinationIP =
                connection->remoteIP,
            .sequenceNumber =
                connection->sendUnacknowledged,
            .acknowledgmentNumber =
                connection->receiveNext,
            .sourcePort =
                connection->localPort,
            .destinationPort =
                connection->remotePort,
            .windowSize = localWindow,
            .payloadLength =
                connection->transmitFIFO.sentLength,
            .identification =
                self->nextIPv4Identification,
            .flags =
                TCP_FLAG_ACK | TCP_FLAG_PSH,
            .ttl = TCP_IPV4_DEFAULT_TTL
        };

        if (TCPManager_TryTransmit(
                self,
                ethernetManager,
                &request,
                &connection->transmitFIFO,
                0U) != 0U)
        {
            connection->retransmitCount++;
            connection->retransmitDeadline =
                currentTick +
                TCP_CONNECTION_RETRANSMIT_TICKS;
        }

        return 1U;
    }

    /* FIN occupies one sequence number and is retransmitted like other data. */
    if (((connection->state ==
          TCP_CONNECTION_STATE_FIN_WAIT_1) ||
         (connection->state ==
          TCP_CONNECTION_STATE_LAST_ACK) ||
         (connection->state ==
          TCP_CONNECTION_STATE_CLOSING)) &&
        (connection->retransmitDeadline != 0U) &&
        (TCPManager_DeadlineReached(
             currentTick,
             connection->retransmitDeadline) != 0U))
    {
        if (connection->retransmitCount >=
            TCP_CONNECTION_RETRY_LIMIT)
        {
            TCPConnection_Reset(connection);
            return 0U;
        }

        const TCPFrameBuildRequest request =
        {
            .destinationMAC =
                connection->remoteMAC,
            .sourceIP = TCPManager_LocalIP,
            .destinationIP =
                connection->remoteIP,
            .sequenceNumber =
                connection->sendNext - 1U,
            .acknowledgmentNumber =
                connection->receiveNext,
            .sourcePort =
                connection->localPort,
            .destinationPort =
                connection->remotePort,
            .windowSize = localWindow,
            .payloadLength = 0U,
            .identification =
                self->nextIPv4Identification,
            .flags =
                TCP_FLAG_FIN | TCP_FLAG_ACK,
            .ttl = TCP_IPV4_DEFAULT_TTL
        };

        if (TCPManager_TryTransmit(
                self,
                ethernetManager,
                &request,
                &connection->transmitFIFO,
                0U) != 0U)
        {
            connection->retransmitCount++;
            connection->retransmitDeadline =
                currentTick +
                TCP_CONNECTION_RETRANSMIT_TICKS;
        }

        return 1U;
    }

    if (connection->state ==
        TCP_CONNECTION_STATE_FIN_WAIT_2)
    {
        if (connection->retransmitDeadline == 0U)
        {
            connection->retransmitDeadline =
                currentTick +
                TCP_CONNECTION_TIME_WAIT_TICKS;
        }
        else if (TCPManager_DeadlineReached(
                     currentTick,
                     connection->retransmitDeadline) != 0U)
        {
            TCPConnection_Reset(connection);
        }

        return 0U;
    }

    if (connection->state ==
        TCP_CONNECTION_STATE_TIME_WAIT)
    {
        if (connection->timeWaitDeadline == 0U)
        {
            connection->timeWaitDeadline =
                currentTick +
                TCP_CONNECTION_TIME_WAIT_TICKS;
        }
        else if (TCPManager_DeadlineReached(
                     currentTick,
                     connection->timeWaitDeadline) != 0U)
        {
            TCPConnection_Reset(connection);
        }

        return 0U;
    }

    /*
     * Keep at most one data range in flight.  This deliberately small model
     * makes cumulative ACK and retransmission bookkeeping deterministic.
     */
    if (((connection->state ==
          TCP_CONNECTION_STATE_ESTABLISHED) ||
         (connection->state ==
          TCP_CONNECTION_STATE_CLOSE_WAIT)) &&
        (connection->transmitFIFO.sentLength == 0U))
    {
        uint16_t sendLength =
            TCPTransmitFIFO_GetUnsentLength(
                &connection->transmitFIFO);

        sendLength = TCPManager_MinimumU16(
            sendLength,
            connection->remoteWindow);
        sendLength = TCPManager_MinimumU16(
            sendLength,
            connection->remoteMSS);

        if (sendLength != 0U)
        {
            const TCPFrameBuildRequest request =
            {
                .destinationMAC =
                    connection->remoteMAC,
                .sourceIP = TCPManager_LocalIP,
                .destinationIP =
                    connection->remoteIP,
                .sequenceNumber =
                    connection->sendNext,
                .acknowledgmentNumber =
                    connection->receiveNext,
                .sourcePort =
                    connection->localPort,
                .destinationPort =
                    connection->remotePort,
                .windowSize = localWindow,
                .payloadLength = sendLength,
                .identification =
                    self->nextIPv4Identification,
                .flags =
                    TCP_FLAG_ACK | TCP_FLAG_PSH,
                .ttl = TCP_IPV4_DEFAULT_TTL
            };

            if (TCPManager_TryTransmit(
                    self,
                    ethernetManager,
                    &request,
                    &connection->transmitFIFO,
                    0U) != 0U)
            {
                TCPTransmitFIFO_MarkSent(
                    &connection->transmitFIFO,
                    sendLength);
                connection->sendNext +=
                    sendLength;
                connection->retransmitDeadline =
                    currentTick +
                    TCP_CONNECTION_RETRANSMIT_TICKS;
                connection->retransmitCount = 0U;
            }

            return 1U;
        }
    }

    return 0U;
}

/*
 * Return one when the connection owns this Process() turn.  That includes
 * both a successful send and a failed attempt caused by a busy TX frame;
 * lower-priority probe traffic must not jump ahead of connection traffic.
 */
static uint8_t TCPManager_ProcessConnection(
    TCPManager *self,
    ETHManager *ethernetManager,
    uint32_t currentTick)
{
    TCPConnection *const connection =
        &self->connection;

    if (connection->state ==
        TCP_CONNECTION_STATE_CLOSED)
    {
        return 0U;
    }

    const uint16_t localWindow =
        TCPManager_ConnectionWindow(
            connection);

    if (TCPManager_ProcessConnectionControl(
            self,
            ethernetManager,
            connection,
            localWindow,
            currentTick) != 0U)
    {
        return 1U;
    }

    return TCPManager_ProcessConnectionData(
        self,
        ethernetManager,
        connection,
        localWindow,
        currentTick);
}

static uint8_t TCPManager_ProcessResetEvent(
    TCPManager *self,
    ETHManager *ethernetManager)
{
    TCPResetEvent *const event =
        &self->resetEvent;

    if (event->pending == 0U)
    {
        return 0U;
    }

    const TCPFrameBuildRequest request =
    {
        .destinationMAC = event->remoteMAC,
        .sourceIP = TCPManager_LocalIP,
        .destinationIP = event->remoteIP,
        .sequenceNumber =
            event->sequenceNumber,
        .acknowledgmentNumber =
            event->acknowledgmentNumber,
        .sourcePort = event->localPort,
        .destinationPort = event->remotePort,
        .windowSize = 0U,
        .payloadLength = 0U,
        .identification =
            self->nextIPv4Identification,
        .flags = event->flags,
        .ttl = TCP_IPV4_DEFAULT_TTL
    };

    if (TCPManager_TryTransmit(
            self,
            ethernetManager,
            &request,
            &self->connection.transmitFIFO,
            0U) != 0U)
    {
        event->pending = 0U;
    }

    return 1U;
}

static uint8_t TCPManager_ProcessProbe(
    TCPManager *self,
    ETHManager *ethernetManager,
    uint32_t currentTick)
{
    TCPSYNProbe *const probe =
        &self->probe;

    if (probe->state ==
        TCP_PROBE_STATE_WAIT_RESPONSE)
    {
        if ((probe->deadline != 0U) &&
            (TCPManager_DeadlineReached(
                 currentTick,
                 probe->deadline) != 0U))
        {
            if (probe->retryCount <
                TCP_PROBE_RETRY_LIMIT)
            {
                probe->retryCount++;
                probe->state =
                    TCP_PROBE_STATE_SEND_SYN;
            }
            else
            {
                probe->result =
                    TCP_PROBE_RESULT_TIMEOUT;
                probe->state =
                    TCP_PROBE_STATE_RESULT_READY;
            }
        }

        if (probe->state ==
            TCP_PROBE_STATE_WAIT_RESPONSE)
        {
            return 0U;
        }
    }

    if (probe->state ==
        TCP_PROBE_STATE_SEND_SYN)
    {
        const TCPFrameBuildRequest request =
        {
            .destinationMAC = probe->remoteMAC,
            .sourceIP = TCPManager_LocalIP,
            .destinationIP = probe->remoteIP,
            .sequenceNumber =
                probe->sendSequence,
            .acknowledgmentNumber = 0U,
            .sourcePort = probe->localPort,
            .destinationPort = probe->remotePort,
            .windowSize =
                TCP_PROBE_WINDOW_SIZE,
            .payloadLength = 0U,
            .identification =
                self->nextIPv4Identification,
            .flags = TCP_FLAG_SYN,
            .ttl = TCP_IPV4_DEFAULT_TTL
        };

        if (TCPManager_TryTransmit(
                self,
                ethernetManager,
                &request,
                &self->connection.transmitFIFO,
                0U) != 0U)
        {
            probe->deadline =
                currentTick +
                TCP_PROBE_RESPONSE_TIMEOUT_TICKS;
            probe->state =
                TCP_PROBE_STATE_WAIT_RESPONSE;
        }

        return 1U;
    }

    if (probe->state ==
        TCP_PROBE_STATE_SEND_RESET)
    {
        const TCPFrameBuildRequest request =
        {
            .destinationMAC = probe->remoteMAC,
            .sourceIP = TCPManager_LocalIP,
            .destinationIP = probe->remoteIP,
            .sequenceNumber =
                probe->resetSequence,
            .acknowledgmentNumber = 0U,
            .sourcePort = probe->localPort,
            .destinationPort = probe->remotePort,
            .windowSize = 0U,
            .payloadLength = 0U,
            .identification =
                self->nextIPv4Identification,
            .flags = TCP_FLAG_RST,
            .ttl = TCP_IPV4_DEFAULT_TTL
        };

        if (TCPManager_TryTransmit(
                self,
                ethernetManager,
                &request,
                &self->connection.transmitFIFO,
                0U) != 0U)
        {
            probe->state =
                TCP_PROBE_STATE_RESULT_READY;
        }

        return 1U;
    }

    return 0U;
}

void TCPManager_Process(
    TCPManager *self,
    struct ETHManager *ethernetManager)
{
    const uint32_t currentTick =
        *self->tick;

    /*
     * Exactly one TCP frame may be committed per pass.  The priority keeps
     * an established connection responsive while an Nmap probe runs:
     * connection -> unmatched-segment RST -> SYN probe.
     */
    if (TCPManager_ProcessConnection(
            self,
            ethernetManager,
            currentTick) != 0U)
    {
        return;
    }

    if (TCPManager_ProcessResetEvent(
            self,
            ethernetManager) != 0U)
    {
        return;
    }

    (void)TCPManager_ProcessProbe(
        self,
        ethernetManager,
        currentTick);
}
