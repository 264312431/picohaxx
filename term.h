#pragma once
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

// packed scancode constants (Little Endian representation of the string after ESC)
enum {
   KEY_ESC           = 27,
   KEY_BKSP          = 0x7F,
   KEY_LEFT          = 0x445B,
   KEY_UP            = 0x415B,
   KEY_RIGHT         = 0x435B,
   KEY_DOWN          = 0x425B,
   KEY_HOME          = 0x485B,
   KEY_END           = 0x465B,
   KEY_INSERT        = 0x7E325B,
   KEY_DELETE        = 0x7E335B,
   KEY_PGUP          = 0x7E355B,
   KEY_PGDN          = 0x7E365B,
   KEY_HOME_ALT      = 0x7E315B,
   KEY_END_ALT       = 0x7E345B,
   KEY_F1            = 0x504F,
   KEY_F2            = 0x514F,
   KEY_F3            = 0x524F,
   KEY_F4            = 0x534F,
   KEY_F5            = 0x35317F,
   KEY_F6            = 0x7E37315B,
   KEY_F7            = 0x7E38315B,
   KEY_F8            = 0x7E39315B,
   KEY_F9            = 0x7E30325B,
   KEY_F10           = 0x7E31325B,
   KEY_F11           = 0x7E31325B,
   KEY_F12           = 0x7E34325B,
   KEY_SHIFT_LEFT    = 0x44323B315B,
   KEY_SHIFT_UP      = 0x41323B315B,
   KEY_SHIFT_RIGHT   = 0x43323B315B,
   KEY_SHIFT_DOWN    = 0x42323B315B,
   KEY_CTRL_LEFT     = 0x44353B315B,
   KEY_CTRL_RIGHT    = 0x43353B315B,
   KEY_ALT_LEFT      = 0x44333B315B,
   KEY_ALT_UP        = 0x41333B315B,
   KEY_ALT_RIGHT     = 0x43333B315B,
   KEY_ALT_DOWN      = 0x42333B315B,
   KEY_CTRL_ALT_LEFT = 0x44373B315B,
   KEY_CTRL_ALT_UP   = 0x41373B315B,
   KEY_CTRL_ALT_DOWN = 0x42373B315B,
   KEY_CTRL_ALT_RIGHT = 0x43373B315B,
};


#define  xCLS           "\033[2J"            // Clear entire screen
#define  xCLSBELOW      "\033[J"             // Clear from cursor to bottom of screen.
#define  x1_1           "\033[1;1H"
#define  xCLS1_1        "\033[2J\033[1;1H"
#define  xCLSEND        "\033[K"             // Clear to end of line
#define  xINSERTLINE    "\033[L"             // Shifts current line down and inserts a blank line.
#define  xSAVELINE      "\033[s"             // Save current cursor position.
#define  xRESTORELINE   "\033[u"             // Restore cursor to the last saved position.

// "\033[0m"

/* Standard Colors */
#define col(c)     "\033[" #c "m"
#define rst(s)     col(0)

#define black(s)   col(30) s col(0)
#define red(s)     col(31) s col(0)
#define green(s)   col(32) s col(0)
#define yellow(s)  col(33) s col(0)
#define blue(s)    col(34) s col(0)
#define magenta(s) col(35) s col(0)
#define cyan(s)    col(36) s col(0)
#define white(s)   col(37) s col(0)
#define black2(s)   col(90) s col(0)
#define red2(s)     col(91) s col(0)
#define green2(s)   col(92) s col(0)
#define yellow2(s)  col(93) s col(0)
#define blue2(s)    col(94) s col(0)
#define magenta2(s) col(95) s col(0)
#define cyan2(s)    col(96) s col(0)
#define white2(s)   col(97) s col(0)

#define cbl(s) black(s)
#define cr(s) red(s)
#define cg(s) green(s)
#define cy(s) yellow(s)
#define cb(s) blue(s)
#define cm(s) magenta(s)
#define cc(s) cyan(s)
#define cw(s) white(s)
#define cbl2(s) black2(s)
#define cr2(s) red2(s)
#define cg2(s) green2(s)
#define cy2(s) yellow2(s)
#define cb2(s) blue2(s)
#define cm2(s) magenta2(s)
#define cc2(s) cyan2(s)
#define cw2(s) white2(s)

/* Text Effects */
#define bold(s)      col(1) s col(0)
#define dim(s)       col(2) s col(0)
#define italic(s)    col(3) s col(0)
#define underline(s) col(4) s col(0)
#define blink(s)     col(5) s col(0)
#define blink2(s)    col(6) s col(0)
#define reverse(s)   col(7) s col(0)
#define strike(s)    col(9) s col(0)

/* Background Colors (Optional) */
#define bg_black(s)     col(41) s col(0)
#define bg_red(s)       col(41) s col(0)
#define bg_green(s)     col(42) s col(0)
#define bg_blue(s)      col(44) s col(0)
#define bg_white(s)     col(47) s col(0)
#define bg_white2(s)    col(107) s col(0)
#define bg_blue2(s)     col(44+60) s col(0)

#define bbr(x)          bold(blue(reverse(x)))
#define ybr(x)          bold(yellow(reverse(x)))
#define gbr(x)          bold(green(reverse(x)))

//=== RGB ============================================================
#define _fg(r,g,b)     "\033[38;2;" #r ";" #g ";" #b "m"
#define _bg(r,g,b)     "\033[48;2;" #r ";" #g ";" #b "m"
#define _fgc(r,g,b,s)  "\033[38;2;" #r ";" #g ";" #b "m" s "\033[39m"
#define _bgc(r,g,b,s)  "\033[48;2;" #r ";" #g ";" #b "m" s "\033[49m"

