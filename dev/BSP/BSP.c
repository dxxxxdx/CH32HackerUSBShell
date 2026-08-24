//
// Created by dxxdx on 2026/7/13.
//

#include "BSP.h"
#include "ch32v20x.h"
#include "usb_regs.h"
#include "system_IT.h"
#include "ch32v20x_usbfs_host.h"

void BSP_SPI_Init(void)
{

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI1, ENABLE);


    SPI_Cmd(SPI1, DISABLE);


    SPI_InitTypeDef  SPI_InitStructure = {0};
    SPI_InitStructure.SPI_Direction = SPI_Direction_1Line_Tx;
    SPI_InitStructure.SPI_Mode = SPI_Mode_Master;
    SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;
    SPI_InitStructure.SPI_CPOL = SPI_CPOL_Low;
    SPI_InitStructure.SPI_CPHA = SPI_CPHA_1Edge;
    SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;
    SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_64;
    SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;
    SPI_InitStructure.SPI_CRCPolynomial = 7;
    SPI_Init(SPI1, &SPI_InitStructure);

    SPI_I2S_DMACmd(SPI1, SPI_I2S_DMAReq_Tx, ENABLE);

    SPI_Cmd(SPI1, ENABLE);


    ////////////////////////////////////////////


    RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI2, ENABLE);

    SPI_Cmd(SPI2, DISABLE);

    SPI_InitTypeDef  SPI_InitStructure2 = {0};
    SPI_InitStructure2.SPI_Direction = SPI_Direction_1Line_Tx;
    SPI_InitStructure2.SPI_Mode = SPI_Mode_Master;
    SPI_InitStructure2.SPI_DataSize = SPI_DataSize_8b;
    SPI_InitStructure2.SPI_CPOL = SPI_CPOL_Low;
    SPI_InitStructure2.SPI_CPHA = SPI_CPHA_1Edge;
    SPI_InitStructure2.SPI_NSS = SPI_NSS_Soft;
    SPI_InitStructure2.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_4;
    SPI_InitStructure2.SPI_FirstBit = SPI_FirstBit_MSB;
    SPI_InitStructure2.SPI_CRCPolynomial = 7;
    SPI_Init(SPI2, &SPI_InitStructure2);

    SPI_I2S_DMACmd(SPI2, SPI_I2S_DMAReq_Tx, ENABLE);

    SPI_Cmd(SPI2, ENABLE);


}



void BSP_DMA_Tx_Init()
{
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);
    DMA_DeInit(DMA1_Channel5);


    DMA_InitTypeDef DMA_InitStructure = {0};
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);

    DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&SPI2->DATAR;
    DMA_InitStructure.DMA_MemoryBaseAddr =0;
    DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralDST;
    DMA_InitStructure.DMA_BufferSize = 0;
    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
    DMA_InitStructure.DMA_Mode = DMA_Mode_Normal;
    DMA_InitStructure.DMA_Priority = DMA_Priority_High;
    DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;
    DMA_Init(DMA1_Channel5, &DMA_InitStructure);
    DMA_ITConfig(DMA1_Channel5, DMA_IT_TC, ENABLE);
    NVIC_SetPriority(DMA1_Channel5_IRQn, 0x20);
    NVIC_EnableIRQ(DMA1_Channel5_IRQn);
    __asm volatile("fence iorw, iorw" ::: "memory");
}





