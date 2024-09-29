#include "cons.h"

#ifndef ARM_ALLOW_MULTI_CORE
#define ARM_ALLOW_MULTI_CORE
#endif

#include <circle/multicore.h>

struct IntlMapStruct {
    u8 mapNormal[71] ;
    u8 mapShift[71] ;
    struct { int code; int shift; int character; } mapOther[10] ;
    struct { int code; int character; } mapAltGr[12] ;
    u8 keypadDecimal ;
} ;

const struct IntlMapStruct intlMap = {
    { // normal
        0    ,0    ,0    ,0    ,'a'  ,'b'  ,'c'  ,'d'  ,
        'e'  ,'f'  ,'g'  ,'h'  ,'i'  ,'j'  ,'k'  ,'l'  ,
        'm'  ,'n'  ,'o'  ,'p'  ,'q'  ,'r'  ,'s'  ,'t'  ,
        'u'  ,'v'  ,'w'  ,'x'  ,'y'  ,'z'  ,'1'  ,'2'  ,
        '3'  ,'4'  ,'5'  ,'6'  ,'7'  ,'8'  ,'9'  ,'0'  ,
        KEY_ENTER, KEY_ESC, KEY_BACKSPACE, KEY_TAB, ' ', '-', '=', '[',
        ']'  ,'\\' ,'\\' ,';'  ,'\'' ,'`'  ,','  ,'.'  ,
        '/'  ,0 , KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5, KEY_F6   ,
        KEY_F7, KEY_F8, KEY_F9, KEY_F10, KEY_F11, KEY_F12, KEY_PRSCRN
     },
    { // shift
        0    ,0    ,0    ,0    ,'A'  ,'B'  ,'C'  ,'D'  ,
        'E'  ,'F'  ,'G'  ,'H'  ,'I'  ,'J'  ,'K'  ,'L'  ,
        'M'  ,'N'  ,'O'  ,'P'  ,'Q'  ,'R'  ,'S'  ,'T'  ,
        'U'  ,'V'  ,'W'  ,'X'  ,'Y'  ,'Z'  ,'!'  ,'@'  ,
        '#'  ,'$'  ,'%'  ,'^'  ,'&'  ,'*'  ,'('  ,')'  ,
        KEY_ENTER ,KEY_ESC   ,KEY_BACKSPACE  ,KEY_TAB ,' '  ,'_'  ,'+'  ,'{'  ,
        '}'  ,'|'  ,'|'  ,':'  ,'"'  ,'~'  ,'<'  ,'>'  ,
        '?'  ,0 , KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5, KEY_F6   ,
        KEY_F7, KEY_F8, KEY_F9, KEY_F10, KEY_F11, KEY_F12, KEY_PRSCRN
    },
    {
        {0x64, 0, '\\'},
        {0x64, 1, '|'},
        {-1,-1}
    },
    {
        {HID_KEY_ENTER, HID_KEY_KEYPAD_ENTER},
        {HID_KEY_ARROW_UP,    '8'},
        {HID_KEY_ARROW_DOWN,  '2'},
        {HID_KEY_ARROW_LEFT,  '4'},
        {HID_KEY_ARROW_RIGHT, '6'},
        {-1,-1}
    },
    '.'
} ;

