//
// Created by dxxdx on 2026/7/23.
//

#include "ETH_Dispatcher.h"

#include <stddef.h>

#include "ARP.h"
#include "IPv4.h"


/*
 * 结束当前帧。
 *
 * 先清除 Dispatcher 自己的持久化视图，
 * 再通知 manager 归还 RX frame。
 */
static ETHDispatchResult
ETH_DispatchFinishFrame(
    ETHDispatchInfo *self,
    ETHManager *manager,
    ETHDispatchResult result)
{
    self->target =
        ETH_DISPATCH_TARGET_NONE;

    self->frame =
        (ETHFrameView){0};

    manager->operations->
        releaseReceivedFrame(
            manager);

    return result;
}

/*
 * 读取 EtherType，并记录当前帧应该交给谁。
 *
 * 返回：
 * 0：协议不支持。
 * 1：已经选择 ARP 或 IPv4。
 */
static uint8_t ETH_DispatchSelectTarget(
    ETHDispatchInfo *self,
    const uint8_t *ethernetFrame)
{
    const uint16_t etherType =
        (uint16_t)(
            ((uint16_t)ethernetFrame[12] << 8U) |
            ethernetFrame[13]);

    switch (etherType)
    {
    case ETH_TYPE_ARP:
        self->target =
            ETH_DISPATCH_TARGET_ARP;

        return 1U;

    case ETH_TYPE_IPV4:
        self->target =
            ETH_DISPATCH_TARGET_IPV4;

        return 1U;

    default:
        return 0U;
    }
}

ETHDispatchResult ETH_DispatchFrame(
    ETHDispatchInfo *self,
    ETHManager *manager)
{
    const uint8_t *const ethernetFrame =
        manager->operations->
            getReceivedFrame(
                manager);

    if (ethernetFrame == NULL)
    {
        /*
         * Bus Reset 或接口关闭可能已经清除了 RX frame。
         *
         * 这里只清 Dispatcher 自己的状态，
         * 不反向修改 manager。
         */
        self->target =
            ETH_DISPATCH_TARGET_NONE;

        self->frame =
            (ETHFrameView){0};

        return ETH_DISPATCH_NO_FRAME;
    }


    /*
     * target 为 NONE，说明这是第一次处理当前帧。
     *
     * 先读取 Ethernet Header，
     * 决定交给 ARP 还是 IPv4 Handler。
     */
    if (self->target ==
        ETH_DISPATCH_TARGET_NONE)
    {
        if (manager->rxFrameLength <
            ETH_HEADER_LENGTH)
        {
            return ETH_DispatchFinishFrame(
                self,
                manager,
                ETH_DISPATCH_DROPPED);
        }

        self->frame.destinationMAC =
            &ethernetFrame[0];

        self->frame.sourceMAC =
            &ethernetFrame[6];

        self->frame.packet =
            &ethernetFrame[
                ETH_HEADER_LENGTH];

        self->frame.packetLength =
            manager->rxFrameLength -
            ETH_HEADER_LENGTH;

        if (ETH_DispatchSelectTarget(
                self,
                ethernetFrame) == 0U)
        {
            return ETH_DispatchFinishFrame(
                self,
                manager,
                ETH_DISPATCH_DROPPED);
        }
    }

    ETHDispatchResult result;

    switch (self->target)
    {
    case ETH_DISPATCH_TARGET_ARP:
        result =
            self->handleARP(
                manager,
                &self->frame);

        break;

    case ETH_DISPATCH_TARGET_IPV4:
        result =
            self->handleIPv4(
                manager,
                &self->frame);

        break;

    default:
        return ETH_DispatchFinishFrame(
            self,
            manager,
            ETH_DISPATCH_DROPPED);
    }

    /*
     * Handler 仍然忙。
     *
     * 不释放 manager 的 RX frame，
     * 也不清除 target 和 frame 视图。
     */
    if (result ==
        ETH_DISPATCH_DEFERRED)
    {
        return ETH_DISPATCH_DEFERRED;
    }

    if (result !=
        ETH_DISPATCH_HANDLED)
    {
        result =
            ETH_DISPATCH_DROPPED;
    }

    return ETH_DispatchFinishFrame(
        self,
        manager,
        result);
}
