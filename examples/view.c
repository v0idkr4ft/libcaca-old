/*
 *   view          image viewer for libcaca
 *   Copyright (c) 2003 Sam Hocevar <sam@zoy.org>
 *                 All Rights Reserved
 *
 *   $Id$
 *
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU Lesser General Public
 *   License as published by the Free Software Foundation; either
 *   version 2 of the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *   Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public
 *   License along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 *   02111-1307  USA
 */

#include "config.h"

#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <unistd.h>

#include <Imlib2.h>

#include "caca.h"

/* Local functions */
static void load_image(const char *);
static void draw_checkers(unsigned int, unsigned int,
                          unsigned int, unsigned int);

/* Local variables */
Imlib_Image image = NULL;
char *pixels = NULL;
struct caca_bitmap *bitmap = NULL;
int x, y, w, h;

int main(int argc, char **argv)
{
    int quit = 0, update = 1, help = 0, reload = 0, fullscreen = 0, zoom = 0;

    char **list = NULL;
    int current = 0, items = 0, opts = 1;
    int i;

    /* Initialise libcaca */
    if(caca_init())
    {
        fprintf(stderr, "%s: unable to initialise libcaca\n", argv[0]);
        return 1;
    }

    /* Load items into playlist */
    for(i = 1; i < argc; i++)
    {
        /* Skip options except after `--' */
        if(opts && argv[i][0] == '-')
        {
            if(argv[i][1] == '-' && argv[i][2] == '\0')
                opts = 0;
            continue;
        }

        /* Add argv[i] to the list */
        if(items)
            list = realloc(list, (items + 1) * sizeof(char *));
        else
            list = malloc(sizeof(char *));
        list[items] = argv[i];
        items++;

        reload = 1;
    }

    /* Go ! */
    while(!quit)
    {
        int ww = caca_get_width();
        int wh = caca_get_height();

        int event;

        while((event = caca_get_event()))
        {
            switch(event)
            {
            case CACA_EVENT_KEY_PRESS | 'n':
            case CACA_EVENT_KEY_PRESS | 'N':
                if(items) current = (current + 1) % items;
                reload = 1;
                break;
            case CACA_EVENT_KEY_PRESS | 'p':
            case CACA_EVENT_KEY_PRESS | 'P':
                if(items) current = (items + current - 1) % items;
                reload = 1;
                break;
            case CACA_EVENT_KEY_PRESS | 'f':
            case CACA_EVENT_KEY_PRESS | 'F':
                fullscreen = ~fullscreen;
                update = 1;
                break;
            case CACA_EVENT_KEY_PRESS | 'b':
                i = 1 + caca_get_feature(CACA_BACKGROUND);
                if(i > CACA_BACKGROUND_MAX) i = CACA_BACKGROUND_MIN;
                caca_set_feature(i);
                update = 1;
                break;
            case CACA_EVENT_KEY_PRESS | 'B':
                i = -1 + caca_get_feature(CACA_BACKGROUND);
                if(i < CACA_BACKGROUND_MIN) i = CACA_BACKGROUND_MAX;
                caca_set_feature(i);
                update = 1;
                break;
            case CACA_EVENT_KEY_PRESS | 'a':
                i = 1 + caca_get_feature(CACA_ANTIALIASING);
                if(i > CACA_ANTIALIASING_MAX) i = CACA_ANTIALIASING_MIN;
                caca_set_feature(i);
                update = 1;
                break;
            case CACA_EVENT_KEY_PRESS | 'A':
                i = -1 + caca_get_feature(CACA_ANTIALIASING);
                if(i < CACA_ANTIALIASING_MIN) i = CACA_ANTIALIASING_MAX;
                caca_set_feature(i);
                update = 1;
                break;
            case CACA_EVENT_KEY_PRESS | 'd':
                i = 1 + caca_get_feature(CACA_DITHERING);
                if(i > CACA_DITHERING_MAX) i = CACA_DITHERING_MIN;
                caca_set_feature(i);
                update = 1;
                break;
            case CACA_EVENT_KEY_PRESS | 'D':
                i = -1 + caca_get_feature(CACA_DITHERING);
                if(i < CACA_DITHERING_MIN) i = CACA_DITHERING_MAX;
                caca_set_feature(i);
                update = 1;
                break;
            case CACA_EVENT_KEY_PRESS | '+':
                zoom++;
                if(zoom > 48) zoom = 48; else update = 1;
                break;
            case CACA_EVENT_KEY_PRESS | '-':
                zoom--;
                if(zoom < -48) zoom = -48; else update = 1;
                break;
            case CACA_EVENT_KEY_PRESS | 'x':
            case CACA_EVENT_KEY_PRESS | 'X':
                zoom = 0;
                update = 1;
                break;
            case CACA_EVENT_KEY_PRESS | 'k':
            case CACA_EVENT_KEY_PRESS | 'K':
            case CACA_EVENT_KEY_PRESS | CACA_KEY_UP:
                if(zoom > 0) y -= 1 + h / (2 + zoom) / 8;
                update = 1;
                break;
            case CACA_EVENT_KEY_PRESS | 'j':
            case CACA_EVENT_KEY_PRESS | 'J':
            case CACA_EVENT_KEY_PRESS | CACA_KEY_DOWN:
                if(zoom > 0) y += 1 + h / (2 + zoom) / 8;
                update = 1;
                break;
            case CACA_EVENT_KEY_PRESS | 'h':
            case CACA_EVENT_KEY_PRESS | 'H':
            case CACA_EVENT_KEY_PRESS | CACA_KEY_LEFT:
                if(zoom > 0) x -= 1 + w / (2 + zoom) / 8;
                update = 1;
                break;
            case CACA_EVENT_KEY_PRESS | 'l':
            case CACA_EVENT_KEY_PRESS | 'L':
            case CACA_EVENT_KEY_PRESS | CACA_KEY_RIGHT:
                if(zoom > 0) x += 1 + w / (2 + zoom) / 8;
                update = 1;
                break;
            case CACA_EVENT_KEY_PRESS | '?':
                help = 1;
                update = 1;
                break;
            case CACA_EVENT_KEY_PRESS | 'q':
            case CACA_EVENT_KEY_PRESS | 'Q':
                quit = 1;
                break;
            }
        }

        if(items && reload)
        {
            char *buffer = malloc(ww + 1);

            /* Reset image-specific runtime variables */
            zoom = 0;

            snprintf(buffer, ww, " Loading `%s'... ", list[current]);
            buffer[ww] = '\0';
            caca_set_color(CACA_COLOR_WHITE, CACA_COLOR_BLUE);
            caca_putstr((ww - strlen(buffer)) / 2, wh / 2, buffer);
            caca_refresh();

            load_image(list[current]);
            reload = 0;
            update = 1;

            free(buffer);
        }

        if(!update)
        {
            usleep(10000);
            continue;
        }

        caca_clear();
        caca_set_color(CACA_COLOR_WHITE, CACA_COLOR_BLUE);

        if(!items)
            caca_printf(ww / 2 - 5, wh / 2, " No image. ");
        else if(!image)
        {
            char *buffer = malloc(ww + 1);
            snprintf(buffer, ww, " Error loading `%s'. ", list[current]);
            buffer[ww] = '\0';
            caca_putstr((ww - strlen(buffer)) / 2, wh / 2, buffer);
            free(buffer);
        }
        else if(zoom < 0)
        {
            int xo = (ww - 1) / 2;
            int yo = (wh - 1) / 2;
            int xn = (ww - 1) / (2 - zoom);
            int yn = (wh - 1) / (2 - zoom);
            draw_checkers(xo - xn, yo - yn, xo + xn, yo + yn);
            caca_draw_bitmap(xo - xn, yo - yn, xo + xn, yo + yn,
                             bitmap, pixels);
        }
        else if(zoom > 0)
        {
            struct caca_bitmap *newbitmap;
            int xn = w / (2 + zoom);
            int yn = h / (2 + zoom);
            if(x < xn) x = xn;
            if(y < yn) y = yn;
            if(xn + x > w) x = w - xn;
            if(yn + y > h) y = h - yn;
            newbitmap = caca_create_bitmap(32, 2 * xn, 2 * yn, 4 * w,
                                           0x00ff0000, 0x0000ff00, 0x000000ff,
                                           0xff000000);
            draw_checkers(0, fullscreen ? 0 : 1,
                          ww - 1, fullscreen ? wh - 1 : wh - 2);
            caca_draw_bitmap(0, fullscreen ? 0 : 1,
                             ww - 1, fullscreen ? wh - 1 : wh - 2,
                             newbitmap,
                             pixels + 4 * (x - xn) + 4 * w * (y - yn));
            caca_free_bitmap(newbitmap);
        }
        else
        {
            draw_checkers(0, fullscreen ? 0 : 1,
                          ww - 1, fullscreen ? wh - 1 : wh - 2);
            caca_draw_bitmap(0, fullscreen ? 0 : 1,
                             ww - 1, fullscreen ? wh - 1 : wh - 2,
                             bitmap, pixels);
        }

        caca_set_color(CACA_COLOR_WHITE, CACA_COLOR_BLUE);

        if(!fullscreen)
        {
            caca_draw_line(0, 0, ww - 1, 0, ' ');
            caca_draw_line(0, wh - 1, ww - 1, wh - 1, '-');
            caca_putstr(0, 0, "q:Quit  np:Next/Prev  +-x:Zoom  "
                              "hjkl:Move  d:Dithering  a:Antialias");
            caca_putstr(ww - strlen("?:Help"), 0, "?:Help");
            caca_printf(3, wh - 1, "cacaview %s (%s, %s)", VERSION,
                caca_get_feature_name(caca_get_feature(CACA_DITHERING)),
                caca_get_feature_name(caca_get_feature(CACA_ANTIALIASING)));
            caca_printf(ww - 14, wh - 1,
                        "(zoom: %s%i)", zoom > 0 ? "+" : "", zoom);
        }

        if(help)
        {
            caca_putstr(ww - 22, 2,  " +: zoom in             ");
            caca_putstr(ww - 22, 3,  " -: zoom out            ");
            caca_putstr(ww - 22, 4,  " x: reset zoom          ");
            caca_putstr(ww - 22, 5,  " ---------------------- ");
            caca_putstr(ww - 22, 6,  " hjkl: move view        ");
            caca_putstr(ww - 22, 7,  " arrows: move view      ");
            caca_putstr(ww - 22, 8,  " ---------------------- ");
            caca_putstr(ww - 22, 9,  " a: antialiasing method ");
            caca_putstr(ww - 22, 9,  " d: dithering method    ");
            caca_putstr(ww - 22, 10, " ---------------------- ");
            caca_putstr(ww - 22, 11, " ?: help                ");
            caca_putstr(ww - 22, 12, " q: quit                ");

            help = 0;
        }

        caca_refresh();
        update = 0;
    }

    if(bitmap)
        caca_free_bitmap(bitmap);
    if(image)
        imlib_free_image();

    /* Clean up */
    caca_end();

    return 0;
}

