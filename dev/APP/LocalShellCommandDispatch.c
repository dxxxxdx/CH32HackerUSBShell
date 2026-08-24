//
// Created by dxxdx on 2026/7/31.
//

#include "LocalShellCommandDispatch.h"

#include "LocalShell.h"


#define LOCAL_SHELL_APPLICATION_POOL_SIZE \
    5U


extern const LocalShellApplication
    appPool[
        LOCAL_SHELL_APPLICATION_POOL_SIZE];


enum
{
    LOCAL_SHELL_ESCAPE_IDLE = 0U,
    LOCAL_SHELL_ESCAPE_BEGIN,
    LOCAL_SHELL_ESCAPE_CSI,
    LOCAL_SHELL_ESCAPE_SS3
};


#define LOCAL_SHELL_ESCAPE_PARAMETER_IGNORE \
    0xFFU


static const uint8_t localShellPrompt[] =
    "\x1B[1;36m"
    "> "
    "\x1B[0m";

static const uint8_t localShellNewLine[] =
    "\r\n";

static const uint8_t localShellSpace[] =
    " ";

static const uint8_t localShellClearScreen[] =
    "\x1B[0m"
    "\x1B[2J"
    "\x1B[H";

static const uint8_t localShellCommandListHeader[] =
    "\r\n"
    "\x1B[1;33m"
    "COMMANDS"
    "\x1B[0m"
    "\r\n";

static const uint8_t localShellCommandNotFound[] =
    "\x1B[1;31m"
    "Command not found: "
    "\x1B[0m";

static const uint8_t localShellTooManyArguments[] =
    "\x1B[1;31m"
    "Too many arguments"
    "\x1B[0m"
    "\r\n";

static const uint8_t localShellCommandRejected[] =
    "\x1B[1;31m"
    "Command rejected"
    "\x1B[0m"
    "\r\n";

static const uint8_t localShellUsagePrefix[] =
    "\x1B[1;33m"
    "Usage: "
    "\x1B[0m";

static const uint8_t localShellCancelLine[] =
    "^C\r\n";


static void LocalShellCommandDispatch_Write(
    LocalShell *shell,
    const uint8_t *data,
    uint32_t length)
{
    shell->inherited.standardOutput
        ->write(
            shell->inherited.standardOutput,
            data,
            length);
}


static uint32_t LocalShellCommandDispatch_TextLength(
    const char *text)
{
    uint32_t length = 0U;

    while (text[length] != '\0')
    {
        length++;
    }

    return length;
}


static uint8_t LocalShellCommandDispatch_TextEqual(
    const char *left,
    const char *right)
{
    uint32_t index = 0U;

    while (left[index] ==
           right[index])
    {
        if (left[index] == '\0')
        {
            return 1U;
        }

        index++;
    }

    return 0U;
}


static void LocalShellCommandDispatch_MoveTerminalCursor(
    LocalShell *shell,
    uint8_t count,
    uint8_t direction)
{
    uint8_t sequence[5];
    uint8_t length = 0U;

    if (count == 0U)
    {
        return;
    }

    sequence[length++] = 0x1BU;
    sequence[length++] = '[';

    if (count >= 10U)
    {
        sequence[length++] =
            (uint8_t)
            ('0' + (count / 10U));
    }

    sequence[length++] =
        (uint8_t)
        ('0' + (count % 10U));

    sequence[length++] =
        direction;

    LocalShellCommandDispatch_Write(
        shell,
        sequence,
        length);
}


static void LocalShellCommandDispatch_MoveLeft(
    LocalShell *shell,
    uint8_t count)
{
    if (count >
        shell->commandDispatch.commandCursor)
    {
        count =
            shell->commandDispatch.commandCursor;
    }

    LocalShellCommandDispatch_MoveTerminalCursor(
        shell,
        count,
        'D');

    shell->commandDispatch.commandCursor -=
        count;
}


static void LocalShellCommandDispatch_MoveRight(
    LocalShell *shell,
    uint8_t count)
{
    const uint8_t remaining =
        shell->commandDispatch.commandLength -
        shell->commandDispatch.commandCursor;

    if (count > remaining)
    {
        count = remaining;
    }

    LocalShellCommandDispatch_MoveTerminalCursor(
        shell,
        count,
        'C');

    shell->commandDispatch.commandCursor +=
        count;
}


static void LocalShellCommandDispatch_MoveHome(
    LocalShell *shell)
{
    LocalShellCommandDispatch_MoveLeft(
        shell,
        shell->commandDispatch.commandCursor);
}


