/*
    Copyright (C) 2013  Jonathan Marten <jjm@keelhaul.me.uk>

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

#include "errorreporter.h"

#include <QCoreApplication>
#include <QString>

#include <klocalizedstring.h>

#include <iostream>

#include "terminalcolour.h"

static bool runningApplication = false;

static void message(const QString &severity, TerminalColour::Attributes colour, const QString &msg)
{
    std::cerr << qPrintable(QCoreApplication::applicationName()) << " (" << TerminalColour::string(colour) << qPrintable(severity)
              << TerminalColour::string(TerminalColour::Normal) << "): " << TerminalColour::string(colour) << qPrintable(msg)
              << TerminalColour::string(TerminalColour::Normal) << std::endl;
}

void ErrorReporter::error(const QString &msg)
{
    message(i18nc("@info:shell", "error"), (TerminalColour::Bold | TerminalColour::Red), msg);
}

void ErrorReporter::warning(const QString &msg)
{
    message(i18nc("@info:shell", "warning"), (TerminalColour::Bold | TerminalColour::Yellow), msg);
}

void ErrorReporter::notice(const QString &msg)
{
    message(i18nc("@info:shell", "notice"), (TerminalColour::Bold | TerminalColour::Cyan), msg);
}

void ErrorReporter::success(const QString &msg)
{
    message(i18nc("@info:shell", "success"), (TerminalColour::Bold | TerminalColour::Green), msg);
}

void ErrorReporter::info(const QString &msg)
{
    message(i18nc("@info:shell", "info"), TerminalColour::Green, msg);
}

void ErrorReporter::fatal(const QString &msg)
{
    error(msg);

    // If the QCoreApplication event loop has not been started yet, exit now
    if (!runningApplication) {
        exit(EXIT_FAILURE);
    }
    // Otherwise just tell the event loop to exit
    QCoreApplication::exit(EXIT_FAILURE);
}

void ErrorReporter::progress(const QString &msg)
{
    std::cerr << "** " << TerminalColour::string(TerminalColour::Bold | TerminalColour::Magenta) << qPrintable(msg)
              << TerminalColour::string(TerminalColour::Normal) << std::endl;
}

void ErrorReporter::instruct(const QString &msg)
{
    std::cerr << std::endl << TerminalColour::string(TerminalColour::Bold) << qPrintable(msg) << TerminalColour::string(TerminalColour::Normal) << std::endl;
}

void ErrorReporter::setRunningApplication(bool running)
{
    runningApplication = running;
}
