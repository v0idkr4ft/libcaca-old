/*
 *  libcaca       Colour ASCII-Art library
 *  Copyright (c) 2002-2006 Sam Hocevar <sam@zoy.org>
 *                All Rights Reserved
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the Do What The Fuck You Want To
 *  Public License, Version 2, as published by Sam Hocevar. See
 *  http://sam.zoy.org/wtfpl/COPYING for more details.
 */

/** \file driver_ncurses.c
 *  \version \$Id$
 *  \author Sam Hocevar <sam@zoy.org>
 *  \brief Ncurses driver
 *
 *  This file contains the libcaca Ncurses input and output driver
 */

#include "config.h"

#if defined(USE_NCURSES)

#if defined(HAVE_NCURSESW_NCURSES_H)
#   include <ncursesw/ncurses.h>
#elif defined(HAVE_NCURSES_NCURSES_H)
#   include <ncurses/ncurses.h>
#elif defined(HAVE_NCURSES_H)
#   include <ncurses.h>
#else
#   include <curses.h>
#endif

#include <stdlib.h>
#include <string.h>

#if defined(HAVE_SIGNAL_H)
#   include <signal.h>
#endif
#if defined(HAVE_SYS_IOCTL_H)
#   include <sys/ioctl.h>
#endif

#include "caca.h"
#include "caca_internals.h"
#include "cucul.h"
#include "cucul_internals.h"

/*
 * Local functions
 */

#if defined(HAVE_SIGNAL)
static RETSIGTYPE sigwinch_handler(int);
static caca_t *sigwinch_kk; /* FIXME: we ought to get rid of this */
#endif
#if defined(HAVE_GETENV) && defined(HAVE_PUTENV)
static void ncurses_check_terminal(void);
#endif
static void ncurses_write_utf32(uint32_t);

struct driver_private
{
    int attr[16*16];
    mmask_t oldmask;
};

static int ncurses_init_graphics(caca_t *kk)
{
    static int curses_colors[] =
    {
        /* Standard curses colours */
        COLOR_BLACK,
        COLOR_BLUE,
        COLOR_GREEN,
        COLOR_CYAN,
        COLOR_RED,
        COLOR_MAGENTA,
        COLOR_YELLOW,
        COLOR_WHITE,
        /* Extra values for xterm-16color */
        COLOR_BLACK + 8,
        COLOR_BLUE + 8,
        COLOR_GREEN + 8,
        COLOR_CYAN + 8,
        COLOR_RED + 8,
        COLOR_MAGENTA + 8,
        COLOR_YELLOW + 8,
        COLOR_WHITE + 8
    };

    mmask_t newmask;
    int fg, bg, max;

    kk->drv.p = malloc(sizeof(struct driver_private));

#if defined(HAVE_GETENV) && defined(HAVE_PUTENV)
    ncurses_check_terminal();
#endif

#if defined(HAVE_SIGNAL)
    sigwinch_kk = kk;
    signal(SIGWINCH, sigwinch_handler);
#endif

    initscr();
    keypad(stdscr, TRUE);
    nonl();
    raw();
    noecho();
    nodelay(stdscr, TRUE);
    curs_set(0);

    /* Activate mouse */
    newmask = REPORT_MOUSE_POSITION | ALL_MOUSE_EVENTS;
    mousemask(newmask, &kk->drv.p->oldmask);
    mouseinterval(-1); /* No click emulation */

    /* Set the escape delay to a ridiculously low value */
    ESCDELAY = 10;

    /* Activate colour */
    start_color();

    /* If COLORS == 16, it means the terminal supports full bright colours
     * using setab and setaf (will use \e[90m \e[91m etc. for colours >= 8),
     * we can build 16*16 colour pairs.
     * If COLORS == 8, it means the terminal does not know about bright
     * colours and we need to get them through A_BOLD and A_BLINK (\e[1m
     * and \e[5m). We can only build 8*8 colour pairs. */
    max = COLORS >= 16 ? 16 : 8;

    for(bg = 0; bg < max; bg++)
        for(fg = 0; fg < max; fg++)
        {
            /* Use ((max + 7 - fg) % max) instead of fg so that colour 0
             * is light gray on black. Some terminals don't like this
             * colour pair to be redefined. */
            int col = ((max + 7 - fg) % max) + max * bg;
            init_pair(col, curses_colors[fg], curses_colors[bg]);
            kk->drv.p->attr[fg + 16 * bg] = COLOR_PAIR(col);

            if(max == 8)
            {
                /* Bright fg on simple bg */
                kk->drv.p->attr[fg + 8 + 16 * bg] = A_BOLD | COLOR_PAIR(col);
                /* Simple fg on bright bg */
                kk->drv.p->attr[fg + 16 * (bg + 8)] = A_BLINK
                                                    | COLOR_PAIR(col);
                /* Bright fg on bright bg */
                kk->drv.p->attr[fg + 8 + 16 * (bg + 8)] = A_BLINK | A_BOLD
                                                        | COLOR_PAIR(col);
            }
        }

    _cucul_set_size(kk->qq, COLS, LINES);

    return 0;
}

