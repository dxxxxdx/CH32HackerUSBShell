/*
 * USBD CDC-ECM 数据端点与 ETHManager 适配。
 *
 * USB Core 不直接认识 ETHManager。
 * Core 到达数据端点或传输状态变化位置时调用本文件的钩子，
 * 由这里绑定具体的 ethernetManager 实例。
 */

#include "USBD_Ethernet.h"

#include <stddef.h>
#include <stdint.h>

#include "NetworkManager.h"
#include "USBD.h"
#include "USBD_Internal.h"
#include "system_IT.h"
#include "ch32v20x.h"
#include "usb_regs.h"


static void USB_EthernetResetTransferState(
    ETHManager *manager)
{
    manager->rxWriteOffset = 0U;
    manager->rxFrameLength = 0U;
    manager->rxDropping = 0U;
    manager->rxFrameReady = 0U;

    manager->txFrameLength = 0U;
    manager->txReadOffset = 0U;
    manager->txZLPPending = 0U;
    manager->txBusy = 0U;
}


static void USB_EthernetRearmOutEndpoint(void)
{
    USB_SetEndpointRxCapacity(
        USB_ETHERNET_OUT_ENDPOINT,
        USB_FULL_SPEED_PACKET_SIZE);

    _SetEPRxStatus(
        USB_ETHERNET_OUT_ENDPOINT,
        EP_RX_VALID);
}


/*
 * 把 txFrame 的下一段挂到 EP3。
 *
 * 第一次由主循环发送接口调用。
 * 后续由 EP3 IN 完成 transaction 钩子调用。
 */
static void USB_EthernetArmNextInPacket(
    ETHManager *manager)
{
    if (manager->txBusy == 0U)
    {
        _SetEPTxCount(
            USB_ETHERNET_IN_ENDPOINT,
            0U);

        _SetEPTxStatus(
            USB_ETHERNET_IN_ENDPOINT,
            EP_TX_NAK);

        return;
    }

    if (manager->txReadOffset <
        manager->txFrameLength)
    {
        uint16_t packetLength =
            manager->txFrameLength -
            manager->txReadOffset;

        if (packetLength >
            USB_FULL_SPEED_PACKET_SIZE)
        {
            packetLength =
                USB_FULL_SPEED_PACKET_SIZE;
        }

        USB_PMA_Write(
            USB_PMA_EP3_TX_ADDRESS,
            &manager->txFrame[
                manager->txReadOffset],
            packetLength);

        manager->txReadOffset +=
            packetLength;

        _SetEPTxCount(
            USB_ETHERNET_IN_ENDPOINT,
            packetLength);

        __asm__ volatile(
            "fence iorw, iorw"
            ::: "memory");

        _SetEPTxStatus(
            USB_ETHERNET_IN_ENDPOINT,
            EP_TX_VALID);

        return;
    }

    if (manager->txZLPPending != 0U)
    {
        manager->txZLPPending = 0U;

        _SetEPTxCount(
            USB_ETHERNET_IN_ENDPOINT,
            0U);

        _SetEPTxStatus(
            USB_ETHERNET_IN_ENDPOINT,
            EP_TX_VALID);

        return;
    }

    _SetEPTxCount(
        USB_ETHERNET_IN_ENDPOINT,
        0U);

    _SetEPTxStatus(
        USB_ETHERNET_IN_ENDPOINT,
        EP_TX_NAK);

    manager->txFrameLength = 0U;
    manager->txReadOffset = 0U;

    __asm__ volatile(
        "fence rw, rw"
        ::: "memory");

    /*
     * 最后清 busy，主循环随后才允许改写 txFrame。
     */
    manager->txBusy = 0U;
}


/*
 * Bus Reset、SET_CONFIGURATION 和 SET_INTERFACE
 * 到达原有状态清理位置时调用。
 */
void USBD_HookResetEthernetTransferState(void)
{
    USB_EthernetResetTransferState(
        &networkManager.ethernet);
}


/*
 * EP2 OUT transaction 完成钩子。
 *
 * 这里保留原有 frame 拼接、超长帧丢弃和所有权发布顺序。
 */
