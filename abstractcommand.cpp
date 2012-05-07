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

AbstractCommand::AbstractCommand( QObject *parent )
  : QObject( parent )
{
}

AbstractCommand::~AbstractCommand()
{
}

int AbstractCommand::init( KCmdLineArgs *parsedArgs )
{
  parsedArgs->clear();
  KCmdLineArgs::reset();
  
  KCmdLineOptions options;
  setupCommandOptions( options );
  
  KCmdLineArgs::addCmdLineOptions( options );
  
  KCmdLineArgs *parseCommandArgs = KCmdLineArgs::parsedArgs();
  Q_ASSERT( parseCommandArgs != 0 );
  
  const int result = initCommand( parseCommandArgs );
  
  KCmdLineArgs::reset();
  KCmdLineArgs::addStdCmdLineOptions();
  KCmdLineArgs::addCmdLineOptions( options, ki18nc( "@info:shell", "Options for command" ) );
  
  return result;
}

QString AbstractCommand::shortHelp() const
{
  return mShortHelp;
}

void AbstractCommand::start()
{
  QMetaObject::invokeMethod( this, "finished", Qt::QueuedConnection, Q_ARG( int, NoError ) );
}

void AbstractCommand::setupCommandOptions(KCmdLineOptions& options)
{
  Q_UNUSED( options );
}
