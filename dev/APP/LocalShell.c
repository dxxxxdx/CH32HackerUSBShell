
#include "LocalShell.h"

#include "BSP.h"

#include "LocalShellCommandDispatch.h"


LocalShellInputStatus LocalShell_Input(
    LocalShell *self,
    const uint8_t *source,
    uint32_t length)
{
    uint32_t remainingLength =
        LOCAL_SHELL_INPUT_BUFFER_SIZE -
        self->inputLength;

    /*
     * 剩余空间不足，整包拒绝。
     */
    if (length > remainingLength)
    {
        return LOCAL_SHELL_INPUT_FULL;
    }

    for (uint32_t index = 0U;
         index < length;
         index++)
    {
        self->inputBuffer[
            self->inputWriteIndex] =
                source[index];

        self->inputWriteIndex++;

        if (self->inputWriteIndex >=
            LOCAL_SHELL_INPUT_BUFFER_SIZE)
        {
            self->inputWriteIndex = 0U;
        }
    }

    self->inputLength +=
        (uint8_t)length;

    return LOCAL_SHELL_INPUT_ACCEPTED;
}




#define LOCAL_SHELL_CONTROL_C    0x03U


/*
 * 从输入FIFO取出一个字节。
 *
 * 返回0表示FIFO为空；
 * 返回1表示成功取出一个字节。
 */
static uint8_t LocalShell_TakeInput(
    LocalShell *self,
    uint8_t *data)
{
    if (self->inputLength == 0U)
    {
        return 0U;
    }

    *data =
        self->inputBuffer[
            self->inputReadIndex];

    self->inputReadIndex++;

    if (self->inputReadIndex >=
        LOCAL_SHELL_INPUT_BUFFER_SIZE)
    {
        self->inputReadIndex = 0U;
    }

    self->inputLength--;

    return 1U;
}


/*
 * 将一个输入字节交给当前前台应用。
 *
 * Ctrl+C属于Shell控制字符，不转发给应用，
 * 只通知应用请求退出。
 */
static uint8_t LocalShell_RouteApplicationInput(
    LocalShell *self,
    uint8_t data)
{
    if (data == LOCAL_SHELL_CONTROL_C)
    {
        self->foregroundApplication
            ->requestStop(
                self->foregroundApplication
                    ->instance);

        return 1U;
    }

    self->foregroundApplication
        ->input(
            self->foregroundApplication
                ->instance,
            &data,
            1U);

    return 0U;
}


static void LocalShell_FinishApplication(
    LocalShell *self)
{
    self->foregroundApplication = 0;

    /*
     * Daemon可能已经抢占前台。
     * 这种情况下只释放应用所有权，不恢复命令行界面。
     */
    if (self->state ==
        LOCAL_SHELL_STATE_APPLICATION)
    {
        self->state =
            LOCAL_SHELL_STATE_COMMAND;

        LocalShellCommandDispatch_Reset(
            self);

        LocalShellCommandDispatch_PrintPrompt(
            self);
    }
}


static void LocalShell_ProcessApplication(
    LocalShell *self,
    LocalShellProcessStatus *processStatus)
{
    LocalShellApplicationStatus applicationStatus;

    /*
     * foregroundApplication是可选前台资源。
     * Daemon抢占时它仍可能存在，用于完成分阶段退出。
     */
    if (self->foregroundApplication == 0)
    {
        return;
    }

    *processStatus =
        LOCAL_SHELL_PROCESS_ACTIVE;

    applicationStatus =
        self->foregroundApplication
            ->process(
                self->foregroundApplication
                    ->instance,
                &self->inherited);

    if (applicationStatus ==
        LOCAL_SHELL_APPLICATION_RUNNING)
    {
        return;
    }

    LocalShell_FinishApplication(
        self);
}


LocalShellProcessStatus LocalShell_Process(
    LocalShell *self)
{
    LocalShellProcessStatus status =
        LOCAL_SHELL_PROCESS_IDLE;

    uint8_t data;
    uint8_t processedLength = 0U;

    /*
     * Daemon每轮首先检查键盘和网络状态。
     *
     * 它可能将Shell切换到DAEMON状态，
     * 因此必须在键盘路由之前运行。
     */
    LocalShellDaemon_Process(
        &self->daemon,
        self);

    while (processedLength <
           LOCAL_SHELL_INPUT_BUDGET)
    {
        if (LocalShell_TakeInput(
                self,
                &data) == 0U)
        {
            break;
        }

        processedLength++;

        status =
            LOCAL_SHELL_PROCESS_ACTIVE;

        switch (self->state)
        {
        case LOCAL_SHELL_STATE_COMMAND:
        {
            /*
             * Shell拥有键盘。
             *
             * 后续在这里实现普通字符、退格、
             * 回车提交和方向键处理。
             */
            LocalShellCommandDispatch_Input(self, data);
            break;
        }

        case LOCAL_SHELL_STATE_APPLICATION:
        {
            /*
             * 前台应用拥有键盘。
             *
             * Ctrl+C由Shell截获，其他字节
             * 逐个转发给前台应用。
             */
            const uint8_t stopRequested =
                LocalShell_RouteApplicationInput(
                    self,
                    data);

            /*
             * Ctrl+C后停止继续消费FIFO。
             *
             * 本轮后续应立即给前台应用一个
             * process()时间片处理退出请求。
             */
            if (stopRequested != 0U)
            {
                processedLength =
                    LOCAL_SHELL_INPUT_BUDGET;
            }

            break;
        }

        case LOCAL_SHELL_STATE_DAEMON:
        {
            /*
             * Daemon已经抢占Shell前台。
             *
             * 输入字节已经从FIFO取出，
             * 这里直接丢弃，避免断网错误界面
             * 存续期间键盘FIFO逐渐塞满。
             */
            break;
        }

        default:
        {
            /*
             * LocalShell状态损坏。
             *
             * 不给损坏状态提供隐藏恢复路径，
             * 调试版本可以在这里直接HardFault。
             */
            break;
        }
        }
    }

    LocalShell_ProcessApplication(
        self,
        &status);

    return status;
}

LocalShell Local_Shell;



void LocalShell_Init(
    LocalShell *self,
    NetworkManager *networkManager)
{
    self->inherited.networkManager = networkManager;
    self->inherited.standardOutput = &displayer_Rx;
    self->inherited.tick = &systemTick20ms;

    LocalShellDaemon_Init(&self->daemon);

    self->foregroundApplication = 0;
    self->inputReadIndex = 0U;
    self->inputWriteIndex = 0U;
    self->inputLength = 0U;

    LocalShellCommandDispatch_Init(self);

    self->state = LOCAL_SHELL_STATE_COMMAND;
}