void USBD_HookEthernetOutTransaction(void)
{
    ETHManager *const manager =
        &networkManager.ethernet;

    const uint16_t packetLength =
        _GetEPRxCount(
            USB_ETHERNET_OUT_ENDPOINT);

    /*
     * 完整帧发布后，EP2 应一直保持 NAK，
     * 直到主循环显式释放 rxFrame。
     *
     * 按正常所有权流程，这个分支不应收到新 packet。
     * 如果仍然进入，保留旧帧并继续关闭 EP2。
     */
    if (manager->rxFrameReady != 0U)
    {
        _SetEPRxStatus(
            USB_ETHERNET_OUT_ENDPOINT,
            EP_RX_NAK);

        return;
    }

    if (manager->rxDropping == 0U)
    {
        if ((manager->rxWriteOffset <=
             ETH_FRAME_CAPACITY) &&
            (packetLength <=
             (ETH_FRAME_CAPACITY -
              manager->rxWriteOffset)))
        {
            USB_PMA_Read(
                &manager->rxFrame[
                    manager->rxWriteOffset],
                USB_PMA_EP2_RX_ADDRESS,
                packetLength);

            manager->rxWriteOffset +=
                packetLength;
        }
        else
        {
            /*
             * 当前帧超过 rxFrame 容量。
             *
             * 不能立刻把下一包当成新帧。
             * 必须一直丢弃到 short packet 或 ZLP，
             * 才重新获得 ECM frame 边界。
             */
            manager->rxDropping = 1U;
        }
    }

    /*
     * short packet 或 ZLP 结束一个 ECM frame。
     */
    if (packetLength <
        USB_FULL_SPEED_PACKET_SIZE)
    {
        if (manager->rxDropping != 0U)
        {
            manager->rxWriteOffset = 0U;
            manager->rxDropping = 0U;
        }
        else if (manager->rxWriteOffset != 0U)
        {
            manager->rxFrameLength =
                manager->rxWriteOffset;

            manager->rxWriteOffset = 0U;

            __asm__ volatile(
                "fence rw, rw"
                ::: "memory");

            /*
             * 最后置 ready。
             * 主循环随后才允许读取完整帧。
             */
            manager->rxFrameReady = 1U;

            /*
             * 不重新置 VALID。
             *
             * 当前 rxFrame 的所有权已经交给主循环。
             * USBD_EthernetReleaseReceivedFrame()
             * 会在归还缓冲区后重新开放 EP2。
             */
            _SetEPRxStatus(
                USB_ETHERNET_OUT_ENDPOINT,
                EP_RX_NAK);

            return;
        }
    }

    /*
     * 当前 frame 尚未结束，或无效 frame 已经丢弃完毕。
     * 缓冲区仍归 USB 所有，可以继续接收。
     */
    USB_EthernetRearmOutEndpoint();
}


/*
 * EP3 IN transaction 完成钩子。
 */
void USBD_HookEthernetInTransaction(void)
{
    USB_EthernetArmNextInPacket(
        &networkManager.ethernet);
}

__attribute__((always_inline)) inline
const uint8_t *USBD_EthernetGetReceivedFrame(
    const ETHManager *manager)
{
    if (manager->rxFrameReady == 0U)
    {
        return NULL;
    }

    /*
     * ready 在完整帧写入后才会置位。
     */
    __asm__ volatile(
        "fence rw, rw"
        ::: "memory");

    return
        manager->rxFrame;
}


