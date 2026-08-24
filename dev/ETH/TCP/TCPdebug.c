//
// Created by dxxdx on 2026/8/8.
//

#include "TCPdebug.h"

#include "ETH_TCPManager.h"
#define TCP_DEBUG_ECHO_CHUNK_SIZE 64U

static uint8_t tcpDebugEchoBuffer[
    TCP_DEBUG_ECHO_CHUNK_SIZE];

void TCPDebugEcho_Process(
    TCPManager *self)
{
    TCPConnection *const connection =
        &self->connection;

    uint16_t copyLength =
        connection->receiveFIFO.length;

    const uint16_t transmitFree =
        TCPTransmitFIFO_GetFreeSize(
            &connection->transmitFIFO);

    if (copyLength > transmitFree)
    {
        copyLength = transmitFree;
    }

    if (copyLength >
        TCP_DEBUG_ECHO_CHUNK_SIZE)
    {
        copyLength =
            TCP_DEBUG_ECHO_CHUNK_SIZE;
    }

    if (copyLength != 0U)
    {
        const uint16_t readLength =
            TCPManager_Read(
                self,
                tcpDebugEchoBuffer,
                copyLength);

        /*
         * The read length was limited to the available TX space,
         * so this write is expected to be complete.
         */
        (void)TCPManager_Write(
            self,
            tcpDebugEchoBuffer,
            readLength);
    }

    /*
     * nc -N half-closes its sending direction after stdin reaches EOF.
     * Finish echoing buffered data, then close the MCU direction too.
     */
    if ((connection->state ==
         TCP_CONNECTION_STATE_CLOSE_WAIT) &&
        (connection->receiveFIFO.length ==
         0U))
    {
        TCPManager_RequestClose(self);
    }
}