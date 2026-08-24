#include "VT100.h"

#include <string.h>

#include "displayerRx.h"

#define VT100_ASCII_ESC 0x1BU
#define VT100_ASCII_BEL 0x07U
#define VT100_ASCII_CAN 0x18U
#define VT100_ASCII_SUB 0x1AU
#define VT100_ASCII_DEL 0x7FU

#if defined(__GNUC__)
#define VT100_ALWAYS_INLINE static inline __attribute__((always_inline))
#else
#define VT100_ALWAYS_INLINE static inline
#endif

/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */
/* Empty operation backend                                                   */
/* ------------------------------------------------------------------------- */


/* ------------------------------------------------------------------------- */
/* Parser helpers                                                            */
/* ------------------------------------------------------------------------- */

static void VT100_ClearCSI(VT100 *self)
{
    memset(&self->csi, 0, sizeof(self->csi));
}

static void VT100_ClearEscape(VT100 *self)
{
    memset(self->escapeIntermediate, 0, sizeof(self->escapeIntermediate));
    self->escapeIntermediateCount = 0U;
}

static void VT100_EnterEscape(VT100 *self)
{
    VT100_ClearCSI(self);
    VT100_ClearEscape(self);
    self->stringKind = VT100_STRING_NONE;
    self->state = VT100_STATE_ESCAPE;
}

static void VT100_FinishCSI(VT100 *self)
{
    VT100_ClearCSI(self);
    self->state = VT100_STATE_GROUND;
}

VT100_ALWAYS_INLINE uint8_t VT100_IsCSIFinal(uint8_t byte)
{
    return (uint8_t)((byte >= 0x40U) && (byte <= 0x7EU));
}

VT100_ALWAYS_INLINE uint8_t VT100_IsIntermediate(uint8_t byte)
{
    return (uint8_t)((byte >= 0x20U) && (byte <= 0x2FU));
}

VT100_ALWAYS_INLINE uint8_t VT100_IsPrintable(uint8_t byte)
{
    return (uint8_t)((byte >= 0x20U) && (byte <= 0x7EU));
}

#undef VT100_ALWAYS_INLINE

static uint16_t VT100_ParameterOr(
    const VT100CSIParameters *parameters,
    uint8_t index,
    uint16_t fallback)
{
    if ((index >= VT100_MAX_PARAMS) ||
        ((parameters->presentMask & (uint16_t)(1U << index)) == 0U))
    {
        return fallback;
    }

    return parameters->value[index];
}

static uint16_t VT100_CountOrOne(
    const VT100CSIParameters *parameters,
    uint8_t index)
{
    uint16_t value = VT100_ParameterOr(parameters, index, 1U);
    return value == 0U ? 1U : value;
}

static void VT100_CSIAppendDigit(VT100 *self, uint8_t byte)
{
    VT100CSIParameters *parameters = &self->csi;

    if (parameters->count == 0U)
    {
        parameters->count = 1U;
    }

    const uint8_t index = (uint8_t)(parameters->count - 1U);
    const uint16_t digit = (uint16_t)(byte - (uint8_t)'0');
    uint16_t value = parameters->value[index];

    parameters->presentMask |= (uint16_t)(1U << index);

    if (value > (uint16_t)((UINT16_MAX - digit) / 10U))
    {
        value = UINT16_MAX;
    }
    else
    {
        value = (uint16_t)((value * 10U) + digit);
    }

    parameters->value[index] = value;
}

static void VT100_CSINextParameter(VT100 *self)
{
    VT100CSIParameters *parameters = &self->csi;

    if (parameters->count == 0U)
    {
        parameters->count = 1U;
    }

    if (parameters->count >= VT100_MAX_PARAMS)
    {
        self->state = VT100_STATE_CSI_IGNORE;
        return;
    }

    parameters->count++;
}

static void VT100_CSIAppendIntermediate(VT100 *self, uint8_t byte)
{
    VT100CSIParameters *parameters = &self->csi;

    if (parameters->intermediateCount >= VT100_MAX_INTERMEDIATES)
    {
        self->state = VT100_STATE_CSI_IGNORE;
        return;
    }

    parameters->intermediate[parameters->intermediateCount] = byte;
    parameters->intermediateCount++;
}

