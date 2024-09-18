#include "cons.h"

static const u8 graphics_char_mapping[31] = {
    0x04, // diamond/caret
    0xB1, // scatter
    0x0B, // HT
    0x0C, // FF
    0x0D, // CR
    0x0E, // LF
    0xF8, // degree symbol
    0xF1, // plusminus
    0x0F, // NL
    0x10, // VT
    0xD9, // left-top corner
    0xBF, // left-bottom corner
    0xDA, // right-bottom corner
    0xC0, // right-top corner
    0xC5, // cross
    0x11, // horizontal line 1
    0x12, // horizontal line 2
    0xC4, // horizontal line 3
    0x13, // horizontal line 4
    0x5F, // horizontal line 5
    0xC3, // right "T"
    0xB4, // left "T"
    0xC1, // top "T"
    0xC2, // bottom "T"
    0xB3, // vertical line
    0xF3, // less-equal
    0xF2, // greater-equal
    0xE3, // pi
    0x1C, // not equal
    0x9C, // pound sterling
    0xFA  // center dot
} ;

// 0x40 - 0x7E
static const u8 ru_char_mapping[63] = {
    0xEE, 0xA0, 0xA1, 0xE6, 0xA4, 0xA5, 0xE4, 0xA3, 0xE5, 0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE,
    0xAF, 0xEF, 0xE0, 0xE1, 0xE2, 0xE3, 0xA6, 0xA2, 0xEC, 0xEB, 0xA7, 0xE8, 0xED, 0xE9, 0xE7, 0xEA,
    0x9E, 0x80, 0x81, 0x96, 0x84, 0x85, 0x94, 0x83, 0x95, 0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E,
    0x8F, 0x9F, 0x90, 0x91, 0x92, 0x93, 0x86, 0x82, 0x9C, 0x9B, 0x87, 0x98, 0x9D, 0x99, 0x97
} ;

#define MAX(a, b) ((a)>(b)?(a):(b))
#define MIN(a, b) ((b)>(a)?(a):(b))

void Console::vtScrollRegion(u8 top_limit, u8 bottom_limit, int offset, TScreenColor bg_color) {
    this->screen->scrollRegion(top_limit, bottom_limit, offset, bg_color) ;
}

void Console::vtShowCursor(boolean show) {
	unsigned px = cursor_col * screen->getCharWidth() ;
	unsigned py1 = (cursor_row + 1) * screen->getCharHeight() - 3 ;
	unsigned py2 = (cursor_row + 1) * screen->getCharHeight() - 2 ;
	unsigned py3 = (cursor_row + 1) * screen->getCharHeight() - 1 ;

    for (unsigned x = px; x < (px + screen->getCharWidth()); x++) {
        screen->SetPixel(x, py1, show ? color_fg : color_bg) ;
        screen->SetPixel(x, py2, show ? color_fg : color_bg) ;
        screen->SetPixel(x, py3, show ? color_fg : color_bg) ;
    }
}

void Console::vtMoveCursorWrap(u8 row, u8 col) {
    if (row == cursor_row && col == cursor_col) {
        return ;
    }

    int top_limit    = scroll_region_start ;
    int bottom_limit = scroll_region_end ;
    
    if (cursor_shown && cursor_row >= 0 && cursor_col >= 0) {
        vtShowCursor(false) ;
    }
    
    while (col < 0) {
        col += TEXTMODE_COLS ;
        row--;
    }
    
    while (row < top_limit) {
        row++;
        vtScrollRegion(top_limit, bottom_limit, -1, color_bg) ;
    }
    while (col >= TEXTMODE_COLS) {
        col -= TEXTMODE_COLS ;
        row++ ;
    }
    while (row > bottom_limit) {
        row--;
        vtScrollRegion(top_limit, bottom_limit, 1, color_bg) ;
    }

    cursor_row = row ;
    cursor_col = col ;
    cursor_eol = false ;
    
    vtShowCursor(true) ;
}

void Console::vtMoveCursorWithinRegion(u8 row, u8 col, u8 top_limit, u8 bottom_limit) {
    if (row == cursor_row && col == cursor_col) {
        return ;
    }

    if (cursor_shown && cursor_row >= 0 && cursor_col >= 0) {
        vtShowCursor(false) ;
    }

    if (col < 0) {
        col = 0 ;
    } else if (col >= TEXTMODE_COLS) {
        col = TEXTMODE_COLS - 1 ;
    }

    if (row < top_limit) {
        row = top_limit ;
    } else if (row > bottom_limit) {
        row = bottom_limit ;
    }
        
          
    cursor_row = row ;
    cursor_col = col ;
    cursor_eol = false ;

    vtShowCursor(true) ;
}

