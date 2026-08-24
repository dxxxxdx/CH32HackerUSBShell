//
// Created by dxxdx on 2026/8/8.
//

#ifndef CH32V203C8U_NMAP_H
#define CH32V203C8U_NMAP_H

#include <stdint.h>

#include "App.h"


#define NMAP_COMMAND_NAME \
    "nmap"

#define NMAP_USAGE \
    "nmap <port|-a>"


/*
 * nmap每次只占用TCPManager中的一个SYN Probe。
 * 扫描全部端口时，当前Probe结束后再启动下一个。
 */
typedef uint8_t NmapState;

enum
{
    NMAP_STATE_IDLE = 0U,
    NMAP_STATE_PRINT_HEADER,
    NMAP_STATE_START_PROBE,
    NMAP_STATE_WAIT_RESULT
};


typedef struct Nmap
{
    /* nmap <port>时保存已经解析好的端口号。 */
    uint16_t requestedPort;

    /* nmap -a时指向常用端口表中的当前项。 */
    uint8_t portIndex;

    uint8_t scanAll;
    uint8_t probeOwned;
    uint8_t stopRequested;
    NmapState state;
} Nmap;


extern Nmap nmap;


LocalShellApplicationStartStatus Nmap_Start(
    void *application,
    uint8_t argumentCount,
    const char *const *argumentVector);


void Nmap_Input(
    void *application,
    const uint8_t *data,
    uint32_t length);


LocalShellApplicationStatus Nmap_Process(
    void *application,
    const LocalShellInherited *inherited);


void Nmap_RequestStop(
    void *application);


#define NMAP_APPLICATION_INITIALIZER \
    { \
        .name = NMAP_COMMAND_NAME, \
        .usage = NMAP_USAGE, \
        .instance = &nmap, \
        .start = Nmap_Start, \
        .input = Nmap_Input, \
        .process = Nmap_Process, \
        .requestStop = Nmap_RequestStop \
    }
























#endif //CH32V203C8U_NMAP_H
