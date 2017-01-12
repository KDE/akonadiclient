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

#include <KCmdLineArgs>
#include <KCmdLineOptions>

#include <QMetaObject>

#include "errorreporter.h"
#include "commandshell.h"

#define ENV_VAR_DANGEROUS   "AKONADICLIENT_DANGEROUS"
#define ENV_VAL_DANGEROUS   "enabled"

AbstractCommand::AbstractCommand(QObject *parent)
    : QObject(parent)
{
}

AbstractCommand::~AbstractCommand()
{
}

int AbstractCommand::init(KCmdLineArgs *parsedArgs)
{
    parsedArgs->clear();
    KCmdLineArgs::reset();

    KCmdLineOptions options;
    setupCommandOptions(options);

    KCmdLineArgs::addCmdLineOptions(options);
    KCmdLineArgs::addStdCmdLineOptions(KCmdLineArgs::CmdLineArgNone);

    KCmdLineArgs *parseCommandArgs = KCmdLineArgs::parsedArgs();
    Q_ASSERT(parseCommandArgs != nullptr);

    const int result = initCommand(parseCommandArgs);

    KCmdLineArgs::reset();
    KCmdLineArgs::addStdCmdLineOptions(KCmdLineArgs::CmdLineArgNone);
    KCmdLineArgs::addCmdLineOptions(options);

    return result;
}

void AbstractCommand::setupCommandOptions(KCmdLineOptions &options)
{
    options.add(("+" + name()).toLocal8Bit(), ki18nc("@info:shell", "The name of the command"));
}

void AbstractCommand::addOptionsOption(KCmdLineOptions &options)
{
    options.add("+[options]", ki18nc("@info:shell", "Options for command"));
}

void AbstractCommand::addOptionSeparator(KCmdLineOptions &options)
{
    options.add(":", ki18nc("@info:shell", "Options for command:"));
}

void AbstractCommand::addCollectionItemOptions(KCmdLineOptions &options)
{
    options.add("c").add("collection", ki18nc("@info:shell", "Assume that a collection is specified"));
    options.add("i").add("item", ki18nc("@info:shell", "Assume that an item is specified"));
}

void AbstractCommand::addDryRunOption(KCmdLineOptions &options)
{
    options.add("n").add("dryrun", ki18nc("@info:shell", "Run without making any actual changes"));
}

void AbstractCommand::emitErrorSeeHelp(const KLocalizedString &msg)
{
    QString s;
    if (CommandShell::isActive()) {
        s = ki18nc("@info:shell %1 is subcommand name, %2 is error message",
                   "%2. See 'help %1'")
            .subs(this->name())
            .subs(msg.toString()).toString();
    } else {
        s = ki18nc("@info:shell %1 is application name, %2 is subcommand name, %3 is error message",
                   "%3. See '%1 help %2'")
            .subs(KCmdLineArgs::appName())
            .subs(this->name())
            .subs(msg.toString()).toString();
    }

    emit error(s);
}

bool AbstractCommand::allowDangerousOperation() const
{
    if (qgetenv(ENV_VAR_DANGEROUS) == ENV_VAL_DANGEROUS) {
        // check set in environment
        return true;
    }

    ErrorReporter::error(i18n("Dangerous or destructive operations are not allowed"));
    ErrorReporter::error(i18n("Set %1=\"%2\" in environment",
                              QLatin1String(ENV_VAR_DANGEROUS),
                              QLatin1String(ENV_VAL_DANGEROUS)));
    return false;
}
