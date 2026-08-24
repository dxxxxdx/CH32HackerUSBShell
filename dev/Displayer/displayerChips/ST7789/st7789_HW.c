//
// Created by dxxdx on 2026/8/7.
//

#include "displayerChipConfig.h"

#if (DISPLAYER_CHIP_USE_ST7789 != 0U)

#include "ch32v20x.h"
#include "st7789.h"
#include "system_IT.h"

static const ST7789_IO lcd_io =
{
    .CTRL_BSHR = &GPIOA->BSHR,
    .RST_BSHR  = &GPIOB->BSHR,

    /*
     * PA8 是 DC，PA9 是 CS。
     * 写 BSHR 时同时拉低 CS、拉高 DC，避免命令/数据边界留下旧电平。
     */
    .begin_data =
        GPIO_Pin_8 |
        ((uint32_t)GPIO_Pin_9 << 16U),

    /* CS=0，DC=0。 */
    .begin_cmd =
        (uint32_t)(GPIO_Pin_9 | GPIO_Pin_8) << 16U,

    /* CS=1，DC=1。 */
    .end_send =
        GPIO_Pin_9 | GPIO_Pin_8,

    /* PB12 是 ST7789 硬复位脚。 */
    .rst_low  = (uint32_t)GPIO_Pin_12 << 16U,
    .rst_high = GPIO_Pin_12,
};

void st7789_CH32_Transmit_Block(
    ST7789 *self,
    const void *bitstream,
    uint32_t lengthByMode);

void st7789_CH32_Transmit_DMA_U16(
    ST7789 *self,
    uint16_t *bitstream,
    uint32_t u16Length);

void st7789_CH32_HalfWord_Switch(ST7789 *self);

ST7789 st7789 = {
    .io = &lcd_io,
    .transmitBlock = st7789_CH32_Transmit_Block,
    .transmitDMA_U16 = st7789_CH32_Transmit_DMA_U16,
    .transmit_callback = ST7789_TransmitComplete,
    .status = ST7789_IDLE,
    .SPIMode = ST7789_SPI_U8_MODE,
    .scrollOffset = 0U,
};

__attribute__((always_inline))
static inline void ST7789_WaitIdle(ST7789 *self)
{
    while (self->status != ST7789_IDLE)
    {
        __NOP();
    }
}

void st7789_CH32_Transmit_Block(
    ST7789 *self,
    const void *bitstream,
    uint32_t lengthByMode)
{
    if (self->SPIMode == ST7789_SPI_U8_MODE)
    {
        const uint8_t *data =
            (const uint8_t *)bitstream;

        for (uint32_t i = 0U; i < lengthByMode; ++i)
        {
            while ((SPI2->STATR & SPI_I2S_FLAG_TXE) == 0U)
            {
            }

            /*
             * 8 位命令阶段必须做 byte store，防止编译器为了 DATAR
             * 的寄存器宽度生成 halfword/word store。
             */
            *(volatile uint8_t *)&SPI2->DATAR = data[i];
        }
    }
    else if (self->SPIMode == ST7789_SPI_U16_MODE)
    {
        if (((uintptr_t)bitstream & 1U) != 0U)
        {
            while (1)
            {
                __NOP();
            }
        }

        const uint16_t *data =
            (const uint16_t *)bitstream;

        for (uint32_t i = 0U; i < lengthByMode; ++i)
        {
            while ((SPI2->STATR & SPI_I2S_FLAG_TXE) == 0U)
            {
            }

            SPI2->DATAR = data[i];
        }
    }
    else
    {
        while (1)
        {
            __NOP();
        }
    }

    /*
     * TXE 只表示最后一个数据进了移位寄存器；
     * 必须等 BSY 清零后才能释放 CS。
     */
    while ((SPI2->STATR & SPI_I2S_FLAG_TXE) == 0U)
    {
    }

    while ((SPI2->STATR & SPI_I2S_FLAG_BSY) != 0U)
    {
    }

    self->transmit_callback(self);
}

void st7789_CH32_Transmit_DMA_U16(
    ST7789 *self,
    uint16_t *bitstream,
    uint32_t u16Length)
{
    (void)self;

    DMA1_Channel5->MADDR = (uint32_t)bitstream;
    DMA1_Channel5->CNTR  = (uint16_t)u16Length;
    DMA_Cmd(DMA1_Channel5, ENABLE);
}

void DMA1_Channel5_IRQHandler(void)
{
    if (DMA_GetITStatus(DMA1_IT_TC5) != RESET)
    {
        DMA_ClearITPendingBit(DMA1_IT_TC5);
        DMA_Cmd(DMA1_Channel5, DISABLE);

        /*
         * DMA TC 只代表最后一个 halfword 写进 SPI2->DATAR，
         * 不代表 MOSI 已经移完。
         */
        while ((SPI2->STATR & SPI_I2S_FLAG_BSY) != 0U)
        {
        }

        st7789.transmit_callback(&st7789);
    }

    if (DMA_GetITStatus(DMA1_IT_TE5) != RESET)
    {
        DMA_ClearITPendingBit(DMA1_IT_TE5);
        DMA_Cmd(DMA1_Channel5, DISABLE);
    }
}

void st7789_CH32_HalfWord_Switch(ST7789 *self)
{
    while ((SPI2->STATR & SPI_STATR_TXE) == 0U)
    {
    }

    while ((SPI2->STATR & SPI_STATR_BSY) != 0U)
    {
    }

    if (self->SPIMode == ST7789_SPI_U16_MODE)
    {
        return;
    }

    ST7789_WaitIdle(self);

    while ((SPI2->STATR & SPI_I2S_FLAG_TXE) == 0U)
    {
    }

    while ((SPI2->STATR & SPI_I2S_FLAG_BSY) != 0U)
    {
    }

    SPI_Cmd(SPI2, DISABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI2, ENABLE);

    SPI_InitTypeDef SPI_InitStructure2 = {0};
    SPI_InitStructure2.SPI_Direction = SPI_Direction_1Line_Tx;
    SPI_InitStructure2.SPI_Mode = SPI_Mode_Master;
    SPI_InitStructure2.SPI_DataSize = SPI_DataSize_16b;
    SPI_InitStructure2.SPI_CPOL = SPI_CPOL_Low;
    SPI_InitStructure2.SPI_CPHA = SPI_CPHA_1Edge;
    SPI_InitStructure2.SPI_NSS = SPI_NSS_Soft;
    SPI_InitStructure2.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_2;
    SPI_InitStructure2.SPI_FirstBit = SPI_FirstBit_MSB;
    SPI_InitStructure2.SPI_CRCPolynomial = 7U;
    SPI_Init(SPI2, &SPI_InitStructure2);

    SPI_I2S_DMACmd(SPI2, SPI_I2S_DMAReq_Tx, ENABLE);

    SPI_Cmd(SPI2, ENABLE);
    self->SPIMode = ST7789_SPI_U16_MODE;
    __asm volatile("fence iorw, iorw" ::: "memory");
}

#endif /* DISPLAYER_CHIP_USE_ST7789 */
