/*
    Copyright (C) 2013-2025  Jonathan Marten <jjm@keelhaul.me.uk>

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

class QString;

/**
 * @short Functions for displaying error and status messages,
 *
 * Since this is a command line tool, if terminal colouring is available
 * the messages are colour coded so that they can be clearly seem among
 * what may be plentiful log messages.  The various severity levels and
 * their intended usage are:
 *
 *    fatal       An error has been encountered and the action cannot
 *                start or continue;  the command will exit immediately.
 *
 *    error       It is not possible to carry out the requested action,
 *                but it may be possible to continue with other unrelated
 *                actions if there is more to be done.
 *
 *    warning     As much as possible of the requested action will be done,
 *                but it may not be possible to do everything.
 *
 *    notice      The requested action was carried out, but there is
 *                something that should be taken note of.
 *
 *    info        The requested action was carried out;  this information
 *                may be useful, but it is not a problem if it is ignored.
 *
 *    success     All of the requested actions were successfully carried out.
 *                This should normally be the final message shown by an
 *                operation.
 *
 *    progress    Something is going on.
 *
 *    instruct    Instructions to the user as to what needs to be done.
 **/

namespace ErrorReporter
{
void fatal(const QString &msg);
void error(const QString &msg);
void warning(const QString &msg);
void notice(const QString &msg);
void info(const QString &msg);
void success(const QString &msg);
void progress(const QString &msg);
void instruct(const QString &msg);
void setRunningApplication(bool running = true);
};