void Console::vtMoveCursorLimited(u8 row, u8 col) {
    if (cursor_row >= scroll_region_start && cursor_row <= scroll_region_end) {
        vtMoveCursorWithinRegion(row, col, scroll_region_start, scroll_region_end) ;
    }
}

void Console::vtInitCursor(u8 row, u8 col) {
    cursor_row = -1;
    cursor_col = -1;
    vtMoveCursorWithinRegion(row, col, 0, CONS_LAST_ROW) ;
}

static u8 map_graphics(u8 c) {
    if (c < 0x60 || c > 0x7E) {
        return c ;
    }

    return graphics_char_mapping[c - 0x60] ;
}

static u8 map_ru(u8 c) {
    if (c < 0x40 || c > 0x7E) {
        return c ;
    }

    return ru_char_mapping[c - 0x40] ;
}

void Console::vtPutChar(char c) {
    if (cursor_eol) {
        vtMoveCursorWrap(cursor_row + 1, 0) ;
        cursor_eol = false ;
    }

    TScreenColor fg = (attr & ATTR_INVERSE) ? color_bg : color_fg ;
    TScreenColor bg = (attr & ATTR_INVERSE) ? color_fg : color_bg ;

    if (attr & ATTR_UNDERLINE) {
        bg = MAGENTA_COLOR ;
    }

    // if (attr & ATTR_BOLD) {
    //     fg += 8 ;
    // }

    this->screen->displayChar(*charset ? (koi7n1 ? map_ru(c) : map_graphics(c)) : c, cursor_col, cursor_row, fg, bg) ;

    if (cursor_col == TEXTMODE_COLS - 1) {
        // cursor stays in last column but will wrap if another character is typed
        vtShowCursor(cursor_shown);
        cursor_eol = true ;
    } else {
        vtInitCursor(cursor_row, cursor_col + 1) ;
    }
}

void Console::vtFillRegion(u8 xs, u8 ys, u8 xe, u8 ye, char c, TScreenColor fg, TScreenColor bg) {
    int from = CONSADDR(ys) + xs ;
    int to = CONSADDR(ye) + xe ;
    for (u8 y = ys; y <= ye; y++) {
        for (u8 x = 0; x < TEXTMODE_COLS; x++) {
            int a = CONSADDR(y) + x ;
            if (a >= from && a <= to) {
                screen->displayChar(c, x, y, fg, bg) ;
            }
        }
    }
}

void Console::vtInsert(u8 x, u8 y, u8 n, TScreenColor fg) {
    if (x >= TEXTMODE_COLS || y >= CONS_LAST_ROW) {
        return ;
    }

    int xfrom = x * screen->getCharWidth() ;
    int xto = (x + n) * screen->getCharWidth() ;
    int xn = n * screen->getCharWidth() ;
    int yfrom = y * screen->getCharHeight() ;
    int yto   = (y + 1) * screen->getCharHeight() ;

    for (int r = yfrom; r < yto; r++) {
        for (int c = xto; c >= xfrom; c--) {
            screen->SetPixel(c, r, screen->GetPixel(c - xn, r)) ;
        }
    }

    for (int i = 0; i < n; i++) {
        screen->displayChar(' ', x + i, y, fg) ;
    }
}

void Console::vtDelete(u8 x, u8 y, u8 n, TScreenColor fg) {
    if (x >= TEXTMODE_COLS || y >= CONS_LAST_ROW) {
        return ;
    }

    int xfrom = x * screen->getCharWidth() ;
    int xto = (x + n) * screen->getCharWidth() ;
    int xn = n * screen->getCharWidth() ;
    int yfrom = y * screen->getCharHeight() ;
    int yto   = (y + 1) * screen->getCharHeight() ;

    for (int r = yfrom; r < yto; r++) {
        for (int c = xfrom; c <= xto; c++) {
            screen->SetPixel(c, r, screen->GetPixel(c + xn, r)) ;
        }
    }

    for (int i = n; i > 0; i--) {
        screen->displayChar(' ', x + i, y, fg) ;
    }
}

