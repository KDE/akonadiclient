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

#include "abstractcommandtest.h"

#include <akonadi/control.h>

#include <kcmdlineargs.h>
#include <kaboutdata.h>

#include <QEventLoop>
#include <QSignalSpy>
#include <QTimer>

using namespace Akonadi;

AbstractCommandTest::AbstractCommandTest()
    : m_finishedSpy(0),
      m_errorSpy(0)
{
    mAboutData = new KAboutData("akonadiclient", 0, ki18nc("@title program name", "Akonadi Client"),
                         "0.1",
                         ki18nc("@info:shell short description", "A command-line/shell client for Akonadi"),
                         KAboutData::License_GPL);
}

AbstractCommandTest::~AbstractCommandTest()
{

}

KCmdLineArgs* AbstractCommandTest::getParsedArgs(int argc, const char **argv)
{
    KCmdLineArgs::init(argc, const_cast<char**>(argv), mAboutData);
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

int AbstractCommandTest::runCommand(AbstractCommand *command, int maxWaitTime)
{
    m_finishedSpy = new QSignalSpy(command, SIGNAL(finished(int)));
    m_errorSpy = new QSignalSpy(command, SIGNAL(error(QString)));

    QEventLoop loop;
    connect(command, SIGNAL(finished(int)), &loop, SLOT(quit()));
    QTimer::singleShot(maxWaitTime, &loop, SLOT(quit()));

    QMetaObject::invokeMethod(command, "start", Qt::QueuedConnection);
    return loop.exec();
}

void AbstractCommandTest::cleanup()
{
    delete m_finishedSpy;
    delete m_errorSpy;
}