static const int keyMapSpecial[] = {KEY_PAUSE, KEY_INSERT, KEY_HOME, KEY_PUP, KEY_DELETE, KEY_END, KEY_PDOWN, KEY_RIGHT, KEY_LEFT, KEY_DOWN, KEY_UP} ;
static const int keyMapKeypad[]    = {'\\', '*', '-', '+', KEY_ENTER, KEY_END, KEY_DOWN, KEY_PDOWN, KEY_LEFT, 0, KEY_RIGHT, KEY_HOME, KEY_UP, KEY_PUP, KEY_INSERT, KEY_DELETE} ;
static const int keyMapKeypadNum[] = {'\\', '*', '-', '+', KEY_ENTER , '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '.'} ;

static u8 keyboard_led_status = 0 ;

static unsigned char keyboard_map_key_ascii(unsigned char key, unsigned char modifier) {
    unsigned char ascii = 0 ;

    if (!modifier) {
        if (key == HID_KEY_CAPS_LOCK) {
            keyboard_led_status ^= LED_CAPS_LOCK ;
        } else if (key == HID_KEY_NUM_LOCK) {
            keyboard_led_status ^= LED_NUM_LOCK ;
        } else if (key == HID_KEY_SCROLL_LOCK) {
            keyboard_led_status ^= LED_SCROLL_LOCK ;
        }
    }

    if (modifier & KEYBOARD_MODIFIER_RIGHTALT) {
        for (u8 i = 0; intlMap.mapAltGr[i].code >= 0; i++) {
            if (intlMap.mapAltGr[i].code == key) {
                ascii = intlMap.mapAltGr[i].character ;
                break ;
            }
        }
    } else if (key <= HID_KEY_PRINT_SCREEN) {
        bool ctrl  = (modifier & (KEYBOARD_MODIFIER_LEFTCTRL | KEYBOARD_MODIFIER_RIGHTCTRL)) !=0 ;
        bool shift = (modifier & (KEYBOARD_MODIFIER_LEFTSHIFT | KEYBOARD_MODIFIER_RIGHTSHIFT)) != 0 ;
        bool caps  = (key >= HID_KEY_A) && (key <= HID_KEY_Z) && (keyboard_led_status & LED_CAPS_LOCK) != 0 ;

        if(shift ^ caps) {
            ascii = intlMap.mapShift[key] ;
        } else {
            ascii = intlMap.mapNormal[key] ;
        }

        if (ctrl && ascii >= 0x40 && ascii < 0x7f) {
            ascii &= 0x1f ;
        }
    } else if ((key >= HID_KEY_PAUSE) && (key <= HID_KEY_ARROW_UP)) {
        ascii = keyMapSpecial[key - HID_KEY_PAUSE] ;
    } else if (key == HID_KEY_RETURN) {
        ascii = HID_KEY_RETURN ;
    } else if (key == HID_KEY_KEYPAD_ENTER) {
        ascii = HID_KEY_KEYPAD_ENTER ;
    } else if ((key >= HID_KEY_KEYPAD_DIVIDE) && (key <= HID_KEY_KEYPAD_DECIMAL)) {
        if ((keyboard_led_status & LED_NUM_LOCK) == 0) {
            ascii = keyMapKeypad[key - HID_KEY_KEYPAD_DIVIDE] ;
        } else if (key == HID_KEY_KEYPAD_DECIMAL) {
            ascii = intlMap.keypadDecimal ;
        } else {
            ascii = keyMapKeypadNum[key - HID_KEY_KEYPAD_DIVIDE] ;
        }
    } else {
        bool shift = (modifier & (KEYBOARD_MODIFIER_LEFTSHIFT | KEYBOARD_MODIFIER_RIGHTSHIFT)) != 0 ;

        for(u8 i = 0; intlMap.mapOther[i].code >= 0; i++) {
            if (intlMap.mapOther[i].code == key && intlMap.mapOther[i].shift == shift) {
                ascii = intlMap.mapOther[i].character ;
            }
        }
    }

    return ascii ;
}

void Console::keyboardRemovedHandler(CDevice *device, void *context) {
    if (pthis == 0) {
        return ;
    }

    pthis->keyboard = 0 ;
}

void Console::processKeyboardVT(u8 key) {
    switch (key) {
        case KEY_UP:    sendEscapeSequence('A') ; break ;
        case KEY_DOWN:  sendEscapeSequence('B') ; break ;
        case KEY_RIGHT: sendEscapeSequence('C') ; break ;
        case KEY_LEFT:  sendEscapeSequence('D') ; break ;

        case KEY_F1:
        case KEY_F2:
        case KEY_F3: {
            sendString("\033") ;
            if (!vt52_mode) {
                sendChar('O') ;
            }
            sendChar('P' + (key - KEY_F1)) ;
            break;
        }

        case KEY_F5:
            sendString("\033Ow") ;
            break;

        default:
            sendChar(key) ;
            break;
    }
}

static unsigned char lastKey = 0 ;
static unsigned char lastMod = 0 ;
static u64 lastTick = 0 ;

void Console::keyStatusHandler(unsigned char modifiers, const unsigned char keys[6]) {
    if (pthis == 0) {
        return ;
    }

    if (!modifiers && !keys[0]) {
        lastKey = 0 ;
        lastMod = 0 ;
        return ;
    }

    unsigned char k = keys[0] ;

    if (!k) {
        lastKey = 0 ;
        lastMod = modifiers ;
        return ;
    }

    if (keys[1]) {
        k = keys[1] ;
    }

    if (lastKey == k && lastMod != modifiers) {
        lastMod = modifiers ;
        return ;
    }

    if (
        modifiers & (KEYBOARD_MODIFIER_LEFTCTRL | KEYBOARD_MODIFIER_RIGHTCTRL) &&
        modifiers & (KEYBOARD_MODIFIER_LEFTALT | KEYBOARD_MODIFIER_RIGHTALT) &&
        k == HID_KEY_DELETE
    ) {
        pthis->shutdownMode = ShutdownReboot ;
        CMultiCoreSupport::SendIPI(0, IPI_USER) ;
    }

    if (!modifiers && k == HID_KEY_F12) {
        pthis->vt52_mode = !pthis->vt52_mode ;
        pthis->showStatus() ;
        return ;
    }

    if (modifiers & KEYBOARD_MODIFIER_LEFTGUI && k == HID_KEY_SPACE) {
        pthis->koi7n1 = !pthis->koi7n1 ;
        pthis->showRusLat() ;
        return ;
    }

    u64 now = pthis->timer->GetClockTicks64() ;
    if (now - lastTick < 30000UL) {
        return ;
    }

    lastMod = modifiers ;
    lastKey = k ;
    lastTick = now ;

    if (!modifiers && (k == HID_KEY_RETURN || k == HID_KEY_KEYPAD_ENTER)) {
            pthis->sendString("\033OM") ;
    } else if (modifiers & KEYBOARD_MODIFIER_RIGHTALT && k == HID_KEY_ENTER) {
            pthis->sendString("\033OM") ;
    } else {
        u8 ascii = keyboard_map_key_ascii(k, modifiers) ;
        pthis->processKeyboardVT(ascii) ;
    }

    pthis->actLED->On() ;
    pthis->led_ticks = pthis->timer->GetTicks() + 2 ;
}

void Console::keyboardLoop() {
    if (this->keyboard == 0) {
        if (this->usbhci.UpdatePlugAndPlay()) {
            keyboard = (CUSBKeyboardDevice *) this->deviceNameService->GetDevice("ukbd1", FALSE) ;
            if (keyboard != 0) {
                keyboard->RegisterRemovedHandler(keyboardRemovedHandler) ;
                keyboard->RegisterKeyStatusHandlerRaw(keyStatusHandler) ;
            }
        }
    }

    if (this->keyboard != 0) {
        if (this->keyboard->GetLEDs() != keyboard_led_status) {
            this->keyboard->SetLEDs(keyboard_led_status) ;
        }
    }
}
