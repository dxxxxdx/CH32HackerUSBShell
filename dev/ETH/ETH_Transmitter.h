//
// Created by dxxdx on 2026/7/23.
//
#ifndef CH32V203C8U_ETH_TRANSMITTER_H
#define CH32V203C8U_ETH_TRANSMITTER_H

#include <stdint.h>

#include "ETH.h"


#define ETH_MAC_LENGTH            6U
#define ETH_HEADER_LENGTH         14U

/*
 * Ethernet frame 不包含最后的硬件 FCS。
 */
#define ETH_MIN_FRAME_LENGTH      60U


/*
 * 借出 TX frame，并填写 Ethernet Header。
 *
 * 返回值直接指向 Ethernet payload 区域。
 * TX buffer 暂时不可用时返回 NULL。
 *
 * Begin 和 Commit 应当在同一次状态机调用中完成，
 * 中间不要主动让出执行权。
 */
uint8_t *ETH_TransmitBegin(
    ETHManager *manager,
    const uint8_t destinationMAC[
        ETH_MAC_LENGTH],
    uint16_t etherType);


/*
 * 发布刚刚填写完成的 Ethernet frame。
 *
 * payloadLength 不包含 14 字节 Ethernet Header。
 *
 * 返回：
 * 0：长度非法。
 * 1：帧已经进入等待发送状态。
 */
uint8_t ETH_TransmitCommit(
    ETHManager *manager,
    uint16_t payloadLength);


/*
 * 尝试把已经 Commit 的 frame 提交给底层链路。
 *
 * 主循环可以无条件频繁调用。
 */
uint8_t ETH_TransmitService(
    ETHManager *manager);


#endif /* CH32V203C8U_ETH_TRANSMITTER_H */