void Console::vtProcessText(char c) {
    switch (c) {
        case 5: // ENQ => send answer-back string
        sendString("murm-pdp-11") ;
        break;
      
        case 7: // BEL => produce beep
        //   sound_play_tone(config_get_audible_bell_frequency(), 
        //                   config_get_audible_bell_duration(), 
        //                   config_get_audible_bell_volume(), 
        //                   false);
        //   framebuf_flash_screen(config_get_visual_bell_color(), config_get_visual_bell_duration());
            // graphics_set_flashmode(false, true) ;
            break ;
      
        case 0x08:   // backspace
        case 0x7F: { // delete
            u8 mode = c == 8 ? 1 : 2 ;
            if (mode > 0) {
                int top_limit = scroll_region_start ;
                if (cursor_row > top_limit) {
                    vtMoveCursorWrap(cursor_row, cursor_col - 1) ;
                } else {
                    vtMoveCursorLimited(cursor_row, cursor_col - 1) ;
                }

                if (mode == 2) {
                    screen->displayChar(' ', cursor_col, cursor_row) ;
                    vtShowCursor(cursor_shown) ;
                }
            }

            break;
        }

    case '\t': // horizontal tab
      {
        int col = cursor_col + 4 ;
        vtMoveCursorLimited(cursor_row, col) ;
        break;
      }
      
    case '\n': // newline
    case 0x0B:   // vertical tab (interpreted as newline)
        vtMoveCursorWrap(cursor_row + 1, cursor_col) ;
        break ;
    case 0x0C:   // form feed (interpreted as newline)
    case '\r': // carriage return
        vtMoveCursorWrap(cursor_row, 0) ;
        break ;

    case 0x0E:  // SO
        charset = &charset_G1;
        break;

    case 0x0F:  // SI
        charset = &charset_G0; 
        break;

    default: // regular character
        if (c >= 32 ) {
            vtPutChar(c) ;
        }
        break;
    }
}

void Console::vtReset() {
    saved_col = 0;
    saved_row = 0;
    cursor_shown = true;
    color_fg = YELLOW_COLOR ;
    color_bg = 0;
    scroll_region_start = 0;
    scroll_region_end = CONS_LAST_ROW ;
    attr = 0;
    cursor_eol = false ;
    saved_attr = 0;
    charset_G0 = CS_TEXT;
    charset_G1 = CS_GRAPHICS;
    saved_charset_G0 = CS_TEXT;
    saved_charset_G1 = CS_GRAPHICS;
    charset = &charset_G0;
    vt52_mode = false ;
    koi7n1 = false ;
}

void Console::vtCls() {
    for (int r = CONS_TOP; r < TEXTMODE_ROWS; r++) {
        for (int c = 0; c < TEXTMODE_COLS; c++) {
            screen->displayChar(' ', c, r) ;
        }
    }
}

