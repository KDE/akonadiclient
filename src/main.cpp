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

#include "abstractcommand.h"
#include "commandrunner.h"
#include "errorreporter.h"

#include <KAboutData>
#include <KCmdLineArgs>
#include <QCoreApplication>

#include <iostream>

#include "version.h"

const char *appname = "akonadiclient";

int main(int argc, char **argv)
{
    // Need this before any use of i18n() or similar
    // TODO: should we allow commands to optionally support GUI?
    QCoreApplication application(argc, argv);

    KAboutData aboutData(appname,							// componentName
                         i18nc("@title program name", "Akonadi Client"),		// displayName
#ifdef VCS_HAVE_VERSION
                         (VERSION " (" VCS_TYPE_STRING " " VCS_REVISION_STRING ")"),	// version
#else
                         VERSION,							// version
#endif
                         i18nc("@info:shell short description", "A command-line/shell client for Akonadi"),
								                         // shortDescription
                         KAboutLicense::GPL);						 // licenseType

    aboutData.addAuthor(i18n("Kevin Krammer"), i18nc("@title about data task", "Original Author"), "krammer@kde.org");
    aboutData.addAuthor(i18n("Jonathan Marten"), i18nc("@title about data task", "Additions and new commands"), "jjm@keelhaul.me.uk");
    aboutData.addAuthor(i18n("Bhaskar Kandiyal"), i18nc("@title about data task", "New commands, GSOC 2014"), "bkandiyal@gmail.com");

    KAboutData::setApplicationData(aboutData);

    KCmdLineArgs::init(argc, argv,
                       QCoreApplication::applicationName().toLocal8Bit(),
                       QByteArray(),
                       ki18nc("@title program name", "Akonadi Client"),
                       QCoreApplication::applicationVersion().toLocal8Bit());
    KCmdLineArgs::addStdCmdLineOptions(KCmdLineArgs::CmdLineArgNone);

    KCmdLineOptions options;
    options.add("!+command", ki18nc("@info:shell", "Command to execute"));
    options.add("+[options]", ki18nc("@info:shell", "Options for command"));
    options.add("+[args]", ki18nc("@info:shell", "Arguments for command"));
    options.add("", ki18nc("@info:shell",
                           "See '%1 help'"
                           " for available commands"
                           "\n"
                           "See '%1 help command'"
                           " for more information on a specific command.").subs(appname));
    KCmdLineArgs::addCmdLineOptions(options);


    // call right away so standard options like --version can terminate the program right here
    KCmdLineArgs *parsedArgs = KCmdLineArgs::parsedArgs();

    CommandRunner runner(parsedArgs);
    int ret = runner.start();
    if (ret != AbstractCommand::NoError) {
        return ret;
    }

    return application.exec();
}
