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
    const QString message = i18nc( "@info:shell",
                                   "<application>%1</application>: Unknown command '%2' specified!\n\nAvailable Commands:"
                                 ).arg( KCmdLineArgs::appName(), commandName );

    printHelpAndExit( false, message );                             
  }
  
  return command;
}

void CommandFactory::registerCommands()
{
}

void CommandFactory::checkAndHandleHelp()
{
  const bool userRequestedHelp = mParsedArgs->count() == 1 && mParsedArgs->arg( 0 ) == QLatin1String( "help" );
  
  if ( mParsedArgs->count() == 0 || userRequestedHelp ) {
    KCmdLineArgs::enable_i18n();
    
    const QString message = userRequestedHelp
      ? i18nc( "@info:shell", "<application>%1</application>: Available Commands:" ).arg( KCmdLineArgs::appName() )
      : i18nc( "@info:shell", "<application>%1</application>: No command specified!\n\nAvailable Commands:" ).arg( KCmdLineArgs::appName() );
      
    printHelpAndExit( userRequestedHelp, message );
  }
}

void CommandFactory::printHelpAndExit( bool userRequestedHelp, const QString &leadingMessage )
{
  int maxNameLength = 0;
  Q_FOREACH( const QString &commandName, mCommands.keys() ) {
    maxNameLength = qMax( maxNameLength, commandName.length() );
  }
  
  // if the user requested help output to stdout,
  // otherwise we are missing the mandatory command argument and output to stderr
  std::ostream &stream = userRequestedHelp ? std::cout : std::cerr;
  
  const QString linePattern = QLatin1String( "\t%1\t\2" );
  
  stream << leadingMessage.toLocal8Bit().constData() << std::endl << std::endl;
  
  QHash<QString, AbstractCommand*>::const_iterator it    = mCommands.constBegin();
  QHash<QString, AbstractCommand*>::const_iterator endIt = mCommands.constEnd();
  for (; it != endIt; ++it ) {
    stream << linePattern.arg( it.key().leftJustified( maxNameLength, QLatin1Char( ' ' ) ),
                               it.value()->shortHelp() ).toLocal8Bit().constData()
           << std::endl;
  }
  
  ::exit( userRequestedHelp ? AbstractCommand::NoError : AbstractCommand::InvalidUsage );
}
