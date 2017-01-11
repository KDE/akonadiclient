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
#include "errorreporter.h"

#include <AkonadiCore/CollectionCreateJob>
#include <AkonadiCore/CollectionFetchJob>
#include <AkonadiCore/Item>
#include <AkonadiCore/ItemCreateJob>

#include <KCmdLineOptions>
#include <KGlobal>
#include <KUrl>
#include <KDebug>
#include <KMimeType>

#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <iostream>

#include "commandfactory.h"

using namespace Akonadi;


DEFINE_COMMAND("add", AddCommand, "Add items to a collection");


AddCommand::AddCommand( QObject *parent )
  : AbstractCommand( parent ),
    mResolveJob( 0 )
{
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

  addOptionsOption(options);
  options.add( "+collection", ki18nc( "@info:shell", "The collection to add to, either as a path or akonadi URL" ) );
  options.add( "+files...", ki18nc( "@info:shell", "The files or directories to add to the collection." ) );
  addOptionSeparator(options);
  options.add("b").add("base <dir>", ki18nc("@info:shell", "Base directory for input files/directories, default is current"));
  options.add("f").add("flat", ki18nc("@info:shell", "Flat mode, do not duplicate subdirectory structure"));
  options.add("m").add("mime <mime-type>", ki18nc("@info:shell", "MIME type to use (instead of auto-detection)"));
  addDryRunOption(options);
}

