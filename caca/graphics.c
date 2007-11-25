/*
 *  libcaca       Colour ASCII-Art library
 *  Copyright (c) 2002-2006 Sam Hocevar <sam@zoy.org>
 *                All Rights Reserved
 *
 *  $Id$
 *
 *  This library is free software. It comes without any warranty, to
 *  the extent permitted by applicable law. You can redistribute it
 *  and/or modify it under the terms of the Do What The Fuck You Want
 *  To Public License, Version 2, as published by Sam Hocevar. See
 *  http://sam.zoy.org/wtfpl/COPYING for more details.
 */

/*
 *  This file contains character and string drawing functions.
 */

#include "config.h"
#include "common.h"

#if !defined(__KERNEL__)
#   include <stdio.h>
#   include <stdlib.h>
#   include <string.h>
#endif

#include "caca.h"
#include "caca_internals.h"
#include "cucul.h"
#include "cucul_internals.h"

/** \brief Set the display title.
 *
 *  If libcaca runs in a window, try to change its title. This works with
 *  the ncurses, S-Lang, OpenGL, X11 and Win32 drivers.
 *
 *  If an error occurs, -1 is returned and \b errno is set accordingly:
 *  - \c ENOSYS Display driver does not support setting the window title.
 *
 *  \param dp The libcaca display context.
 *  \param title The desired display title.
 *  \return 0 upon success, -1 if an error occurred.
 */
int caca_set_display_title(caca_display_t *dp, char const *title)
{
    int ret = dp->drv.set_display_title(dp, title);

    if(ret)
        seterrno(ENOSYS);

    return ret;
}

/** \brief Get the display width.
 *
 *  If libcaca runs in a window, get the usable window width. This value can
 *  be used for aspect ratio calculation. If libcaca does not run in a window
 *  or if there is no way to know the font size, most drivers will assume a
 *  6x10 font is being used. Note that the units are not necessarily pixels.
 *
 *  This function never fails.
 *
 *  \param dp The libcaca display context.
 *  \return The display width.
 */
unsigned int caca_get_display_width(caca_display_t const *dp)
{
    return dp->drv.get_display_width(dp);
}

/** \brief Get the display height.
 *
 *  If libcaca runs in a window, get the usable window height. This value can
 *  be used for aspect ratio calculation. If libcaca does not run in a window
 *  or if there is no way to know the font size, assume a 6x10 font is being
 *  used. Note that the units are not necessarily pixels.
 *
 *  This function never fails.
 *
 *  \param dp The libcaca display context.
 *  \return The display height.
 */
unsigned int caca_get_display_height(caca_display_t const *dp)
{
    return dp->drv.get_display_height(dp);
}

/** \brief Set the refresh delay.
 *
 *  Set the refresh delay in microseconds. The refresh delay is used by
 *  caca_refresh_display() to achieve constant framerate. See the
 *  caca_refresh_display() documentation for more details.
 *
 *  If the argument is zero, constant framerate is disabled. This is the
 *  default behaviour.
 *
 *  This function never fails.
 *
 *  \param dp The libcaca display context.
 *  \param usec The refresh delay in microseconds.
 *  \return This function always returns 0.
 */
int caca_set_display_time(caca_display_t *dp, unsigned int usec)
{
    dp->delay = usec;
    return 0;
}

/** \brief Get the display's average rendering time.
 *
 *  Get the average rendering time, which is the average measured time
 *  between two caca_refresh_display() calls, in microseconds. If constant
 *  framerate was activated by calling caca_set_display_time(), the average
 *  rendering time will be close to the requested delay even if the real
 *  rendering time was shorter.
 *
 *  This function never fails.
 *
 *  \param dp The libcaca display context.
 *  \return The render time in microseconds.
 */
unsigned int caca_get_display_time(caca_display_t const *dp)
{
    return dp->rendertime;
}

