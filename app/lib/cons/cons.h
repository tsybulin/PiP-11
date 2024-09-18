#ifndef _cons_cons_h
#define _cons_cons_h

#include <circle/types.h>
#include <circle/screen.h>
#include <circle/actled.h>
#include <circle/devicenameservice.h>
#include <circle/usb/usbhcidevice.h>
#include <circle/usb/usbkeyboard.h>

#include "shutdown.h"
#include <util/queue.h>

#define CONS_LAST_ROW 23

void gprintf(const char *__restrict format, ...) ;
void iprintf(const char *__restrict format, ...) ;

class Console {
	public:
		Console(CActLED *actLED, CDeviceNameService *deviceNameService, CInterruptSystem *interrupt, CTimer *timer);
		~Console(void);

		void init(CScreenDevice *screen) ;
		void write(const char *buffer, unsigned col, unsigned row, TScreenColor fg = WHITE_COLOR, TScreenColor bg = BLACK_COLOR) ;
		void drawLine(const char* title, unsigned row) ;
		TShutdownMode loop() ;
		void printf(const char *__restrict format, ...) ;
		void vtFillRegion(u8 xs, u8 ys, u8 xe, u8 ye, char c, TScreenColor fg, TScreenColor bg) ;
		void vtCls() ;
		void putCharVT(char c) ;
		void showStatus() ;
		void showRusLat() ;

		static Console* get() ;

	private:
		void keyboardLoop() ;
		static void keyboardRemovedHandler(CDevice *device, void *context) ;
		static void keyStatusHandler(unsigned char modifiers, const unsigned char keys[6]) ;
		void sendChar(char c) ;
		void sendString(const char *str) ;
		void sendEscapeSequence(u8 key) ;
		void processKeyboardVT(u8 key) ;

		volatile TShutdownMode shutdownMode ;
		CActLED *actLED ;
		CDeviceNameService *deviceNameService ;
		CInterruptSystem *interrupt ;
		CTimer *timer ;
		CUSBHCIDevice usbhci ;
		CUSBKeyboardDevice * volatile keyboard ;
		CScreenDevice *screen ;
		unsigned led_ticks ;
		bool koi7n1 ;

		static Console *pthis ;

	private:
		u8 terminal_state = 0 ;
		int cursor_col = 0, cursor_row = 0, saved_col = 0, saved_row = 0;
		int scroll_region_start = 0, scroll_region_end = CONS_LAST_ROW ;
		bool cursor_shown = true, cursor_eol = false, saved_eol = false, vt52_mode = false ;
		TScreenColor color_fg = YELLOW_COLOR, color_bg = BLACK_COLOR, saved_fg, saved_bg ;
		u8 attr = 0, saved_attr, saved_charset_G0, saved_charset_G1, *charset, charset_G0, charset_G1 ;

		void putCharVT100(char c) ;
		void putCharVT52(char c) ;
		void vtScrollRegion(u8 top_limit, u8 bottom_limit, int offset, TScreenColor bg_color) ;
		void vtShowCursor(boolean show) ;
		void vtMoveCursorWrap(u8 row, u8 col) ;
		void vtMoveCursorWithinRegion(u8 row, u8 col, u8 top_limit, u8 bottom_limit) ;
		void vtMoveCursorLimited(u8 row, u8 col) ;
		void vtInitCursor(u8 row, u8 col) ;
		void vtPutChar(char c) ;
		void vtInsert(u8 x, u8 y, u8 n, TScreenColor fg) ;
		void vtDelete(u8 x, u8 y, u8 n, TScreenColor fg) ;
		void vtProcessText(char c) ;
		void vtReset() ;
		void vtProcessCommand(char start_char, char final_char, u8 num_params, u8 *params) ;
		void putStringVT100(const char *c) ;
}  ;

#define TEXTMODE_COLS 80
#define TEXTMODE_ROWS 24
#define CONS_TOP      0
#define CONS_LAST_ROW 23