static int ncurses_end_graphics(caca_t *kk)
{
    mousemask(kk->drv.p->oldmask, NULL);
    curs_set(1);
    noraw();
    endwin();

    free(kk->drv.p);

    return 0;
}

static int ncurses_set_window_title(caca_t *kk, char const *title)
{
    return 0;
}

static unsigned int ncurses_get_window_width(caca_t *kk)
{
    /* Fallback to a 6x10 font */
    return kk->qq->width * 6;
}

static unsigned int ncurses_get_window_height(caca_t *kk)
{
    /* Fallback to a 6x10 font */
    return kk->qq->height * 10;
}

static void ncurses_display(caca_t *kk)
{
    int x, y;
    uint8_t *attr = kk->qq->attr;
    uint32_t *chars = kk->qq->chars;
    for(y = 0; y < (int)kk->qq->height; y++)
    {
        move(y, 0);
        for(x = kk->qq->width; x--; )
        {
            attrset(kk->drv.p->attr[*attr++]);
            ncurses_write_utf32(*chars++);
        }
    }
    refresh();
}

static void ncurses_handle_resize(caca_t *kk)
{
    struct winsize size;

    if(ioctl(fileno(stdout), TIOCGWINSZ, &size) == 0)
    {
        kk->resize.w = size.ws_col;
        kk->resize.h = size.ws_row;
#if defined(HAVE_RESIZE_TERM)
        resize_term(kk->resize.h, kk->resize.w);
#else
        resizeterm(*kk->resize.h, *kk->resize.w);
#endif
        wrefresh(curscr);
        return;
    }

    /* Fallback */
    kk->resize.w = kk->qq->width;
    kk->resize.h = kk->qq->height;
}

