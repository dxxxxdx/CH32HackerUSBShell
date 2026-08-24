#include "displayerChipConfig.h"

#if (DISPLAYER_CHIP_USE_ST7735S != 0U)

#include "ch32v20x.h"
#include "st7735s.h"
#include "system_IT.h"
extern ST7735S st7735s;


static const ST7735_IO lcd_io =
{
    .CTRL_BSHR = &GPIOA->BSHR,
    .RST_BSHR  = &GPIOB->BSHR,

    /*
     * CS=0，同时显式保证 DC=1。
     * 仍然只有一次 MMIO，没有任何额外成本。
     */
    .begin_data =
        GPIO_Pin_8 |
        ((uint32_t)GPIO_Pin_9 << 16U),

    /* CS=0，DC=0 */
    .begin_cmd =
        (uint32_t)(GPIO_Pin_9 | GPIO_Pin_8) << 16U,

    /* CS=1，DC=1 */
    .end_send =
        GPIO_Pin_9 | GPIO_Pin_8,

    .rst_low  = (uint32_t)GPIO_Pin_14 << 16U,
    .rst_high = GPIO_Pin_14,
};


void st7735s_CH32_Transmit_Block(ST7735S *self,const void *bitstream,uint32_t lengthByMode);
void st7735s_CH32_HalfWord_Switch(ST7735S *self);
void st7735s_CH32_Transmit_DMA_U16(ST7735S *self,uint16_t* bitstream,uint32_t u16length);

ST7735S st7735s = {
    .io = &lcd_io,
    .status = ST7735S_IDLE,
    .SPIMode = SPI_U8_MODE,
    .scrollOffset = 0U,
    .transmitBlock = st7735s_CH32_Transmit_Block,
    .transmit_callback = ST7735_TransmitComplete,
    .transmitDMA_U16 = st7735s_CH32_Transmit_DMA_U16,
};

__attribute__((always_inline)) static inline void ST7735S_WaitIdle(ST7735S *self)
{
    while(self->status != ST7735S_IDLE)
    {
        __NOP();
    }
}
/*
 * 阻塞式 SPI 发送。
 *
 * LengthByMode 的单位取决于 self->SPIMode：
 *
 * SPI_U8_MODE：
 *   bitstream 指向 uint8_t 数组；
 *   LengthByMode 表示字节数量。
 *
 * SPI_U16_MODE：
 *   bitstream 指向 uint16_t 数组；
 *   LengthByMode 表示 uint16_t 元素数量；
 *   地址必须至少按 2 字节对齐。
 *
 * U16 模式下，SPI 配置为 MSB first：
 *
 *   uint16_t pixel = 0x07FF;
 *
 * 线上发送顺序为 07 FF。
 *
 * 不要把高字节优先排列的 uint8_t 数组直接当作 uint16_t 数组。
 * RV32 为小端，例如 {0x00, 0x02} 会被读取为 0x0200，
 * SPI 最终发送 02 00。
*/void st7735s_CH32_Transmit_Block(
    ST7735S *self,
    const void *bitstream,
    uint32_t lengthByMode)
{
    if (self->SPIMode == SPI_U8_MODE)
    {
        const uint8_t *data = (const uint8_t *)bitstream;

        for (uint32_t i = 0U; i < lengthByMode; ++i)
        {
            while ((SPI2->STATR & SPI_I2S_FLAG_TXE) == 0U) {
            }

            /*
             * 8 位模式最好明确进行 8 位 MMIO 写入，
             * 防止编译器生成 halfword/word store。
             */
            *(volatile uint8_t *)&SPI2->DATAR = data[i];
        }
    }
    else if (self->SPIMode == SPI_U16_MODE)
    {
        if (((uintptr_t)bitstream & 1U) != 0U)
        {
            while (1) {
                __NOP();
            }
        }

        const uint16_t *data = (const uint16_t *)bitstream;

        for (uint32_t i = 0U; i < lengthByMode; ++i)
        {
            while ((SPI2->STATR & SPI_I2S_FLAG_TXE) == 0U) {
            }

            SPI2->DATAR = data[i];
        }
    }
    else
    {
        while (1) {
            __NOP();
        }
    }

    /*
     * TXE：最后一个数据已经进入移位寄存器。
     * BSY=0：最后一个 bit 已经真正发送完成。
     */
    while ((SPI2->STATR & SPI_I2S_FLAG_TXE) == 0U) {
    }

    while ((SPI2->STATR & SPI_I2S_FLAG_BSY) != 0U) {
    }

    self->transmit_callback(self);
}

