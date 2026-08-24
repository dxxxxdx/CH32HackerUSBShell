//
// Created by dxxdx on 2026/8/1.
//

#include "echo.h"


static const uint8_t echoNewLine[] =
    "\r\n";


Echo echo;


static uint8_t Echo_CopyArgument(
    Echo *self,
    const char *argument)
{
    uint8_t length = 0U;

    while (argument[length] != '\0')
    {
        if (length >=
            (ECHO_ARGUMENT_BUFFER_SIZE - 1U))
        {
            return 0U;
        }

        self->argumentBuffer[length] =
            argument[length];

        length++;
    }

    self->argumentBuffer[length] =
        '\0';

    self->argumentLength =
        length;

    return 1U;
}


LocalShellApplicationStartStatus Echo_Start(
    void *application,
    uint8_t argumentCount,
    const char *const *argumentVector)
{
    Echo *self =
        application;

    /*
     * argv[0]是命令名，echo只使用后面的第一个参数。
     * 多余参数不参与输出，避免在这里重新实现命令行拼接。
     */
    if (argumentCount < 2U)
    {
        return
            LOCAL_SHELL_APPLICATION_START_REJECTED;
    }

    if (Echo_CopyArgument(
            self,
            argumentVector[1]) == 0U)
    {
        return
            LOCAL_SHELL_APPLICATION_START_REJECTED;
    }

    self->printPending = 1U;
    self->stopRequested = 0U;

    return
        LOCAL_SHELL_APPLICATION_START_SUCCESS;
}


void Echo_Input(
    void *application,
    const uint8_t *data,
    uint32_t length)
{
    /*
     * echo是一次性命令，不消费启动后的键盘输入。
     */
    (void)application;
    (void)data;
    (void)length;
}


LocalShellApplicationStatus Echo_Process(
    void *application,
    const LocalShellInherited *inherited)
{
    Echo *self =
        application;

    if (self->stopRequested != 0U)
    {
        self->printPending = 0U;

        return
            LOCAL_SHELL_APPLICATION_EXIT_SUCCESS;
    }

    if (self->printPending != 0U)
    {
        inherited->standardOutput
            ->write(
                inherited->standardOutput,
                (const uint8_t *)
                    self->argumentBuffer,
                self->argumentLength);

        inherited->standardOutput
            ->write(
                inherited->standardOutput,
                echoNewLine,
                sizeof(echoNewLine) - 1U);

        self->printPending = 0U;
    }

    return
        LOCAL_SHELL_APPLICATION_EXIT_SUCCESS;
}


void Echo_RequestStop(
    void *application)
{
    Echo *self =
        application;

    self->stopRequested = 1U;
}
