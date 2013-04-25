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

#include <KAboutData>
#include <KCmdLineArgs>
#include <KDebug>

#include <QCoreApplication>

CommandRunner::CommandRunner( const KAboutData &aboutData, KCmdLineArgs *parsedArgs )
  : mApplication( 0 ),
    mCommand( 0 )
{
  CommandFactory factory( parsedArgs );
  
  mCommand = factory.createCommand();
  Q_ASSERT( mCommand != 0 );
  
  connect( mCommand, SIGNAL(error(QString)), this, SLOT(onCommandError(QString)) );
  
  if ( mCommand->init( parsedArgs ) == AbstractCommand::InvalidUsage ) {
    delete mCommand;
    mCommand = 0;
    ::exit( AbstractCommand::InvalidUsage );
  }
  
  connect( mCommand, SIGNAL(finished(int)), this, SLOT(onCommandFinished(int)) );

  // TODO should we allow commands to optionally support GUI?
  mApplication = new QCoreApplication( KCmdLineArgs::qtArgc(), KCmdLineArgs::qtArgv() );
  mApplication->setApplicationName( aboutData.appName() );
  mApplication->setApplicationVersion( aboutData.version() );
  mApplication->setOrganizationDomain( aboutData.organizationDomain() );
}

CommandRunner::~CommandRunner()
{
  delete mCommand;
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
