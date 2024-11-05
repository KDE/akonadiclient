/*
    Copyright (C) 2024 Jonathan Marten <jjm@keelhaul.me.uk>

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

#pragma once

#include <qbytearray.h>
#include <qflags.h>

class TerminalColour
{
public:
    static TerminalColour *self();

    void init();

    enum Attribute {
        Bold = 0x100,
        Reverse = 0x200,
        Normal = 0x400,

        ColourFlag = 0x800,
        ColourMask = 7,

        Black = 0 | ColourFlag,
        Red = 1 | ColourFlag,
        Green = 2 | ColourFlag,
        Yellow = 3 | ColourFlag,
        Blue = 4 | ColourFlag,
        Magenta = 5 | ColourFlag,
        Cyan = 6 | ColourFlag,
        White = 7 | ColourFlag,
    };
    Q_DECLARE_FLAGS(Attributes, Attribute)

    static const char *string(TerminalColour::Attributes attr);

private:
    explicit TerminalColour() = default;
    ~TerminalColour() = default;

    const char *stringInternal(TerminalColour::Attributes attr) const;

private:
    QByteArray mColourString;
    QByteArray mBoldString;
    QByteArray mReverseString;
    QByteArray mNormalString;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(TerminalColour::Attributes)
