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

#include "listcommand.h"

#include <Akonadi/CollectionFetchJob>
#include <Akonadi/ItemFetchJob>

#include <akonadi/private/collectionpathresolver_p.h>

#include <KCmdLineOptions>
#include <KUrl>

#include <iostream>

using namespace Akonadi;

ListCommand::ListCommand( QObject *parent )
  : AbstractCommand( parent )
{
  mShortHelp = ki18nc( "@info:shell", "Lists sub collections and items in a specified collection" ).toString();
}

ListCommand::~ListCommand()
{
}

void ListCommand::start()
{
  if ( mCollection.isValid() ) {
    fetchBase();
    return;
  }
  
  CollectionPathResolver *resolver = new CollectionPathResolver( mPath, this );
  connect( resolver, SIGNAL(result(KJob*)), this, SLOT(onPathResolved(KJob*)) );
  resolver->start();
}

void ListCommand::setupCommandOptions( KCmdLineOptions &options )
{
  options.add( "+list", ki18nc( "@info:shell", "The name of the command" ) );
  options.add( "+collection", ki18nc( "@info:shell", "The collection to list, either as a path or akonadi URL" ) );
}

int ListCommand::initCommand( KCmdLineArgs *parsedArgs )
{
  if ( parsedArgs->count() < 2 ) {
    emit error( ki18nc( "@info:shell",
                         "Missing collection argument. See <application>%1</application> help list'" ).subs( KCmdLineArgs::appName()
                       ).toString()
              );
    return InvalidUsage;
  }
  
  // check if we have an Akonadi URL. if not check if we have a path
  const QString collectionArg = parsedArgs->arg( 1 );
  const QUrl url = parsedArgs->url( 1 );
  if ( url.isValid() && url.scheme() == QLatin1String( "akonadi" ) ) {
    mCollection = Collection::fromUrl( url );
  } else {
    if ( collectionArg.startsWith( CollectionPathResolver::pathDelimiter() ) ) {
      mPath = collectionArg;
    }
  }
    
  if ( !mCollection.isValid() && mPath.isEmpty() ) {
    emit error( ki18nc( "@info:shell",
                         "Invalid collection argument '%1. See <application>%2</application> help list'" ).subs( collectionArg )
                                                                                                          .subs( KCmdLineArgs::appName()
                       ).toString()
              );

    return InvalidUsage;
  }
  
  return NoError;
}

void ListCommand::fetchBase()
{
  if ( mCollection == Collection::root() ) {
    fetchCollections();
    return;
  }
  
  CollectionFetchJob *job = new CollectionFetchJob( mCollection, CollectionFetchJob::Base, this );
  connect( job, SIGNAL(result(KJob*)), this, SLOT(onBaseFetched(KJob*)) );
}

void ListCommand::fetchCollections()
{
  Q_ASSERT( mCollection.isValid() );
  
  CollectionFetchJob *job = new CollectionFetchJob( mCollection, CollectionFetchJob::FirstLevel, this );
  connect( job, SIGNAL(result(KJob*)), this, SLOT(onCollectionsFetched(KJob*)) );
}

void ListCommand::fetchItems()
{
  Q_ASSERT( mCollection.isValid() );
  
  // only attempt item listing if collection has non-collection content MIME types
  QStringList contentMimeTypes = mCollection.contentMimeTypes();
  contentMimeTypes.removeAll( Collection::mimeType() );
  if ( !contentMimeTypes.isEmpty() ) { 
    ItemFetchJob *job = new ItemFetchJob( mCollection, this );
    connect( job, SIGNAL(result(KJob*)), this, SLOT(onItemsFetched(KJob*)) );
  } else {
    emit finished( NoError );
  }
}

void ListCommand::onPathResolved( KJob *job )
{
  if ( job->error() != 0 ) {
    emit error( job->errorString() );
    emit finished( -1 ); // TODO correct error code
    return;
  }

  CollectionPathResolver *resolver = qobject_cast<CollectionPathResolver*>( job );
  Q_ASSERT( resolver != 0 );
  
  mCollection = Collection( resolver->collection() );
  fetchBase();
}

void ListCommand::onBaseFetched(KJob* job)
{
  if ( job->error() != 0 ) {
    emit error( job->errorString() );
    emit finished( -1 ); // TODO correct error code
    return;
  }
  
  CollectionFetchJob *fetchJob = qobject_cast<CollectionFetchJob*>( job );
  Q_ASSERT( fetchJob != 0 );
  
  const Collection::List collections = fetchJob->collections();
  if ( collections.isEmpty() ) {
    emit error( job->errorString() );
    emit finished( -1 ); // TODO correct error code
    return;
  }

  mCollection = fetchJob->collections().first();
  
  fetchCollections();
}

void ListCommand::onCollectionsFetched( KJob *job )
{
  if ( job->error() != 0 ) {
    emit error( job->errorString() );
    emit finished( -1 ); // TODO correct error code
    return;
  }

  CollectionFetchJob *fetchJob = qobject_cast<CollectionFetchJob*>( job );
  Q_ASSERT( fetchJob != 0 );
  
  std::cout << i18nc( "@info:shell output section header", "Collections:" ).toLocal8Bit().constData() << std::endl;
  const Collection::List collections = fetchJob->collections();
  Q_FOREACH ( const Collection &collection, collections ) {
    std::cout << '\t' << collection.name().toLocal8Bit().constData() << std::endl; 
  }
  
  fetchItems();
}

void ListCommand::onItemsFetched(KJob* job)
{
  if ( job->error() != 0 ) {
    emit error( job->errorString() );
    emit finished( -1 ); // TODO correct error code
    return;
  }

  ItemFetchJob *fetchJob = qobject_cast<ItemFetchJob*>( job );
  Q_ASSERT( fetchJob != 0 );
  
  std::cout << i18nc( "@info:shell output section header", "Items:" ).toLocal8Bit().constData() << std::endl;
  const Item::List items = fetchJob->items();
  Q_FOREACH ( const Item &item, items ) {
    std::cout << '\t' << item.id() << std::endl; 
  }  
  
  emit finished( NoError );
}
