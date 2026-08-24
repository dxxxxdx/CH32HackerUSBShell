#ifndef DISPLAYER_RX_H
#define DISPLAYER_RX_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#ifndef DISPLAYER_RX_BUFFER_SIZE
#define DISPLAYER_RX_BUFFER_SIZE 512U
#endif

#if (DISPLAYER_RX_BUFFER_SIZE < 2U) || \
    ((DISPLAYER_RX_BUFFER_SIZE & (DISPLAYER_RX_BUFFER_SIZE - 1U)) != 0U)
#error "DISPLAYER_RX_BUFFER_SIZE must be a power of two"
#endif
#define DISPLAYER_RX_COMPILER_BARRIER() \
__asm volatile ("" ::: "memory")

#define DISPLAYER_RX_BUFFER_MASK \
    (DISPLAYER_RX_BUFFER_SIZE - 1U)

typedef enum
{
    DISPLAYER_RX_OWNER_NONE = 0,
    DISPLAYER_RX_OWNER_STARTUP,
    DISPLAYER_RX_OWNER_USB_CDC,
    DISPLAYER_RX_OWNER_NETWORK,
    DISPLAYER_RX_OWNER_UART
} displayerRxOwner;
#if defined(__GNUC__)

#define DISPLAYER_RX_ALWAYS_INLINE \
    static inline __attribute__((always_inline))


#else

#define DISPLAYER_RX_ALWAYS_INLINE \
    static inline

#define DISPLAYER_RX_COMPILER_BARRIER() \
    ((void)0)

#endif

typedef struct displayerRx
{
    uint8_t buffer[DISPLAYER_RX_BUFFER_SIZE];

    /*
     * Written only by the current producer.
     * The producer publishes data by advancing this counter.
     */
    volatile uint32_t producer;

    /*
     * Written only by the consumer.
     * The consumer releases data by advancing this counter.
     */
    volatile uint32_t consumer;

    volatile uint32_t dropped;
    volatile uint8_t overflowed;

    uint8_t(*const write)( struct displayerRx *self,
    const uint8_t *source,
    uint32_t length);

} displayerRx;

extern displayerRx displayer_Rx;



/*
 * Borrow the largest currently contiguous readable block.
 *
 * consumer is not advanced here. The returned memory remains owned by the
 * consumer until displayerRx_ConsumeDone() is called.
 *
 * Return:
 *   0     = FIFO empty;
 *   other = contiguous readable byte count.
 */
DISPLAYER_RX_ALWAYS_INLINE uint32_t displayerRx_Acquire(
    displayerRx *self,
    const uint8_t **data)
{
    const uint32_t consumer = self->consumer;
    const uint32_t available =
        self->producer - consumer;

    if (available == 0U)
    {
        *data = NULL;
        return 0U;
    }

    /*
     * Observe ring data only after observing the producer commit.
     */
    DISPLAYER_RX_COMPILER_BARRIER();

    const uint32_t readIndex =
        consumer & DISPLAYER_RX_BUFFER_MASK;

    const uint32_t contiguous =
        DISPLAYER_RX_BUFFER_SIZE - readIndex;

    const uint32_t length =
        available < contiguous
            ? available
            : contiguous;

    *data = &self->buffer[readIndex];

    return length;
}

/*
 * Release only the bytes actually consumed from the latest acquired block.
 *
 * The void pointer signature allows this function to be used by VT100 as a
 * completion callback.
 */
DISPLAYER_RX_ALWAYS_INLINE void displayerRx_ConsumeDone(
    void *context,
    uint32_t usedLength)
{
    displayerRx *self = (displayerRx *)context;

    const uint32_t available =
        self->producer - self->consumer;

    if (usedLength == 0U)
    {
        return;
    }

    /*
     * Reject an invalid completion instead of advancing beyond producer.
     */
    if (usedLength > available)
    {
        return;
    }

    /*
     * Finish all reads before releasing the corresponding ring slots.
     */
    DISPLAYER_RX_COMPILER_BARRIER();
    self->consumer += usedLength;
}

#undef DISPLAYER_RX_ALWAYS_INLINE
#undef DISPLAYER_RX_COMPILER_BARRIER

#endif /* DISPLAYER_RX_H */
