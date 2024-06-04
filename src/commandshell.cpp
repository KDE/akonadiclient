/*
 * Copyright (C) 2014  Bhaskar Kandiyal <bkandiyal@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#include "commandshell.h"

#include "commandfactory.h"
#include "errorreporter.h"

#include <kshell.h>

#include <QTextStream>
#include <QCoreApplication>

DEFINE_COMMAND("shell", CommandShell,
               kli18n("Enter commands in an interactive shell"));

bool CommandShell::sIsActive = false;

CommandShell::CommandShell(QObject *parent)
    : AbstractCommand(parent)
    , mTextStream(new QTextStream(stdin))
{
}

CommandShell::~CommandShell()
{
    delete mTextStream;
}

void CommandShell::setupCommandOptions(QCommandLineParser *parser)
{
}

int CommandShell::initCommand(QCommandLineParser *parser)
{
    return (AbstractCommand::NoError);
}

void CommandShell::start()
{
    sIsActive = true;
    while (enterCommandLoop()) {}
    sIsActive = false;
    QCoreApplication::quit();
}

bool CommandShell::enterCommandLoop()
{
    const QString input = mTextStream->readLine();
    if (mTextStream->atEnd()) {
        return (false);
    }

    AbstractCommand *toInvoke = nullptr;

    KShell::Errors err;
    const QStringList args = KShell::splitArgs(input, KShell::AbortOnMeta, &err);
    if (err != KShell::NoError) {			// error splitting line
        ErrorReporter::error(i18nc("@info:shell", "Invalid command"));
    }							// but continue command loop
    else if (!args.isEmpty()) {				// non-empty input line
        const QString cmd = args.first();		// look at command name
        if (cmd == "quit" || cmd == "exit") {		// exit shell on these
            return (false);
        }

        CommandFactory factory(&args);
        if (cmd == "help") return (true);		// handled above by CommandFactory

        toInvoke = factory.createCommand();
        if (toInvoke != nullptr) {
            connect(toInvoke, &AbstractCommand::error, this, &CommandShell::onCommandError);
            if (toInvoke->init(args) == AbstractCommand::NoError) {
                QEventLoop loop;			// hopefully safe, because not a child of us
                connect(toInvoke, &AbstractCommand::finished, &loop, &QEventLoop::quit);
                QMetaObject::invokeMethod(toInvoke, "start", Qt::QueuedConnection);
                loop.exec();
            }

            toInvoke->deleteLater();
        }
    }

    return (true);
}

void CommandShell::onCommandError(const QString &error)
{
    ErrorReporter::error(error);
}

#include "moc_commandshell.cpp"