static void VT100_EscapeAppendIntermediate(VT100 *self, uint8_t byte)
{
    if (self->escapeIntermediateCount >= VT100_MAX_INTERMEDIATES)
    {
        return;
    }

    self->escapeIntermediate[self->escapeIntermediateCount] = byte;
    self->escapeIntermediateCount++;
}

static void VT100_DispatchC0(VT100 *self, uint8_t byte)
{
    VT100GroundOperations const *ground = &self->operations->ground;

    switch (byte)
    {
    case 0x07U:
        ground->bell(self->operationContext);
        break;
    case 0x08U:
        ground->backspace(self->operationContext);
        break;
    case 0x09U:
        ground->horizontalTab(self->operationContext);
        break;
    case 0x0AU:
    case 0x0BU:
    case 0x0CU:
        ground->lineFeed(self->operationContext);
        break;
    case 0x0DU:
        ground->carriageReturn(self->operationContext);
        break;
    default:
        break;
    }
}

static void VT100_DispatchEscape(VT100 *self, uint8_t finalByte)
{
    VT100EscapeOperations const *escape = &self->operations->escape;

    if (self->escapeIntermediateCount != 0U)
    {
        if ((self->escapeIntermediateCount == 1U) &&
            ((self->escapeIntermediate[0] == (uint8_t)'(') ||
              (self->escapeIntermediate[0] == (uint8_t)')')))
        {
            const uint8_t slot =
                self->escapeIntermediate[0] == (uint8_t)'(' ? 0U : 1U;
            escape->selectCharset(self->operationContext, slot, finalByte);
        }
        return;
    }

    switch (finalByte)
    {
    case '7':
        escape->saveCursor(self->operationContext);
        break;
    case '8':
        escape->restoreCursor(self->operationContext);
        break;
    case 'D':
        escape->index(self->operationContext);
        break;
    case 'E':
        escape->nextLine(self->operationContext);
        break;
    case 'M':
        escape->reverseIndex(self->operationContext);
        break;
    case 'c':
        escape->reset(self->operationContext);
        break;
    default:
        break;
    }
}

static void VT100_ClearScreen(VT100 *self)
{
    self->operations->csi.eraseDisplay(
        self->operationContext,
        2U);
}

static void VT100_DispatchCSI(VT100 *self, uint8_t finalByte)
{
    VT100CSIParameters const *parameters = &self->csi;
    VT100CSIOperations const *csi = &self->operations->csi;

    /* Commands with an intermediate byte are not implemented in this pass. */
    if (parameters->intermediateCount != 0U)
    {
        return;
    }

    if ((finalByte == (uint8_t)'h') || (finalByte == (uint8_t)'l'))
    {
        csi->setMode(
            self->operationContext,
            parameters->privateMarker,
            parameters,
            (uint8_t)(finalByte == (uint8_t)'h'));
        return;
    }

    /* Other first-pass operations are standard CSI commands only. */
    if (parameters->privateMarker != 0U)
    {
        return;
    }

    const uint16_t count = VT100_CountOrOne(parameters, 0U);

    switch (finalByte)
    {
    case 'A':
        csi->cursorUp(self->operationContext, count);
        break;
    case 'B':
        csi->cursorDown(self->operationContext, count);
        break;
    case 'C':
        csi->cursorForward(self->operationContext, count);
        break;
    case 'D':
        csi->cursorBackward(self->operationContext, count);
        break;
    case 'E':
        csi->cursorNextLine(self->operationContext, count);
        break;
    case 'F':
        csi->cursorPreviousLine(self->operationContext, count);
        break;
    case 'G':
        csi->cursorHorizontalAbsolute(self->operationContext, count);
        break;
    case 'd':
        csi->cursorVerticalAbsolute(self->operationContext, count);
        break;
    case 'H':
    case 'f':
        csi->cursorPosition(
            self->operationContext,
            VT100_CountOrOne(parameters, 0U),
            VT100_CountOrOne(parameters, 1U));
        break;
    case 'J':
    {
        const uint8_t eraseMode =
            (uint8_t)VT100_ParameterOr(parameters, 0U, 0U);

        if (eraseMode == 2U)
        {
            VT100_ClearScreen(self);
        }
        else
        {
            csi->eraseDisplay(
                self->operationContext,
                eraseMode);
        }
        break;
    }
    case 'K':
        csi->eraseLine(
            self->operationContext,
            (uint8_t)VT100_ParameterOr(parameters, 0U, 0U));
        break;
    case '@':
        csi->insertCharacters(self->operationContext, count);
        break;
    case 'P':
        csi->deleteCharacters(self->operationContext, count);
        break;
    case 'X':
        csi->eraseCharacters(self->operationContext, count);
        break;
    case 'L':
        csi->insertLines(self->operationContext, count);
        break;
    case 'M':
        csi->deleteLines(self->operationContext, count);
        break;
    case 'S':
        csi->scrollUp(self->operationContext, count);
        break;
    case 'T':
        csi->scrollDown(self->operationContext, count);
        break;
    case 'm':
        csi->setGraphics(self->operationContext, parameters);
        break;
    case 'r':
        csi->setScrollRegion(
            self->operationContext,
            VT100_CountOrOne(parameters, 0U),
            VT100_ParameterOr(parameters, 1U, 0U));
        break;
    case 's':
        csi->saveCursor(self->operationContext);
        break;
    case 'u':
        csi->restoreCursor(self->operationContext);
        break;
    case 'n':
        csi->deviceStatus(
            self->operationContext,
            VT100_ParameterOr(parameters, 0U, 0U));
        break;
    default:
        break;
    }
}

