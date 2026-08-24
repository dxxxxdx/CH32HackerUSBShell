#include "boardGPIOInit.h"
#include "BSP.h"
#include "NetworkManager.h"
#include "screen.h"
#include "displayerRx.h"
#include "USBH.h"
#include <stdint.h>
#include "LocalShell.h"
#include "TCPdebug.h"

/*
 * 供调试器观察。
 */
volatile uint32_t debugLoopsLast20ms = 0U;
volatile uint32_t debugLoopsMin20ms  = UINT32_MAX;
volatile uint32_t debugLoopsMax20ms  = 0U;
volatile uint32_t debugLoopsAvg20ms  = 0U;

volatile uint32_t debugTickGap       = 0U;
volatile uint32_t debugMaxTickGap    = 0U;
volatile uint32_t debugStallCount    = 0U;


int main(void)
{
    GPIO_InitAll2();
    BSP_SysTick20ms_Init();
    for (volatile int i = 0U; i < 100000U; i++);
    BSP_SPI_Init();
    BSP_DMA_Tx_Init();
    BSP_USBH_Init();
    BSP_USBD_Init();
    DisplayerChip_Init(&displayerChip);
    Screen_Init(&screen);
    VT100_Init(&VT100_Instance, &screenOperations, &screen);
    LocalShell_Init(&Local_Shell, &networkManager);


    uint32_t lastTick =
    systemTick20ms;

    uint32_t loopCount = 0U;

    uint32_t sampleSum = 0U;
    uint32_t sampleCount = 0U;


    (void)TCPManager_Listen(
    &networkManager.tcp,
    51888U);

    while (1)
    {
        (void)Screen_Service(
            &screen);

        (void)VT100_Process(
            &VT100_Instance,
            8U);

        NetworkManager_Process(
            &networkManager);

        (void)USBH_Process(
            &usbKeyboard,
            systemTick20ms);

        LocalShell_Process(&Local_Shell);

        // TCPDebugEcho_Process(
        //     &networkManager.tcp);


        /*
         * 到这里才算完成一轮完整的主循环。
         */
        loopCount++;

        uint32_t currentTick =
            systemTick20ms;

        uint32_t elapsedTick =
            currentTick - lastTick;

        if (elapsedTick != 0U)
        {
            /*
             * 正常情况下 elapsedTick == 1。
             *
             * 如果一次跨过多个20ms周期，就取这段时间的
             * 平均循环次数，并记录发生了调度延迟。
             */
            uint32_t loopsPer20ms =
                loopCount / elapsedTick;

            debugLoopsLast20ms =
                loopsPer20ms;

            debugTickGap =
                elapsedTick;

            if (elapsedTick >
                debugMaxTickGap)
            {
                debugMaxTickGap =
                    elapsedTick;
            }

            if (elapsedTick > 1U)
            {
                debugStallCount++;
            }

            if (loopsPer20ms <
                debugLoopsMin20ms)
            {
                debugLoopsMin20ms =
                    loopsPer20ms;
            }

            if (loopsPer20ms >
                debugLoopsMax20ms)
            {
                debugLoopsMax20ms =
                    loopsPer20ms;
            }

            sampleSum +=
                loopsPer20ms;

            sampleCount++;

            /*
             * 每50个采样，也就是大约1秒，更新一次平均值。
             */
            if (sampleCount >= 50U)
            {
                debugLoopsAvg20ms =
                    sampleSum /
                    sampleCount;

                sampleSum = 0U;
                sampleCount = 0U;
            }

            loopCount = 0U;
            lastTick = currentTick;
        }
    }
}