#define CONSADDR(addr) (TEXTMODE_COLS * (CONS_TOP + addr))

#define TU_BIT(n) (1UL << (n))

typedef enum {
    KEYBOARD_MODIFIER_LEFTCTRL   = TU_BIT(0), ///< Left Control
    KEYBOARD_MODIFIER_LEFTSHIFT  = TU_BIT(1), ///< Left Shift
    KEYBOARD_MODIFIER_LEFTALT    = TU_BIT(2), ///< Left Alt
    KEYBOARD_MODIFIER_LEFTGUI    = TU_BIT(3), ///< Left Window
    KEYBOARD_MODIFIER_RIGHTCTRL  = TU_BIT(4), ///< Right Control
    KEYBOARD_MODIFIER_RIGHTSHIFT = TU_BIT(5), ///< Right Shift
    KEYBOARD_MODIFIER_RIGHTALT   = TU_BIT(6), ///< Right Alt
    KEYBOARD_MODIFIER_RIGHTGUI   = TU_BIT(7)  ///< Right Window
} hid_keyboard_modifier_bm_t ;

// key values for control keys in ASCII range
#define KEY_BACKSPACE   0x08
#define KEY_TAB         0x09
#define KEY_ENTER       0x0d
#define KEY_ESC         0x1b
#define KEY_DELETE      0x7f

// key values for special (non-ASCII) control keys (returned by keyboard_map_key_ascii)
#define KEY_UP		    0x80
#define KEY_DOWN	    0x81
#define KEY_LEFT	    0x82
#define KEY_RIGHT	    0x83
#define KEY_INSERT	    0x84
#define KEY_HOME	    0x85
#define KEY_END		    0x86
#define KEY_PUP		    0x87
#define KEY_PDOWN	    0x88
#define KEY_PAUSE       0x89
#define KEY_PRSCRN      0x8a
#define KEY_F1      	0x8c
#define KEY_F2      	0x8d
#define KEY_F3      	0x8e
#define KEY_F4      	0x8f
#define KEY_F5      	0x90
#define KEY_F6      	0x91
#define KEY_F7      	0x92
#define KEY_F8      	0x93
#define KEY_F9      	0x94
#define KEY_F10     	0x95
#define KEY_F11     	0x96
#define KEY_F12     	0x97

