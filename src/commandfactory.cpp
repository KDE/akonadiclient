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

#include "commandfactory.h"

#include "abstractcommand.h"
#include "commandshell.h"
#include "errorreporter.h"
#include "terminalcolour.h"

#include <QDebug>
#include <QHash>

#include <iostream>
using namespace Qt::Literals::StringLiterals;
struct CommandData {
    KLocalizedString shortHelp;
    CommandFactory::creatorFunction creator;
};

static QHash<QString, CommandData *> *sCommands = nullptr;

CommandFactory::CommandFactory(const QStringList *parsedArgs)
    : mParsedArgs(parsedArgs)
{
    Q_ASSERT(mParsedArgs != nullptr);

    checkAndHandleHelp();
}

AbstractCommand *CommandFactory::createCommand()
{
    const QString commandName = mParsedArgs->first();
    CommandData *data = nullptr;

    if (commandName.length() >= 2) {
        const QStringList availableCommands = sCommands->keys();
        QStringList matchingCommands;
        for (const QString &command : std::as_const(availableCommands)) {
            if (command.startsWith(commandName))
                matchingCommands.append(command);
        }

        if (matchingCommands.count() == 1) {
            data = sCommands->value(matchingCommands.first());
        } else if (!matchingCommands.isEmpty()) {
            ErrorReporter::error(i18nc("@info:shell", "Ambiguous command '%1', possibilities are '%2'", commandName, matchingCommands.join("'/'")));
            if (!CommandShell::isActive())
                std::exit(EXIT_FAILURE);
            return (nullptr);
        }
    }

    if (data == nullptr) {
        ErrorReporter::error(i18nc("@info:shell", "Unknown command '%1'", commandName));
        if (!CommandShell::isActive())
            printHelpAndExit(false);
        return (nullptr);
    }

    AbstractCommand *command = (data->creator)(nullptr);
    Q_ASSERT(command != nullptr);
    return (command);
}

void CommandFactory::registerCommand(const QString &name, const KLocalizedString &shortHelp, CommandFactory::creatorFunction creator)
{
    CommandData *data = new CommandData;
    data->shortHelp = shortHelp;
    data->creator = creator;

    if (sCommands == nullptr) {
        sCommands = new QHash<QString, CommandData *>;
    }
    sCommands->insert(name, data);
}

// There are 3 cases to consider here:
//
//  1.  No arguments specified.  Display an error message and the list of
//  available commands to stderr.
//
//  2.  One argument specified and it is "help".  Just display the list of
//  available commands to stdout.
//
//  3.  More than one argument is specified and the first is "help".  For
//  each argument, display the help for its options to stdout.
//  If there is no such command, display an error message.
//
// If none of the above apply, then do nothing.

void CommandFactory::checkAndHandleHelp()
{
    if (mParsedArgs->isEmpty()) { // case 1
        ErrorReporter::error(i18nc("@info:shell", "No command specified (try '%1 --help')", QCoreApplication::applicationName()));
        std::exit(EXIT_FAILURE);
    }

    if (mParsedArgs->first() == QLatin1String("help")) {
        if (mParsedArgs->count() == 1) { // case 2
            printHelpAndExit(true);
        }

        for (int a = 1; a < mParsedArgs->count(); ++a) { // case 3
            const QString commandName = mParsedArgs->at(a);
            if (!sCommands->contains(commandName)) {
                ErrorReporter::warning(i18nc("@info:shell", "Unknown command '%1'", commandName));
                continue;
            }

            CommandData *data = sCommands->value(commandName);
            Q_ASSERT(data != nullptr);
            AbstractCommand *command = (data->creator)(nullptr);
            Q_ASSERT(command != nullptr);
            command->init(*mParsedArgs, true); // set up and display help
        }

        if (!CommandShell::isActive())
            std::exit(EXIT_SUCCESS);
    }
}

void CommandFactory::printHelpAndExit(bool userRequestedHelp)
{
    int maxNameLength = 0;
    QStringList commands = sCommands->keys();
    std::sort(commands.begin(), commands.end());
    for (const QString &commandName : std::as_const(commands)) {
        maxNameLength = qMax(maxNameLength, commandName.length());
    }

    // if the user requested help output to stdout,
    // otherwise we are missing the mandatory command argument and output to stderr
    std::ostream &stream = userRequestedHelp ? std::cout : std::cerr;

    const QString linePattern = QLatin1String("  %3%1%4  %2");
    const bool shellActive = CommandShell::isActive();

    stream << std::endl << qPrintable(i18nc("@info:shell", "Available commands are:")) << std::endl;

    for (const QString &commandName : std::as_const(commands)) {
        if (commandName == "shell"_L1 && shellActive)
            continue;

        stream << qPrintable(linePattern.arg(commandName.leftJustified(maxNameLength),
                                             sCommands->value(commandName)->shortHelp.toString(Kuit::TermText),
                                             TerminalColour::string(TerminalColour::Bold),
                                             TerminalColour::string(TerminalColour::Normal)))
               << std::endl;
    }

    if (!shellActive)
        std::exit(userRequestedHelp ? EXIT_SUCCESS : EXIT_FAILURE);
    stream << std::endl;
}