void Console::vtProcessCommand(char start_char, char final_char, u8 num_params, u8 *params) {
  // NOTE: num_params>=1 always holds, if no parameters were received then params[0]=0
    if (final_char == 'l' || final_char == 'h') {
        bool enabled = final_char == 'h';
        if (start_char == '?') {
            switch(params[0]) {
                case 2:
                    if (!enabled) {
                        vtReset();
                        vt52_mode = true ;
                    }
                    break ;

                case 3: // switch 80/132 columm mode - 132 columns not supported but we can clear the screen
                    vtCls() ;
                    break ;

                case 4: // enable smooth scrolling (emulated via scroll delay)
                    //framebuf_set_scroll_delay(enabled ? config_get_terminal_scrolldelay() : 0);
                    break;
              
                case 5: // invert screen
                    // framebuf_set_screen_inverted(enabled);
                    break;
          
                case 6: // origin mode
                    // origin_mode = enabled; 
                    vtMoveCursorLimited(scroll_region_start, 0); 
                    break;
              
                case 7: // auto-wrap mode
                    // auto_wrap_mode = enabled; 
                    break;

                case 12: // local echo (send-receive mode)
                    // localecho = !enabled;
                    break;
              
                case 25: // show/hide cursor
                    cursor_shown = enabled;
                    vtShowCursor(cursor_shown);
                    break;
            }
        } else if (start_char == 0) {
            switch (params[0]) {
                case 4: // insert mode
                    // insert_mode = enabled;
                    break;
            }
        }
    } else if (final_char == 'J') {
        switch (params[0]) {
            case 0:
                vtFillRegion(cursor_col, cursor_row, TEXTMODE_COLS - 1, CONS_LAST_ROW, ' ', color_fg, color_bg);
                break;
          
            case 1:
                vtFillRegion(0, 0, cursor_col, cursor_row, ' ', color_fg, color_bg);
                break;
          
            case 2:
                vtFillRegion(0, 0, TEXTMODE_COLS - 1, CONS_LAST_ROW, ' ', color_fg, color_bg);
                break;
        }

        vtShowCursor(cursor_shown);
    } else if (final_char == 'K') {
        switch (params[0]) {
            case 0:
                vtFillRegion(cursor_col, cursor_row, TEXTMODE_COLS - 1, cursor_row, ' ', color_fg, color_bg);
                break;
          
            case 1:
                vtFillRegion(0, cursor_row, cursor_col, cursor_row, ' ', color_fg, color_bg);
                break;
          
            case 2:
                vtFillRegion(0, cursor_row, TEXTMODE_COLS - 1, cursor_row, ' ', color_fg, color_bg);
                break;
        }

        // cur_attr = framebuf_get_attr(cursor_col, cursor_row);
        vtShowCursor(cursor_shown);
    } else if (final_char == 'A') {
        vtMoveCursorLimited(cursor_row - MAX(1, params[0]), cursor_col);
    } else if (final_char == 'B') {
        vtMoveCursorLimited(cursor_row + MAX(1, params[0]), cursor_col);
    } else if (final_char == 'C' || final_char == 'a') {
        vtMoveCursorLimited(cursor_row, cursor_col + MAX(1, params[0]));
    } else if (final_char == 'D' || final_char == 'j') {
        vtMoveCursorLimited(cursor_row, cursor_col - MAX(1, params[0]));
    } else if (final_char == 'E' || final_char == 'e') {
        vtMoveCursorLimited(cursor_row + MAX(1, params[0]), 0);
    } else if (final_char == 'F' || final_char == 'k') {
        vtMoveCursorLimited(cursor_row - MAX(1, params[0]), 0);
    } else if (final_char == 'd') {
        vtMoveCursorLimited(MAX(1, params[0]), cursor_col);
    } else if (final_char == 'G' || final_char == '`') {
        vtMoveCursorLimited(cursor_row, MAX(1, params[0])-1);
    } else if (final_char == 'H' || final_char=='f') {
        int top_limit    = scroll_region_start ;
        int bottom_limit = scroll_region_end ;
        vtMoveCursorWithinRegion(top_limit + MAX(params[0],1) - 1, num_params < 2 ? 0 : MAX(params[1],1) - 1, top_limit, bottom_limit);
    } else if (final_char == 'I') {
        int n = MAX(1, params[0]);
        int col = cursor_col + 1;
        while (n > 0 && col < TEXTMODE_COLS - 1) {
                while (col < TEXTMODE_COLS - 1) {
                    col++;
                }
                n--;
        }
        vtMoveCursorLimited(cursor_row, col); 
    } else if (final_char == 'Z') {
        int n = MAX(1, params[0]);
        int col = cursor_col-1;
        while (n > 0 && col > 0) {
            while (col > 0) {
                col--;
            }
            n--;
        }
        vtMoveCursorLimited(cursor_row, col); 
    } else if (final_char == 'L' || final_char == 'M') {
        int n = MAX(1, params[0]);
        int bottom_limit = scroll_region_end ;
        vtShowCursor(false);
        vtScrollRegion(cursor_row, bottom_limit, final_char == 'M' ? n : -n, color_bg);
        vtShowCursor(cursor_shown);
    } else if (final_char=='@') {
        int n = MAX(1, params[0]);
        vtShowCursor(false);
        vtInsert(cursor_col, cursor_row, n, color_fg);
        // cur_attr = framebuf_get_attr(cursor_col, cursor_row);
        vtShowCursor(cursor_shown);
    } else if (final_char == 'P') {
        int n = MAX(1, params[0]);
        vtDelete(cursor_col, cursor_row, n, color_fg);
        vtShowCursor(cursor_shown);
    } else if (final_char == 'S' || final_char == 'T') {
        int top_limit    = scroll_region_start;
        int bottom_limit = scroll_region_end;
        int n = MAX(1, params[0]);
        vtShowCursor(false);
        while (n--) {
            vtScrollRegion(top_limit, bottom_limit, final_char == 'S' ? n : -n, color_bg);
        }
        vtShowCursor(cursor_shown);
    } else if (final_char == 'g') {
        int p = params[0];
        if (p==0) {
            // tabs[cursor_col] = false;
        } else if (p == 3) {
            // memset(tabs, 0, TEXTMODE_COLS) ;
        }
    } else if (final_char == 'm') {
        unsigned int i;
        for (i = 0; i < num_params; i++) {
            int p = params[i] ;
            if (p == 0) {
                color_fg = YELLOW_COLOR;
                color_bg = 0;
                attr     = 0 ;
                cursor_shown = true;
                vtShowCursor(cursor_shown);
            } else if (p == 1) {
                attr |= ATTR_BOLD;
            } else if (p == 4) {
                attr |= ATTR_UNDERLINE;
            } else if (p == 5) {
                attr |= ATTR_BLINK;
            } else if (p == 7) {
                attr |= ATTR_INVERSE;
            } else if (p == 22) {
                attr &= ~ATTR_BOLD;
            } else if( p==24 ) {
                attr &= ~ATTR_UNDERLINE;
            } else if(p == 25) {
                attr &= ~ATTR_BLINK;
            } else if(p == 27) {
                attr &= ~ATTR_INVERSE;
            } else if (p >= 30 && p <= 37) {
                color_fg = p - 30;
            } else if (p == 38 && num_params >= i + 2 && params[i+1] == 5) {
                color_fg = params[i + 2] & 0x0F;
                i += 2;
            } else if (p == 39) {
                color_fg = YELLOW_COLOR;
            } else if (p >= 40 && p <= 47) {
                color_bg = p - 40 ;
            } else if (p == 48 && num_params >= i + 2 && params[i + 1] == 5) {
                color_bg = params[i+2] & 0x0F;
                i+=2;
            } else if (p == 49) {
                color_bg = 0;
            }

            vtShowCursor(cursor_shown);
        }
    } else if (final_char == 'r') {
        if (num_params == 2 && params[1] > params[0]) {
            scroll_region_start = MAX(params[0], 1)-1;
            scroll_region_end   = MIN(params[1], CONS_LAST_ROW) ;
        } else if (params[0] == 0) {
            scroll_region_start = 0;
            scroll_region_end   = CONS_LAST_ROW;
        }

        vtMoveCursorWithinRegion(scroll_region_start, 0, scroll_region_start, scroll_region_end);
    } else if (final_char == 's') {
        saved_row = cursor_row;
        saved_col = cursor_col;
        saved_eol = cursor_eol;
        //   saved_origin_mode = origin_mode;
          saved_fg  = color_fg;
          saved_bg  = color_bg;
          saved_attr = attr;
          saved_charset_G0 = charset_G0;
          saved_charset_G1 = charset_G1;
    } else if (final_char == 'u') {
        vtMoveCursorLimited(saved_row, saved_col);
        // origin_mode = saved_origin_mode;      
        cursor_eol = saved_eol;
        color_fg = saved_fg;
        color_bg = saved_bg;
        attr = saved_attr;
        charset_G0 = saved_charset_G0;
        charset_G1 = saved_charset_G1;
    } else if (final_char == 'c') {
        sendString("\033[?6c");
    } else if (final_char == 'n') {
        if (params[0] == 5) {
          // terminal status report
          sendString("\033[0n");
        } else if (params[0] == 6) {
          // cursor position report
            int top_limit = scroll_region_start ;
            CString buf ;
            buf.Format("\033[%u;%uR", cursor_row-top_limit+1, cursor_col+1);
            sendString(buf);
        }
    }
}