void BSP_USBD_Init(void)
{
    /*
     * 系统 PLL 为 96 MHz。
     *
     * 96 MHz / 2 = 48 MHz。
     */
    RCC_USBCLKConfig(
        RCC_USBCLKSource_PLLCLK_Div2);

    /*
     * 打开 PA11/PA12 USBD 外设时钟。
     */
    RCC_APB1PeriphClockCmd(
        RCC_APB1Periph_USB,
        ENABLE);

    /*
     * 初始化期间关闭内部 D+ 上拉。
     *
     * 此时主机看不到设备。
     */
    EXTEN->EXTEN_CTR &=
        ~EXTEN_USBD_PU_EN;

    /*
     * 强制复位 USBD 协议引擎。
     */
    _SetCNTR(
        CNTR_FRES);

    /*
     * 退出复位。
     *
     * 同时清除 PDWN、FSUSP 等控制位。
     */
    _SetCNTR(0U);

    /*
     * 清除全部中断状态。
     *
     * ISTR 中的事件位写 0 清除。
     */
    _SetISTR(0U);

    /*
     * Buffer Table 放在 PMA 的 0 偏移处。
     *
     * PMA 的 CPU 地址从 0x40006000 开始。
     */
    _SetBTABLE(0U);

    /*
     * 暂时关闭 USB Device Function。
     *
     * Bus Reset 处理完成后再设置 DADDR_EF。
     */
    _SetDADDR(0U);

    /*
     * 配置 USBD 低优先级中断。
     *
     * 该中断与 CAN1 RX0 共用入口。
     */
    NVIC_ClearPendingIRQ(
        USB_LP_CAN1_RX0_IRQn);

    NVIC_SetPriority(
        USB_LP_CAN1_RX0_IRQn,
        0x20U);

    NVIC_EnableIRQ(
        USB_LP_CAN1_RX0_IRQn);

    /*
     * 当前只启用：
     *
     * RESET：主机发出 Bus Reset。
     * CTR：端点完成 SETUP、OUT 或 IN。
     *
     * Suspend 和 Wakeup 后面再加。
     */
    _SetCNTR(
        CNTR_RESETM |
        CNTR_CTRM);

    __asm__ volatile(
        "fence iorw, iorw"
        ::: "memory");

    /*
     * 打开内部 D+ 上拉。
     *
     * 主机从这里开始检测到设备。
     */
    EXTEN->EXTEN_CTR |=
        EXTEN_USBD_PU_EN;
}

void BSP_USBH_Init(void)
{
    /* SYSCLK = 96 MHz，USBFS = 48 MHz */
    RCC_USBCLKConfig(RCC_USBCLKSource_PLLCLK_Div2);
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_USBFS, ENABLE);

    /* 进入 Host 模式 */
    USBFSH->BASE_CTRL = USBFS_UC_HOST_MODE;

    while((USBFSH->BASE_CTRL & USBFS_UC_HOST_MODE) == 0U)
    {
    }

    USBFSH->HOST_CTRL   = 0U;
    USBFSH->HOST_SETUP  = 0U;
    USBFSH->DEV_ADDR    = 0U;

    USBFSH->HOST_EP_MOD =
        USBFS_UH_EP_TX_EN |
        USBFS_UH_EP_RX_EN;

    /*
     * DMA 地址稍后由 USBH_HW 绑定。
     * 地址还是 0 时，不能开启 USBFS_UC_DMA_EN。
     */
    USBFSH->HOST_RX_DMA = 0U;
    USBFSH->HOST_TX_DMA = 0U;

    USBFSH->HOST_RX_CTRL = 0U;
    USBFSH->HOST_TX_CTRL = 0U;
    USBFSH->HOST_TX_LEN  = 0U;

    /*
     * 默认按 Full-Speed 配置。
     * 设备接入并完成总线复位后，再检测实际速率。
     */
    USBFSH->BASE_CTRL =
        USBFS_UC_HOST_MODE |
        USBFS_UC_INT_BUSY;

    USBFSH->INT_FG = 0xFFU;

    /* 纯轮询，不启用 USBFS CPU 中断 */
    USBFSH->INT_EN = 0U;
}




























volatile uint32_t systemTick20ms;
void BSP_SysTick20ms_Init(void)
{
    /*
     * CTLR bit:
     * bit0: 计数器使能
     * bit1: 中断使能
     * bit2: 使用 HCLK
     * bit3: 自动重装
     */

    SysTick->CTLR = 0U;
    SysTick->SR   = 0U;
    SysTick->CNT  = 0U;

    /*
     * 96 MHz 时：
     * 96,000,000 / 50 = 1,920,000
     */
    SysTick->CMP =
        ((uint64_t)SystemCoreClock / 50U) - 1U;

    NVIC_ClearPendingIRQ(SysTicK_IRQn);

    /*
     * 给它较低优先级，不要压 USBFS 和网络中断。
     * 具体优先级仍按你的 PriorityGroup 调整。
     */
    NVIC_SetPriority(SysTicK_IRQn, 0xE0U);
    NVIC_EnableIRQ(SysTicK_IRQn);

    SysTick->CTLR =
        (1U << 0) |
        (1U << 1) |
        (1U << 2) |
        (1U << 3);
}

void SysTick_Handler(void)
{
    SysTick->SR = 0U;
    systemTick20ms++;
}

