static void VT100_ProcessEscapeByte(VT100 *self, uint8_t byte)
{
    switch (byte)
    {
    case '[':
        VT100_ClearCSI(self);
        self->state = VT100_STATE_CSI_ENTRY;
        return;
    case ']':
        self->stringKind = VT100_STRING_OSC;
        self->state = VT100_STATE_STRING_IGNORE;
        return;
    case 'P':
        self->stringKind = VT100_STRING_DCS;
        self->state = VT100_STATE_STRING_IGNORE;
        return;
    case 'X':
        self->stringKind = VT100_STRING_SOS;
        self->state = VT100_STATE_STRING_IGNORE;
        return;
    case '^':
        self->stringKind = VT100_STRING_PM;
        self->state = VT100_STATE_STRING_IGNORE;
        return;
    case '_':
        self->stringKind = VT100_STRING_APC;
        self->state = VT100_STATE_STRING_IGNORE;
        return;
    default:
        break;
    }

    if (VT100_IsIntermediate(byte) != 0U)
    {
        VT100_EscapeAppendIntermediate(self, byte);
        self->state = VT100_STATE_ESCAPE_INTERMEDIATE;
        return;
    }

    if ((byte >= 0x30U) && (byte <= 0x7EU))
    {
        VT100_DispatchEscape(self, byte);
    }

    VT100_ClearEscape(self);
    self->state = VT100_STATE_GROUND;
}

static void VT100_ProcessEscapeIntermediateByte(VT100 *self, uint8_t byte)
{
    if (VT100_IsIntermediate(byte) != 0U)
    {
        VT100_EscapeAppendIntermediate(self, byte);
        return;
    }

    if ((byte >= 0x30U) && (byte <= 0x7EU))
    {
        VT100_DispatchEscape(self, byte);
    }

    VT100_ClearEscape(self);
    self->state = VT100_STATE_GROUND;
}

