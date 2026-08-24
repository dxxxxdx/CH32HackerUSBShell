//
// Created by dxxdx on 2026/7/20.
//

#include "displayerRx.h"
#define DISPLAYER_RX_COMPILER_BARRIER() \
__asm volatile ("" ::: "memory")

/*
 * Copy one complete source block into the ring.
 *
 * The operation is all-or-nothing:
 *   1 = the complete block was accepted;
 *   0 = insufficient free space, nothing was copied.
 *
 * The caller is responsible for checking ownership when multiple potential
 * producers exist.
 */


uint8_t displayerRx_Write(
    displayerRx *self,
    const uint8_t *source,
    uint32_t length)
{
    const uint32_t producer = self->producer;
    const uint32_t used =
        producer - self->consumer;
    const uint32_t free =
        DISPLAYER_RX_BUFFER_SIZE - used;

    if (length > free)
    {
        self->dropped += length;
        self->overflowed = 1U;
        return 0U;
    }

    if (length == 0U)
    {
        return 1U;
    }

    const uint32_t writeIndex =
        producer & DISPLAYER_RX_BUFFER_MASK;

    const uint32_t contiguous =
        DISPLAYER_RX_BUFFER_SIZE - writeIndex;

    const uint32_t firstLength =
        length < contiguous
            ? length
            : contiguous;

    memcpy(
        &self->buffer[writeIndex],
        source,
        firstLength);

    if (length > firstLength)
    {
        memcpy(
            &self->buffer[0],
            &source[firstLength],
            length - firstLength);
    }

    /*
     * Publish producer only after all copied bytes are visible.
     */
    DISPLAYER_RX_COMPILER_BARRIER();
    self->producer = producer + length;

    return 1U;
}


displayerRx displayer_Rx = {
    .buffer = {0},
    .consumer = 0U,
    .dropped = 0U,
    .overflowed = 0U,
    .producer = 0U,
    .write = displayerRx_Write
};
