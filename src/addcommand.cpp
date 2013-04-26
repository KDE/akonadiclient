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

#include "addcommand.h"

#include "collectionresolvejob.h"

#include <Akonadi/Collection>
#include <Akonadi/CollectionCreateJob>
#include <Akonadi/CollectionFetchJob>
#include <Akonadi/Item>
#include <Akonadi/ItemCreateJob>

#include <KCmdLineOptions>
#include <KMimeType>
#include <KUrl>

#include <QDir>
#include <QFile>
#include <QFileInfo>

using namespace Akonadi;

AddCommand::AddCommand( QObject *parent )
  : AbstractCommand( parent ),
    mResolveJob( 0 )
{
  mShortHelp = ki18nc( "@info:shell", "Add items to a specified collection" ).toString();
}

AddCommand::~AddCommand()
{
}

void AddCommand::start()
{
  Q_ASSERT( mResolveJob != 0 );
  
  connect( mResolveJob, SIGNAL(result(KJob*)), this, SLOT(onTargetFetched(KJob*)) );
  mResolveJob->start();  
}

void AddCommand::setupCommandOptions( KCmdLineOptions &options )
{
  AbstractCommand::setupCommandOptions( options );
  options.add( "+collection", ki18nc( "@info:shell", "The collection to add to, either as a path or akonadi URL" ) );
  options.add( "+files...", ki18nc( "@info:shell", "The files to add to the collection." ) );
}

int AddCommand::initCommand( KCmdLineArgs *parsedArgs )
{
  if ( parsedArgs->count() < 2 ) {
    emitErrorSeeHelp( ki18nc( "@info:shell", "Missing collection argument" ) );
    return InvalidUsage;
  }
  
  if ( parsedArgs->count() < 3 ) {
    emitErrorSeeHelp( ki18nc( "@info:shell", "Missing file argument" ) );
    return InvalidUsage;
  }
  
  const QString collectionArg = parsedArgs->arg( 1 );
  mResolveJob = new CollectionResolveJob( collectionArg, this );
    
  if ( !mResolveJob->hasUsableInput() ) {
    emit error( mResolveJob->errorString() );
    delete mResolveJob;
    mResolveJob = 0;
    
    return InvalidUsage;
  }
  
  const QString currentPath = QDir::currentPath();
  
  for ( int i = 2; i < parsedArgs->count(); ++i ) {
    const QFileInfo fileInfo( parsedArgs->arg( i ) );
    
    const QString absolutePath = fileInfo.absoluteFilePath();
    if ( !absolutePath.startsWith( currentPath ) ) {
      if ( fileInfo.isDir() ) {
        emit error( ki18nc( "@info:shell",
                            "Invalid directory argument '%1'. Needs to be a path in or below '%2'" ).subs( parsedArgs->arg( i ) )
                                                                                                     .subs( currentPath ).toString() );
      } else {
        emit error( ki18nc( "@info:shell",
                            "Invalid file argument '%1'. Needs to be on a path in or below '%2'"  ).subs( parsedArgs->arg( i ) )
                                                                                                    .subs( currentPath ).toString() );
      }
    } else {
      if ( fileInfo.isDir() ) {
        mDirectories[ absolutePath ] = AddRecursive;
      } else{
        mDirectories[ fileInfo.absolutePath() ] = AddDirOnly;
        mFiles.insert( absolutePath );
      }
    }
  }
  
  if ( mFiles.isEmpty() && mDirectories.isEmpty() ) {
    emitErrorSeeHelp( ki18nc( "@info:shell", "No valid file or directory arguments" ) );
    return InvalidUsage;    
  }
  
  return NoError;
}

void AddCommand::processNextDirectory()
{
  if ( mDirectories.isEmpty() ) {
    processNextFile();
    return;
  }
  
  const QMap<QString, AddDirectoryMode>::iterator directoriesBegin = mDirectories.begin();
  const QString path = directoriesBegin.key();
  const AddDirectoryMode mode = directoriesBegin.value();
  mDirectories.erase( directoriesBegin );
  
  if ( mCollectionsByPath.value( path ).isValid() ) {
    if ( mode == AddDirOnly ) {
      // already added
      QMetaObject::invokeMethod( this, "processNextDirectory", Qt::QueuedConnection );
      return;
    }
    
    // exists but needs recursion and items
    QDir dir( path );
    if ( !dir.exists() ) {
      kWarning() << "Directory" << path << "does no longer exist";
      QMetaObject::invokeMethod( this, "processNextDirectory", Qt::QueuedConnection );
      return;      
    }
    
    const QFileInfoList children = dir.entryInfoList( QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot );
    Q_FOREACH ( const QFileInfo &fileInfo, children ) {
      if ( fileInfo.isDir() ) {
        mDirectories[ fileInfo.absoluteFilePath() ] = AddRecursive;
      } else {
        mFiles.insert( fileInfo.absoluteFilePath() );
      }
    }
    
    QMetaObject::invokeMethod( this, "processNextDirectory", Qt::QueuedConnection );
    return;    
  }
  
  QDir dir( path );
  if ( !dir.exists() ) {
    kWarning() << "Directory" << path << "does no longer exist";
    QMetaObject::invokeMethod( this, "processNextDirectory", Qt::QueuedConnection );
    return;      
  }
  
  // re-examine again later
  mDirectories[ path ] = mode;
  
  dir.cdUp();
  
  const Collection parent = mCollectionsByPath.value( dir.absolutePath() );
  if ( parent.isValid() ) {
    Collection collection;
    collection.setName( QFileInfo( path ).fileName() );
    collection.setParent( parent ); // set parent
    collection.setContentMimeTypes( parent.contentMimeTypes() );// "inherit" mime types from parent
    
    CollectionFetchJob *job = new CollectionFetchJob( collection, CollectionFetchJob::Base );
    job->setProperty( "path", path );
    job->setProperty( "collection", QVariant::fromValue( collection ) );
    connect( job, SIGNAL(result(KJob*)), this, SLOT(onCollectionFetched(KJob*)) );
    return;
  }
  
  // parent doesn't exist, generate parent chain creation entries
  while ( !mCollectionsByPath.value( dir.absolutePath() ).isValid() ) {
    mDirectories[ dir.absolutePath() ] = AddDirOnly;
    dir.cdUp();
  }
  
  QMetaObject::invokeMethod( this, "processNextDirectory", Qt::QueuedConnection );
}