int AddCommand::initCommand( KCmdLineArgs *parsedArgs )
{
  if ( parsedArgs->count() < 2 ) {
    emitErrorSeeHelp( ki18nc( "@info:shell", "Missing collection argument" ) );
    return InvalidUsage;
  }

  if ( parsedArgs->count() < 3 ) {
    emitErrorSeeHelp( ki18nc( "@info:shell", "No file or directory arguments" ) );
    return InvalidUsage;
  }

  const QString collectionArg = parsedArgs->arg( 1 );
  mResolveJob = new CollectionResolveJob( collectionArg, this );

  if ( !mResolveJob->hasUsableInput() ) {
    emit error( ki18nc( "@info:shell",
                        "Invalid collection argument '%1', %2" )
                .subs( collectionArg )
                .subs( mResolveJob->errorString() ).toString() );
    delete mResolveJob;
    mResolveJob = 0;

    return InvalidUsage;
  }

  mFlatMode = parsedArgs->isSet( "flat" );
  mDryRun = parsedArgs->isSet( "dryrun" );

  const QString mimeTypeArg = parsedArgs->getOption( "mime" );
  if ( !mimeTypeArg.isEmpty() ) {
    mMimeType = KMimeType::mimeType( mimeTypeArg );
    if ( mMimeType.isNull() /*|| !mMimeType->isValid() FIXME */ ) {
        emit error( ki18nc( "@info:shell",
                            "Invalid MIME type argument '%1'").subs( mimeTypeArg ).toString() );
        return InvalidUsage;
    }
  }

  mBasePath = parsedArgs->getOption( "base" );
  if ( !mBasePath.isEmpty() ) {				// base is specified
    QDir dir(mBasePath);
    if ( !dir.exists() )
    {
      emit error( ki18nc( "@info:shell",
                          "Base directory '%1' not found" ).subs( mBasePath ).toString() );
      return InvalidUsage;
    }
    mBasePath = dir.absolutePath();
  }
  else {						// base is not specified
    mBasePath = QDir::currentPath();
  }

  for ( int i = 2; i < parsedArgs->count(); ++i ) {
    QString path = parsedArgs->arg( i );
    while (path.endsWith( QLatin1Char( '/' ) ) ) {	// gives null collection name later
      path.chop(1);
    }

    QFileInfo fileInfo( path );
    if ( fileInfo.isRelative() ) {
        fileInfo.setFile( mBasePath, path );
    }

    if ( !fileInfo.exists() ) {
        emit error( i18n( "File '%1' does not exist", path ) );
        return InvalidUsage;
    } else if ( !fileInfo.isReadable() ) {
        emit error( i18n( "Error accessing file '%1'", path ) );
        return InvalidUsage;
    }

    const QString absolutePath = fileInfo.absoluteFilePath();

    if ( fileInfo.isDir() ) {
      mDirectories[ absolutePath ] = AddRecursive;
    } else {
      mDirectories[ fileInfo.absolutePath() ] = AddDirOnly;
      mFiles.insert( absolutePath );
    }

    if ( absolutePath.startsWith( mBasePath ) ) {
      if ( fileInfo.isDir() ) {
        mBasePaths.insert( absolutePath, fileInfo.absoluteFilePath() );
      } else {
        mBasePaths.insert( absolutePath, fileInfo.absolutePath() );
      }
    } else {
      if ( fileInfo.isFile() ) {
          mBasePaths.insert( fileInfo.absolutePath(), mBasePath );
      }
      mBasePaths.insert( absolutePath, mBasePath );
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
    ErrorReporter::progress( i18n( "No more directories to process" ) );
    processNextFile();
    return;
  }

  const QMap<QString, AddDirectoryMode>::iterator directoriesBegin = mDirectories.begin();
  const QString path = directoriesBegin.key();
  const AddDirectoryMode mode = directoriesBegin.value();
  mDirectories.erase( directoriesBegin );

  if ( mFlatMode ) {
    mCollectionsByPath[ path ] = mBaseCollection;
  }

  if ( mCollectionsByPath.value( mBasePaths[ path ] ).isValid() ) {
    if ( mode == AddDirOnly ) {
      // already added
      QMetaObject::invokeMethod( this, "processNextDirectory", Qt::QueuedConnection );
      return;
    }

    // exists but needs recursion and items
    QDir dir( path );
    if ( !dir.exists() ) {
      ErrorReporter::warning( i18n( "Directory <filename>%1</filename> no longer exists", path ) );
      QMetaObject::invokeMethod( this, "processNextDirectory", Qt::QueuedConnection );
      return;
    }

    const QFileInfoList children = dir.entryInfoList( QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot );
    Q_FOREACH ( const QFileInfo &fileInfo, children ) {
      if ( fileInfo.isDir() ) {
        mDirectories[ fileInfo.absoluteFilePath() ] = AddRecursive;
        mBasePaths[ fileInfo.absoluteFilePath() ] = fileInfo.absoluteFilePath();
      } else {
        mFiles.insert( fileInfo.absoluteFilePath() );
        mBasePaths[ fileInfo.absoluteFilePath() ] = fileInfo.absolutePath();
      }
    }

    QMetaObject::invokeMethod( this, "processNextDirectory", Qt::QueuedConnection );
    return;
  }

  QDir dir( path );
  if ( !dir.exists() ) {
    ErrorReporter::warning( i18n( "Directory <filename>%1</filename> no longer exists", path ) );
    QMetaObject::invokeMethod( this, "processNextDirectory", Qt::QueuedConnection );
    return;
  }

  // re-examine again later
  mDirectories[ path ] = mode;

  dir.cdUp();

  const Collection parent = mCollectionsByPath.value( mBasePaths[ dir.absolutePath() ] );
  if ( parent.isValid() ) {
    Collection collection;
    collection.setName( QFileInfo( path ).fileName() );
    collection.setParentCollection ( parent ); // set parent
    collection.setContentMimeTypes( parent.contentMimeTypes() );// "inherit" mime types from parent

    ErrorReporter::progress( i18n( "Fetching collection \"%3\" in parent %1 \"%2\"",
                                   QString::number( parent.id() ), parent.name(), collection.name() ) );

    CollectionFetchJob *job = new CollectionFetchJob( parent, CollectionFetchJob::FirstLevel );
    job->setProperty( "path", path );
    job->setProperty( "collection", QVariant::fromValue( collection ) );
    connect( job, SIGNAL(result(KJob*)), this, SLOT(onCollectionFetched(KJob*)) );
    return;
  }

  // parent doesn't exist, generate parent chain creation entries
  while ( !mCollectionsByPath.value( mBasePaths[ dir.absolutePath() ] ).isValid() ) {
    ErrorReporter::progress( i18n( "Need to create collection for '%1'",
                                   QDir( mBasePath ).relativeFilePath( dir.absolutePath() ) ) );
    mDirectories[ dir.absolutePath() ] = AddDirOnly;
    mBasePaths[ dir.absolutePath() ] = dir.absolutePath();
    dir.cdUp();
  }

  QMetaObject::invokeMethod( this, "processNextDirectory", Qt::QueuedConnection );
}

void AddCommand::processNextFile()
{
  if ( mFiles.isEmpty() ) {
    ErrorReporter::progress( i18n( "No more files to process" ) );
    emit finished( NoError );
    return;
  }

  const QSet<QString>::iterator filesBegin = mFiles.begin();
  const QString fileName = *filesBegin;
  mFiles.erase( filesBegin );

  QFile file( fileName );
  if ( !file.exists() ) {
    emit error( i18nc( "@info:shell", "File <filename>%1</filename> does not exist", fileName ) );
    QMetaObject::invokeMethod( this, "processNextFile", Qt::QueuedConnection );
    return;
  }

  if ( !file.open( QIODevice::ReadOnly ) ) {
    emit error( i18nc( "@info:shell", "File <filename>%1</filename> cannot be read", fileName ) );
    QMetaObject::invokeMethod( this, "processNextFile", Qt::QueuedConnection );
    return;
  }

  KMimeType::Ptr mimeType = !mMimeType.isNull() ? mMimeType
                                                : KMimeType::findByNameAndContent( fileName, &file );
  if ( mimeType.isNull() /*|| !mimeType->isValid() FIXME*/ ) {
    emit error( i18nc( "@info:shell", "Cannot determine MIME type of file <filename>%1</filename>", fileName ) );
    QMetaObject::invokeMethod( this, "processNextFile", Qt::QueuedConnection );
    return;
  }

  const QFileInfo fileInfo( fileName );

  const Collection parent = mCollectionsByPath.value( mBasePaths[ fileInfo.absolutePath() ]);
  if ( !parent.isValid() ) {
    emit error( i18nc( "@info:shell", "Cannot determine parent collection for file <filename>%1</filename>",
                         QDir( mBasePath ).relativeFilePath( fileName ) ) );
    QMetaObject::invokeMethod( this, "processNextFile", Qt::QueuedConnection );
    return;
  }

  ErrorReporter::progress( i18n( "Creating item in collection %1 \"%2\" from '%3' size %4",
                                 QString::number( parent.id() ), parent.name(),
                                 QDir( mBasePath ).relativeFilePath( fileName ),
                                 KGlobal::locale()->formatByteSize( fileInfo.size() ) ) );
  Item item;
  item.setMimeType( mimeType->name() );

  file.reset();
  item.setPayloadFromData( file.readAll() );

  if ( !mDryRun )
  {
    ItemCreateJob *job = new ItemCreateJob( item, parent );
    job->setProperty( "fileName", fileName );
    connect( job, SIGNAL(result(KJob*)), this, SLOT(onItemCreated(KJob*)) );
  }
  else
  {
    processNextFile();
  }
}

void AddCommand::onTargetFetched( KJob *job )
{
  if ( job->error() != 0 ) {
    emit error( ki18nc( "@info:shell",
                        "Cannot fetch target collection, %1" )
                .subs( job->errorString() ).toString() );
    emit finished( RuntimeError );
    return;
  }

  Q_ASSERT( job == mResolveJob && mResolveJob->collection().isValid() );

  mBaseCollection = mResolveJob->collection();
  mCollectionsByPath[ mBasePath ] = mBaseCollection;
  mBasePaths[ mBasePath ] = mBasePath;

  ErrorReporter::progress( i18n( "Root folder is %1 \"%2\"",
                                 QString::number( mBaseCollection.id() ), mBaseCollection.name() ) );

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

    QFileInfo fileInfo( path );
    mBasePaths[ path ] = fileInfo.absoluteFilePath();

    mCollectionsByPath[ path ] = createJob->collection();
  }

  QMetaObject::invokeMethod( this, "processNextDirectory", Qt::QueuedConnection );
}

