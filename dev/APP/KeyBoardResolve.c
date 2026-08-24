//
// Created by dxxdx on 2026/7/26.
//
#include <stdint.h>
#include "USBH.h"
#include "LocalShell.h"

/* HID modifier bitmap */
#define HID_MOD_LEFT_CTRL      0x01U
#define HID_MOD_LEFT_SHIFT     0x02U
#define HID_MOD_LEFT_ALT       0x04U
#define HID_MOD_RIGHT_CTRL     0x10U
#define HID_MOD_RIGHT_SHIFT    0x20U
#define HID_MOD_RIGHT_ALT      0x40U

#define HID_MOD_CTRL_MASK \
    (HID_MOD_LEFT_CTRL | HID_MOD_RIGHT_CTRL)

#define HID_MOD_SHIFT_MASK \
    (HID_MOD_LEFT_SHIFT | HID_MOD_RIGHT_SHIFT)

#define HID_MOD_ALT_MASK \
    (HID_MOD_LEFT_ALT | HID_MOD_RIGHT_ALT)


/* HID Keyboard Usage ID */
#define HID_KEY_A              0x04U
#define HID_KEY_Z              0x1DU

#define HID_KEY_1              0x1EU
#define HID_KEY_0              0x27U

#define HID_KEY_ENTER          0x28U
#define HID_KEY_ESCAPE         0x29U
#define HID_KEY_BACKSPACE      0x2AU
#define HID_KEY_TAB            0x2BU
#define HID_KEY_SPACE          0x2CU
#define HID_KEY_MINUS          0x2DU
#define HID_KEY_EQUAL          0x2EU
#define HID_KEY_LEFT_BRACKET   0x2FU
#define HID_KEY_RIGHT_BRACKET  0x30U
#define HID_KEY_BACKSLASH      0x31U
#define HID_KEY_SEMICOLON      0x33U
#define HID_KEY_APOSTROPHE     0x34U
#define HID_KEY_GRAVE          0x35U
#define HID_KEY_COMMA          0x36U
#define HID_KEY_PERIOD         0x37U
#define HID_KEY_SLASH          0x38U
#define HID_KEY_CAPS_LOCK      0x39U

#define HID_KEY_INSERT         0x49U
#define HID_KEY_HOME           0x4AU
#define HID_KEY_PAGE_UP        0x4BU
#define HID_KEY_DELETE         0x4CU
#define HID_KEY_END            0x4DU
#define HID_KEY_PAGE_DOWN      0x4EU
#define HID_KEY_RIGHT          0x4FU
#define HID_KEY_LEFT           0x50U
#define HID_KEY_DOWN           0x51U
#define HID_KEY_UP             0x52U


static uint8_t keyboardCapsLock;
extern LocalShell Local_Shell;

/*
 * 这里是 USB 键盘到 LocalShell 接收缓冲区的唯一出口。
 *
 * 按你的实际 displayerRx API 修改函数体即可。
 */
static void LocalShell_PushKeyboardData(
    const uint8_t *data,
    uint8_t length)
{

    LocalShell_Input(&Local_Shell,data,length);

}


static uint8_t LocalShell_MapPrintableKey(
    uint8_t modifier,
    uint8_t usageId)
{
    uint8_t shiftPressed =
        ((modifier & HID_MOD_SHIFT_MASK) != 0U)
        ? 1U
        : 0U;

    uint8_t controlPressed =
        ((modifier & HID_MOD_CTRL_MASK) != 0U)
        ? 1U
        : 0U;

    /*
     * HID 0x04..0x1D 对应 A..Z。
     */
    if((usageId >= HID_KEY_A) &&
       (usageId <= HID_KEY_Z))
    {
        uint8_t character =
            (uint8_t)(
                'a' +
                usageId -
                HID_KEY_A
            );

        /*
         * Ctrl+A..Ctrl+Z -> 0x01..0x1A。
         * Shift 和 Caps Lock 在 Ctrl 存在时没有意义。
         */
        if(controlPressed != 0U)
        {
            return character & 0x1FU;
        }

        /*
         * Shift XOR Caps Lock 决定字母大小写。
         */
        if((shiftPressed ^ keyboardCapsLock) != 0U)
        {
            character =
                (uint8_t)(
                    character -
                    'a' +
                    'A'
                );
        }

        return character;
    }

    /*
     * 主键盘数字行 1..9。
     */
    if((usageId >= HID_KEY_1) &&
       (usageId < HID_KEY_0))
    {
        static const uint8_t normalDigits[9] =
        {
            '1', '2', '3',
            '4', '5', '6',
            '7', '8', '9'
        };

        static const uint8_t shiftedDigits[9] =
        {
            '!', '@', '#',
            '$', '%', '^',
            '&', '*', '('
        };

        uint8_t index =
            usageId - HID_KEY_1;

        return (shiftPressed != 0U)
            ? shiftedDigits[index]
            : normalDigits[index];
    }

    if(usageId == HID_KEY_0)
    {
        return (shiftPressed != 0U)
            ? ')'
            : '0';
    }

    switch(usageId)
    {
        case HID_KEY_SPACE:
            return ' ';

        case HID_KEY_MINUS:
            return (shiftPressed != 0U) ? '_' : '-';

        case HID_KEY_EQUAL:
            return (shiftPressed != 0U) ? '+' : '=';

        case HID_KEY_LEFT_BRACKET:
        {
            if(controlPressed != 0U)
            {
                return 0x1BU; /* Ctrl+[ = ESC */
            }

            return (shiftPressed != 0U) ? '{' : '[';
        }

        case HID_KEY_RIGHT_BRACKET:
        {
            if(controlPressed != 0U)
            {
                return 0x1DU; /* Ctrl+] */
            }

            return (shiftPressed != 0U) ? '}' : ']';
        }

        case HID_KEY_BACKSLASH:
        {
            if(controlPressed != 0U)
            {
                return 0x1CU; /* Ctrl+\ */
            }

            return (shiftPressed != 0U) ? '|' : '\\';
        }

        case HID_KEY_SEMICOLON:
            return (shiftPressed != 0U) ? ':' : ';';

        case HID_KEY_APOSTROPHE:
            return (shiftPressed != 0U) ? '"' : '\'';

        case HID_KEY_GRAVE:
            return (shiftPressed != 0U) ? '~' : '`';

        case HID_KEY_COMMA:
            return (shiftPressed != 0U) ? '<' : ',';

        case HID_KEY_PERIOD:
            return (shiftPressed != 0U) ? '>' : '.';

        case HID_KEY_SLASH:
            return (shiftPressed != 0U) ? '?' : '/';

        default:
            return 0U;
    }
}


