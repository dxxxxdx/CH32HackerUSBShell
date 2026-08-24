//
// Created by dxxdx on 2026/8/8.
//

#include "NetworkManager.h"

#include "ARP.h"
#include "BSP.h"
#include "ETH_Transmitter.h"
#include "IPv4.h"
#include "USBD_Ethernet.h"


NetworkManager networkManager
    __attribute__((aligned(4))) =
{
    .ethernet =
        ETH_MANAGER_INITIALIZER(
            &usbdEthernetOperations,
            0x02U,
            0x00U,
            0x00U,
            0x00U,
            0x00U,
            0x02U),

    .tcp =
        TCP_MANAGER_INITIALIZER(
            &systemTick20ms),

    .dispatch =
    {
        .handleARP =
            ARP_Handle,

        .handleIPv4 =
            IPv4_Handle,

        .target =
            ETH_DISPATCH_TARGET_NONE,

        .frame = {0}
    }
};


void NetworkManager_Process(
    NetworkManager *self)
{
    (void)ETH_DispatchFrame(
        &self->dispatch,
        &self->ethernet);

    TCPManager_Process(
        &self->tcp,
        &self->ethernet);

    (void)ETH_TransmitService(
        &self->ethernet);
}
