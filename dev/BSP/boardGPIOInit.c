//
// Created by dxxdx on 2026/7/11.
//

#include "boardGPIOInit.h"


void SetSysClockTo96_HSE(void)
{
    __IO uint32_t StartUpCounter = 0, HSEStatus = 0;

    RCC->CTLR |= ((uint32_t)RCC_HSEON);

    /* Wait till HSE is ready and if Time out is reached exit */
    do
    {
        HSEStatus = RCC->CTLR & RCC_HSERDY;
        StartUpCounter++;
    } while((HSEStatus == 0) && (StartUpCounter != HSE_STARTUP_TIMEOUT));

    if ((RCC->CTLR & RCC_HSERDY) != RESET)
    {
        HSEStatus = (uint32_t)0x01;
    }
    else
    {
        HSEStatus = (uint32_t)0x00;
    }

    if (HSEStatus == (uint32_t)0x01)
    {
        /* HCLK = SYSCLK */
        RCC->CFGR0 |= (uint32_t)RCC_HPRE_DIV1;
        /* PCLK2 = HCLK */
        RCC->CFGR0 |= (uint32_t)RCC_PPRE2_DIV1;
        /* PCLK1 = HCLK */
        RCC->CFGR0 |= (uint32_t)RCC_PPRE1_DIV2;

        /*  CH32V20x_D6-PLL configuration: PLLCLK = HSE * 12 = 96 MHz (HSE=8MHZ)
         *  CH32V20x_D8-PLL configuration: PLLCLK = HSE/4 * 12 = 96 MHz (HSE=32MHZ)
         *  CH32V20x_D8W-PLL configuration: PLLCLK = HSE/4 * 12 = 96 MHz (HSE=32MHZ)
         */
        RCC->CFGR0 &= (uint32_t)((uint32_t)~(RCC_PLLSRC | RCC_PLLXTPRE |
                                            RCC_PLLMULL));

        RCC->CFGR0 |= (uint32_t)(RCC_PLLSRC_HSE | RCC_PLLXTPRE_HSE | RCC_PLLMULL12);

        /* Enable PLL */
        RCC->CTLR |= RCC_PLLON;
        /* Wait till PLL is ready */
        while((RCC->CTLR & RCC_PLLRDY) == 0)
        {
        }
        /* Select PLL as system clock source */
        RCC->CFGR0 &= (uint32_t)((uint32_t)~(RCC_SW));
        RCC->CFGR0 |= (uint32_t)RCC_SW_PLL;
        /* Wait till PLL is used as system clock source */
        while ((RCC->CFGR0 & (uint32_t)RCC_SWS) != (uint32_t)0x08)
        {
        }
    }
    else
    {
        /*
         * If HSE fails to start-up, the application will have wrong clock
     * configuration. User can add here some code to deal with this error
         */
    }
}

void GPIO_InitAll()
{

    RCC_APB2PeriphClockCmd(
        RCC_APB2Periph_GPIOA |
        RCC_APB2Periph_GPIOB |
        RCC_APB2Periph_GPIOC,
        ENABLE
    );
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
        /*
         * 开漏输出先把 OUTDR 置 1：
         * PB8/PB9、PC13/PC14 切成开漏后立即处于“释放”状态。
         */
        GPIOB->BSHR = 0x00000300U;
        GPIOC->BSHR = 0x00006000U;

        /* PA2/3/4：通用推挽输出，10 MHz，配置值 0x1 5/7复用输出 */
        GPIOA->CFGLR = (GPIOA->CFGLR & ~0xF0FFFF00U) | 0x90911100U;


        /* PA8/9/15 推挽输出 10 MHz，PA11/12 USBD 浮空输入 */
        GPIOA->CFGHR = (GPIOA->CFGHR & ~0xF00FF0FFU) | 0x10044011U;

        /* PB0/3/4/5：通用推挽输出，10 MHz */
        GPIOB->CFGLR =
            (GPIOB->CFGLR & ~0x00FFF00FU) | 0x00111001U;

        /*
         * PB8/9：开漏输出 10 MHz，0x5
         * PB10/11/14：普通推挽输出 10 MHz，0x1
         * PB13：SPI2_SCK，复用推挽输出 10 MHz，0x9
         * PB15：SPI2_MOSI，复用推挽输出 10 MHz，0x9
         * PB12：保持原配置
         */
        GPIOB->CFGHR =
        (GPIOB->CFGHR & ~0xFFF0FFFFU) | 0x91901155U;
        /* PC13/14：通用开漏输出，10 MHz */
        GPIOC->CFGHR =
            (GPIOC->CFGHR & ~0x0FF00000U) | 0x05500000U;

        __asm volatile("fence iorw, iorw" ::: "memory");


}



//懒得改bsp的抽象层了
void GPIO_InitAll2()
{

    RCC_APB2PeriphClockCmd(
        RCC_APB2Periph_GPIOA |
        RCC_APB2Periph_GPIOB |
        RCC_APB2Periph_GPIOC,
        ENABLE
    );
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
    /*
    * 先写 OUTDR，再切换 GPIO 模式：
    *
    * PA8  DC2  = 1
    * PA9  CS2  = 1，禁止选中屏幕
    *
    * PB12 RST  = 1，退出复位
    * PB13 SCK  = 0，SPI Mode 0 空闲电平
    * PB15 MOSI = 0
    */
    GPIOA->BSHR = 0x00000300U;
    GPIOB->BSHR = 0xA0001000U;

    /*
     * PA8/PA9：
     * 普通推挽输出，10 MHz，配置值 0x1
     */
    GPIOA->CFGHR =
        (GPIOA->CFGHR & ~0x000000FFU) |
        0x00000011U;

    /*
     * PB12：普通推挽输出，10 MHz，0x1
     * PB13：SPI2_SCK，复用推挽，10 MHz，0x9
     * PB14：SPI2_MISO，浮空输入，0x4
     * PB15：SPI2_MOSI，复用推挽，10 MHz，0x9
     *
     * 从高到低：PB15..PB12 = 9 4 9 1
     */
    GPIOB->CFGHR =
        (GPIOB->CFGHR & ~0xFFFF0000U) |
        0x94910000U;

    __asm volatile("fence iorw, iorw" ::: "memory");


}