static u8 get_charset(char c) {
    switch (c) {
        case 'A' : return CS_TEXT;
        case 'B' : return CS_TEXT;
        case '0' : return CS_GRAPHICS;
        case '1' : return CS_TEXT;
        case '2' : return CS_GRAPHICS;
    }

    return CS_TEXT;
}

void Console::putCharVT(char c)  {
    if (vt52_mode) {
        putCharVT52(c) ;
        return ;
    }

    putCharVT100(c) ;
}

void Console::putCharVT100(char c) {
    static char    start_char = 0 ;
    static u8 num_params = 0 ;
    static u8 params[16] ;

    if (terminal_state != TS_NORMAL) {
        if (c == 8 || c == 10 || c == 13) {
            // processe some cursor control characters within escape sequences
            // (otherwise we fail "vttest" cursor control tests)
            vtProcessText(c);
            return;
        } else if (c == 11) {
            // ignore VT character plus the following character
            // (otherwise we fail "vttest" cursor control tests)
            terminal_state = TS_READCHAR ;
            return;
        }
    }

    switch (terminal_state) {
        case TS_NORMAL: {
            if (c == 0x1B) {
                terminal_state = TS_WAITBRACKET ;
            } else {
                vtProcessText(c) ;
            }

            break;
        }

        case TS_WAITBRACKET: {
            terminal_state = TS_NORMAL;

            switch (c) {
                case '[':
                    start_char = 0;
                    num_params = 1;
                    params[0] = 0;
                    terminal_state = TS_STARTCHAR;
                    break;
            
                case '#':
                    terminal_state = TS_HASH;
                    break;
            
                case 0x1B: vtPutChar(c); break;                           // escaped ESC
                case 'c': vtReset(); break;                           // reset
                case '7': vtProcessCommand(0, 's', 0, nullptr); break;  // save cursor position
                case '8': vtProcessCommand(0, 'u', 0, nullptr); break;  // restore cursor position
                case 'H':                                                // set tab
                    // tabs[cursor_col] = true;
                    break;
                case 'J': vtProcessCommand(0, 'J', 0, nullptr); break;  // clear to end of screen
                case 'K': vtProcessCommand(0, 'K', 0, nullptr); break;  // clear to end of row
                case 'D': vtMoveCursorWrap(cursor_row+1, cursor_col); break; // cursor down
                case 'E': vtMoveCursorWrap(cursor_row+1, 0); break;          // cursor down and to first column
                case 'I': vtMoveCursorWrap(cursor_row-1, 0); break;          // cursor up and to furst column
                case 'M': vtMoveCursorWrap(cursor_row-1, cursor_col); break; // cursor up
                case '(': 
                case ')': 
                case '+':
                case '*':
                    start_char = c;
                    terminal_state = TS_READCHAR;
                    break;
            }

            break;
        }

    case TS_STARTCHAR:
    case TS_READPARAM: {
        if (c >= '0' && c <= '9') {
            params[num_params - 1] = params[num_params - 1] * 10 + (c - '0') ;
            terminal_state = TS_READPARAM;
        } else if (c == ';') {
            // next parameter (max 16 parameters)
            num_params++ ;
            if (num_params > 16) {
                terminal_state = TS_NORMAL;
            } else {
                params[num_params-1] = 0 ;
                terminal_state = TS_READPARAM;
            }
        } else if (terminal_state == TS_STARTCHAR && (c == '?' || c == '#')) {
            start_char = c;
            terminal_state = TS_READPARAM;
        } else {
            // not a parameter value or startchar => command is done
            vtProcessCommand(start_char, c, num_params, params);
            terminal_state = TS_NORMAL;
        }
        
        break;
    }

    case TS_HASH: {
        switch (c) {
            case '3': {
                // framebuf_set_row_attr(cursor_row, ROW_ATTR_DBL_WIDTH | ROW_ATTR_DBL_HEIGHT_TOP);
                break;
            }

            case '4': {
                // framebuf_set_row_attr(cursor_row, ROW_ATTR_DBL_WIDTH | ROW_ATTR_DBL_HEIGHT_BOT);
                break;
            }
            
            case '5': {
                // framebuf_set_row_attr(cursor_row, 0) ;
                break;
            }

            case '6': {
                // framebuf_set_row_attr(cursor_row, ROW_ATTR_DBL_WIDTH);
                break;
            }

            case '8': {
                // fill screen with 'E' characters (DEC test feature)
                int top_limit    = scroll_region_start ;
                int bottom_limit = scroll_region_end ;
                vtShowCursor(false);
                vtFillRegion(0, top_limit, TEXTMODE_COLS - 1, bottom_limit, 'E', color_fg, color_bg);
                // cur_attr = framebuf_get_attr(cursor_col, cursor_row);
                vtShowCursor(true);
                break;
            }
        }
        
        terminal_state = TS_NORMAL;
        break;
    }

    case TS_READCHAR: {
        if (start_char=='(') {
            charset_G0 = get_charset(c);
        } else if (start_char==')') {
            charset_G1 = get_charset(c);
        }

        terminal_state = TS_NORMAL;
        break;
      }
    }
}