/** \brief Flush pending changes and redraw the screen.
 *
 *  Flush all graphical operations and print them to the display device.
 *  Nothing will show on the screen until this function is called.
 *
 *  If caca_set_display_time() was called with a non-zero value,
 *  caca_refresh_display() will use that value to achieve constant
 *  framerate: if two consecutive calls to caca_refresh_display() are within
 *  a time range shorter than the value set with caca_set_display_time(),
 *  the second call will be delayed before performing the screen refresh.
 *
 *  This function never fails.
 *
 *  \param dp The libcaca display context.
 *  \return This function always returns 0.
 */
int caca_refresh_display(caca_display_t *dp)
{
#if !defined(_DOXYGEN_SKIP_ME)
#define IDLE_USEC 5000
#endif
    int ticks = dp->lastticks + _caca_getticks(&dp->timer);

    dp->drv.display(dp);

    /* Once the display is finished, we can ack resizes */
    if(dp->resize.resized)
    {
        dp->resize.resized = 0;
        _caca_handle_resize(dp);
    }

    /* Wait until dp->delay + time of last call */
    ticks += _caca_getticks(&dp->timer);
    for(ticks += _caca_getticks(&dp->timer);
        ticks + IDLE_USEC < (int)dp->delay;
        ticks += _caca_getticks(&dp->timer))
    {
        _caca_sleep(IDLE_USEC);
    }

    /* Update the sliding mean of the render time */
    dp->rendertime = (7 * dp->rendertime + ticks) / 8;

    dp->lastticks = ticks - dp->delay;

    /* If we drifted too much, it's bad, bad, bad. */
    if(dp->lastticks > (int)dp->delay)
        dp->lastticks = 0;

    return 0;
}

/** \brief Show or hide the cursor.
 *
 *  Show or hide the cursor, for devices that support such a feature.
 *
 *  If an error occurs, -1 is returned and \b errno is set accordingly:
 *  - \c ENOSYS Display driver does not support showing the cursor.
 *
 *  \param dp The libcaca display context.
 *  \param flag 0 hides the cursor, 1 shows the system's default cursor
 *              (usually a white rectangle). Other values are reserved for
 *              future use.
 *  \return 0 upon success, -1 if an error occurred.
 */
int caca_set_cursor(caca_display_t *dp, int flag)
{
    if(!dp->drv.set_cursor)
    {
        seterrno(ENOSYS);
        return -1;
    }

    dp->drv.set_cursor(dp, flag);
    return 0;
}

/** \brief Show or hide the mouse pointer.
 *
 *  Show or hide the mouse pointer. This function works with the ncurses,
 *  S-Lang and X11 drivers.
 *
 *  If an error occurs, -1 is returned and \b errno is set accordingly:
 *  - \c ENOSYS Display driver does not support hiding the mouse pointer.
 *
 *  \param dp The libcaca display context.
 *  \param flag 0 hides the pointer, 1 shows the system's default pointer
 *              (usually an arrow). Other values are reserved for future use.
 *  \return 0 upon success, -1 if an error occurred.
 */
int caca_set_mouse(caca_display_t *dp, int flag)
{
    if(!dp->drv.set_mouse)
    {
        seterrno(ENOSYS);
        return -1;
    }

    dp->drv.set_mouse(dp, flag);
    return 0;
}

/*
 * XXX: following functions are local
 */

void _caca_handle_resize(caca_display_t *dp)
{
    dp->drv.handle_resize(dp);

    /* Tell libcucul we changed size */
    if(dp->resize.w != dp->cv->width || dp->resize.h != dp->cv->height)
    {
        dp->resize.allow = 1;
        cucul_set_canvas_size(dp->cv, dp->resize.w, dp->resize.h);
        dp->resize.allow = 0;
    }
}

void _caca_set_term_title(char const *str)
{
#if defined(HAVE_GETENV)
    char *term;

    term = getenv("TERM");

    if(!term || !strcmp(term, "linux"))
        return;
#endif

    fprintf(stdout, "\x1b]0;%s\x07", str);
    fflush(stdout);
}