// immediate color codes
#define fg(r,g,b)      _fg(r,g,b)
#define bg(r,g,b)      _bg(r,g,b)

// text wrappers
#define fgc(r,g,b,s)   _fgc(r,g,b,s)
#define bgc(r,g,b,s)   _bgc(r,g,b,s)

uint64_t getch();
int kbhit();
void setup_terminal();
void sig_handler(int signo);
void restore_terminal(void);
void Wait_Key(char* str);

void gotoxy(int x, int y);
int gettermx();
int gettermy();

void test_input();

// Decode UTF-8 to Unicode codepoint, return bytes consumed
static int utf8_decode(const char* s, unsigned int* codepoint)
{
    #define margin " "
    #define margin_len (sizeof(margin) - 1)

    unsigned char c0 = (unsigned char)s[0];
    
    if ((c0 & 0x80) == 0) {
        *codepoint = c0;
        return 1;
    }
    if ((c0 & 0xE0) == 0xC0 && (s[1] & 0xC0) == 0x80) {
        *codepoint = ((c0 & 0x1F) << 6) | (s[1] & 0x3F);
        return 2;
    }
    if ((c0 & 0xF0) == 0xE0 && (s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80) {
        *codepoint = ((c0 & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F);
        return 3;
    }
    if ((c0 & 0xF8) == 0xF0 && (s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80 && (s[3] & 0xC0) == 0x80) {
        *codepoint = ((c0 & 0x07) << 18) | ((s[1] & 0x3F) << 12) | ((s[2] & 0x3F) << 6) | (s[3] & 0x3F);
        return 4;
    }
    *codepoint = '?';
    return 1;
}

// check if codepoint is emoji (assume 2 columns width)
static int is_emoji(unsigned int cp)
{
    return (cp >= 0x1F300 && cp <= 0x1F9FF);    // emoji blocks
}

// Count display width of UTF-8 string (emoji as 2 cols, ANSI escapes as 0)
static size_t utf8_width(const char* s)
{
    size_t width = 0;
    unsigned int codepoint;
    
    for (const char* p = s; *p; ) {
        // Skip ANSI escape sequences: ESC [ ... m
        if ((unsigned char)*p == 0x1B && p[1] == '[') {
            p += 2;
            while (*p && *p != 'm') p++;
            if (*p == 'm') p++;
            continue;
        }
        
        int bytes = utf8_decode(p, &codepoint);
        if (is_emoji(codepoint)) {
            width += 2;
        } else {
            width += 1;
        }
        p += bytes;
    }
    return width;
}

// █▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀█
// █ return all the lines █
// █ in a neat box        █
// █▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄█
static char* boxit(char* line1, ...)
{
    if (!line1) return "";
    
    // 4 rotating static buffers
    static char* buffers[4] = {0};
    static size_t sizes[4] = {0};
    static int idx = 0;
    
    // Collect all lines into array
    va_list args;
    va_start(args, line1);
    
    char* lines[256];
    int count = 0;
    
    lines[count++] = line1;
    char* arg;
    while ((arg = va_arg(args, char*)) != NULL) {
        lines[count++] = arg;
    }
    va_end(args);
    
    // Find maximum line display width
    size_t max_len = utf8_width(line1);
    for (int i = 1; i < count; i++) {
        size_t len = utf8_width(lines[i]);
        if (len > max_len) max_len = len;
    }
    
    // Calculate required size (UTF-8: each char up to 4 bytes, plus overhead)
    size_t required = (max_len + 10) * (count + 2) * 4;
    
    // Grow buffer if needed
    if (sizes[idx] < required) {
        buffers[idx] = realloc(buffers[idx], required);
        sizes[idx] = required;
    }
    
    char* output = buffers[idx];
    char* ptr = output;
    
    // Top border: █▀▀▀...▀█
    ptr += sprintf(ptr, "█");
    for (size_t i = 0; i < max_len + margin_len * 2; i++) {
        ptr += sprintf(ptr, "▀");
    }
    ptr += sprintf(ptr, "█\n");
    
    // Content lines: █ margin text padded margin █
    for (int i = 0; i < count; i++) {
        ptr += sprintf(ptr, "█" margin);
        ptr += sprintf(ptr, "%s", lines[i]);
        // Pad to max display width
        size_t line_width = utf8_width(lines[i]);
        for (size_t j = line_width; j < max_len; j++) {
            *ptr++ = ' ';
        }
        ptr += sprintf(ptr, margin "█\n");
    }
    
    // Bottom border: █▄▄▄...▄█
    ptr += sprintf(ptr, "█");
    for (size_t i = 0; i < max_len + margin_len * 2; i++) {
        ptr += sprintf(ptr, "▄");
    }
    ptr += sprintf(ptr, "█\n");
    
    // Rotate to next buffer
    idx = (idx + 1) % 4;
    
    return output;
}

#define RAINBOW_NBUF  4
#define RAINBOW_BUFSZ 512

static const char *rainbow(const char *text)
{
    static const char *colors[] = {
        "33","32","34","35","36","31",
        "91","92","93","94","95","96"
    };
    static char bufs[RAINBOW_NBUF][RAINBOW_BUFSZ];
    static int  bufidx;

    char *buf = bufs[bufidx++ % RAINBOW_NBUF];
    char *p = buf, *end = buf + RAINBOW_BUFSZ - 8;
    size_t ci = 0;

    for (const unsigned char *s = (const unsigned char *)text; *s && p < end; s++) {
        if (isspace(*s)) { *p++ = (char)*s; continue; }
        p += sprintf(p, "\033[%sm%c", colors[ci % (sizeof colors / sizeof *colors)], *s);
        ci++;
    }
    p += sprintf(p, "\033[0m");
    *p = 0;
    return buf;
}
