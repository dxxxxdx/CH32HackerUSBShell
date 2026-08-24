//
// Created by dxxdx on 2026/7/20.
//
/*
 * cold_boot_magic.c
 *
 * CH32V203 冷上电后二次复位实验模块。
 *
 * 行为：
 *   1. 真正 POR/PDR 冷上电：
 *      等待一段时间，然后自动执行一次系统复位。
 *
 *   2. 自动软件复位后的第二次启动：
 *      不再复位，正常返回 main。
 *
 *   3. 普通 NRST 按键复位：
 *      不额外复位，正常返回 main。
 *
 * 注意：
 *   必须在 main() 的最前面调用，放在 GPIO/SPI/DMA 初始化之前。
 */

#include "ch32v20x.h"
#include <stdint.h>


/*
 * 延时循环次数。
 *
 * 这里不追求精确毫秒，只需要让电源、晶振、LCD 电源域等稳定。
 * 96 MHz 下 10,000,000 次 volatile 循环通常已经相当长。
 *
 * 若感觉启动太慢，可以改成：
 *   3000000UL
 *
 * 若仍想更保守，可以改成：
 *   30000000UL
 */
#ifndef COLD_BOOT_DELAY_LOOPS
#define COLD_BOOT_DELAY_LOOPS    10000000UL
#endif


/*
 * 保存本次进入函数时观察到的复位原因，方便调试。
 *
 * bit 0：PIN reset
 * bit 1：POR/PDR reset
 * bit 2：software reset
 * bit 3：independent watchdog reset
 * bit 4：window watchdog reset
 * bit 5：low-power reset
 *
 * 注意：这是普通 .bss 变量，每次启动都会重新清零，
 * 只用于当前这一次启动过程中查看。
 */
volatile uint32_t g_cold_boot_reset_cause = 0U;


/*
 * 纯 CPU 延时。
 *
 * 不依赖 SysTick、SystemCoreClock、定时器或任何外设，
 * 可以在 main() 最开始直接运行。
 */
__attribute__((noinline))
static void ColdBoot_RawDelay(void)
{
    for (volatile uint32_t i = 0U;
         i < COLD_BOOT_DELAY_LOOPS;
         ++i)
    {
        __asm volatile("nop");
    }

    /*
     * 确保此前的寄存器访问已经按顺序完成。
     * 它不是延时的主要来源，只是作为复位前的顺序屏障。
     */
    __asm volatile("fence iorw, iorw" ::: "memory");
}


/*
 * 读取当前复位原因。
 */
static uint32_t ColdBoot_ReadResetCause(void)
{
    uint32_t cause = 0U;

    if (RCC_GetFlagStatus(RCC_FLAG_PINRST) != RESET)
    {
        cause |= (1UL << 0U);
    }

    if (RCC_GetFlagStatus(RCC_FLAG_PORRST) != RESET)
    {
        cause |= (1UL << 1U);
    }

    if (RCC_GetFlagStatus(RCC_FLAG_SFTRST) != RESET)
    {
        cause |= (1UL << 2U);
    }

    if (RCC_GetFlagStatus(RCC_FLAG_IWDGRST) != RESET)
    {
        cause |= (1UL << 3U);
    }

    if (RCC_GetFlagStatus(RCC_FLAG_WWDGRST) != RESET)
    {
        cause |= (1UL << 4U);
    }

    if (RCC_GetFlagStatus(RCC_FLAG_LPWRRST) != RESET)
    {
        cause |= (1UL << 5U);
    }

    return cause;
}


/*
 * 魔法函数：
 *
 * 真正冷上电时自动执行一次：
 *
 *     POR 启动
 *         ↓
 *     延时等待电源稳定
 *         ↓
 *     软件系统复位
 *         ↓
 *     第二次进入 main
 *         ↓
 *     正常初始化 GPIO、SPI、DMA、LCD
 */
void ColdBoot_MagicResetOnce(void)
{
    const uint32_t reset_cause = ColdBoot_ReadResetCause();

    g_cold_boot_reset_cause = reset_cause;

    /*
     * 必须在触发软件复位之前清除旧的 POR 标志。
     *
     * 否则第二次启动时 PORRST 仍可能保持为 1，
     * 从而再次进入复位，形成无限复位循环。
     */
    RCC_ClearFlag();

    __asm volatile("fence iorw, iorw" ::: "memory");

    /*
     * bit 1 表示本次启动包含 POR/PDR 冷启动标志。
     */
    if ((reset_cause & (1UL << 1U)) != 0U)
    {
        /*
         * 第一次冷上电：
         * 不初始化 GPIO、SPI、DMA、LCD，单纯等待。
         */
        ColdBoot_RawDelay();

        /*
         * WCH 提供的系统软件复位。
         * 第二次进入 main 时，POR 标志已经被清除，
         * 因此不会再次走到这里。
         */
        NVIC_SystemReset();

        /*
         * 理论上不会执行到这里。
         */
        while (1)
        {
            __asm volatile("nop");
        }
    }

    /*
     * 软件复位、NRST、看门狗复位等路径直接返回。
     * main() 可以继续初始化外设。
     */
}


static const uint8_t testStream[] =
    /* 清屏并归位。 */
    "\033[2J"
    "\033[H"

    /* 普通字符和颜色。 */
    "VT100 SCREEN\r\n"
    "\033[31mRED "
    "\033[32mGREEN "
    "\033[34mBLUE\r\n"
    "\033[35mBtest"
    "\033[76mtest"
    "\033[0m\r\n"

    /* 刚好占满16列，测试延迟换行。 */
    "0123456789ABCDEF"
    "\r\n"

    /* 退格：最终应该显示 BS:AXY。 */
    "BS:ABC\b\bXY"
    "\r\n"

    /* 回车、绝对横向移动，在第8列写X。 */
    "CURSOR:........"
    "\r"
    "\033[8G"
    "X"
    "\r\n"

    /* 插入两个字符：EDIT:ABXYCDE。 */
    "EDIT:ABCDE"
    "\033[3D"
    "\033[2@"
    "XY"
    "\r\n"

    /* 删除两个字符：DELETE:ABE。 */
    "DELETE:ABCDE"
    "\033[3D"
    "\033[2P"
    "\r\n"

    /* 保存和恢复光标，R会覆盖保存位置。 */
    "SAVE:"
    "\033[s"
    "12345"
    "\033[u"
    "R"
    "\r\n"

    /* 连续输出，测试硬件滚屏。 */
    "SCROLL 01\r\n"
    "SCROLL 02\r\n"
    "SCROLL 03\r\n"
    "SCROLL 04\r\n"
    "SCROLL 05\r\n"
    "SCROLL 06\r\n"
    "SCROLL 07\r\n"
    "SCROLL 08\r\n"
    "SCROLL 09\r\n"
    "SCROLL 10\r\n"
    "SCROLL 11\r\n"
    "SCROLL 12\r\n"
    "SCROLL 13\r\n"
    "SCROLL 14\r\n"
    "SCROLL 15\r\n"
    "SCROLL 16\r\n"
    "SCROLL 17\r\n"
    "SCROLL 18\r\n"
    "SCROLL 19\r\n"
    "SCROLL 20\r\n"
    "SCROLL 21\r\n"
    "SCROLL 22\r\n"

    "\033[33mTEST DONE"
    "\033[0m";
