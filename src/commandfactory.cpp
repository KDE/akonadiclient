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
#include "errorreporter.h"
#include "commandshell.h"

#include <KCmdLineArgs>

#include <QDebug>
#include <QHash>

#include <iostream>


struct CommandData
{
  KLocalizedString shortHelp;
  CommandFactory::creatorFunction creator;
};

static QHash<QString, CommandData *> *sCommands = nullptr;


CommandFactory::CommandFactory( KCmdLineArgs *parsedArgs )
  : mParsedArgs( parsedArgs )
{
  Q_ASSERT( mParsedArgs != nullptr );

  checkAndHandleHelp();
}

CommandFactory::~CommandFactory()
{
}

AbstractCommand *CommandFactory::createCommand()
{
  const QString commandName = mParsedArgs->arg(0);

  CommandData *data = sCommands->value(commandName);
  if (data==nullptr)
  {
    KCmdLineArgs::enable_i18n();
    ErrorReporter::error(i18nc("@info:shell", "Unknown command '%1'", commandName));
    printHelpAndExit(false);
  }

  AbstractCommand *command = (data->creator)(nullptr);
  Q_ASSERT(command!=nullptr);
  return (command);
}


void CommandFactory::registerCommand(const QString &name,
                                     const KLocalizedString &shortHelp,
                                     CommandFactory::creatorFunction creator)
{
  CommandData *data = new CommandData;
  data->shortHelp = shortHelp;
  data->creator = creator;

  if (sCommands==nullptr) sCommands = new QHash<QString, CommandData *>;
  sCommands->insert(name, data);
}


// There are 3 cases to consider here:
//
//  1.	No arguments specified.  Display an error message and the list of
//	available commands to stderr.
//
//  2.	One argument specified and it is "help".  Just display the list of
//	available commands to stdout.
//
//  3.	More than one argument is specified and the first is "help".  For
//	the first argument, display the help for its options to stdout.
//	If there is no such command, display an error message.  Only help
//	for one command can be displayed, because KCmdLineArgs::usage()
//	exits when it has finished displaying the help.
//
// If none of the above apply, then do nothing.

void CommandFactory::checkAndHandleHelp()
{
  if ( mParsedArgs->count() == 0 )			// case 1
  {
    KCmdLineArgs::enable_i18n();
    ErrorReporter::error( i18nc( "@info:shell",
                                 "No command specified (try '%1 --help')",
                                 KCmdLineArgs::appName() ) );
    std::exit( AbstractCommand::InvalidUsage );
  }

  if ( mParsedArgs->arg( 0 ) == QLatin1String( "help" ) )
  {
    if ( mParsedArgs->count() == 1 )			// case 2
    {
      printHelpAndExit( true );
    }

    for (int a = 1; a<mParsedArgs->count(); ++a)	// case 3
    {
      const QString commandName = mParsedArgs->arg(a);
      if ( !sCommands->contains( commandName ) )
      {
        ErrorReporter::warning( i18nc( "@info:shell", "Unknown command '%1'", commandName ) );
        continue;
      }

      CommandData *data = sCommands->value(commandName);
      Q_ASSERT(data!=nullptr);
      AbstractCommand *command = (data->creator)(nullptr);
      Q_ASSERT(command!=nullptr);
      command->init(mParsedArgs);
      KCmdLineArgs::usage();
    }

    std::exit( AbstractCommand::NoError );
  }
}


void CommandFactory::printHelpAndExit( bool userRequestedHelp )
{
  int maxNameLength = 0;
  QStringList commands = sCommands->keys();
  Q_FOREACH (const QString &commandName, commands)
  {
    maxNameLength = qMax(maxNameLength, commandName.length());
  }

  // if the user requested help output to stdout,
  // otherwise we are missing the mandatory command argument and output to stderr
  std::ostream &stream = userRequestedHelp ? std::cout : std::cerr;

  const QString linePattern = QLatin1String( "  %1  %2" );

  stream << std::endl << qPrintable(i18nc("@info:shell", "Available commands are:")) << std::endl;

  qSort(commands);
  Q_FOREACH (const QString &commandName, commands)
  {
    if (commandName=="shell" && CommandShell::isActive()) continue;
    stream << qPrintable(linePattern.arg(commandName.leftJustified(maxNameLength),
                                         sCommands->value(commandName)->shortHelp.toString()))
           << std::endl;
  }

  std::exit( userRequestedHelp ? AbstractCommand::NoError : AbstractCommand::InvalidUsage );
}
