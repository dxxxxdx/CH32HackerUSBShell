//
// Created by dxxdx on 2026/7/25.
//





//
// Created by dxxdx on 2026/7/25.
//

#include "USBH.h"

#include "ch32v20x.h"
#include "ch32v20x_usb.h"

/* 将协议层抽象 Token 类型映射为 WCH HOST_EP_PID 的 PID 编码。 */
static inline uint8_t USBH_HW_EncodePid(uint8_t tokenType)
{
    switch((USBH_TOKEN_TYPE)tokenType)
    {
        case USBH_TOKEN_SETUP:
            return USB_PID_SETUP;

        case USBH_TOKEN_OUT:
            return USB_PID_OUT;

        case USBH_TOKEN_IN:
            return USB_PID_IN;

        default:
            return 0U;
    }
}


static void USBH_HW_ForceFullSpeed(void)
{
    USBFSH->BASE_CTRL &= ~USBFS_UC_LOW_SPEED;
    USBFSH->HOST_CTRL &= ~USBFS_UH_LOW_SPEED;
    USBFSH->HOST_SETUP &= ~USBFS_UH_PRE_PID_EN;
}


void USBH_HW_Init(USBH_KB *self)
{
    self->HW_status = (uint8_t)USBH_HW_IDLE;
    self->rxLength = 0U;

    USBFSH->HOST_RX_DMA =
        (uint32_t)(uintptr_t)self->rxBuffer.bytes;

    USBFSH->HOST_TX_DMA =
        (uint32_t)(uintptr_t)self->txBuffer.bytes;

    USBFSH->BASE_CTRL |= USBFS_UC_DMA_EN;
}


uint8_t USBH_HW_IsAttached(void)
{
    uint8_t attached =
        ((USBFSH->MIS_ST & USBFS_UMS_DEV_ATTACH) != 0U) ? 1U : 0U;

    if((USBFSH->INT_FG & USBFS_UIF_DETECT) != 0U)
    {
        USBFSH->INT_FG = USBFS_UIF_DETECT;
    }

    return attached;
}


void USBH_HW_SetBusReset(USBH_KB *self, uint8_t enable)
{
    if(enable != 0U)
    {
        USBFSH->HOST_EP_PID = 0U;
        USBFSH->HOST_SETUP &= ~USBFS_UH_SOF_EN;
        USBFSH->HOST_CTRL &= ~USBFS_UH_PORT_EN;
        USBH_HW_SetAddress(0U);
        USBH_HW_ForceFullSpeed();

        self->HW_status = (uint8_t)USBH_HW_IDLE;
        self->rxLength = 0U;

        USBFSH->HOST_CTRL |= USBFS_UH_BUS_RESET;
    }
    else
    {
        USBFSH->HOST_CTRL &= ~USBFS_UH_BUS_RESET;
    }
}


void USBH_HW_EnableAttachedDevice(USBH_KB *self)
{
    USBH_HW_SetAddress(0U);

    if((USBFSH->MIS_ST & USBFS_UMS_DM_LEVEL) != 0U)
    {
        self->deviceSpeed = (uint8_t)USBH_SPEED_LOW;

        USBFSH->BASE_CTRL |= USBFS_UC_LOW_SPEED;
        USBFSH->HOST_CTRL |= USBFS_UH_LOW_SPEED;
        USBFSH->HOST_SETUP |= USBFS_UH_PRE_PID_EN;
    }
    else
    {
        self->deviceSpeed = (uint8_t)USBH_SPEED_FULL;
        USBH_HW_ForceFullSpeed();
    }

    USBFSH->HOST_CTRL |= USBFS_UH_PORT_EN;
    USBFSH->HOST_SETUP |= USBFS_UH_SOF_EN;
}


void USBH_HW_SetAddress(uint8_t address)
{
    USBFSH->DEV_ADDR =
        (USBFSH->DEV_ADDR & USBFS_UDA_GP_BIT) |
        (address & USBFS_USB_ADDR_MASK);
}


void USBH_HW_Disable(USBH_KB *self)
{
    USBFSH->HOST_EP_PID = 0U;
    USBFSH->HOST_CTRL &= ~USBFS_UH_BUS_RESET;
    USBFSH->HOST_SETUP &= ~USBFS_UH_SOF_EN;
    USBFSH->HOST_CTRL &= ~USBFS_UH_PORT_EN;

    USBH_HW_SetAddress(0U);
    USBH_HW_ForceFullSpeed();

    self->HW_status = (uint8_t)USBH_HW_IDLE;
    self->rxLength = 0U;
}


USBH_IO_RESULT USBH_HW_Transact(
    USBH_KB *self,
    USBH_TRANSACTION transaction)
{
    uint8_t response;
    uint8_t pid;

    if((USBH_HW_STATUS)self->HW_status == USBH_HW_IDLE)
    {
        pid = USBH_HW_EncodePid(transaction.tokenType);
        if(pid == 0U)
        {
            return USBH_IO_ERROR;
        }

        self->currentTransaction = transaction;
        self->rxLength = 0U;

        USBFSH->HOST_TX_CTRL =
            (transaction.toggle != 0U) ? USBFS_UH_T_TOG : 0U;

        USBFSH->HOST_RX_CTRL =
            (transaction.toggle != 0U) ? USBFS_UH_R_TOG : 0U;

        if((USBH_TOKEN_TYPE)transaction.tokenType != USBH_TOKEN_IN)
        {
            USBFSH->HOST_TX_LEN = transaction.length;
        }

        /* 写 1 清除上一笔完成标志，再写 PID 启动新事务。 */
        USBFSH->INT_FG = USBFS_UIF_TRANSFER;
        USBFSH->HOST_EP_PID =
            (uint8_t)((pid << 4U) | (transaction.endpoint & 0x0FU));

        self->HW_status = (uint8_t)USBH_HW_WAIT;
        return USBH_IO_BUSY;
    }

    if((USBFSH->INT_FG & USBFS_UIF_TRANSFER) == 0U)
    {
        if((USBFSH->MIS_ST & USBFS_UMS_DEV_ATTACH) == 0U)
        {
            USBFSH->HOST_EP_PID = 0U;
            self->HW_status = (uint8_t)USBH_HW_IDLE;
            return USBH_IO_DISCONNECT;
        }

        return USBH_IO_BUSY;
    }

    USBFSH->HOST_EP_PID = 0U;
    self->HW_status = (uint8_t)USBH_HW_IDLE;

    if((USBFSH->INT_ST & USBFS_UIS_TOG_OK) != 0U)
    {
        if((USBH_TOKEN_TYPE)self->currentTransaction.tokenType == USBH_TOKEN_IN)
        {
            self->rxLength = (uint8_t)USBFSH->RX_LEN;
        }

        return USBH_IO_SUCCESS;
    }

    response = USBFSH->INT_ST & USBFS_UIS_H_RES_MASK;

    if(response == USB_PID_NAK)
    {
        return USBH_IO_NAK;
    }

    if(response == USB_PID_STALL)
    {
        return USBH_IO_STALL;
    }

    if((USBFSH->MIS_ST & USBFS_UMS_DEV_ATTACH) == 0U)
    {
        return USBH_IO_DISCONNECT;
    }

    return USBH_IO_ERROR;
}


