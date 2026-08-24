//
// Created by dxxdx on 2026/7/13.
//

#ifndef CH32V203C8U_SPI_H
#define CH32V203C8U_SPI_H
#include <stdint.h>

void BSP_SPI_Init(void);

void BSP_DMA_Tx_Init(void);

void BSP_USBD_Init(void);

void BSP_USBH_Init(void);



void BSP_SysTick20ms_Init(void);

extern volatile uint32_t systemTick20ms;


#endif //CH32V203C8U_SPI_H
