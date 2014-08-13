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

#include "abstractcommand.h"
#include "commandfactory.h"
#include "errorreporter.h"

#include <kaboutdata.h>
#include <kcmdlineargs.h>

#include <QTextStream>
#include <QCoreApplication>
#include <QVector>
#include <QDebug>
#include <QTextCodec>
#include <QVarLengthArray>

bool CommandShell::mRestart = true;

CommandShell::CommandShell(const KAboutData &aboutData)
    : mCommand(0),
      mAboutData(aboutData)
{
    mTextStream = new QTextStream(stdin);
    mTextStream->setCodec(QTextCodec::codecForLocale());

    ErrorReporter::setAppName(aboutData.appName());
}

CommandShell::~CommandShell()
{
    delete mTextStream;
}

void CommandShell::start()
{
    QString input = mTextStream->readLine();
    if (mTextStream->atEnd()) {
        mRestart = false;
        QCoreApplication::quit();
        return;
    }

    // TODO: Need to implement a better way to separate arguments to accomodate spaces in filenames
    QStringList list = input.split(" ");
    list.insert(0, mAboutData.appName());

    QVarLengthArray<char*> tempArgs;
    tempArgs.reserve(list.size());

    for (int i = 0; i < list.size(); i++) {
        std::string str = list.at(i).toStdString();
        char *temp = new char[str.length()+1];
        str.copy(temp, str.length());
        temp[str.length()] = '\0';
        tempArgs.append(temp);
    }

    char **args = tempArgs.data();

    KCmdLineArgs *parsedArgs = getParsedArgs(list.length(), args);
    CommandFactory factory(parsedArgs);

    mCommand = factory.createCommand();
    if (mCommand == 0) {
        freeArguments(tempArgs);
        QMetaObject::invokeMethod(this, "start", Qt::QueuedConnection);
        return;
    }

    connect(mCommand, SIGNAL(error(QString)), this, SLOT(onCommandError(QString)));

    if (mCommand->init(parsedArgs) != AbstractCommand::NoError) {
        freeArguments(tempArgs);
        QMetaObject::invokeMethod(this, "start", Qt::QueuedConnection);
        return;
    }

    connect(mCommand, SIGNAL(finished(int)), this, SLOT(onCommandFinished(int)));
    QMetaObject::invokeMethod(mCommand, "start", Qt::QueuedConnection);
    freeArguments(tempArgs);
}

KCmdLineArgs* CommandShell::getParsedArgs(int argc, char **argv)
{
    KCmdLineArgs::reset();
    KCmdLineArgs::init(argc, argv, &mAboutData);
    KCmdLineArgs::addStdCmdLineOptions(KCmdLineArgs::CmdLineArgNone);

    KCmdLineOptions options;
    options.add("!+command", ki18nc("@info:shell", "Command to execute"));
    options.add("+[options]", ki18nc( "@info:shell", "Options for command"));
    options.add("+[args]", ki18nc( "@info:shell", "Arguments for command"));
    options.add("", ki18nc("@info:shell",
                           "See '<application>%1</application> help'"
                           " for available commands"
                           "\n"
                           "See '<application>%1</application> help command'"
                           " for more information on a specific command.").subs("akonadiclient"));
    KCmdLineArgs::addCmdLineOptions(options);
    return KCmdLineArgs::parsedArgs();
}

void CommandShell::onCommandFinished(int exitCode)
{
    QMetaObject::invokeMethod(this, "start", Qt::QueuedConnection);
}

void CommandShell::onCommandError(const QString &error)
{
    ErrorReporter::error(error);
}

void CommandShell::freeArguments(const QVarLengthArray<char*> &args)
{
    for (int i = 0; i < args.size(); i++) {
        delete[] args.at(i);
    }
}
