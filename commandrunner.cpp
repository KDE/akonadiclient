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

#include "commandrunner.h"

#include "abstractcommand.h"

#include <KCmdLineArgs>
#include <KDebug>

#include <QCoreApplication>

CommandRunner::CommandRunner( KCmdLineArgs *parsedArgs )
  : mApplication( 0 ),
    mCommand( 0 )
{
}

CommandRunner::~CommandRunner()
{
  delete mApplication;
}

int CommandRunner::exec()
{
  if ( mApplication && mCommand ) {
    QMetaObject::invokeMethod( mCommand, "start", Qt::QueuedConnection );
    return mApplication->exec();
  }
  
  return AbstractCommand::InvalidUsage;
}

void CommandRunner::onCommandFinished( int exitCode )
{
  mApplication->exit( exitCode );
}

void CommandRunner::onCommandError( const QString &error )
{
  kError() << error;
}
