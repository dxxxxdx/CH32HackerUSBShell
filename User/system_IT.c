//
// Created by dxxdx on 2026/7/9.
//

#include "system_IT.h"

#include "ch32v20x.h"

void HardFault_Handler(void)
{
    NVIC_SystemReset();
    while (1)
    {
    }
}
