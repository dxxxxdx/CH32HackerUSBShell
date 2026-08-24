#ifndef ETH_TCP_MANAGER_H
#define ETH_TCP_MANAGER_H

#include "ETH_TCPConnection.h"
#include "ETH_TCPSYNProbe.h"

struct ETHManager;

/*
 * 不属于 probe、现有连接和监听端口的合法 segment，需要回一个 RST。
 *
 * RX 路径只记录这个小事件；真正构包发送由 TCPManager_Process() 在共享
 * TX frame 可用时完成。这里只保留一个槽位，RST 风暴下允许丢后来的事件。
 */
typedef struct TCPResetEvent
{
    uint8_t remoteMAC[6];
    uint8_t remoteIP[4];

    uint32_t sequenceNumber;
    uint32_t acknowledgmentNumber;

    uint16_t localPort;
    uint16_t remotePort;

    uint8_t flags;
    uint8_t pending;
} TCPResetEvent;

#define TCP_RESET_EVENT_INITIALIZER \
{ \
    .remoteMAC = { 0U, 0U, 0U, 0U, 0U, 0U }, \
    .remoteIP = { 0U, 0U, 0U, 0U }, \
    .sequenceNumber = 0U, \
    .acknowledgmentNumber = 0U, \
    .localPort = 0U, \
    .remotePort = 0U, \
    .flags = 0U, \
    .pending = 0U \
}

typedef struct TCPManager
{
    const volatile uint32_t *const tick;

    TCPSYNProbe probe;
    TCPConnection connection;
    TCPResetEvent resetEvent;

    uint32_t sequenceCursor;

    uint16_t nextIPv4Identification;
    uint16_t listeningPort;
    uint8_t listenerEnabled;
} TCPManager;

#define TCP_MANAGER_INITIALIZER(tick_) \
{ \
    .tick = (tick_), \
    .probe = TCP_SYN_PROBE_INITIALIZER, \
    .connection = TCP_CONNECTION_INITIALIZER, \
    .resetEvent = TCP_RESET_EVENT_INITIALIZER, \
    .sequenceCursor = 0x51888518UL, \
    .nextIPv4Identification = 1U, \
    .listeningPort = 0U, \
    .listenerEnabled = 0U \
}

/*
 * TCP 主轮询入口。
 *
 * 每次调用最多发布一帧 TCP 输出，优先级为：
 * 现有连接、未匹配 segment 的 RST、SYN probe。
 */
void TCPManager_Process(
    TCPManager *self,
    struct ETHManager *ethernetManager);

/*
 * IPv4 层收到 TCP payload 后调用。
 *
 * 这里完成 TCP header 解析、checksum 校验和连接/probe/listener 分发。
 * 返回 1 表示这个 TCP segment 已被 TCP 层消费；返回 0 表示丢弃或不归本机。
 * 需要回复的 ACK/RST 只会记录状态，发送由 TCPManager_Process() 完成。
 */
uint8_t TCPManager_ReceiveIPv4Segment(
    TCPManager *self,
    const uint8_t sourceMAC[6],
    const uint8_t sourceIP[4],
    const uint8_t destinationIP[4],
    const uint8_t *segment,
    uint16_t segmentLength);

/*
 * 启动一次 SYN probe。
 *
 * 当前只允许一个 probe 同时运行。remotePort 为 0 或 probe 忙时返回 0。
 * 返回 1 只表示请求已进入状态机，结果需要后续轮询取得。
 */
uint8_t TCPManager_StartSYNProbe(
    TCPManager *self,
    const uint8_t remoteIP[4],
    uint16_t remotePort);

/*
 * 查询 probe 是否完全空闲，可以开始新的 probe。
 */
uint8_t TCPManager_IsProbeIdle(
    const TCPManager *self);

/*
 * 查询 probe 是否已有最终结果。
 */
uint8_t TCPManager_IsProbeResultReady(
    const TCPManager *self);

/*
 * 取出 probe 结果并复位 probe 状态。
 *
 * 调用者应先确认 TCPManager_IsProbeResultReady() 为 1。
 */
uint8_t TCPManager_TakeProbeResult(
    TCPManager *self);

/*
 * 放弃当前 probe，并释放 probe 状态机。
 */
void TCPManager_StopProbe(
    TCPManager *self);

/*
 * 打开一个监听端口。
 *
 * 当前 TCP manager 只有一个监听端口和一个长连接。已经监听或端口为 0 时返回 0。
 */
uint8_t TCPManager_Listen(
    TCPManager *self,
    uint16_t localPort);

/*
 * 关闭监听端口，不影响已经建立或正在关闭的连接。
 */
void TCPManager_StopListening(
    TCPManager *self);

/*
 * 主动发起一个 TCP 连接。
 *
 * 当前只有一个长连接槽位；连接未关闭或 remotePort 为 0 时返回 0。
 * 返回 1 表示 SYN 已进入状态机，真正发送由 TCPManager_Process() 完成。
 */
uint8_t TCPManager_Connect(
    TCPManager *self,
    const uint8_t remoteIP[4],
    uint16_t remotePort);

/*
 * 查询长连接是否处于 ESTABLISHED。
 */
uint8_t TCPManager_IsConnectionEstablished(
    const TCPManager *self);

/*
 * 从连接 RX FIFO 读取应用数据。
 *
 * 成功读出数据后会挂起一个 ACK，用于通告接收窗口重新打开。
 */
uint16_t TCPManager_Read(
    TCPManager *self,
    uint8_t *destination,
    uint16_t capacity);

/*
 * 写入连接 TX FIFO。
 *
 * 返回实际写入长度；连接不可写或 FIFO 空间不足时可能小于 length。
 */
uint16_t TCPManager_Write(
    TCPManager *self,
    const uint8_t *source,
    uint16_t length);

/*
 * 请求正常关闭连接。
 *
 * FIN 会等已写入的数据全部确认后再由 TCPManager_Process() 发送。
 */
void TCPManager_RequestClose(
    TCPManager *self);

/*
 * 请求异常中止连接。
 *
 * 这里只把连接切到发送 RST 的状态，真正发送和状态复位由 Process 路径完成。
 */
void TCPManager_AbortConnection(
    TCPManager *self);

/*
 * 上层 ICMP/IPv4 发现目标不可达时通知 TCP。
 *
 * 匹配 probe 时生成 UNREACHABLE 结果；匹配长连接时直接复位连接。
 */
void TCPManager_NotifyDestinationUnreachable(
    TCPManager *self,
    const uint8_t remoteIP[4],
    uint16_t localPort,
    uint16_t remotePort);

#endif
