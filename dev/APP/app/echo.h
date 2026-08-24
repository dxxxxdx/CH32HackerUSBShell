//
// Created by dxxdx on 2026/8/1.
//

#ifndef CH32V203C8U_ECHO_H
#define CH32V203C8U_ECHO_H

#include <stdint.h>

#include "App.h"
#include "LocalShellCommandDispatch.h"

/*
 * echo只保存一个命令行字段。
 * 参数来自Shell命令缓冲区，start()返回前必须复制到自己状态里。
 */
#define ECHO_ARGUMENT_BUFFER_SIZE \
    LOCAL_SHELL_COMMAND_SIZE

#define ECHO_COMMAND_NAME \
    "echo"

#define ECHO_USAGE \
    "echo <text>"

typedef struct Echo
{
    /*
     * 等待process()输出的第一个参数。
     */
    char argumentBuffer[
        ECHO_ARGUMENT_BUFFER_SIZE];

    uint8_t argumentLength;

    uint8_t printPending;

    /*
     * Ctrl+C由Shell截获后通过requestStop()发布到这里。
     */
    uint8_t stopRequested;

} Echo;


extern Echo echo;


LocalShellApplicationStartStatus Echo_Start(
    void *application,
    uint8_t argumentCount,
    const char *const *argumentVector);


void Echo_Input(
    void *application,
    const uint8_t *data,
    uint32_t length);


LocalShellApplicationStatus Echo_Process(
    void *application,
    const LocalShellInherited *inherited);


void Echo_RequestStop(
    void *application);


#define ECHO_APPLICATION_INITIALIZER \
    { \
        .name = ECHO_COMMAND_NAME, \
        .usage = ECHO_USAGE, \
        .instance = &echo, \
        .start = Echo_Start, \
        .input = Echo_Input, \
        .process = Echo_Process, \
        .requestStop = Echo_RequestStop \
    }



#endif //CH32V203C8U_ECHO_H