static void LocalShellCommandDispatch_MoveEnd(
    LocalShell *shell)
{
    LocalShellCommandDispatch_MoveRight(
        shell,
        shell->commandDispatch.commandLength -
        shell->commandDispatch.commandCursor);
}


void LocalShellCommandDispatch_PrintPrompt(
    struct LocalShell *shell)
{
    LocalShellCommandDispatch_Write(
        shell,
        localShellPrompt,
        sizeof(localShellPrompt) - 1U);
}


void LocalShellCommandDispatch_Reset(
    struct LocalShell *shell)
{
    shell->commandDispatch.escapeState =
        LOCAL_SHELL_ESCAPE_IDLE;

    shell->commandDispatch.escapeParameter = 0U;

    shell->commandDispatch.commandLength = 0U;
    shell->commandDispatch.commandCursor = 0U;
    shell->commandDispatch.argumentCount = 0U;

    shell->commandDispatch.commandBuffer[0] =
        '\0';
}


static void LocalShellCommandDispatch_RedrawLine(
    LocalShell *shell)
{
    const uint8_t cursorReturn =
        shell->commandDispatch.commandLength -
        shell->commandDispatch.commandCursor;

    LocalShellCommandDispatch_PrintPrompt(
        shell);

    LocalShellCommandDispatch_Write(
        shell,
        (const uint8_t *)
            shell->commandDispatch.commandBuffer,
        shell->commandDispatch.commandLength);

    LocalShellCommandDispatch_MoveTerminalCursor(
        shell,
        cursorReturn,
        'D');
}


static void LocalShellCommandDispatch_PrintApplications(
    LocalShell *shell)
{
    LocalShellCommandDispatch_Write(
        shell,
        localShellCommandListHeader,
        sizeof(localShellCommandListHeader) -
        1U);

    for (uint8_t index = 0U;
         index <
         LOCAL_SHELL_APPLICATION_POOL_SIZE;
         index++)
    {
        /*
         * appPool采用聚合初始化。
         * 未显式填充的槽位name为0，表示没有安装应用。
         */
        if (appPool[index].name == 0)
        {
            continue;
        }

        LocalShellCommandDispatch_Write(
            shell,
            (const uint8_t *)
                appPool[index].name,
            LocalShellCommandDispatch_TextLength(
                appPool[index].name));

        LocalShellCommandDispatch_Write(
            shell,
            localShellNewLine,
            sizeof(localShellNewLine) - 1U);
    }

    LocalShellCommandDispatch_RedrawLine(
        shell);
}


static void LocalShellCommandDispatch_Insert(
    LocalShell *shell,
    uint8_t data)
{
    const uint8_t insertionIndex =
        shell->commandDispatch.commandCursor;

    if (shell->commandDispatch.commandLength >=
        (LOCAL_SHELL_COMMAND_SIZE - 1U))
    {
        return;
    }

    /*
     * 从尾部向右移动，为新字符腾出位置。
     */
    for (uint8_t index =
             shell->commandDispatch.commandLength;
         index > insertionIndex;
         index--)
    {
        shell->commandDispatch.commandBuffer[index] =
            shell->commandDispatch.commandBuffer[index - 1U];
    }

    shell->commandDispatch.commandBuffer[
        insertionIndex] =
            (char)data;

    shell->commandDispatch.commandLength++;
    shell->commandDispatch.commandCursor++;

    shell->commandDispatch.commandBuffer[
        shell->commandDispatch.commandLength] =
            '\0';

    /*
     * 输出新字符以及光标右侧原有内容。
     */
    LocalShellCommandDispatch_Write(
        shell,
        (const uint8_t *)
            &shell->commandDispatch.commandBuffer[
                insertionIndex],
        shell->commandDispatch.commandLength -
        insertionIndex);

    /*
     * 输出后终端光标停在行尾，需要退回逻辑光标位置。
     */
    LocalShellCommandDispatch_MoveTerminalCursor(
        shell,
        shell->commandDispatch.commandLength -
        shell->commandDispatch.commandCursor,
        'D');
}