void AddCommand::processNextFile()
{
  if ( mFiles.isEmpty() ) {
    emit finished( NoError );
    return;
  }
  
  const QSet<QString>::iterator filesBegin = mFiles.begin();
  const QString fileName = *filesBegin;
  mFiles.erase( filesBegin );
  
  QFile file( fileName );
  if ( !file.exists() ) {
    emit error( i18nc( "@info:shell", "File <filename>%1</filename> does not exist" ).arg( fileName ) );
    QMetaObject::invokeMethod( this, "processNextFile", Qt::QueuedConnection );
    return;
  }
  
  if ( !file.open( QIODevice::ReadOnly ) ) {
    emit error( i18nc( "@info:shell", "File <filename>%1</filename> cannot be read" ).arg( fileName ) );
    QMetaObject::invokeMethod( this, "processNextFile", Qt::QueuedConnection );
    return;
  }
  
  const KMimeType::Ptr mimeType = KMimeType::findByNameAndContent( fileName, &file );
  if ( !mimeType->isValid() ) {
    emit error( i18nc( "@info:shell", "Cannot determine MIME type of file <filename>%1</filename>" ).arg( fileName ) );
    QMetaObject::invokeMethod( this, "processNextFile", Qt::QueuedConnection );
    return;
  }

  const QFileInfo fileInfo( fileName );
  
  const Collection parent = mCollectionsByPath.value( fileInfo.absolutePath() );
  if ( !parent.isValid() ) {      
    emit error( i18nc( "@info:shell", "Cannot determine parent collection for file <filename>%1</filename>" ).arg( fileName ) );
    QMetaObject::invokeMethod( this, "processNextFile", Qt::QueuedConnection );
    return;
  }
  
  kDebug() << "file=" << fileName << "mime=" << mimeType->name() << "parent's contentMimes=" << parent.contentMimeTypes();

  Item item;
  item.setMimeType( mimeType->name() );
  
  file.reset();
  item.setPayloadFromData( file.readAll() );
  
  ItemCreateJob *job = new ItemCreateJob( item, parent );
  job->setProperty( "fileName", fileName );
  connect( job, SIGNAL(result(KJob*)), this, SLOT(onItemCreated(KJob*)) );
}

void AddCommand::onTargetFetched( KJob *job )
{
  if ( job->error() != 0 ) {
    emit error( job->errorString() );
    emit finished( -1 ); // TODO correct error code
    return;
  }
  
  Q_ASSERT( job == mResolveJob && mResolveJob->collection().isValid() );
  
  mCollectionsByPath[ QDir::currentPath() ] = mResolveJob->collection();
  
  processNextDirectory();
}

void AddCommand::onCollectionCreated( KJob *job )
{
  const QString path = job->property( "path" ).toString();
  Q_ASSERT( !path.isEmpty() );
    
  if ( job->error() != 0 ) {
    kWarning() << "error=" << job->error() << "errorString=" << job->errorString();
    
    mDirectories.remove( path );
  } else {
    CollectionCreateJob *createJob = qobject_cast<CollectionCreateJob*>( job );
    Q_ASSERT( createJob != 0 );
  
    mCollectionsByPath[ path ] = createJob->collection();
  }
  
  QMetaObject::invokeMethod( this, "processNextDirectory", Qt::QueuedConnection );
}

void AddCommand::onCollectionFetched( KJob *job )
{
  const QString path = job->property( "path" ).toString();
  Q_ASSERT( !path.isEmpty() );
  
  if ( job->error() != 0 ) {
    // no such collection, try creating it
    CollectionCreateJob *createJob = new CollectionCreateJob( job->property( "collection" ).value<Collection>() );
    createJob->setProperty( "path", path );
    connect( createJob, SIGNAL(result(KJob*)), this, SLOT(onCollectionCreated(KJob*)) );
    return;    
  }
  
  CollectionFetchJob *fetchJob = qobject_cast<CollectionFetchJob*>( job );
  Q_ASSERT( fetchJob != 0 );  
  Q_ASSERT( !fetchJob->collections().isEmpty() );
  
  mCollectionsByPath[ path ] = fetchJob->collections().first();

  QMetaObject::invokeMethod( this, "processNextDirectory", Qt::QueuedConnection );
}

void AddCommand::onItemCreated( KJob *job )
{
  const QString fileName = job->property( "fileName" ).toString();
  
  if ( job->error() != 0 ) {
    const QString msg = i18nc( "@info:shell", "Failed to add <filename>%1</filename>: %2" ).arg( fileName ). arg( job->errorString() );
    emit error( msg );
  } else {
    kDebug() << "Successfully added file" << fileName; 
  }
  
  processNextFile();
}
