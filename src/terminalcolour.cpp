/*
    Copyright (C) 2024  Jonathan Marten <jjm@keelhaul.me.uk>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include "terminalcolour.h"

#include <klocalizedstring.h>

#ifdef HAVE_TERMINFO
#include <curses.h>
#include <term.h>
#include <unistd.h>
#endif

#include "errorreporter.h"

/* static */ TerminalColour *TerminalColour::self()
{
    static TerminalColour *instance = new TerminalColour();
    return (instance);
}

void TerminalColour::init()
{
#ifdef HAVE_TERMINFO
    if (!isatty(1))
        return; // not a TTY, no colouring

    int erret;
    if (setupterm(nullptr, 2, &erret) == ERR) {
        ErrorReporter::warning(i18nc("@info:shell", "Could not set up terminal"));
        return;
    }

    const char *str = tigetstr("setaf");
    if (str == nullptr || *str == '\0')
        str = tigetstr("setf");
    if (str != nullptr && *str != '\0')
        mColourString = str;

    str = tigetstr("bold");
    if (str != nullptr && *str != '\0')
        mBoldString = str;

    str = tigetstr("rev");
    if (str != nullptr && *str != '\0')
        mReverseString = str;

    str = tigetstr("sgr0");
    if (str != nullptr && *str != '\0')
        mNormalString = str;
#endif
}

/* private */ const char *TerminalColour::stringInternal(TerminalColour::Attributes attr) const
{
    static QByteArray result;

    result.clear();
    if (attr & TerminalColour::Normal)
        result += mNormalString; // reset everything first
#ifdef HAVE_TERMINFO
    if (attr & TerminalColour::ColourFlag)
        result += tiparm(mColourString.constData(), static_cast<int>(attr & TerminalColour::ColourMask));
#endif
    if (attr & TerminalColour::Bold)
        result += mBoldString;
    if (attr & TerminalColour::Reverse)
        result += mReverseString;

    return (result.constData());
}

/* static */ const char *TerminalColour::string(TerminalColour::Attributes attr)
{
    return (self()->stringInternal(attr));
}
