//
// Created by dxxdx on 2026/8/8.
//

#include "nmap.h"

#include "DHCP.h"


typedef struct NmapPortService
{
    uint16_t port;
    const char *service;
} NmapPortService;


/*
 * nmap -a扫描的端口表。
 *
 * 这里就是常用端口和显示名称的唯一映射表，后续需要增加、删除
 * 或修改目标端口时，直接编辑这个const数组即可。
 */
static const NmapPortService nmapCommonPorts[] =
{
    { 21U,   "ftp" },
    { 22U,   "ssh" },
    { 23U,   "telnet" },
    { 25U,   "smtp" },
    { 53U,   "dns" },
    { 80U,   "http" },
    { 110U,  "pop3" },
    { 143U,  "imap" },
    { 443U,  "https" },
    { 445U,  "smb" },
    { 3306U, "mysql" },
    { 3389U, "rdp" },
    { 5432U, "postgresql" },
    { 6379U, "redis" },
    { 8080U, "http-alt" }
};

#define NMAP_COMMON_PORT_COUNT \
    ((uint8_t)(sizeof(nmapCommonPorts) / \
               sizeof(nmapCommonPorts[0])))

#define NMAP_RESULT_LINE_CAPACITY \
    48U


static const uint8_t nmapRemoteIP[4] =
{
    DHCP_CLIENT_IP_0,
    DHCP_CLIENT_IP_1,
    DHCP_CLIENT_IP_2,
    DHCP_CLIENT_IP_3
};

static const uint8_t nmapHeader[] =
    "PORT STATE SERVICE\r\n";

static const char nmapUnknownService[] =
    "unknown";


Nmap nmap;


static uint8_t Nmap_StringEquals(
    const char *left,
    const char *right)
{
    uint8_t offset = 0U;

    while ((left[offset] != '\0') &&
           (right[offset] != '\0'))
    {
        if (left[offset] != right[offset])
        {
            return 0U;
        }

        offset++;
    }

    return
        (uint8_t)(left[offset] == right[offset]);
}


static uint8_t Nmap_ParsePort(
    const char *text,
    uint16_t *port)
{
    uint32_t value = 0U;
    uint8_t offset = 0U;

    if (text[0] == '\0')
    {
        return 0U;
    }

    while (text[offset] != '\0')
    {
        const char character =
            text[offset];

        if ((character < '0') ||
            (character > '9'))
        {
            return 0U;
        }

        value =
            (value * 10U) +
            (uint32_t)(character - '0');

        if (value > 65535U)
        {
            return 0U;
        }

        offset++;
    }

    if (value == 0U)
    {
        return 0U;
    }

    *port = (uint16_t)value;

    return 1U;
}


static uint16_t Nmap_CurrentPort(
    const Nmap *self)
{
    if (self->scanAll != 0U)
    {
        return
            nmapCommonPorts[self->portIndex]
                .port;
    }

    return self->requestedPort;
}


static const char *Nmap_FindService(
    uint16_t port)
{
    uint8_t index;

    for (index = 0U;
         index < NMAP_COMMON_PORT_COUNT;
         index++)
    {
        if (nmapCommonPorts[index].port ==
            port)
        {
            return
                nmapCommonPorts[index]
                    .service;
        }
    }

    return nmapUnknownService;
}


static const char *Nmap_ResultName(
    uint8_t result)
{
    switch (result)
    {
    case TCP_PROBE_RESULT_OPEN:
        return "open";

    case TCP_PROBE_RESULT_CLOSED:
        return "closed";

    case TCP_PROBE_RESULT_TIMEOUT:
        /*
         * SYN没有得到响应时，无法区分丢包和防火墙静默丢弃。
         * 沿用nmap的习惯，把它显示为filtered。
         */
        return "filtered";

    case TCP_PROBE_RESULT_UNREACHABLE:
        return "unreachable";

    default:
        return "unknown";
    }
}


static uint8_t Nmap_AppendCharacter(
    uint8_t destination[NMAP_RESULT_LINE_CAPACITY],
    uint8_t offset,
    char character)
{
    if (offset < NMAP_RESULT_LINE_CAPACITY)
    {
        destination[offset] =
            (uint8_t)character;

        offset++;
    }

    return offset;
}


static uint8_t Nmap_AppendText(
    uint8_t destination[NMAP_RESULT_LINE_CAPACITY],
    uint8_t offset,
    const char *text)
{
    uint8_t sourceOffset = 0U;

    while ((text[sourceOffset] != '\0') &&
           (offset < NMAP_RESULT_LINE_CAPACITY))
    {
        destination[offset] =
            (uint8_t)text[sourceOffset];

        offset++;
        sourceOffset++;
    }

    return offset;
}


static uint8_t Nmap_AppendPort(
    uint8_t destination[NMAP_RESULT_LINE_CAPACITY],
    uint8_t offset,
    uint16_t port)
{
    uint8_t digits[5];
    uint8_t digitCount = 0U;

    do
    {
        digits[digitCount] =
            (uint8_t)('0' + (port % 10U));

        digitCount++;
        port /= 10U;
    }
    while (port != 0U);

    while (digitCount != 0U)
    {
        digitCount--;

        offset = Nmap_AppendCharacter(
            destination,
            offset,
            (char)digits[digitCount]);
    }

    return offset;
}