static int ncurses_get_event(caca_t *kk, struct caca_event *ev)
{
    int intkey;

    intkey = getch();
    if(intkey == ERR)
    {
        ev->type = CACA_EVENT_NONE;
        return 0;
    }

    if(intkey < 0x100)
    {
        ev->type = CACA_EVENT_KEY_PRESS;
        ev->data.key.c = intkey;
        return 1;
    }

    if(intkey == KEY_MOUSE)
    {
        MEVENT mevent;
        getmouse(&mevent);

        switch(mevent.bstate)
        {
            case BUTTON1_PRESSED:
                ev->data.mouse.button = 1;
                ev->type = CACA_EVENT_MOUSE_PRESS; _push_event(kk, ev);
                break;
            case BUTTON1_RELEASED:
                ev->data.mouse.button = 1;
                ev->type = CACA_EVENT_MOUSE_RELEASE; _push_event(kk, ev);
                break;
            case BUTTON1_CLICKED:
                ev->data.mouse.button = 1;
                ev->type = CACA_EVENT_MOUSE_PRESS; _push_event(kk, ev);
                ev->type = CACA_EVENT_MOUSE_RELEASE; _push_event(kk, ev);
                break;
            case BUTTON1_DOUBLE_CLICKED:
                ev->data.mouse.button = 1;
                ev->type = CACA_EVENT_MOUSE_PRESS; _push_event(kk, ev);
                ev->type = CACA_EVENT_MOUSE_RELEASE; _push_event(kk, ev);
                ev->type = CACA_EVENT_MOUSE_PRESS; _push_event(kk, ev);
                ev->type = CACA_EVENT_MOUSE_RELEASE; _push_event(kk, ev);
                break;
            case BUTTON1_TRIPLE_CLICKED:
                ev->data.mouse.button = 1;
                ev->type = CACA_EVENT_MOUSE_PRESS; _push_event(kk, ev);
                ev->type = CACA_EVENT_MOUSE_RELEASE; _push_event(kk, ev);
                ev->type = CACA_EVENT_MOUSE_PRESS; _push_event(kk, ev);
                ev->type = CACA_EVENT_MOUSE_RELEASE; _push_event(kk, ev);
                ev->type = CACA_EVENT_MOUSE_PRESS; _push_event(kk, ev);
                ev->type = CACA_EVENT_MOUSE_RELEASE; _push_event(kk, ev);
                break;
            case BUTTON1_RESERVED_EVENT:
                break;

            case BUTTON2_PRESSED:
                ev->data.mouse.button = 2;
                ev->type = CACA_EVENT_MOUSE_PRESS; _push_event(kk, ev);
                break;
            case BUTTON2_RELEASED:
                ev->data.mouse.button = 2;
                ev->type = CACA_EVENT_MOUSE_RELEASE; _push_event(kk, ev);
                break;
            case BUTTON2_CLICKED:
                ev->data.mouse.button = 2;
                ev->type = CACA_EVENT_MOUSE_PRESS; _push_event(kk, ev);
                ev->type = CACA_EVENT_MOUSE_RELEASE; _push_event(kk, ev);
                break;
            case BUTTON2_DOUBLE_CLICKED:
                ev->data.mouse.button = 2;
                ev->type = CACA_EVENT_MOUSE_PRESS; _push_event(kk, ev);
                ev->type = CACA_EVENT_MOUSE_RELEASE; _push_event(kk, ev);
                ev->type = CACA_EVENT_MOUSE_PRESS; _push_event(kk, ev);
                ev->type = CACA_EVENT_MOUSE_RELEASE; _push_event(kk, ev);
                break;
            case BUTTON2_TRIPLE_CLICKED:
                ev->data.mouse.button = 2;
                ev->type = CACA_EVENT_MOUSE_PRESS; _push_event(kk, ev);
                ev->type = CACA_EVENT_MOUSE_RELEASE; _push_event(kk, ev);
                ev->type = CACA_EVENT_MOUSE_PRESS; _push_event(kk, ev);
                ev->type = CACA_EVENT_MOUSE_RELEASE; _push_event(kk, ev);
                ev->type = CACA_EVENT_MOUSE_PRESS; _push_event(kk, ev);
                ev->type = CACA_EVENT_MOUSE_RELEASE; _push_event(kk, ev);
                break;
            case BUTTON2_RESERVED_EVENT:
                break;

            case BUTTON3_PRESSED:
                ev->data.mouse.button = 3;
                ev->type = CACA_EVENT_MOUSE_PRESS; _push_event(kk, ev);
                break;
            case BUTTON3_RELEASED:
                ev->data.mouse.button = 3;
                ev->type = CACA_EVENT_MOUSE_RELEASE; _push_event(kk, ev);
                break;
            case BUTTON3_CLICKED:
                ev->data.mouse.button = 3;
                ev->type = CACA_EVENT_MOUSE_PRESS; _push_event(kk, ev);
                ev->type = CACA_EVENT_MOUSE_RELEASE; _push_event(kk, ev);
                break;
            case BUTTON3_DOUBLE_CLICKED:
                ev->data.mouse.button = 3;
                ev->type = CACA_EVENT_MOUSE_PRESS; _push_event(kk, ev);
                ev->type = CACA_EVENT_MOUSE_RELEASE; _push_event(kk, ev);
                ev->type = CACA_EVENT_MOUSE_PRESS; _push_event(kk, ev);
                ev->type = CACA_EVENT_MOUSE_RELEASE; _push_event(kk, ev);
                break;
            case BUTTON3_TRIPLE_CLICKED:
                ev->data.mouse.button = 3;
                ev->type = CACA_EVENT_MOUSE_PRESS; _push_event(kk, ev);
                ev->type = CACA_EVENT_MOUSE_RELEASE; _push_event(kk, ev);
                ev->type = CACA_EVENT_MOUSE_PRESS; _push_event(kk, ev);
                ev->type = CACA_EVENT_MOUSE_RELEASE; _push_event(kk, ev);
                ev->type = CACA_EVENT_MOUSE_PRESS; _push_event(kk, ev);
                ev->type = CACA_EVENT_MOUSE_RELEASE; _push_event(kk, ev);
                break;
            case BUTTON3_RESERVED_EVENT:
                break;

            case BUTTON4_PRESSED:
                ev->data.mouse.button = 4;
                ev->type = CACA_EVENT_MOUSE_PRESS; _push_event(kk, ev);
                break;
            case BUTTON4_RELEASED:
                ev->data.mouse.button = 4;
                ev->type = CACA_EVENT_MOUSE_RELEASE; _push_event(kk, ev);
                break;
            case BUTTON4_CLICKED:
                ev->data.mouse.button = 4;
                ev->type = CACA_EVENT_MOUSE_PRESS; _push_event(kk, ev);
                ev->type = CACA_EVENT_MOUSE_RELEASE; _push_event(kk, ev);
                break;
            case BUTTON4_DOUBLE_CLICKED:
                ev->data.mouse.button = 4;
                ev->type = CACA_EVENT_MOUSE_PRESS; _push_event(kk, ev);
                ev->type = CACA_EVENT_MOUSE_RELEASE; _push_event(kk, ev);
                ev->type = CACA_EVENT_MOUSE_PRESS; _push_event(kk, ev);
                ev->type = CACA_EVENT_MOUSE_RELEASE; _push_event(kk, ev);
                break;
            case BUTTON4_TRIPLE_CLICKED:
                ev->data.mouse.button = 4;
                ev->type = CACA_EVENT_MOUSE_PRESS; _push_event(kk, ev);
                ev->type = CACA_EVENT_MOUSE_RELEASE; _push_event(kk, ev);
                ev->type = CACA_EVENT_MOUSE_PRESS; _push_event(kk, ev);
                ev->type = CACA_EVENT_MOUSE_RELEASE; _push_event(kk, ev);
                ev->type = CACA_EVENT_MOUSE_PRESS; _push_event(kk, ev);
                ev->type = CACA_EVENT_MOUSE_RELEASE; _push_event(kk, ev);
                break;
            case BUTTON4_RESERVED_EVENT:
                break;

            default:
                break;
        }

        if(kk->mouse.x == (unsigned int)mevent.x &&
           kk->mouse.y == (unsigned int)mevent.y)
            return _pop_event(kk, ev);

        kk->mouse.x = mevent.x;
        kk->mouse.y = mevent.y;

        ev->type = CACA_EVENT_MOUSE_MOTION;
        ev->data.mouse.x = kk->mouse.x;
        ev->data.mouse.y = kk->mouse.y;
        return 1;
    }

    switch(intkey)
    {
        case KEY_UP: ev->data.key.c = CACA_KEY_UP; break;
        case KEY_DOWN: ev->data.key.c = CACA_KEY_DOWN; break;
        case KEY_LEFT: ev->data.key.c = CACA_KEY_LEFT; break;
        case KEY_RIGHT: ev->data.key.c = CACA_KEY_RIGHT; break;

        case KEY_IC: ev->data.key.c = CACA_KEY_INSERT; break;
        case KEY_DC: ev->data.key.c = CACA_KEY_DELETE; break;
        case KEY_HOME: ev->data.key.c = CACA_KEY_HOME; break;
        case KEY_END: ev->data.key.c = CACA_KEY_END; break;
        case KEY_PPAGE: ev->data.key.c = CACA_KEY_PAGEUP; break;
        case KEY_NPAGE: ev->data.key.c = CACA_KEY_PAGEDOWN; break;

        case KEY_F(1): ev->data.key.c = CACA_KEY_F1; break;
        case KEY_F(2): ev->data.key.c = CACA_KEY_F2; break;
        case KEY_F(3): ev->data.key.c = CACA_KEY_F3; break;
        case KEY_F(4): ev->data.key.c = CACA_KEY_F4; break;
        case KEY_F(5): ev->data.key.c = CACA_KEY_F5; break;
        case KEY_F(6): ev->data.key.c = CACA_KEY_F6; break;
        case KEY_F(7): ev->data.key.c = CACA_KEY_F7; break;
        case KEY_F(8): ev->data.key.c = CACA_KEY_F8; break;
        case KEY_F(9): ev->data.key.c = CACA_KEY_F9; break;
        case KEY_F(10): ev->data.key.c = CACA_KEY_F10; break;
        case KEY_F(11): ev->data.key.c = CACA_KEY_F11; break;
        case KEY_F(12): ev->data.key.c = CACA_KEY_F12; break;

        default: ev->type = CACA_EVENT_NONE; return 0;
    }

    ev->type = CACA_EVENT_KEY_PRESS;
    ev->data.key.ucs4 = 0;
    ev->data.key.utf8[0] = '\0';
    return 1;
}

