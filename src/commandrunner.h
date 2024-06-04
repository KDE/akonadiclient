/*
    Copyright (C) 2012  Kevin Krammer <krammer@kde.org>

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

#include <QObject>

class AbstractCommand;

class CommandRunner : public QObject
{
    Q_OBJECT

public:
    explicit CommandRunner(const QStringList *args);
    ~CommandRunner() override;

    int start();
    int exitCode() const				{ return (mExitCode); }

private:
    AbstractCommand *mCommand = nullptr;
    const QStringList *mParsedArgs;
    int mExitCode;

private Q_SLOTS:
    void onCommandFinished(int exitCode);
    void onCommandError(const QString &error);
};

