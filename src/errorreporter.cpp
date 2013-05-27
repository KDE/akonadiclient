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

#include <QString>
#include <QCoreApplication>

#include <iostream>


// Non-POD should really be K_GLOBAL_STATIC, but this is not a library
// so hopefully will not matter.
static QString appName;


void ErrorReporter::setAppName(const QString &name)
{
  appName = name;
}

void ErrorReporter::error(const QString &msg)
{
  std::cerr << appName.toLocal8Bit().constData()
            << " (error): "
            << msg.toLocal8Bit().constData()
            << std::endl;
}

void ErrorReporter::warning(const QString &msg)
{
  std::cerr << appName.toLocal8Bit().constData()
            << " (warning): "
            << msg.toLocal8Bit().constData()
            << std::endl;
}

void ErrorReporter::fatal(const QString &msg)
{
    error(msg);

    // If the QCoreApplication event loop has not been started yet, exit now
    if (!QCoreApplication::hasPendingEvents()) exit(1);
    // Otherwise just tell the event loop to exit
    QCoreApplication::exit(1);
}

void ErrorReporter::progress(const QString &msg)
{
    std::cout << "** "
              << msg.toLocal8Bit().constData()
              << std::endl;
}