static void LocalShellCommandDispatch_Backspace(
    LocalShell *shell)
{
    uint8_t index;
    uint8_t tailLength;

    if (shell->commandDispatch.commandCursor == 0U)
    {
        return;
    }

    shell->commandDispatch.commandCursor--;

    for (index =
             shell->commandDispatch.commandCursor;
         (index + 1U) <
             shell->commandDispatch.commandLength;
         index++)
    {
        shell->commandDispatch.commandBuffer[index] =
            shell->commandDispatch.commandBuffer[index + 1U];
    }

    shell->commandDispatch.commandLength--;

    shell->commandDispatch.commandBuffer[
        shell->commandDispatch.commandLength] =
            '\0';

    tailLength =
        shell->commandDispatch.commandLength -
        shell->commandDispatch.commandCursor;

    /*
     * 先把终端光标移到被删除字符的位置。
     */
    LocalShellCommandDispatch_MoveTerminalCursor(
        shell,
        1U,
        'D');

    /*
     * 重绘删除位置右侧的内容。
     */
    LocalShellCommandDispatch_Write(
        shell,
        (const uint8_t *)
            &shell->commandDispatch.commandBuffer[
                shell->commandDispatch.commandCursor],
        tailLength);

    /*
     * 用空格擦除原命令行最后残留的字符。
     */
    LocalShellCommandDispatch_Write(
        shell,
        localShellSpace,
        sizeof(localShellSpace) - 1U);

    /*
     * 恢复逻辑光标位置。
     */
    LocalShellCommandDispatch_MoveTerminalCursor(
        shell,
        tailLength + 1U,
        'D');
}


static void LocalShellCommandDispatch_Delete(
    LocalShell *shell)
{
    uint8_t index;
    uint8_t tailLength;

    if (shell->commandDispatch.commandCursor >=
        shell->commandDispatch.commandLength)
    {
        return;
    }

    for (index =
             shell->commandDispatch.commandCursor;
         (index + 1U) <
             shell->commandDispatch.commandLength;
         index++)
    {
        shell->commandDispatch.commandBuffer[index] =
            shell->commandDispatch.commandBuffer[index + 1U];
    }

    shell->commandDispatch.commandLength--;

    shell->commandDispatch.commandBuffer[
        shell->commandDispatch.commandLength] =
            '\0';

    tailLength =
        shell->commandDispatch.commandLength -
        shell->commandDispatch.commandCursor;

    LocalShellCommandDispatch_Write(
        shell,
        (const uint8_t *)
            &shell->commandDispatch.commandBuffer[
                shell->commandDispatch.commandCursor],
        tailLength);

    LocalShellCommandDispatch_Write(
        shell,
        localShellSpace,
        sizeof(localShellSpace) - 1U);

    LocalShellCommandDispatch_MoveTerminalCursor(
        shell,
        tailLength + 1U,
        'D');
}


static uint8_t
    LocalShellCommandDispatch_ParseArguments(
        LocalShell *shell)
{
    uint8_t readIndex = 0U;
    uint8_t argumentCount = 0U;

    shell->commandDispatch.commandBuffer[
        shell->commandDispatch.commandLength] =
            '\0';

    while (readIndex <
           shell->commandDispatch.commandLength)
    {
        /*
         * 跳过字段之间的连续空格。
         */
        while ((readIndex <
                shell->commandDispatch.commandLength) &&
               (shell->commandDispatch.commandBuffer[
                    readIndex] == ' '))
        {
            readIndex++;
        }

        if (readIndex >=
            shell->commandDispatch.commandLength)
        {
            break;
        }

        if (argumentCount >=
            LOCAL_SHELL_ARGUMENT_COUNT)
        {
            return 0U;
        }

        shell->commandDispatch.argumentVector[
            argumentCount] =
                &shell->commandDispatch.commandBuffer[
                    readIndex];

        argumentCount++;

        while ((readIndex <
                shell->commandDispatch.commandLength) &&
               (shell->commandDispatch.commandBuffer[
                    readIndex] != ' '))
        {
            readIndex++;
        }

        if (readIndex <
            shell->commandDispatch.commandLength)
        {
            shell->commandDispatch.commandBuffer[
                readIndex] =
                    '\0';

            readIndex++;
        }
    }

    shell->commandDispatch.argumentCount =
        argumentCount;

    return 1U;
}


static const LocalShellApplication *
    LocalShellCommandDispatch_FindApplication(
        const char *name)
{
    for (uint8_t index = 0U;
         index <
         LOCAL_SHELL_APPLICATION_POOL_SIZE;
         index++)
    {
        if (appPool[index].name == 0)
        {
            continue;
        }

        if (LocalShellCommandDispatch_TextEqual(
                appPool[index].name,
                name) != 0U)
        {
            return
                &appPool[index];
        }
    }

    return 0;
}