void USBD_EthernetReleaseReceivedFrame(
    ETHManager *manager)
{
    /*
     * 本接口只由主循环调用。
     *
     * 防止清 ready 与重新置 EP2 VALID 之间，
     * 插入 Bus Reset 或 SET_INTERFACE 中断。
     */
    NVIC_DisableIRQ(
        USB_LP_CAN1_RX0_IRQn);

    if (manager->rxFrameReady == 0U)
    {
        NVIC_EnableIRQ(
            USB_LP_CAN1_RX0_IRQn);

        return;
    }

    manager->rxFrameLength = 0U;

    __asm__ volatile(
        "fence rw, rw"
        ::: "memory");

    /*
     * 最后清 ready。
     * USB 中断随后才允许把新帧写入 rxFrame。
     */
    manager->rxFrameReady = 0U;

    __asm__ volatile(
        "fence iorw, iorw"
        ::: "memory");

    /*
     * Alt 1 仍然有效时，缓冲区重新归 USB 所有。
     *
     * 如果接口已被关闭，则保持 EP2 不接收；
     * 下次 SET_INTERFACE(Alt 1) 会重新初始化并开放它。
     */
    if (USBD_IsDataInterfaceEnabled() != 0U)
    {
        USB_EthernetRearmOutEndpoint();
    }

    NVIC_EnableIRQ(
        USB_LP_CAN1_RX0_IRQn);
}

__attribute__((always_inline)) inline
uint8_t *USBD_EthernetGetTransmitFrame(
    ETHManager *manager)
{
    if (
        (manager->txBusy != 0U) ||
        (manager->txFrameLength != 0U))
    {
        return NULL;
    }

    return manager->txFrame;
}


uint8_t USBD_EthernetTransmit(
    ETHManager *manager)
{
    /*
     * 主循环会频繁调用本函数。
     *
     * 没有待发送帧或上一帧仍在发送时，
     * 直接从热路径返回，不操作 NVIC。
     */
    if (__builtin_expect(
            (manager->txBusy != 0U) ||
            (manager->txFrameLength == 0U),
            1))
    {
        return 0U;
    }

    /*
     * Data Interface 尚未进入 Alt 1。
     */
    if (__builtin_expect(
            USBD_IsDataInterfaceEnabled() == 0U,
            0))
    {
        return 0U;
    }

    /*
     * 防止检查结束后发生 Bus Reset 或 SET_INTERFACE，
     * 所以关闭 IRQ 后必须重新检查一次。
     */
    NVIC_DisableIRQ(
        USB_LP_CAN1_RX0_IRQn);

    const uint16_t frameLength =
        manager->txFrameLength;

    if ((manager->txBusy != 0U) ||
        (USBD_IsDataInterfaceEnabled() == 0U) ||
        (frameLength == 0U))
    {
        NVIC_EnableIRQ(
            USB_LP_CAN1_RX0_IRQn);

        return 0U;
    }

    /*
     * 非法长度直接丢弃，避免主循环永久重试。
     */
    if (frameLength >
        ETH_FRAME_CAPACITY)
    {
        manager->txFrameLength = 0U;

        NVIC_EnableIRQ(
            USB_LP_CAN1_RX0_IRQn);

        return 0U;
    }

    manager->txReadOffset = 0U;

    manager->txZLPPending =
        (uint8_t)(
            (frameLength %
             USB_FULL_SPEED_PACKET_SIZE) == 0U);

    /*
     * 发布完整发送状态后，才允许 EP3 读取 txFrame。
     */
    __asm__ volatile(
        "fence rw, rw"
        ::: "memory");

    manager->txBusy = 1U;

    /*
     * 主动提交第一包。
     * 后续数据由 EP3 IN 完成中断继续发送。
     */
    USB_EthernetArmNextInPacket(
        manager);

    NVIC_EnableIRQ(
        USB_LP_CAN1_RX0_IRQn);

    return 1U;
}

static uint8_t USBD_EthernetIsAvailable(
    ETHManager *self)
{
    /*
     * 当前USBD只有一个静态实例，
     * 可用状态由USBD全局设备上下文提供。
     */
    (void)self;

    return
        USBD_IsDataInterfaceEnabled();
}
/*
 * USBD 提供给 ETHManager 的四个操作。
 *
 * 该表位于只读区，不产生可写 BSS。
 */
const ETHOperations
    usbdEthernetOperations =
{
    .isAvailable =
        USBD_EthernetIsAvailable,

    .getReceivedFrame =
        USBD_EthernetGetReceivedFrame,

    .releaseReceivedFrame =
        USBD_EthernetReleaseReceivedFrame,

    .getTransmitFrame =
        USBD_EthernetGetTransmitFrame,

    .transmit =
        USBD_EthernetTransmit
};
