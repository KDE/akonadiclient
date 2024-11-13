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

// This can return a 'bool' result because, during command initialisation,
// the only possible error conditions are NoError or InvalidUsage.  HelpOnly
// simply means to not actually execute the command.
bool CommandRunner::start()
{
    CommandFactory factory(mParsedArgs);
    mCommand = factory.createCommand();
    Q_ASSERT(mCommand != nullptr);

    connect(mCommand, &AbstractCommand::error, this, &CommandRunner::onCommandError);

    mExitCode = mCommand->init(*mParsedArgs);
    if (mExitCode != AbstractCommand::NoError) {
        if (mExitCode == AbstractCommand::HelpOnly)
            mExitCode = AbstractCommand::NoError;

        delete mCommand;
        mCommand = nullptr;
        return false;
    }

    connect(mCommand, &AbstractCommand::finished, this, &CommandRunner::onCommandFinished);
    QMetaObject::invokeMethod(mCommand, "start", Qt::QueuedConnection);
    return true;
}

void CommandRunner::onCommandFinished(AbstractCommand::Error exitCode)
{
    // If no exit code is emplicitly specified, use the accumulated
    // exit code from the processing loop.
    if (exitCode == AbstractCommand::DefaultError)
        exitCode = mExitCode;
    QCoreApplication::exit(static_cast<int>(exitCode));
}

void CommandRunner::onCommandError(const QString &error)
{
    ErrorReporter::error(error);
    // Set the eventual exit code to RuntimeError, but only if
    // it is not already set.
    if (mExitCode == AbstractCommand::NoError)
        mExitCode = AbstractCommand::RuntimeError;
}
