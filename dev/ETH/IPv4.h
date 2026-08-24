#ifndef CH32V203C8U_IPV4_H
#define CH32V203C8U_IPV4_H

#include <stdint.h>

#include "ETH_Dispatcher.h"


#define IPV4_VERSION                4U
#define IPV4_MIN_HEADER_LENGTH      20U

#define IPV4_PROTOCOL_ICMP          1U
#define IPV4_PROTOCOL_TCP           6U
#define IPV4_PROTOCOL_UDP           17U


/*
 * IPv4 已经验证完成后的零拷贝视图。
 *
 * 所有指针只在当前 Ethernet RX frame
 * 被释放之前有效。
 */
typedef struct
{
    const uint8_t *sourceMAC;
    const uint8_t *destinationMAC;

    const uint8_t *sourceAddress;
    const uint8_t *destinationAddress;

    const uint8_t *header;
    uint16_t headerLength;

    const uint8_t *payload;
    uint16_t payloadLength;

    uint8_t protocol;
    uint8_t ttl;
} IPv4PacketView;


/*
 * 读取网络字节序整数。
 */
__attribute__((always_inline))
static inline uint16_t IPv4_ReadU16(
    const uint8_t *data)
{
    return
        (uint16_t)(
            ((uint16_t)data[0] << 8U) |
            data[1]);
}


__attribute__((always_inline))
static inline uint32_t IPv4_ReadU32(
    const uint8_t *data)
{
    return
        ((uint32_t)data[0] << 24U) |
        ((uint32_t)data[1] << 16U) |
        ((uint32_t)data[2] << 8U) |
        data[3];
}


/*
 * 写入网络字节序整数。
 */
__attribute__((always_inline))
static inline void IPv4_WriteU16(
    uint8_t *data,
    uint16_t value)
{
    data[0] =
        (uint8_t)(value >> 8U);

    data[1] =
        (uint8_t)value;
}


__attribute__((always_inline))
static inline void IPv4_WriteU32(
    uint8_t *data,
    uint32_t value)
{
    data[0] =
        (uint8_t)(value >> 24U);

    data[1] =
        (uint8_t)(value >> 16U);

    data[2] =
        (uint8_t)(value >> 8U);

    data[3] =
        (uint8_t)value;
}


/*
 * 向 Internet Checksum 累加器加入一段数据。
 *
 * IPv4、UDP、TCP、ICMP 使用相同的一补码求和算法。
 */
static inline uint32_t IPv4_ChecksumAdd(
    uint32_t sum,
    const uint8_t *data,
    uint16_t length)
{
    while (length >= 2U)
    {
        sum +=
            IPv4_ReadU16(data);

        data += 2U;
        length -= 2U;
    }

    if (length != 0U)
    {
        sum +=
            (uint16_t)data[0] << 8U;
    }

    return sum;
}


/*
 * 把进位折回低 16 位。
 */
static inline uint16_t IPv4_ChecksumFold(
    uint32_t sum)
{
    while ((sum >> 16U) != 0U)
    {
        sum =
            (sum & 0xFFFFU) +
            (sum >> 16U);
    }

    return
        (uint16_t)sum;
}


/*
 * 验证包含原 checksum 字段的数据。
 */
static inline uint8_t IPv4_ChecksumIsValid(
    uint32_t sum)
{
    return
        (uint8_t)(
            IPv4_ChecksumFold(sum) ==
            0xFFFFU);
}


/*
 * 生成需要写入 packet 的 checksum。
 *
 * 调用前必须先把 packet 中原 checksum 字段清零。
 */
static inline uint16_t IPv4_ChecksumGenerate(
    uint32_t sum)
{
    return
        (uint16_t)~
            IPv4_ChecksumFold(sum);
}


ETHDispatchResult IPv4_Handle(
    ETHManager *manager,
    const ETHFrameView *ethernetFrame);


#endif /* CH32V203C8U_IPV4_H */