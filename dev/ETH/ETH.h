#ifndef CH32V203C8U_ETH_H
#define CH32V203C8U_ETH_H

#include <stdint.h>


#define ETH_FRAME_CAPACITY 1536U

typedef struct ETHManager ETHManager;


/*
 * Ethernet manager 可以调用的底层操作。
 *
 * 具体实现可以是 USBD、SPI Ethernet 或其他链路。
 */
typedef struct
{
    /*
     * 借出已经接收完成的 Ethernet frame。
     * 没有完整帧时返回 NULL。
     */
    const uint8_t *(*const getReceivedFrame)(
        const ETHManager *manager);

    /*
     * 归还当前 RX frame。
     */
    void (*const releaseReceivedFrame)(
        ETHManager *manager);

    /*
     * 借出可写的 TX frame。
     * 当前仍有待发送帧时返回 NULL。
     */
    uint8_t *(*const getTransmitFrame)(
        ETHManager *manager);

    /*
     * 提交 manager->txFrameLength 指定的帧。
     */
    uint8_t (*const transmit)(
        ETHManager *manager);

    uint8_t (*const isAvailable)(
    ETHManager *self);

} ETHOperations;


struct ETHManager
{
    /*
     * manager 随身携带的底层工具。
     *
     * 指向 const 操作表，运行期间不修改。
     */
    const ETHOperations *operations;

    uint8_t rxFrame[
        ETH_FRAME_CAPACITY];

    uint8_t txFrame[
        ETH_FRAME_CAPACITY];

    uint16_t rxWriteOffset;

    volatile uint16_t rxFrameLength;

    uint8_t rxDropping;

    volatile uint8_t rxFrameReady;

    volatile uint16_t txFrameLength;

    volatile uint16_t txReadOffset;

    volatile uint8_t txZLPPending;

    volatile uint8_t txBusy;

    const uint8_t localMAC[6];

};


#define ETH_MANAGER_INITIALIZER(operations_, mac0_, mac1_, mac2_, mac3_, mac4_, mac5_) \
{ \
    .operations = (operations_), \
    .rxWriteOffset = 0U, \
    .rxFrameLength = 0U, \
    .rxDropping = 0U, \
    .rxFrameReady = 0U, \
    .txFrameLength = 0U, \
    .txReadOffset = 0U, \
    .txZLPPending = 0U, \
    .txBusy = 0U, \
    .localMAC = \
    { \
        (mac0_), \
        (mac1_), \
        (mac2_), \
        (mac3_), \
        (mac4_), \
        (mac5_) \
    } \
}

#endif /* CH32V203C8U_ETH_H */