/*
 * XXX: following functions are local
 */

#if defined(HAVE_SIGNAL)
static RETSIGTYPE sigwinch_handler(int sig)
{
    sigwinch_kk->resize.resized = 1;

    signal(SIGWINCH, sigwinch_handler);
}
#endif

#if defined(HAVE_GETENV) && defined(HAVE_PUTENV)
static void ncurses_check_terminal(void)
{
    char *term, *colorterm, *other;

    term = getenv("TERM");
    colorterm = getenv("COLORTERM");

    if(term && !strcmp(term, "xterm"))
    {
        /* If we are using gnome-terminal, it's really a 16 colour terminal */
        if(colorterm && !strcmp(colorterm, "gnome-terminal"))
        {
            SCREEN *screen;
            screen = newterm("xterm-16color", stdout, stdin);
            if(screen == NULL)
                return;
            endwin();
            (void)putenv("TERM=xterm-16color");
            return;
        }

        /* Ditto if we are using Konsole */
        other = getenv("KONSOLE_DCOP_SESSION");
        if(other)
        {
            SCREEN *screen;
            screen = newterm("xterm-16color", stdout, stdin);
            if(screen == NULL)
                return;
            endwin();
            (void)putenv("TERM=xterm-16color");
            return;
        }
    }
}
#endif

