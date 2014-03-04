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
#include "commandfactory.h"
#include "errorreporter.h"

#include <KAboutData>
#include <KCmdLineArgs>
#include <KDebug>

#include <QCoreApplication>

#include <iostream>


CommandRunner::CommandRunner( const KAboutData &aboutData, KCmdLineArgs *parsedArgs )
  : mCommand( 0 ),
    mParsedArgs( parsedArgs ),
    mFactory( parsedArgs )
{
    ErrorReporter::setAppName( aboutData.appName() );
}

CommandRunner::~CommandRunner()
{
  delete mCommand;
}

int CommandRunner::start()
{

  mCommand = mFactory.createCommand();
  Q_ASSERT( mCommand != 0 );

  connect( mCommand, SIGNAL(error(QString)), this, SLOT(onCommandError(QString)) );

  if ( mCommand->init( mParsedArgs ) == AbstractCommand::InvalidUsage ) {
    delete mCommand;
    mCommand = 0;
    return AbstractCommand::InvalidUsage;
  }

  connect( mCommand, SIGNAL(finished(int)), this, SLOT(onCommandFinished(int)) );

  QMetaObject::invokeMethod( mCommand, "start", Qt::QueuedConnection );

  return AbstractCommand::NoError;
}

void CommandRunner::onCommandFinished( int exitCode )
{
  QCoreApplication::exit(exitCode);
}

void CommandRunner::onCommandError( const QString &error )
{
  ErrorReporter::fatal(error);
}