static void VT100_ProcessCSIEntryByte(VT100 *self, uint8_t byte)
{
    if ((byte >= (uint8_t)'0') && (byte <= (uint8_t)'9'))
    {
        VT100_CSIAppendDigit(self, byte);
        self->state = VT100_STATE_CSI_PARAM;
        return;
    }

    if (byte == (uint8_t)';')
    {
        VT100_CSINextParameter(self);
        if (self->state != VT100_STATE_CSI_IGNORE)
            self->state = VT100_STATE_CSI_PARAM;
        return;
    }

    if ((byte >= (uint8_t)'<') && (byte <= (uint8_t)'?'))
    {
        self->csi.privateMarker = byte;
        self->state = VT100_STATE_CSI_PARAM;
        return;
    }

    if (byte == (uint8_t)':')
    {
        self->state = VT100_STATE_CSI_IGNORE;
        return;
    }

    if (VT100_IsIntermediate(byte) != 0U)
    {
        VT100_CSIAppendIntermediate(self, byte);
        if (self->state != VT100_STATE_CSI_IGNORE)
            self->state = VT100_STATE_CSI_INTERMEDIATE;
        return;
    }

    if (VT100_IsCSIFinal(byte) != 0U)
    {
        VT100_DispatchCSI(self, byte);
        VT100_FinishCSI(self);
        return;
    }

    self->state = VT100_STATE_CSI_IGNORE;
}

static void VT100_ProcessCSIParamByte(VT100 *self, uint8_t byte)
{
    if ((byte >= (uint8_t)'0') && (byte <= (uint8_t)'9'))
    {
        VT100_CSIAppendDigit(self, byte);
        return;
    }

    if (byte == (uint8_t)';')
    {
        VT100_CSINextParameter(self);
        return;
    }

    if (VT100_IsIntermediate(byte) != 0U)
    {
        VT100_CSIAppendIntermediate(self, byte);
        if (self->state != VT100_STATE_CSI_IGNORE)
            self->state = VT100_STATE_CSI_INTERMEDIATE;
        return;
    }

    if (VT100_IsCSIFinal(byte) != 0U)
    {
        VT100_DispatchCSI(self, byte);
        VT100_FinishCSI(self);
        return;
    }

    self->state = VT100_STATE_CSI_IGNORE;
}

static void VT100_ProcessCSIIntermediateByte(VT100 *self, uint8_t byte)
{
    if (VT100_IsIntermediate(byte) != 0U)
    {
        VT100_CSIAppendIntermediate(self, byte);
        return;
    }

    if (VT100_IsCSIFinal(byte) != 0U)
    {
        VT100_DispatchCSI(self, byte);
        VT100_FinishCSI(self);
        return;
    }

    self->state = VT100_STATE_CSI_IGNORE;
}

static void VT100_ProcessStringByte(VT100 *self, uint8_t byte)
{
    if ((self->stringKind == VT100_STRING_OSC) && (byte == VT100_ASCII_BEL))
    {
        self->stringKind = VT100_STRING_NONE;
        self->state = VT100_STATE_GROUND;
        return;
    }

    if (byte == VT100_ASCII_ESC)
    {
        self->state = VT100_STATE_STRING_ESCAPE;
    }
}

static void VT100_ProcessStringEscapeByte(VT100 *self, uint8_t byte)
{
    if (byte == (uint8_t)'\\')
    {
        self->stringKind = VT100_STRING_NONE;
        self->state = VT100_STATE_GROUND;
        return;
    }

    self->state =
        byte == VT100_ASCII_ESC
            ? VT100_STATE_STRING_ESCAPE
            : VT100_STATE_STRING_IGNORE;
}

static void VT100_ProcessByte(VT100 *self, uint8_t byte)
{
    if ((byte == VT100_ASCII_CAN) || (byte == VT100_ASCII_SUB))
    {
        VT100_ResetParser(self);
        return;
    }

    if (self->state == VT100_STATE_STRING_IGNORE)
    {
        VT100_ProcessStringByte(self, byte);
        return;
    }

    if (self->state == VT100_STATE_STRING_ESCAPE)
    {
        VT100_ProcessStringEscapeByte(self, byte);
        return;
    }

    if (byte == VT100_ASCII_ESC)
    {
        VT100_EnterEscape(self);
        return;
    }

    if (byte < 0x20U)
    {
        VT100_DispatchC0(self, byte);
        return;
    }

    if (byte == VT100_ASCII_DEL)
    {
        return;
    }

    switch (self->state)
    {
    case VT100_STATE_GROUND:
        /* Printable runs are handled directly by VT100_Process(). */
        break;
    case VT100_STATE_ESCAPE:
        VT100_ProcessEscapeByte(self, byte);
        break;
    case VT100_STATE_ESCAPE_INTERMEDIATE:
        VT100_ProcessEscapeIntermediateByte(self, byte);
        break;
    case VT100_STATE_CSI_ENTRY:
        VT100_ProcessCSIEntryByte(self, byte);
        break;
    case VT100_STATE_CSI_PARAM:
        VT100_ProcessCSIParamByte(self, byte);
        break;
    case VT100_STATE_CSI_INTERMEDIATE:
        VT100_ProcessCSIIntermediateByte(self, byte);
        break;
    case VT100_STATE_CSI_IGNORE:
        if (VT100_IsCSIFinal(byte) != 0U)
        {
            VT100_FinishCSI(self);
        }
        break;
    case VT100_STATE_STRING_IGNORE:
    case VT100_STATE_STRING_ESCAPE:
    default:
        break;
    }
}

