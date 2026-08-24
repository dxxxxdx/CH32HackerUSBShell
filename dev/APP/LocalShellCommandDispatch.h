#ifndef CH32V203C8U_LOCALSHELLCOMMANDDISPATCH_H
#define CH32V203C8U_LOCALSHELLCOMMANDDISPATCH_H

#include <stdint.h>


struct LocalShell;

/*
 * 单条命令占用的最大空间，包含字符串末尾的'\0'。
 */
#define LOCAL_SHELL_COMMAND_SIZE           96U

/*
 * 单条命令允许解析出的最大字段数量。
 *
 * argumentVector[0]为命令名，
 * 后续元素为传递给应用的参数。
 */
#define LOCAL_SHELL_ARGUMENT_COUNT          8U


/*
 * 单行命令编辑器内部状态。
 *
 * 该结构由LocalShell直接持有，
 * 不作为独立对象传递。
 */
typedef struct LocalShellCommandDispatchStage
{
    /*
     * Shell命令行缓冲区。
     */
    char commandBuffer[
        LOCAL_SHELL_COMMAND_SIZE];

    /*
    * 命令提交后，Shell在commandBuffer中原地插入'\0'，
    * 然后让这些指针分别指向各个字段。
    *
    * 这些指针只在调用应用start()期间有效。
    */
       const char *argumentVector[
        LOCAL_SHELL_ARGUMENT_COUNT];
    /*
     * 当前命令长度，不包含末尾的'\0'。
     */
    uint8_t commandLength;

    /*
     * 当前命令行光标位置。
     *
     * 应当始终满足：
     *
     * commandCursor <= commandLength
     */
    uint8_t commandCursor;
    /*
     * argumentVector中当前有效的元素数量。
     */
    uint8_t argumentCount;
    uint8_t escapeState;
    uint8_t escapeParameter;
} LocalShellCommandDispatchStage;


/*
 * 初始化LocalShell内部的命令编辑状态。
 */
void LocalShellCommandDispatch_Init(
    struct LocalShell *shell);


/*
 * 清除当前命令行和转义序列状态。
 *
 * 不输出内容，也不修改Shell前台状态。
 */
void LocalShellCommandDispatch_Reset(
    struct LocalShell *shell);


/*
 * 输出Shell提示符。
 */
void LocalShellCommandDispatch_PrintPrompt(
    struct LocalShell *shell);


/*
 * 向命令编辑器输入一个键盘字节。
 */
void LocalShellCommandDispatch_Input(
    struct LocalShell *shell,
    uint8_t data);


#endif
