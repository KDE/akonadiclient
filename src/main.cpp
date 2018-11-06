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
#include <KLocalizedString>

#include <QCoreApplication>
#include <QCommandLineParser>

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

    QCommandLineParser parser;
    parser.setApplicationDescription(aboutData.shortDescription());
    parser.setOptionsAfterPositionalArgumentsMode(QCommandLineParser::ParseAsPositionalArguments);
    parser.addHelpOption();
    parser.addVersionOption();

    parser.addPositionalArgument("command", i18nc("@info:shell", "Command to execute"));
    parser.addPositionalArgument("options", i18nc("@info:shell", "Options for command"),
                                            i18nc("@info:shell", "[options]"));
    parser.addPositionalArgument("args", i18nc("@info:shell", "Arguments for command"),
                                         i18nc("@info:shell", "[args]"));

    // Just parse the command line and options, do not do any actions
    bool ok = parser.parse(QCoreApplication::arguments());

    // Handle parser errors and global application options here.
    // They would normally be handled automatically by QCommandLineParser::process(),
    // but we need to keep control so that the additional help text as below
    // can be displayed.

    if (!ok) {
        ErrorReporter::fatal(parser.errorText());
    }

    if (parser.isSet("help")) {
        std::cout << qPrintable(parser.helpText());	// does not exit application
        std::cout << std::endl;				// so can print extra
        std::cout << qPrintable(i18nc("@info:shell",
                                      "See '%1 help' for available commands."
                                      "\n"
                                      "See '%1 help command' for information on a specific command.",
                                      appname));
        std::cout << std::endl;
        return (EXIT_SUCCESS);				// and then exit
    }

    if (parser.isSet("version")) {
        parser.showVersion();				// exits the application
    }

    // The QCommandLineParser::positionalArguments() remaining are the
    // command name followed by any options or arguments.  An empty list
    // (no command specified) is handled by CommandFactory::checkAndHandleHelp().
    const QStringList args = parser.positionalArguments();

    CommandRunner runner(&args);
    int ret = runner.start();
    if (ret != AbstractCommand::NoError) return ret;

    ErrorReporter::setRunningApplication();
    return (application.exec());
}