/* ------------------------------------------------------------------------- */
/* Public API                                                                */
/* ------------------------------------------------------------------------- */

void VT100_Init(
    VT100 *self,
    const VT100Operations *operations,
    void *operationContext)
{
    self->operations = operations;
    self->operationContext = operationContext;
    self->byteStreamPtr = NULL;
    self->availableLength = 0U;
    self->consumedLength = 0U;
    VT100_ResetParser(self);
}

void VT100_ResetParser(VT100 *self)
{
    self->state = VT100_STATE_GROUND;
    self->stringKind = VT100_STRING_NONE;
    VT100_ClearCSI(self);
    VT100_ClearEscape(self);
}

uint32_t VT100_Process(VT100 *self, uint32_t length)
{
    if (length == 0U)
    {
        return 0U;
    }

    self->consumedLength = 0U;
    self->availableLength = self->getAvailableLength(self);

    if ((self->byteStreamPtr == NULL) || (self->availableLength == 0U))
    {
        return 0U;
    }

    if (length > self->availableLength)
    {
        length = self->availableLength;
    }

    while (self->consumedLength < length)
    {
        if ((self->state == VT100_STATE_GROUND) &&
            (VT100_IsPrintable(
                 self->byteStreamPtr[self->consumedLength]) != 0U))
        {
            const uint32_t runStart = self->consumedLength;

            do
            {
                self->consumedLength++;
            }
            while ((self->consumedLength < length) &&
                   (VT100_IsPrintable(
                        self->byteStreamPtr[self->consumedLength]) != 0U));

            self->operations->ground.writeRun(
                self->operationContext,
                &self->byteStreamPtr[runStart],
                self->consumedLength - runStart);

            continue;
        }

        VT100_ProcessByte(
            self,
            self->byteStreamPtr[self->consumedLength]);
        self->consumedLength++;
    }

    (void)VT100_ProcessCallBack(self);
    return self->consumedLength;
}

uint32_t VT100_ProcessCallBack(VT100 *self)
{
    return self->processCallBack(self);
}

/* ------------------------------------------------------------------------- */
/* Default global SPSC terminal instance                                     */
/* ------------------------------------------------------------------------- */
//懒得新开文件了原地拉屎


static uint32_t VT100_AcquireDisplayerRx(VT100 *self)
{
    const uint8_t *data;
    const uint32_t length = displayerRx_Acquire(&displayer_Rx, &data);

    self->byteStreamPtr = (uint8_t *)data;
    return length;
}

static uint32_t VT100_ReleaseDisplayerRx(VT100 *self)
{
    displayerRx_ConsumeDone(&displayer_Rx, self->consumedLength);
    return self->consumedLength;
}



extern const VT100Operations VT100_NopOperations;
extern const VT100Operations screenOperations;

VT100 VT100_Instance = {
    .operations = &screenOperations,
    .operationContext = NULL,
    .processCallBack = VT100_ReleaseDisplayerRx,
    .getAvailableLength = VT100_AcquireDisplayerRx,
    .byteStreamPtr = NULL,
    .availableLength = 0U,
    .consumedLength = 0U,
    .state = VT100_STATE_GROUND,
    .stringKind = VT100_STRING_NONE,
};