void AddCommand::onCollectionFetched( KJob *job )
{
  const QString path = job->property( "path" ).toString();
  Q_ASSERT( !path.isEmpty() );

  Akonadi::Collection newCollection = job->property( "collection" ).value<Collection>();

  CollectionFetchJob *fetchJob = qobject_cast<CollectionFetchJob*>( job );
  Q_ASSERT( fetchJob != 0 );

  bool found = false;
  Collection::List collections = fetchJob->collections();
  Q_FOREACH( const Collection &col, collections ) {
    if ( col.name() == newCollection.name() ) {
        found = true;
        newCollection = col;
    }
  }

  if ( !found ) {
    if ( mFlatMode ) {					// not creating any collections
      ErrorReporter::error( i18n( "Error fetching collection %1 \"%2\", %3",
                                  QString::number( newCollection.id() ), newCollection.name(),
                                  job->errorString() ) );
      QMetaObject::invokeMethod( this, "processNextDirectory", Qt::QueuedConnection );
      return;
    }

    // no such collection, try creating it
    QString name = newCollection.name();
    // Workaround for bug 319513
    if ( ( name == "cur" ) || ( name == "new" ) || ( name == "tmp" ) ) {
      QString parentResource = newCollection.parentCollection().resource();
      if ( parentResource.startsWith( QLatin1String( "akonadi_maildir_resource" ) ) ) {
        name += "_";
        newCollection.setName(name);
        ErrorReporter::warning( i18n( "Changed maildir folder name to '%1'", name ) );
      }
    }

    if (!mDryRun)
    {
      CollectionCreateJob *createJob = new CollectionCreateJob( newCollection );
      createJob->setProperty( "path", path );

      Akonadi::Collection parent = newCollection.parentCollection();
      ErrorReporter::progress( i18n( "Creating collection \"%3\" under parent %1 \"%2\"",
				    QString::number( parent.id() ), parent.name(),
				    newCollection.name() ) );

      connect( createJob, SIGNAL(result(KJob*)), this, SLOT(onCollectionCreated(KJob*)) );
    }
    else
    {
      QMetaObject::invokeMethod( this, "processNextDirectory", Qt::QueuedConnection );
    }
    return;
  }

  mCollectionsByPath[ path ] = newCollection;

  QMetaObject::invokeMethod( this, "processNextDirectory", Qt::QueuedConnection );
}

void AddCommand::onItemCreated( KJob *job )
{
  const QString fileName = job->property( "fileName" ).toString();

  if ( job->error() != 0 ) {
    const QString msg = i18nc( "@info:shell", "Failed to add <filename>%1</filename>, %2", fileName, job->errorString() );
    emit error( msg );
  } else {

    ItemCreateJob *createJob = qobject_cast<ItemCreateJob *>( job );
    Q_ASSERT( createJob != 0 );

    ErrorReporter::progress( i18n( "Added file '%2' as item %1",
                                   QString::number( createJob->item().id() ),
                                   QDir( mBasePath ).relativeFilePath( fileName ) ) );
  }

  processNextFile();
}