static void Nmap_PrintResult(
    const LocalShellInherited *inherited,
    uint16_t port,
    uint8_t result)
{
    uint8_t line[NMAP_RESULT_LINE_CAPACITY];
    uint8_t length = 0U;

    length = Nmap_AppendPort(
        line,
        length,
        port);

    length = Nmap_AppendText(
        line,
        length,
        "/tcp ");

    length = Nmap_AppendText(
        line,
        length,
        Nmap_ResultName(result));

    length = Nmap_AppendCharacter(
        line,
        length,
        ' ');

    length = Nmap_AppendText(
        line,
        length,
        Nmap_FindService(port));

    length = Nmap_AppendText(
        line,
        length,
        "\r\n");

    inherited->standardOutput->write(
        inherited->standardOutput,
        line,
        length);
}


LocalShellApplicationStartStatus Nmap_Start(
    void *application,
    uint8_t argumentCount,
    const char *const *argumentVector)
{
    Nmap *self = application;
    uint16_t port;

    /* argv[0]是命令名，本命令必须且只能再带一个参数。 */
    if (argumentCount != 2U)
    {
        return
            LOCAL_SHELL_APPLICATION_START_REJECTED;
    }

    if (Nmap_StringEquals(
            argumentVector[1],
            "-a") != 0U)
    {
        self->scanAll = 1U;
        self->requestedPort = 0U;
    }
    else
    {
        if (Nmap_ParsePort(
                argumentVector[1],
                &port) == 0U)
        {
            return
                LOCAL_SHELL_APPLICATION_START_REJECTED;
        }

        self->scanAll = 0U;
        self->requestedPort = port;
    }

    self->portIndex = 0U;
    self->probeOwned = 0U;
    self->stopRequested = 0U;
    self->state = NMAP_STATE_PRINT_HEADER;

    return
        LOCAL_SHELL_APPLICATION_START_SUCCESS;
}


void Nmap_Input(
    void *application,
    const uint8_t *data,
    uint32_t length)
{
    /* nmap是一次性扫描命令，不消费启动后的键盘输入。 */
    (void)application;
    (void)data;
    (void)length;
}


LocalShellApplicationStatus Nmap_Process(
    void *application,
    const LocalShellInherited *inherited)
{
    Nmap *self = application;
    TCPManager *const tcp =
        &inherited->networkManager->tcp;

    if (self->stopRequested != 0U)
    {
        /* 只能撤销由本应用启动的Probe。 */
        if (self->probeOwned != 0U)
        {
            TCPManager_StopProbe(tcp);
            self->probeOwned = 0U;
        }

        self->state = NMAP_STATE_IDLE;

        return
            LOCAL_SHELL_APPLICATION_EXIT_SUCCESS;
    }

    switch (self->state)
    {
    case NMAP_STATE_PRINT_HEADER:
        inherited->standardOutput->write(
            inherited->standardOutput,
            nmapHeader,
            sizeof(nmapHeader) - 1U);

        self->state =
            NMAP_STATE_START_PROBE;

        return
            LOCAL_SHELL_APPLICATION_RUNNING;

    case NMAP_STATE_START_PROBE:
        /*
         * TCPManager目前只有一个SYN Probe。如果它正被别的事务占用，
         * 本应用保持协作式等待，不覆盖也不消费别人的探测结果。
         */
        if (TCPManager_IsProbeIdle(tcp) == 0U)
        {
            return
                LOCAL_SHELL_APPLICATION_RUNNING;
        }

        if (TCPManager_StartSYNProbe(
                tcp,
                nmapRemoteIP,
                Nmap_CurrentPort(self)) == 0U)
        {
            return
                LOCAL_SHELL_APPLICATION_RUNNING;
        }

        self->probeOwned = 1U;
        self->state =
            NMAP_STATE_WAIT_RESULT;

        return
            LOCAL_SHELL_APPLICATION_RUNNING;

    case NMAP_STATE_WAIT_RESULT:
        if (TCPManager_IsProbeResultReady(
                tcp) == 0U)
        {
            return
                LOCAL_SHELL_APPLICATION_RUNNING;
        }

        {
            const uint16_t port =
                Nmap_CurrentPort(self);

            const uint8_t result =
                TCPManager_TakeProbeResult(tcp);

            self->probeOwned = 0U;

            Nmap_PrintResult(
                inherited,
                port,
                result);
        }

        if ((self->scanAll == 0U) ||
            ((uint8_t)(self->portIndex + 1U) >=
             NMAP_COMMON_PORT_COUNT))
        {
            self->state = NMAP_STATE_IDLE;

            return
                LOCAL_SHELL_APPLICATION_EXIT_SUCCESS;
        }

        self->portIndex++;
        self->state =
            NMAP_STATE_START_PROBE;

        return
            LOCAL_SHELL_APPLICATION_RUNNING;

    default:
        return
            LOCAL_SHELL_APPLICATION_EXIT_FAILURE;
    }
}


void Nmap_RequestStop(
    void *application)
{
    Nmap *self = application;

    self->stopRequested = 1U;
}