void st7735s_CH32_Transmit_DMA_U16(ST7735S *self,uint16_t* bitstream,uint32_t u16length)
{
    //这里不注册self到底是捅那个spi通道的，自己要绑就if一下
    DMA1_Channel5->MADDR = (uint32_t)bitstream;
    DMA1_Channel5->CNTR  = (uint16_t)u16length;
    DMA_Cmd(DMA1_Channel5, ENABLE);
}
void DMA1_Channel5_IRQHandler(void)
{
    if(DMA_GetITStatus(DMA1_IT_TC5) != RESET)
    {
        /* 先清中断标志 */
        DMA_ClearITPendingBit(DMA1_IT_TC5);

        /* Normal 模式传完以后关掉，方便下次重装 MADDR/CNTR */
        DMA_Cmd(DMA1_Channel5, DISABLE);

        /*
         * DMA TC 只代表最后一个 halfword 写进 SPI2->DATAR，
         * 不代表 MOSI 已经移完，所以不能立刻拉高 CS。
         */
        while((SPI2->STATR & SPI_I2S_FLAG_BSY) != 0U);
        /* 通知驱动传输完成 */
        st7735s.transmit_callback(&st7735s);
    }

    if(DMA_GetITStatus(DMA1_IT_TE5) != RESET)
    {
        DMA_ClearITPendingBit(DMA1_IT_TE5);
        DMA_Cmd(DMA1_Channel5, DISABLE);
    }
}

void st7735s_CH32_HalfWord_Switch(ST7735S *self)
{

    /* 等发送寄存器可接受数据 */
    while ((SPI2->STATR & SPI_STATR_TXE) == 0U) {
    }

    /* 必须等最后一个 bit 真正发完 */
    while ((SPI2->STATR & SPI_STATR_BSY) != 0U) {
    }

    //这里不注册self到底是捅那个spi通道的，自己要绑就if一下
    if(self->SPIMode == SPI_U16_MODE) return;

    ST7735S_WaitIdle(self);

    while((SPI2->STATR & SPI_I2S_FLAG_TXE) == 0U);
    while((SPI2->STATR & SPI_I2S_FLAG_BSY) != 0U);
    SPI_Cmd(SPI2, DISABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI2, ENABLE);
    SPI_InitTypeDef  SPI_InitStructure2 = {0};

    SPI_InitStructure2.SPI_Direction = SPI_Direction_1Line_Tx;
    SPI_InitStructure2.SPI_Mode = SPI_Mode_Master;
    SPI_InitStructure2.SPI_DataSize = SPI_DataSize_16b;
    SPI_InitStructure2.SPI_CPOL = SPI_CPOL_Low;
    SPI_InitStructure2.SPI_CPHA = SPI_CPHA_1Edge;
    SPI_InitStructure2.SPI_NSS = SPI_NSS_Soft;
    SPI_InitStructure2.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_2;
    SPI_InitStructure2.SPI_FirstBit = SPI_FirstBit_MSB;
    SPI_InitStructure2.SPI_CRCPolynomial = 7;
    SPI_Init(SPI2, &SPI_InitStructure2);

    SPI_I2S_DMACmd(SPI2, SPI_I2S_DMAReq_Tx, ENABLE);

    SPI_Cmd(SPI2, ENABLE);
    self->SPIMode = SPI_U16_MODE;
    __asm volatile("fence iorw, iorw" ::: "memory");
}



#endif /* DISPLAYER_CHIP_USE_ST7735S */