static void load_image(const char *name)
{
    /* Empty previous data */
    if(image)
        imlib_free_image();

    if(bitmap)
        caca_free_bitmap(bitmap);

    image = NULL;
    bitmap = NULL;

    /* Load the new image */
    image = imlib_load_image(name);

    if(!image)
    {
        return;
    }

    imlib_context_set_image(image);
    pixels = (char *)imlib_image_get_data_for_reading_only();
    w = imlib_image_get_width();
    h = imlib_image_get_height();
    x = w / 2;
    y = h / 2;

    /* Create the libcaca bitmap */
    bitmap = caca_create_bitmap(32, w, h, 4 * w, 0x00ff0000, 0x0000ff00,
                                0x000000ff, 0xff000000);
    if(!bitmap)
    {
        imlib_free_image();
        image = NULL;
    }
}

static void draw_checkers(unsigned int x1, unsigned int y1,
                          unsigned int x2, unsigned int y2)
{
    unsigned int xn, yn;

    for(yn = y1; yn <= y2; yn++)
        for(xn = x1; xn <= x2; xn++)
    {
        if(((xn / 4) ^ (yn / 2)) & 1)
            caca_set_color(CACA_COLOR_LIGHTGRAY, CACA_COLOR_DARKGRAY);
        else
            caca_set_color(CACA_COLOR_DARKGRAY, CACA_COLOR_LIGHTGRAY);
        caca_putchar(xn, yn, ' ');
    }
}

