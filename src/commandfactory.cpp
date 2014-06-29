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

#include "addcommand.h"
#include "copycommand.h"
#include "createcommand.h"
#include "editcommand.h"
#include "errorreporter.h"
#include "expandcommand.h"
#include "infocommand.h"
#include "listcommand.h"
#include "movecommand.h"
#include "renamecommand.h"
#include "showcommand.h"

#include <KCmdLineArgs>

#include <QDebug>
#include <QHash>

#include <iostream>


CommandFactory::CommandFactory( KCmdLineArgs *parsedArgs )
  : mParsedArgs( parsedArgs )
{
  Q_ASSERT( mParsedArgs != 0 );

  registerCommands();
  checkAndHandleHelp();
}

CommandFactory::~CommandFactory()
{
  qDeleteAll( mCommands );
}

AbstractCommand *CommandFactory::createCommand()
{
  const QString commandName = mParsedArgs->arg( 0 );

  AbstractCommand *command = mCommands.take( commandName );
  if ( command == 0 ) {
    KCmdLineArgs::enable_i18n();
    ErrorReporter::error( i18nc( "@info:shell", "Unknown command '%1'", commandName ) );
    printHelpAndExit( false );
  }

  return command;
}

void CommandFactory::registerCommands()
{
  AbstractCommand *command;

  command = new ListCommand;
  mCommands.insert( command->name(), command );
  command = new InfoCommand;
  mCommands.insert( command->name(), command );
  command = new ShowCommand;
  mCommands.insert( command->name(), command );
  command = new CreateCommand;
  mCommands.insert( command->name(), command );
  command = new AddCommand;
  mCommands.insert( command->name(), command );
  command = new CopyCommand;
  mCommands.insert( command->name(), command );
  command = new MoveCommand;
  mCommands.insert( command->name(), command );
  command = new ExpandCommand;
  mCommands.insert( command->name(), command );
  command = new EditCommand;
  mCommands.insert( command->name(), command );
  command = new RenameCommand;
  mCommands.insert( command->name(), command );
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
      if ( !mCommands.contains( commandName ) )
      {
        ErrorReporter::warning( i18nc( "@info:shell", "Unknown command '%1'", commandName ) );
        continue;
      }

      AbstractCommand *command = mCommands.value( commandName );
      command->init( mParsedArgs );
      KCmdLineArgs::usage();
    }

    std::exit( AbstractCommand::NoError );
  }
}


void CommandFactory::printHelpAndExit( bool userRequestedHelp )
{
  int maxNameLength = 0;
  Q_FOREACH( const QString &commandName, mCommands.keys() ) {
    maxNameLength = qMax( maxNameLength, commandName.length() );
  }

  // if the user requested help output to stdout,
  // otherwise we are missing the mandatory command argument and output to stderr
  std::ostream &stream = userRequestedHelp ? std::cout : std::cerr;

  const QString linePattern = QLatin1String( "  %1  %2" );

  stream << std::endl << i18nc( "@info:shell", "Available commands are:" ).toLocal8Bit().constData() << std::endl;

  QHash<QString, AbstractCommand *>::const_iterator it    = mCommands.constBegin();
  QHash<QString, AbstractCommand *>::const_iterator endIt = mCommands.constEnd();
  for (; it != endIt; ++it ) {
    stream << linePattern.arg( it.key().leftJustified( maxNameLength, QLatin1Char( ' ' ) ),
                               it.value()->shortHelp() ).toLocal8Bit().constData()
           << std::endl;
  }

  std::exit( userRequestedHelp ? AbstractCommand::NoError : AbstractCommand::InvalidUsage );
}
