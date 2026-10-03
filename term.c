#include "picohaxx.h"

static struct termios orig_termios;


void cls()
{
    fprintf(stderr,"%s",xCLS1_1);
}

int g_terminal_setup=0;

void setup_terminal()
{
    if (!isatty(STDIN_FILENO)) return;

    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(restore_terminal);
    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);

    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ICANON | ECHO);    // Disable line buffer and  no echo
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
    g_terminal_setup=1;
}

void restore_terminal(void) {
    if(!g_terminal_setup) return;
    // cls();
    tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);
    g_terminal_setup=0;
}

void sig_handler(int signo) {
    restore_terminal();
    _exit(128 + signo);
}

void gettermsize(int *x, int *y)
{
    struct winsize w;
    ioctl(STDERR_FILENO, TIOCGWINSZ, &w);
    *x = (w.ws_col > 0) ? w.ws_col : 150;
    *y = (w.ws_row > 0) ? w.ws_row : 28;
}

int gettermx()
{
    int x,y;
    gettermsize(&x, &y);
    return x;
}

int gettermy()
{
    int x,y;
    gettermsize(&x, &y);
    return y;
}

void gotoxy(int x, int y)
{
    fprintf(stderr,"\033[%d;%dH",y,x);
}

void getcursor(int *x, int *y) {
    struct termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    write(STDERR_FILENO, "\033[6n", 4);

    char buf[32];
    int i = 0;
    char c;
    while (i < sizeof(buf) - 1) {
        read(STDIN_FILENO, &c, 1);
        buf[i++] = c;
        if (c == 'R') break;
    }
    buf[i] = '\0';

    // buf is now "\033[row;colR"
    sscanf(buf, "\033[%d;%dR", y, x);
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
}

int kbhit() {
    if(!g_terminal_setup) setup_terminal();

    struct timeval tv = {0, 0};
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    return select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) > 0;
}

// "simplified" getch
uint64_t getch() {
    unsigned char ch;
    if(!g_terminal_setup) setup_terminal();

    if (read(STDIN_FILENO, &ch, 1) != 1) return -1;
    if (ch != 27) return ch;

    // Wait briefly to distinguish standalone ESC from an ESC sequence
    struct timeval tv = {0, 50000}; // 50ms
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    if (select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) <= 0) return KEY_ESC;

    uint64_t packed = 0;
    for (int shift = 0; shift < 64; shift += 8) {
        if (read(STDIN_FILENO, &ch, 1) != 1) break;
        packed |= ((uint64_t)ch << shift);

        // If it's a terminator byte (letters or '~'), sequence is complete
        if (ch >= 0x40 && ch <= 0x7E && shift > 0) {
            // log2("terminator: 0x%02X (%c) at shift %d", ch, ch, shift);
            break;
        }

        // If the first byte after ESC isn't  '['' (5b) or 'O' (4F: F1-F4), it's likely an Alt+key combo
        if (shift == 0 && ch != 0x5B && ch != 0x4F) return (ch << 8) | 27;

        // Wait for the next byte with a tiny timeout
        tv.tv_sec = 0; tv.tv_usec = 10000; // 10ms
        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO, &fds);
        if (select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) <= 0) break;
    }
    return packed;
}

void test_input()
{
    if(!g_terminal_setup) setup_terminal();

    int x,y;
    gettermsize(&x,&y); 
    log2("Terminal X:%d Y:%d",x,y);

    #define PCH(c) (c) ? c:'0'
    
    log2("press keys\n");
    while(1) {
        if (kbhit()) {
            uint64_t k = getch();
            if (k == 'q') break;
            log2("Key pressed: %02X %02X %02X %02X %02X - 0x%lX (%04d) %c,%c,%c,%c,%c", k&0xFF ,k>>8 & 0xFF,k>>16 & 0xFF,k>>24 & 0xFF,k>>32 & 0xFF,k ,k ,PCH(k) ,PCH(k>>8),PCH(k>>16),PCH(k>>24),PCH(k>>32 & 0xFF));
        } else {
            usleep(10000);
        }
    }
}

void Wait_Key(char* str)
{
    if(str) {
        log2("Press any key  to: %s", str);
    }
    else {
        log2("Press any key");
    }

    getch();
}
