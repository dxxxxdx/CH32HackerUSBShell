//
// Created by dxxdx on 2026/7/31.
//

#include "LocalShellDaemon.h"


#include "LocalShell.h"

#if defined(__GNUC__) || defined(__clang__)
#define LOCAL_SHELL_DAEMON_LIKELY(condition) \
__builtin_expect(!!(condition), 1)
#define LOCAL_SHELL_DAEMON_UNLIKELY(condition) \
__builtin_expect(!!(condition), 0)
#else
#define LOCAL_SHELL_DAEMON_LIKELY(condition)   (condition)
#define LOCAL_SHELL_DAEMON_UNLIKELY(condition) (condition)
#endif

/*
 * 系统Tick周期为20 ms。
 *
 * 2000 ms / 20 ms = 100 Tick。
 */
#define LOCAL_SHELL_DAEMON_START_DELAY_TICKS \
    100U

static const uint8_t localShellDaemonNetworkError[] =
    "\x1B[0m"
    "\x1B[2J"
    "\x1B[H"

    "\x1B[1;31m"
    "SYSTEM OFFLINE"
    "\x1B[0m"
    "\r\n\r\n"

    "\x1B[31m"
    "NET : DOWN"
    "\x1B[0m"
    "\r\n"

    "\x1B[32m"
    "KBD : READY"
    "\x1B[0m"
    "\r\n\r\n"

    "\x1B[33m"
    "Waiting for network..."
    "\x1B[0m"
    "\r\n";

static const uint8_t localShellDaemonKeyboardError[] =
    "\x1B[0m"
    "\x1B[2J"
    "\x1B[H"

    "\x1B[1;31m"
    "SYSTEM OFFLINE"
    "\x1B[0m"
    "\r\n\r\n"

    "\x1B[32m"
    "NET : READY"
    "\x1B[0m"
    "\r\n"

    "\x1B[31m"
    "KBD : DOWN"
    "\x1B[0m"
    "\r\n\r\n"

    "\x1B[33m"
    "Waiting for keyboard..."
    "\x1B[0m"
    "\r\n";

static const uint8_t localShellDaemonAllError[] =
    "\x1B[0m"
    "\x1B[2J"
    "\x1B[H"

    "\x1B[1;31m"
    "SYSTEM OFFLINE"
    "\x1B[0m"
    "\r\n\r\n"

    "\x1B[31m"
    "NET : DOWN"
    "\x1B[0m"
    "\r\n"

    "\x1B[31m"
    "KBD : DOWN"
    "\x1B[0m"
    "\r\n\r\n"

    "\x1B[33m"
    "Waiting for devices..."
    "\x1B[0m"
    "\r\n";
static const uint8_t
    localShellDaemonPowerOnPenguin[] =
    "\x1B[0m"
    "\x1B[2J"
    "\x1B[H"

    "\x1B[1;37m"
    "        .--."
    "\x1B[0m"
    "\r\n"

    "\x1B[1;37m"
    "       |"
    "\x1B[1;36m"
    "o_o"
    "\x1B[1;37m"
    " |"
    "\x1B[0m"
    "\r\n"

    "\x1B[1;37m"
    "       |:"
    "\x1B[1;33m"
    "_/"
    "\x1B[1;37m"
    " |"
    "\x1B[0m"
    "\r\n"

    "\x1B[1;37m"
    "      //   \\ \\"
    "\x1B[0m"
    "\r\n"

    "\x1B[1;37m"
    "     (|     | )"
    "\x1B[0m"
    "\r\n"

    "\x1B[1;37m"
    "    /'\\_   _/`\\"
    "\x1B[0m"
    "\r\n"

    "\x1B[1;33m"
    "    \\___)=(___/"
    "\x1B[0m"
    "\r\n\r\n"

    "\x1B[1;36m"
    "       TINY CHIP."
    "\x1B[0m"
    "\r\n"

    "\x1B[1;35m"
    "     SERIOUS SHELL."
    "\x1B[0m"
    "\r\n\r\n"

    "\x1B[33m"
    "   Feeding USB for 2s..."
    "\x1B[0m"
    "\r\n"
;


static const uint8_t
    localShellDaemonReady[] =

    "\x1B[0m"
    "\x1B[2J"
    "\x1B[H"

    "\x1B[1;32m"
    "SYSTEM READY"
    "\x1B[0m"
    "\r\n\r\n"

    "\x1B[32m"
    "NET  [OK]"
    "\x1B[0m"
    "\r\n"

    "\x1B[32m"
    "KBD  [OK]"
    "\x1B[0m"
    "\r\n\r\n"
;