//--------------------------------------------------------------------+
// HID KEYCODE
//--------------------------------------------------------------------+
#define HID_KEY_NONE                      0x00
#define HID_KEY_A                         0x04
#define HID_KEY_B                         0x05
#define HID_KEY_C                         0x06
#define HID_KEY_D                         0x07
#define HID_KEY_E                         0x08
#define HID_KEY_F                         0x09
#define HID_KEY_G                         0x0A
#define HID_KEY_H                         0x0B
#define HID_KEY_I                         0x0C
#define HID_KEY_J                         0x0D
#define HID_KEY_K                         0x0E
#define HID_KEY_L                         0x0F
#define HID_KEY_M                         0x10
#define HID_KEY_N                         0x11
#define HID_KEY_O                         0x12
#define HID_KEY_P                         0x13
#define HID_KEY_Q                         0x14
#define HID_KEY_R                         0x15
#define HID_KEY_S                         0x16
#define HID_KEY_T                         0x17
#define HID_KEY_U                         0x18
#define HID_KEY_V                         0x19
#define HID_KEY_W                         0x1A
#define HID_KEY_X                         0x1B
#define HID_KEY_Y                         0x1C
#define HID_KEY_Z                         0x1D
#define HID_KEY_1                         0x1E
#define HID_KEY_2                         0x1F
#define HID_KEY_3                         0x20
#define HID_KEY_4                         0x21
#define HID_KEY_5                         0x22
#define HID_KEY_6                         0x23
#define HID_KEY_7                         0x24
#define HID_KEY_8                         0x25
#define HID_KEY_9                         0x26
#define HID_KEY_0                         0x27
#define HID_KEY_ENTER                     0x28
#define HID_KEY_ESCAPE                    0x29
#define HID_KEY_BACKSPACE                 0x2A
#define HID_KEY_TAB                       0x2B
#define HID_KEY_SPACE                     0x2C
#define HID_KEY_MINUS                     0x2D
#define HID_KEY_EQUAL                     0x2E
#define HID_KEY_BRACKET_LEFT              0x2F
#define HID_KEY_BRACKET_RIGHT             0x30
#define HID_KEY_BACKSLASH                 0x31
#define HID_KEY_EUROPE_1                  0x32
#define HID_KEY_SEMICOLON                 0x33
#define HID_KEY_APOSTROPHE                0x34
#define HID_KEY_GRAVE                     0x35
#define HID_KEY_COMMA                     0x36
#define HID_KEY_PERIOD                    0x37
#define HID_KEY_SLASH                     0x38
#define HID_KEY_CAPS_LOCK                 0x39
#define HID_KEY_F1                        0x3A
#define HID_KEY_F2                        0x3B
#define HID_KEY_F3                        0x3C
#define HID_KEY_F4                        0x3D
#define HID_KEY_F5                        0x3E
#define HID_KEY_F6                        0x3F
#define HID_KEY_F7                        0x40
#define HID_KEY_F8                        0x41
#define HID_KEY_F9                        0x42
#define HID_KEY_F10                       0x43
#define HID_KEY_F11                       0x44
#define HID_KEY_F12                       0x45
#define HID_KEY_PRINT_SCREEN              0x46
#define HID_KEY_SCROLL_LOCK               0x47
#define HID_KEY_PAUSE                     0x48
#define HID_KEY_INSERT                    0x49
#define HID_KEY_HOME                      0x4A
#define HID_KEY_PAGE_UP                   0x4B
#define HID_KEY_DELETE                    0x4C
#define HID_KEY_END                       0x4D
#define HID_KEY_PAGE_DOWN                 0x4E
#define HID_KEY_ARROW_RIGHT               0x4F
#define HID_KEY_ARROW_LEFT                0x50
#define HID_KEY_ARROW_DOWN                0x51
#define HID_KEY_ARROW_UP                  0x52
#define HID_KEY_NUM_LOCK                  0x53
#define HID_KEY_KEYPAD_DIVIDE             0x54
#define HID_KEY_KEYPAD_MULTIPLY           0x55
#define HID_KEY_KEYPAD_SUBTRACT           0x56
#define HID_KEY_KEYPAD_ADD                0x57
#define HID_KEY_KEYPAD_ENTER              0x58
#define HID_KEY_KEYPAD_1                  0x59
#define HID_KEY_KEYPAD_2                  0x5A
#define HID_KEY_KEYPAD_3                  0x5B
#define HID_KEY_KEYPAD_4                  0x5C
#define HID_KEY_KEYPAD_5                  0x5D
#define HID_KEY_KEYPAD_6                  0x5E
#define HID_KEY_KEYPAD_7                  0x5F
#define HID_KEY_KEYPAD_8                  0x60
#define HID_KEY_KEYPAD_9                  0x61
#define HID_KEY_KEYPAD_0                  0x62
#define HID_KEY_KEYPAD_DECIMAL            0x63
#define HID_KEY_EUROPE_2                  0x64
#define HID_KEY_APPLICATION               0x65
#define HID_KEY_POWER                     0x66
#define HID_KEY_KEYPAD_EQUAL              0x67
#define HID_KEY_F13                       0x68
#define HID_KEY_F14                       0x69
#define HID_KEY_F15                       0x6A
#define HID_KEY_F16                       0x6B
#define HID_KEY_F17                       0x6C
#define HID_KEY_F18                       0x6D
#define HID_KEY_F19                       0x6E
#define HID_KEY_F20                       0x6F
#define HID_KEY_F21                       0x70
#define HID_KEY_F22                       0x71
#define HID_KEY_F23                       0x72
#define HID_KEY_F24                       0x73
#define HID_KEY_EXECUTE                   0x74
#define HID_KEY_HELP                      0x75
#define HID_KEY_MENU                      0x76
#define HID_KEY_SELECT                    0x77
#define HID_KEY_STOP                      0x78
#define HID_KEY_AGAIN                     0x79
#define HID_KEY_UNDO                      0x7A
#define HID_KEY_CUT                       0x7B
#define HID_KEY_COPY                      0x7C
#define HID_KEY_PASTE                     0x7D
#define HID_KEY_FIND                      0x7E
#define HID_KEY_MUTE                      0x7F
#define HID_KEY_VOLUME_UP                 0x80
#define HID_KEY_VOLUME_DOWN               0x81
#define HID_KEY_LOCKING_CAPS_LOCK         0x82
#define HID_KEY_LOCKING_NUM_LOCK          0x83
#define HID_KEY_LOCKING_SCROLL_LOCK       0x84
#define HID_KEY_KEYPAD_COMMA              0x85
#define HID_KEY_KEYPAD_EQUAL_SIGN         0x86
#define HID_KEY_KANJI1                    0x87
#define HID_KEY_KANJI2                    0x88
#define HID_KEY_KANJI3                    0x89
#define HID_KEY_KANJI4                    0x8A
#define HID_KEY_KANJI5                    0x8B
#define HID_KEY_KANJI6                    0x8C
#define HID_KEY_KANJI7                    0x8D
#define HID_KEY_KANJI8                    0x8E
#define HID_KEY_KANJI9                    0x8F
#define HID_KEY_LANG1                     0x90
#define HID_KEY_LANG2                     0x91
#define HID_KEY_LANG3                     0x92
#define HID_KEY_LANG4                     0x93
#define HID_KEY_LANG5                     0x94
#define HID_KEY_LANG6                     0x95
#define HID_KEY_LANG7                     0x96
#define HID_KEY_LANG8                     0x97
#define HID_KEY_LANG9                     0x98
#define HID_KEY_ALTERNATE_ERASE           0x99
#define HID_KEY_SYSREQ_ATTENTION          0x9A
#define HID_KEY_CANCEL                    0x9B
#define HID_KEY_CLEAR                     0x9C
#define HID_KEY_PRIOR                     0x9D
#define HID_KEY_RETURN                    0x9E
#define HID_KEY_SEPARATOR                 0x9F
#define HID_KEY_OUT                       0xA0
#define HID_KEY_OPER                      0xA1
#define HID_KEY_CLEAR_AGAIN               0xA2
#define HID_KEY_CRSEL_PROPS               0xA3
#define HID_KEY_EXSEL                     0xA4
// RESERVED					              0xA5-DF
#define HID_KEY_CONTROL_LEFT              0xE0
#define HID_KEY_SHIFT_LEFT                0xE1
#define HID_KEY_ALT_LEFT                  0xE2
#define HID_KEY_GUI_LEFT                  0xE3
#define HID_KEY_CONTROL_RIGHT             0xE4
#define HID_KEY_SHIFT_RIGHT               0xE5
#define HID_KEY_ALT_RIGHT                 0xE6
#define HID_KEY_GUI_RIGHT                 0xE7

// charset
#define CS_TEXT     0
#define CS_GRAPHICS 2

// vt state
#define TS_NORMAL      0
#define TS_WAITBRACKET 1
#define TS_STARTCHAR   2 
#define TS_READPARAM   3
#define TS_HASH        4
#define TS_READCHAR    5

#define ATTR_UNDERLINE 0x01
#define ATTR_BLINK     0x02
#define ATTR_BOLD      0x04
#define ATTR_INVERSE   0x08

#endif

