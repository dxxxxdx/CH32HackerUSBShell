//
// Created by dxxdx on 2026/7/20.
//


#include "VT100.h"
#if defined(__GNUC__)
#define VT100_NOP_ATTRIBUTES __attribute__((noinline, used))
#else
#define VT100_NOP_ATTRIBUTES
#endif



#include <stdint.h>
static VT100_NOP_ATTRIBUTES void VT100_NopSimple(void *context)
{
    (void)context;
}

static VT100_NOP_ATTRIBUTES void VT100_NopCount(
    void *context,
    uint16_t count)
{
    (void)context;
    (void)count;
}

static VT100_NOP_ATTRIBUTES void VT100_NopMode(
    void *context,
    uint8_t mode)
{
    (void)context;
    (void)mode;
}

static VT100_NOP_ATTRIBUTES void VT100_NopWriteRun(
    void *context,
    const uint8_t *data,
    uint32_t length)
{
    (void)context;
    (void)data;
    (void)length;
}

static VT100_NOP_ATTRIBUTES void VT100_NopCursorPosition(
    void *context,
    uint16_t row,
    uint16_t column)
{
    (void)context;
    (void)row;
    (void)column;
}

static VT100_NOP_ATTRIBUTES void VT100_NopSetGraphics(
    void *context,
    const VT100CSIParameters *parameters)
{
    (void)context;
    (void)parameters;
}

static VT100_NOP_ATTRIBUTES void VT100_NopSetMode(
    void *context,
    uint8_t privateMarker,
    const VT100CSIParameters *parameters,
    uint8_t enabled)
{
    (void)context;
    (void)privateMarker;
    (void)parameters;
    (void)enabled;
}

static VT100_NOP_ATTRIBUTES void VT100_NopSetScrollRegion(
    void *context,
    uint16_t top,
    uint16_t bottom)
{
    (void)context;
    (void)top;
    (void)bottom;
}

static VT100_NOP_ATTRIBUTES void VT100_NopDeviceStatus(
    void *context,
    uint16_t request)
{
    (void)context;
    (void)request;
}

static VT100_NOP_ATTRIBUTES void VT100_NopSelectCharset(
    void *context,
    uint8_t slot,
    uint8_t designator)
{
    (void)context;
    (void)slot;
    (void)designator;
}

const VT100Operations VT100_NopOperations = {
    .ground = {
        .writeRun = VT100_NopWriteRun,
        .bell = VT100_NopSimple,
        .backspace = VT100_NopSimple,
        .horizontalTab = VT100_NopSimple,
        .lineFeed = VT100_NopSimple,
        .carriageReturn = VT100_NopSimple,
    },
    .escape = {
        .saveCursor = VT100_NopSimple,
        .restoreCursor = VT100_NopSimple,
        .index = VT100_NopSimple,
        .nextLine = VT100_NopSimple,
        .reverseIndex = VT100_NopSimple,
        .reset = VT100_NopSimple,
        .selectCharset = VT100_NopSelectCharset,
    },
    .csi = {
        .cursorUp = VT100_NopCount,
        .cursorDown = VT100_NopCount,
        .cursorForward = VT100_NopCount,
        .cursorBackward = VT100_NopCount,
        .cursorNextLine = VT100_NopCount,
        .cursorPreviousLine = VT100_NopCount,
        .cursorHorizontalAbsolute = VT100_NopCount,
        .cursorVerticalAbsolute = VT100_NopCount,
        .cursorPosition = VT100_NopCursorPosition,
        .eraseDisplay = VT100_NopMode,
        .eraseLine = VT100_NopMode,
        .insertCharacters = VT100_NopCount,
        .deleteCharacters = VT100_NopCount,
        .eraseCharacters = VT100_NopCount,
        .insertLines = VT100_NopCount,
        .deleteLines = VT100_NopCount,
        .scrollUp = VT100_NopCount,
        .scrollDown = VT100_NopCount,
        .setGraphics = VT100_NopSetGraphics,
        .setMode = VT100_NopSetMode,
        .setScrollRegion = VT100_NopSetScrollRegion,
        .saveCursor = VT100_NopSimple,
        .restoreCursor = VT100_NopSimple,
        .deviceStatus = VT100_NopDeviceStatus,
    },
};
#undef VT100_NOP_ATTRIBUTES