static void LocalShellCommandDispatch_Submit(
    LocalShell *shell)
{
    const LocalShellApplication *application;

    if (LocalShellCommandDispatch_ParseArguments(
            shell) == 0U)
    {
        LocalShellCommandDispatch_Write(
            shell,
            localShellTooManyArguments,
            sizeof(
                localShellTooManyArguments) -
            1U);

        LocalShellCommandDispatch_Reset(
            shell);

        LocalShellCommandDispatch_PrintPrompt(
            shell);

        return;
    }

    /*
     * 输入行只有空格。
     */
    if (shell->commandDispatch.argumentCount == 0U)
    {
        LocalShellCommandDispatch_Reset(
            shell);

        LocalShellCommandDispatch_PrintPrompt(
            shell);

        return;
    }

    application =
        LocalShellCommandDispatch_FindApplication(
            shell->commandDispatch.argumentVector[0]);

    /*
     * 这里的判空是应用池查找失败的合法结果。
     */
    if (application == 0)
    {
        LocalShellCommandDispatch_Write(
            shell,
            localShellCommandNotFound,
            sizeof(localShellCommandNotFound) -
            1U);

        LocalShellCommandDispatch_Write(
            shell,
            (const uint8_t *)
                shell->commandDispatch.argumentVector[0],
            LocalShellCommandDispatch_TextLength(
                shell->commandDispatch.argumentVector[0]));

        LocalShellCommandDispatch_Write(
            shell,
            localShellNewLine,
            sizeof(localShellNewLine) - 1U);

        LocalShellCommandDispatch_Reset(
            shell);

        LocalShellCommandDispatch_PrintPrompt(
            shell);

        return;
    }

    if (application->start(
            application->instance,
            shell->commandDispatch.argumentCount,
            shell->commandDispatch.argumentVector) ==
        LOCAL_SHELL_APPLICATION_START_SUCCESS)
    {
        shell->foregroundApplication =
            application;

        shell->state =
            LOCAL_SHELL_STATE_APPLICATION;

        /*
         * start()返回后，argumentVector中的指针不再有效。
         */
        LocalShellCommandDispatch_Reset(
            shell);

        return;
    }

    LocalShellCommandDispatch_Write(
        shell,
        localShellCommandRejected,
        sizeof(localShellCommandRejected) -
        1U);

    LocalShellCommandDispatch_Write(
        shell,
        localShellUsagePrefix,
        sizeof(localShellUsagePrefix) - 1U);

    LocalShellCommandDispatch_Write(
        shell,
        (const uint8_t *)
            application->usage,
        LocalShellCommandDispatch_TextLength(
            application->usage));

    LocalShellCommandDispatch_Write(
        shell,
        localShellNewLine,
        sizeof(localShellNewLine) - 1U);

    LocalShellCommandDispatch_Reset(
        shell);

    LocalShellCommandDispatch_PrintPrompt(
        shell);
}


static void LocalShellCommandDispatch_HandleCSI(
    LocalShell *shell,
    uint8_t data)
{
    if ((data >= '0') &&
        (data <= '9'))
    {
        if (shell->commandDispatch
                .escapeParameter !=
            LOCAL_SHELL_ESCAPE_PARAMETER_IGNORE)
        {
            shell->commandDispatch
                .escapeParameter =
                (uint8_t)
                (
                    shell->commandDispatch
                        .escapeParameter *
                    10U +
                    (data - '0')
                );
        }

        return;
    }

    /*
     * 暂不支持带修饰键的复合CSI参数。
     */
    if (data == ';')
    {
        shell->commandDispatch
            .escapeParameter =
            LOCAL_SHELL_ESCAPE_PARAMETER_IGNORE;

        return;
    }

    if (shell->commandDispatch
            .escapeParameter !=
        LOCAL_SHELL_ESCAPE_PARAMETER_IGNORE)
    {
        const uint8_t count =
            (shell->commandDispatch
                 .escapeParameter == 0U) ?
            1U :
            shell->commandDispatch
                .escapeParameter;

        switch (data)
        {
        case 'C':
            LocalShellCommandDispatch_MoveRight(
                shell,
                count);
            break;

        case 'D':
            LocalShellCommandDispatch_MoveLeft(
                shell,
                count);
            break;

        case 'H':
            LocalShellCommandDispatch_MoveHome(
                shell);
            break;

        case 'F':
            LocalShellCommandDispatch_MoveEnd(
                shell);
            break;

        case '~':
            switch (shell->commandDispatch
                        .escapeParameter)
            {
            case 1U:
            case 7U:
                LocalShellCommandDispatch_MoveHome(
                    shell);
                break;

            case 3U:
                LocalShellCommandDispatch_Delete(
                    shell);
                break;

            case 4U:
            case 8U:
                LocalShellCommandDispatch_MoveEnd(
                    shell);
                break;

            default:
                break;
            }
            break;

        /*
         * 上下方向键暂不实现命令历史。
         */
        case 'A':
        case 'B':
        default:
            break;
        }
    }

    shell->commandDispatch.escapeState =
        LOCAL_SHELL_ESCAPE_IDLE;

    shell->commandDispatch.escapeParameter = 0U;
}


