#ifndef ETH_TCP_TYPES_H
#define ETH_TCP_TYPES_H

#include <stdint.h>

#define TCP_PROTOCOL_NUMBER                      6U

#define TCP_HEADER_MINIMUM_SIZE                 20U
#define TCP_HEADER_MAXIMUM_SIZE                 60U

#define TCP_FLAG_FIN                          0x01U
#define TCP_FLAG_SYN                          0x02U
#define TCP_FLAG_RST                          0x04U
#define TCP_FLAG_PSH                          0x08U
#define TCP_FLAG_ACK                          0x10U
#define TCP_FLAG_URG                          0x20U
#define TCP_FLAG_ECE                          0x40U
#define TCP_FLAG_CWR                          0x80U

#define TCP_PROBE_LOCAL_PORT                 51888U
#define TCP_CONNECTION_LOCAL_PORT            51889U

#define TCP_PROBE_RESPONSE_TIMEOUT_TICKS        50U
#define TCP_PROBE_RETRY_LIMIT                    2U
#define TCP_PROBE_WINDOW_SIZE                 1024U

#define TCP_CONNECTION_RETRANSMIT_TICKS         50U
#define TCP_CONNECTION_RETRY_LIMIT               4U
#define TCP_CONNECTION_TIME_WAIT_TICKS         200U

#define TCP_DEFAULT_REMOTE_MSS                  536U
#define TCP_LOCAL_MSS                          1460U

#define TCP_IPV4_DEFAULT_TTL                     64U

#define TCP_RECEIVE_BUFFER_SIZE                1536U
#define TCP_TRANSMIT_BUFFER_SIZE                512U

typedef enum TCPProbeState
{
    TCP_PROBE_STATE_IDLE = 0U,
    TCP_PROBE_STATE_SEND_SYN,
    TCP_PROBE_STATE_WAIT_RESPONSE,
    TCP_PROBE_STATE_SEND_RESET,
    TCP_PROBE_STATE_RESULT_READY
} TCPProbeState;

typedef enum TCPProbeResult
{
    TCP_PROBE_RESULT_NONE = 0U,
    TCP_PROBE_RESULT_OPEN,
    TCP_PROBE_RESULT_CLOSED,
    TCP_PROBE_RESULT_TIMEOUT,
    TCP_PROBE_RESULT_UNREACHABLE
} TCPProbeResult;

typedef enum TCPConnectionState
{
    TCP_CONNECTION_STATE_CLOSED = 0U,
    TCP_CONNECTION_STATE_SEND_SYN,
    TCP_CONNECTION_STATE_SYN_SENT,
    TCP_CONNECTION_STATE_SEND_ACK,
    TCP_CONNECTION_STATE_SEND_SYN_ACK,
    TCP_CONNECTION_STATE_SYN_RECEIVED,
    TCP_CONNECTION_STATE_ESTABLISHED,
    TCP_CONNECTION_STATE_SEND_FIN,
    TCP_CONNECTION_STATE_SEND_LAST_ACK,
    TCP_CONNECTION_STATE_FIN_WAIT_1,
    TCP_CONNECTION_STATE_FIN_WAIT_2,
    TCP_CONNECTION_STATE_CLOSE_WAIT,
    TCP_CONNECTION_STATE_LAST_ACK,
    TCP_CONNECTION_STATE_CLOSING,
    TCP_CONNECTION_STATE_TIME_WAIT,
    TCP_CONNECTION_STATE_SEND_RESET
} TCPConnectionState;

typedef struct TCPSegmentView
{
    const uint8_t *payload;

    uint32_t sequenceNumber;
    uint32_t acknowledgmentNumber;

    uint16_t sourcePort;
    uint16_t destinationPort;
    uint16_t windowSize;
    uint16_t urgentPointer;
    uint16_t payloadLength;

    uint8_t headerLength;
    uint8_t flags;
} TCPSegmentView;

#endif
