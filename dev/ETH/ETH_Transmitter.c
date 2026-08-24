//
// Created by dxxdx on 2026/7/23.
//

#include "ETH_Transmitter.h"

#include <stddef.h>


uint8_t *ETH_TransmitBegin(
    ETHManager *manager,
    const uint8_t destinationMAC[
        ETH_MAC_LENGTH],
    uint16_t etherType)
{
    uint8_t *const frame =
        manager->operations->
            getTransmitFrame(
                manager);

    if (frame == NULL)
    {
        /*
         * 上一帧仍在等待或正在发送。
         *
         * 调用者应返回 DEFERRED，
         * 下次主循环再尝试构造回复。
         */
        return NULL;
    }

    /*
     * Destination MAC。
     */
    for (uint8_t index = 0U;
         index < ETH_MAC_LENGTH;
         index++)
    {
        frame[index] =
            destinationMAC[index];
    }

    /*
     * Source MAC。
     */
    for (uint8_t index = 0U;
         index < ETH_MAC_LENGTH;
         index++)
    {
        frame[
            ETH_MAC_LENGTH +
            index] =
            manager->localMAC[index];
    }

    /*
     * EtherType 使用网络字节序。
     */
    frame[12] =
        (uint8_t)(
            etherType >> 8U);

    frame[13] =
        (uint8_t)etherType;

    /*
     * 应用层从 Ethernet payload 开始填写。
     */
    return
        &frame[
            ETH_HEADER_LENGTH];
}


uint8_t ETH_TransmitCommit(
    ETHManager *manager,
    uint16_t payloadLength)
{
    if (payloadLength >
        (ETH_FRAME_CAPACITY -
         ETH_HEADER_LENGTH))
    {
        return 0U;
    }

    uint16_t frameLength =
        ETH_HEADER_LENGTH +
        payloadLength;

    /*
     * Ethernet 最短帧为 64 字节，
     * 其中最后 4 字节 FCS 不属于 USB ECM 数据。
     *
     * 因此发送给 USB 的 frame 至少为 60 字节。
     */
    while (frameLength <
           ETH_MIN_FRAME_LENGTH)
    {
        manager->txFrame[
            frameLength] = 0U;

        frameLength++;
    }

    /*
     * Header、payload 和 padding 全部完成后，
     * 最后发布 txFrameLength。
     *
     * 主循环或 USB 中断看到非零长度时，
     * 整个 frame 已经完整可读。
     */
    __asm__ volatile(
        "fence rw, rw"
        ::: "memory");

    manager->txFrameLength =
        frameLength;

    return 1U;
}


uint8_t ETH_TransmitService(
    ETHManager *manager)
{
    /*
     * 底层 transmit 自己检查：
     *
     * - 是否存在已 Commit 的 frame
     * - 上一帧是否仍在发送
     * - USB Data Interface 是否已经启用
     */
    return
        manager->operations->
            transmit(
                manager);
}