/*
 * USBH.c 要求应用层提供的强符号。
 *
 * 不要加 static，不要再加 weak。
 * 忘记实现或 LocalShell.c 没参与链接时，链接器会直接报错。
 */
void USBH_OnKeyDown(
    USBH_KB *self,
    uint8_t modifier,
    uint8_t usageId)
{
    uint8_t output[4];
    uint8_t outputLength = 0U;
    uint8_t character;

    (void)self;

    switch(usageId)
    {
        case HID_KEY_CAPS_LOCK:
        {
            keyboardCapsLock ^= 1U;
            return;
        }

        case HID_KEY_ENTER:
        {
            /*
             * Linux/VT100 风格通常发送 CR。
             * Shell 输入层可以再把 CR 视作提交命令。
             */
            output[0] = '\r';
            outputLength = 1U;
            break;
        }

        case HID_KEY_ESCAPE:
        {
            output[0] = 0x1BU;
            outputLength = 1U;
            break;
        }

    case HID_KEY_BACKSPACE:
            {
                output[0] = 0x08U;
                outputLength = 1U;
                break;
            }

        case HID_KEY_TAB:
        {
            output[0] = '\t';
            outputLength = 1U;
            break;
        }

        case HID_KEY_UP:
        {
            output[0] = 0x1BU;
            output[1] = '[';
            output[2] = 'A';
            outputLength = 3U;
            break;
        }

        case HID_KEY_DOWN:
        {
            output[0] = 0x1BU;
            output[1] = '[';
            output[2] = 'B';
            outputLength = 3U;
            break;
        }

        case HID_KEY_RIGHT:
        {
            output[0] = 0x1BU;
            output[1] = '[';
            output[2] = 'C';
            outputLength = 3U;
            break;
        }

        case HID_KEY_LEFT:
        {
            output[0] = 0x1BU;
            output[1] = '[';
            output[2] = 'D';
            outputLength = 3U;
            break;
        }

        case HID_KEY_HOME:
        {
            output[0] = 0x1BU;
            output[1] = '[';
            output[2] = 'H';
            outputLength = 3U;
            break;
        }

        case HID_KEY_END:
        {
            output[0] = 0x1BU;
            output[1] = '[';
            output[2] = 'F';
            outputLength = 3U;
            break;
        }

        case HID_KEY_INSERT:
        {
            output[0] = 0x1BU;
            output[1] = '[';
            output[2] = '2';
            output[3] = '~';
            outputLength = 4U;
            break;
        }

        case HID_KEY_DELETE:
        {
            output[0] = 0x1BU;
            output[1] = '[';
            output[2] = '3';
            output[3] = '~';
            outputLength = 4U;
            break;
        }

        case HID_KEY_PAGE_UP:
        {
            output[0] = 0x1BU;
            output[1] = '[';
            output[2] = '5';
            output[3] = '~';
            outputLength = 4U;
            break;
        }

        case HID_KEY_PAGE_DOWN:
        {
            output[0] = 0x1BU;
            output[1] = '[';
            output[2] = '6';
            output[3] = '~';
            outputLength = 4U;
            break;
        }

        default:
        {
            character =
                LocalShell_MapPrintableKey(
                    modifier,
                    usageId
                );

            if(character == 0U)
            {
                return;
            }

            /*
             * Alt+键按传统终端语义加 ESC 前缀。
             */
            if((modifier & HID_MOD_ALT_MASK) != 0U)
            {
                output[0] = 0x1BU;
                output[1] = character;
                outputLength = 2U;
            }
            else
            {
                output[0] = character;
                outputLength = 1U;
            }

            break;
        }
    }

    LocalShell_PushKeyboardData(
        output,
        outputLength
    );
}
