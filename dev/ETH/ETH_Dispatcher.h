//
// Created by dxxdx on 2026/7/23.
//
#ifndef CH32V203C8U_ETH_DISPATCHER_H
#define CH32V203C8U_ETH_DISPATCHER_H

#include <stdint.h>

#include "ETH.h"


#define ETH_HEADER_LENGTH 14U

#define ETH_TYPE_IPV4     0x0800U
#define ETH_TYPE_ARP      0x0806U


typedef enum
{
    /*
     * 当前没有持有分发目标。
     */
    ETH_DISPATCH_TARGET_NONE = 0,

    /*
     * 当前帧已经分发给 ARP Handler。
     */
    ETH_DISPATCH_TARGET_ARP,

    /*
     * 当前帧已经分发给 IPv4 Handler。
     */
    ETH_DISPATCH_TARGET_IPV4
} ETHDispatchTarget;


typedef enum
{
    /*
     * manager 当前没有完整帧。
     */
    ETH_DISPATCH_NO_FRAME = 0,

    /*
     * Handler 已经处理完成。
     */
    ETH_DISPATCH_HANDLED,

    /*
     * 帧无效或协议不受支持。
     */
    ETH_DISPATCH_DROPPED,

    /*
     * Handler 暂时没有处理完成。
     *
     * Dispatcher 保留当前目标和帧视图，
     * 下次继续提交同一帧。
     */
    ETH_DISPATCH_DEFERRED
} ETHDispatchResult;


/*
 * Ethernet frame 的零拷贝视图。
 *
 * 所有指针都指向 manager->rxFrame。
 * 调用 releaseReceivedFrame() 后立即失效。
 */
typedef struct
{
    const uint8_t *destinationMAC;
    const uint8_t *sourceMAC;

    /*
     * 指向 Ethernet Header 后面的协议数据。
     */
    const uint8_t *packet;

    uint16_t packetLength;
} ETHFrameView;


typedef ETHDispatchResult (*ETHProtocolHandler)(
    ETHManager *manager,
    const ETHFrameView *frame);


/*
 * Ethernet 一层分发器自己的全部状态。
 *
 * manager 不保存任何分发状态。
 * DEFERRED、当前协议和零拷贝视图都由本结构体负责。
 */
typedef struct
{
    ETHProtocolHandler handleARP;
    ETHProtocolHandler handleIPv4;

    ETHDispatchTarget target;

    ETHFrameView frame;
} ETHDispatchInfo;

/*
 * 处理至多一个 Ethernet frame。
 *
 * 返回 DEFERRED 时保留当前帧。
 * 返回 HANDLED/DROPPED 时归还当前帧。
 */
ETHDispatchResult ETH_DispatchFrame(
    ETHDispatchInfo *self,
    ETHManager *manager);


#endif /* CH32V203C8U_ETH_DISPATCHER_H */