void LocalShellDaemon_Init(
    LocalShellDaemon *self)
{
    self->delayBeginTick = 0U;

    self->networkAvailable =
        LOCAL_SHELL_DAEMON_AVAILABILITY_UNKNOWN;

    self->keyboardAvailable =
        LOCAL_SHELL_DAEMON_AVAILABILITY_UNKNOWN;
}


void LocalShellDaemon_Process(
    LocalShellDaemon *self,
    struct LocalShell *shell)
{
    /*
     * 第一次运行只记录时间，不立即判断设备状态。
     *
     * USB ECM和USB键盘都需要时间完成枚举，
     * 上电瞬间的不可用状态没有判断价值。
     */
    if (self->networkAvailable ==
    LOCAL_SHELL_DAEMON_AVAILABILITY_UNKNOWN)
    {
        const uint16_t nowTick =
            (uint16_t)
            (*shell->inherited.tick);

        self->delayBeginTick =
            nowTick;

        self->networkAvailable =
            LOCAL_SHELL_DAEMON_AVAILABILITY_WAITING;

        self->keyboardAvailable =
            LOCAL_SHELL_DAEMON_AVAILABILITY_WAITING;

        /*
         * 企鹅获得两秒钟终端最高控制权。
         *
         * 这期间Shell输入会被消费并丢弃，
         * USB键盘和ECM可以在后台完成枚举。
         */
        shell->state =
            LOCAL_SHELL_STATE_DAEMON;

        shell->inherited.standardOutput
            ->write(
                shell->inherited.standardOutput,
                localShellDaemonPowerOnPenguin,
                sizeof(
                    localShellDaemonPowerOnPenguin) -
                1U);

        return;
    }

    /*
     * 非阻塞等待2秒。
     *
     * uint16_t无符号减法允许Tick低16位回绕。
     */
    if (self->networkAvailable ==
        LOCAL_SHELL_DAEMON_AVAILABILITY_WAITING)
    {
        const uint16_t nowTick =
            (uint16_t)
            (*shell->inherited.tick);

        const uint16_t elapsedTick =
            (uint16_t)
            (nowTick -
             self->delayBeginTick);

        if (elapsedTick <
            LOCAL_SHELL_DAEMON_START_DELAY_TICKS)
        {
            return;
        }
    }

    ETHManager *const ethernet =
        &shell->inherited.networkManager->ethernet;

    const uint8_t networkAvailable =
        ethernet->operations
            ->isAvailable(
                ethernet);

    const uint8_t keyboardAvailable =
        usbHostKeyboardAvailable;

    /*
     * 热路径：网络和键盘都已经在线，命令态继续运行。
     *
     * 稳定在线时只发布最新可用状态，然后直接返回；
     * 只有刚从Daemon恢复时才清屏并重新输出提示。
     */
    if ((networkAvailable != 0U) &&
        (keyboardAvailable != 0U))
    {
        self->networkAvailable =
            networkAvailable;

        self->keyboardAvailable =
            keyboardAvailable;

        if (shell->state !=
            LOCAL_SHELL_STATE_DAEMON)
        {
            return;
        }

        LocalShellCommandDispatch_Reset(
            shell);

        shell->state =
            LOCAL_SHELL_STATE_COMMAND;

        shell->inherited.standardOutput
            ->write(
                shell->inherited.standardOutput,
                localShellDaemonReady,
                sizeof(
                    localShellDaemonReady) -
                1U);

        return;
    }

    const uint8_t availabilityChanged =
        (
            (networkAvailable !=
             self->networkAvailable) ||
            (keyboardAvailable !=
             self->keyboardAvailable)
        );

    self->networkAvailable =
        networkAvailable;

    self->keyboardAvailable =
        keyboardAvailable;

    /*
     * 任意必要设备不可用时，Daemon拥有最高前台权。
     */
    if ((shell->state !=
         LOCAL_SHELL_STATE_DAEMON) ||
        (availabilityChanged != 0U))
    {
        shell->state =
            LOCAL_SHELL_STATE_DAEMON;

        if (networkAvailable == 0U)
        {
            if (keyboardAvailable == 0U)
            {
                shell->inherited.standardOutput
                    ->write(
                        shell->inherited.standardOutput,
                        localShellDaemonAllError,
                        sizeof(
                            localShellDaemonAllError) -
                        1U);
            }
            else
            {
                shell->inherited.standardOutput
                    ->write(
                        shell->inherited.standardOutput,
                        localShellDaemonNetworkError,
                        sizeof(
                            localShellDaemonNetworkError) -
                        1U);
            }
        }
        else
        {
            shell->inherited.standardOutput
                ->write(
                    shell->inherited.standardOutput,
                    localShellDaemonKeyboardError,
                    sizeof(
                        localShellDaemonKeyboardError) -
                    1U);
        }
    }
}