static void LocalShellCommandDispatch_HandleEscape(
    LocalShell *shell,
    uint8_t data)
{
    if (data == 0x1BU)
    {
        shell->commandDispatch.escapeState =
            LOCAL_SHELL_ESCAPE_BEGIN;

        shell->commandDispatch.escapeParameter = 0U;
        return;
    }

    switch (shell->commandDispatch.escapeState)
    {
    case LOCAL_SHELL_ESCAPE_BEGIN:
        if (data == '[')
        {
            shell->commandDispatch.escapeState =
                LOCAL_SHELL_ESCAPE_CSI;

            shell->commandDispatch.escapeParameter = 0U;
            return;
        }

        if (data == 'O')
        {
            shell->commandDispatch.escapeState =
                LOCAL_SHELL_ESCAPE_SS3;

            return;
        }

        break;

    case LOCAL_SHELL_ESCAPE_CSI:
        LocalShellCommandDispatch_HandleCSI(
            shell,
            data);
        return;

    case LOCAL_SHELL_ESCAPE_SS3:
        if (data == 'H')
        {
            LocalShellCommandDispatch_MoveHome(
                shell);
        }
        else if (data == 'F')
        {
            LocalShellCommandDispatch_MoveEnd(
                shell);
        }
        break;

    default:
        break;
    }

    shell->commandDispatch.escapeState =
        LOCAL_SHELL_ESCAPE_IDLE;

    shell->commandDispatch.escapeParameter = 0U;
}


void LocalShellCommandDispatch_Init(
    struct LocalShell *shell)
{
    LocalShellCommandDispatch_Reset(
        shell);
}


void LocalShellCommandDispatch_Input(
    struct LocalShell *shell,
    uint8_t data)
{
    if (shell->commandDispatch.escapeState !=
        LOCAL_SHELL_ESCAPE_IDLE)
    {
        LocalShellCommandDispatch_HandleEscape(
            shell,
            data);

        return;
    }

    if (data == 0x1BU)
    {
        shell->commandDispatch.escapeState =
            LOCAL_SHELL_ESCAPE_BEGIN;

        shell->commandDispatch.escapeParameter = 0U;
        return;
    }

    /*
     * Ctrl+C：取消当前命令行。
     */
    if (data == 0x03U)
    {
        LocalShellCommandDispatch_Write(
            shell,
            localShellCancelLine,
            sizeof(localShellCancelLine) - 1U);

        LocalShellCommandDispatch_Reset(
            shell);

        LocalShellCommandDispatch_PrintPrompt(
            shell);

        return;
    }

    /*
     * Ctrl+L：清屏并重绘当前命令行。
     */
    if (data == 0x0CU)
    {
        LocalShellCommandDispatch_Write(
            shell,
            localShellClearScreen,
            sizeof(localShellClearScreen) - 1U);

        LocalShellCommandDispatch_RedrawLine(
            shell);

        return;
    }

    /*
     * Tab：打印应用池中的所有命令。
     */
    if (data == '\t')
    {
        LocalShellCommandDispatch_PrintApplications(
            shell);

        return;
    }

    /*
     * Backspace和ASCII Delete均视为向左删除。
     *
     * 键盘真正的Delete键由ESC [ 3 ~处理。
     */
    if ((data == '\b') ||
        (data == 0x7FU))
    {
        LocalShellCommandDispatch_Backspace(
            shell);

        return;
    }

    if ((data == '\r') ||
        (data == '\n'))
    {
        LocalShellCommandDispatch_Write(
            shell,
            localShellNewLine,
            sizeof(localShellNewLine) - 1U);

        LocalShellCommandDispatch_Submit(
            shell);

        return;
    }

    /*
     * 过滤其他控制字符。
     */
    if ((data < 0x20U) ||
        (data > 0x7EU))
    {
        return;
    }

    LocalShellCommandDispatch_Insert(
        shell,
        data);
}
