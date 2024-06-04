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

#include "commandrunner.h"

#include "abstractcommand.h"
#include "commandfactory.h"
#include "errorreporter.h"

#include <QCoreApplication>

#include <iostream>

CommandRunner::CommandRunner(const QStringList *args)
    : mParsedArgs(args)
{
}

CommandRunner::~CommandRunner()
{
    delete mCommand;
}

int CommandRunner::start()
{
    CommandFactory factory(mParsedArgs);
    mCommand = factory.createCommand();
    Q_ASSERT(mCommand != nullptr);

    connect(mCommand, &AbstractCommand::error, this, &CommandRunner::onCommandError);

    if (mCommand->init(*mParsedArgs) == AbstractCommand::InvalidUsage) {
        delete mCommand;
        mCommand = nullptr;
        return AbstractCommand::InvalidUsage;
    }

    mExitCode = AbstractCommand::NoError; // no errors reported yet

    connect(mCommand, &AbstractCommand::finished, this, &CommandRunner::onCommandFinished);

    QMetaObject::invokeMethod(mCommand, "start", Qt::QueuedConnection);

    return AbstractCommand::NoError;
}

void CommandRunner::onCommandFinished(int exitCode)
{
    // If no exit code is emplicitly specified, use the accumulated
    // exit code from the processing loop.
    if (exitCode == AbstractCommand::DefaultError)
        exitCode = mExitCode;
    QCoreApplication::exit(exitCode);
}

void CommandRunner::onCommandError(const QString &error)
{
    ErrorReporter::error(error);
    // Set the eventual exit code to RuntimeError, but only if
    // it is not already set.
    if (mExitCode == AbstractCommand::NoError)
        mExitCode = AbstractCommand::RuntimeError;
}
