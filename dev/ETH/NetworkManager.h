//
// Created by dxxdx on 2026/8/8.
//

#ifndef CH32V203C8U_NETWORKMANAGER_H
#define CH32V203C8U_NETWORKMANAGER_H
#include "ETH_Dispatcher.h"
#include "TCP/ETH_TCPManager.h"


typedef struct NetworkManager
{
    /*
     * 纯Ethernet设备收发。
     */
    ETHManager ethernet;

    /*
     * 有状态的TCP协议管理器。
     */
    TCPManager tcp;

    /*
     * Ethernet报文分发临时状态。
     */
    ETHDispatchInfo dispatch;
} NetworkManager;

extern NetworkManager networkManager;



void NetworkManager_Process(
    NetworkManager *self);



#endif //CH32V203C8U_NETWORKMANAGER_H