void Console::putCharVT52(char c) {
    static char start_char, row ;

    switch (terminal_state) {
        case TS_NORMAL: {
            if (c == 0x1B) {
                terminal_state = TS_STARTCHAR;
            } else {
                vtProcessText(c) ;
            }
            
                break;
            }

        case TS_STARTCHAR: {
            terminal_state = TS_NORMAL;

            switch (c) {
                case 'A': 
                    vtMoveCursorLimited(cursor_row - 1, cursor_col);
                    break;

                case 'B': 
                    vtMoveCursorLimited(cursor_row + 1, cursor_col);
                    break;

                case 'C': 
                    vtMoveCursorLimited(cursor_row, cursor_col + 1);
                    break;

                case 'D': 
                    vtMoveCursorLimited(cursor_row, cursor_col - 1);
                    break;

                case 'E':
                    vtFillRegion(0, 0, TEXTMODE_COLS - 1, CONS_LAST_ROW, ' ', color_fg, color_bg);
                    // fall through

                case 'H': 
                    vtMoveCursorLimited(0, 0);
                    break;

                case 'I': 
                    vtMoveCursorWrap(cursor_row-1, cursor_col);
                    break;

                case 'J':
                    vtShowCursor(false);
                    vtFillRegion(cursor_col, cursor_row, TEXTMODE_COLS - 1, CONS_LAST_ROW, ' ', color_fg, color_bg);
                    // cur_attr = framebuf_get_attr(cursor_col, cursor_row);
                    vtShowCursor(cursor_shown);
                    break;

                case 'K':
                    vtShowCursor(false);
                    vtFillRegion(cursor_col, cursor_row, TEXTMODE_COLS - 1, cursor_row, ' ', color_fg, color_bg);
                    // cur_attr = framebuf_get_attr(cursor_col, cursor_row);
                    vtShowCursor(cursor_shown);
                    break;

                case 'L':
                case 'M':
                    vtShowCursor(false);
                    vtScrollRegion(cursor_row, CONS_LAST_ROW, c == 'M' ? 1 : -1, color_bg);
                    // cur_attr = framebuf_get_attr(cursor_col, cursor_row);
                    vtShowCursor(cursor_shown);
                    break;

                case 'Y':
                    start_char = c;
                    row = 0;
                    terminal_state = TS_READPARAM;
                    break;
                
                case 'Z':
                    sendString("\033/K");
                    break;

                case 'b':
                case 'c':
                    start_char = c;
                    terminal_state = TS_READPARAM;
                    break;

                case 'd':
                    vtFillRegion(0, 0, cursor_col, cursor_row, ' ', color_fg, color_bg);
                    vtInitCursor(cursor_col, cursor_row);
                    break;
                
                case 'e':
                    vtShowCursor(true);
                    break;

                case 'f':
                    vtShowCursor(false);
                    break;

                case 'j':
                    saved_col = cursor_col;
                    saved_row = cursor_row;
                    break;

                case 'k':
                    vtMoveCursorLimited(saved_row, saved_col);
                    break;

                case 'l':
                    vtFillRegion(0, cursor_row, TEXTMODE_COLS - 1, cursor_row, ' ', color_fg, color_bg);
                    vtInitCursor(0, cursor_row);
                    break;

                case 'o':
                    vtFillRegion(0, cursor_row, cursor_col, cursor_row, ' ', color_fg, color_bg);
                    vtShowCursor(cursor_shown);
                    break;

                case 'p':
                    // framebuf_set_screen_inverted(true);
                    break;

                case 'q':
                    // framebuf_set_screen_inverted(false);
                    break;

                case 'v':
                    // auto_wrap_mode = true;
                    break;

                case 'w':
                    // auto_wrap_mode = false;
                    break;

                case '<':
                    vtReset();
                    vt52_mode = false;
                    break;
                }

            break;
        }

        case TS_READPARAM: {
            if (start_char == 'Y') {
                if (row == 0) {
                    row = c;
                } else {
                    if (row >= 32 && c >= 32) {
                        vtMoveCursorLimited(row - 32, c - 32) ;
                    }
                    terminal_state = TS_NORMAL;
                }
            } else if (start_char == 'b' && c >= 32) {
                color_fg = (c-32) & 15;
                vtShowCursor(cursor_shown);
            } else if (start_char == 'c' && c>=32) {
                color_bg = (c-32) & 15 ;
                vtShowCursor(cursor_shown);
            }

            break;
        }
    }

}

void Console::putStringVT100(const char *s) {
    for (int xi = TEXTMODE_COLS; xi--;) {
        if (!*s) break ;
        putCharVT100(*s++) ;
    }
}
