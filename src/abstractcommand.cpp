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

#include <KLocalizedString>

#include <QCommandLineParser>

#include "errorreporter.h"
#include "commandshell.h"

#include <iostream>

#define ENV_VAR_DANGEROUS   "AKONADICLIENT_DANGEROUS"
#define ENV_VAL_DANGEROUS   "enabled"

AbstractCommand::AbstractCommand(QObject *parent)
    : QObject(parent)
{
}

int AbstractCommand::init(const QStringList &parsedArgs, bool showHelp)
{
    QCommandLineParser parser;
    parser.addPositionalArgument(name(), i18nc("@info:shell", "The name of the command"),  name());
    setupCommandOptions(&parser);			// set options for command

    const bool ok = parser.parse(parsedArgs);		// just parse, do not do actions
    if (!ok)
    {
        ErrorReporter::fatal(parser.errorText());
        return (InvalidUsage);
    }

    if (showHelp) {					// do this here, while the
        QString s = parser.helpText();			// parser is still available

        // This substitutes the first "[options]" (between the "akonadiclient"
        // and the comand name) with blank, because it is not relevant and
        // would be confused with the command options (following the command
        // name).  Cannot use QString::replace() with a regular expression
        // here, as that would substitute any subsequent "[optional]" help
        // text supplied by the command as well.
        int idx1 = s.indexOf(" [");
        int idx2 = s.indexOf("] ");
        if (idx1!=-1 && idx2!=-1) s = s.left(idx1)+s.mid(idx2+1);

        // If the command shell is active, then the executable name also needs
        // to be removed.  Again, cannot use a regular expression replacement
        // because only the first instance needs to be changed.
        if (CommandShell::isActive()) {
            int idx1 = s.indexOf(": ");
            if (idx1!=-1) {
                int idx2 = s.indexOf(' ', idx1+2);
                if (idx2!=-1) s = s.left(idx1+1)+s.mid(idx2);
            }
        }

        std::cout << qPrintable(s) << std::endl;
        return (NoError);
    }

    return (initCommand(&parser));			// read command arguments
}

void AbstractCommand::addOptionsOption(QCommandLineParser *parser)
{
    parser->addPositionalArgument("options", i18nc("@info:shell", "Options for command"), i18nc("@info:shell", "[options]"));
}

void AbstractCommand::addCollectionItemOptions(QCommandLineParser *parser)
{
    parser->addOption(QCommandLineOption((QStringList() << "c" << "collection"),
                                         i18nc("@info:shell", "Assume that a collection is specified")));

    parser->addOption(QCommandLineOption((QStringList() << "i" << "item"),
                                         i18nc("@info:shell", "Assume that an item is specified")));
}

void AbstractCommand::addDryRunOption(QCommandLineParser *parser)
{
    parser->addOption(QCommandLineOption((QStringList() << "n" << "dryrun"),
                                         i18nc("@info:shell", "Run without making any actual changes")));
}

void AbstractCommand::emitErrorSeeHelp(const QString &msg)
{
    QString s;
    if (CommandShell::isActive()) {
        s = i18nc("@info:shell %1 is subcommand name, %2 is error message",
                  "%2. See 'help %1'",
                  this->name(),
                  msg);
    } else {
        s = i18nc("@info:shell %1 is application name, %2 is subcommand name, %3 is error message",
                  "%3. See '%1 help %2'",
                  QCoreApplication::applicationName(),
                  this->name(),
                  msg);
    }

    emit error(s);
}

bool AbstractCommand::allowDangerousOperation() const
{
    if (qgetenv(ENV_VAR_DANGEROUS) == ENV_VAL_DANGEROUS) {
        // check set in environment
        return true;
    }

    ErrorReporter::error(i18nc("@info:shell", "Dangerous or destructive operations are not allowed"));
    ErrorReporter::error(i18nc("@info:shell", "Set %1=\"%2\" in environment",
                              QLatin1String(ENV_VAR_DANGEROUS),
                              QLatin1String(ENV_VAL_DANGEROUS)));
    return false;
}