static void ncurses_write_utf32(uint32_t c)
{
#if defined(HAVE_NCURSESW_NCURSES_H)
    static const uint8_t mark[7] =
    {
        0x00, 0x00, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC
    };

    char buf[10], *parser;
    int bytes;
#endif

    if(c < 0x80)
    {
        addch(c);
        return;
    }

#if defined(HAVE_NCURSESW_NCURSES_H)
    if(c < 0x10000)
    {
        addch(c); /* FIXME: doesn't work either */
        return;
    }

    bytes = (c < 0x800) ? 2 : (c < 0x10000) ? 3 : 4;
    buf[bytes] = '\0';
    parser = buf + bytes;

    switch(bytes)
    {
        case 4: *--parser = (c | 0x80) & 0xbf; c >>= 6;
        case 3: *--parser = (c | 0x80) & 0xbf; c >>= 6;
        case 2: *--parser = (c | 0x80) & 0xbf; c >>= 6;
    }
    *--parser = c | mark[bytes];

    addstr(buf);
#else
    addch(' ');
#endif
}

/*
 * Driver initialisation
 */

int ncurses_install(caca_t *kk)
{
    kk->drv.driver = CACA_DRIVER_NCURSES;

    kk->drv.init_graphics = ncurses_init_graphics;
    kk->drv.end_graphics = ncurses_end_graphics;
    kk->drv.set_window_title = ncurses_set_window_title;
    kk->drv.get_window_width = ncurses_get_window_width;
    kk->drv.get_window_height = ncurses_get_window_height;
    kk->drv.display = ncurses_display;
    kk->drv.handle_resize = ncurses_handle_resize;
    kk->drv.get_event = ncurses_get_event;

    return 0;
}

#endif /* USE_NCURSES */

