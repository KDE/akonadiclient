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

#include "collectionresolvejob.h"

#include <Akonadi/CollectionFetchJob>

#include <akonadi/private/collectionpathresolver_p.h>

#include <KLocale>
#include <KUrl>

using namespace Akonadi;

CollectionResolveJob::CollectionResolveJob( const QString &userInput, QObject* parent )
  : KCompositeJob( parent ),
    mUserInput( userInput )
{
  setAutoDelete( false );
  
  // check if we have an Akonadi URL. if not check if we have a path
  const KUrl url = QUrl::fromUserInput( userInput );
  if ( url.isValid() && url.scheme() == QLatin1String( "akonadi" ) ) {
    mCollection = Collection::fromUrl( url );
  }
}

CollectionResolveJob::~CollectionResolveJob()
{
}

void CollectionResolveJob::start()
{
  if ( !hasUsableInput() ) {
    setError( -1 ); // TODO better error code
    setErrorText( i18nc( "@info:shell", "User input \"%1\" cannot be resolved into an Akonadi collection", mUserInput ) );
    emitResult();
    return;
  }

  if ( mCollection.isValid() ) {
    fetchBase();
  } else {
    CollectionPathResolver *resolver = new CollectionPathResolver( mUserInput, this );
    addSubjob( resolver );
    resolver->start();
  }  
}

bool CollectionResolveJob::hasUsableInput() const
{
  return mCollection.isValid() || mUserInput.startsWith( CollectionPathResolver::pathDelimiter() );
}

Collection CollectionResolveJob::collection() const
{
  return mCollection;
}

void CollectionResolveJob::fetchBase()
{
  if ( mCollection == Collection::root() ) {
    emitResult();
    return;
  }
    
  CollectionFetchJob *job = new CollectionFetchJob( mCollection, CollectionFetchJob::Base, this );
  addSubjob( job );
}

void CollectionResolveJob::slotResult( KJob *job )
{
  if ( job->error() == 0 ) {
    CollectionFetchJob *fetchJob = qobject_cast<CollectionFetchJob*>( job );
    if ( fetchJob != 0 ) {
      mCollection = fetchJob->collections().first();      
    } else {
      CollectionPathResolver *resolver = qobject_cast<CollectionPathResolver*>( job );
      mCollection = Collection( resolver->collection() );
      fetchBase();
    }
  }
  
  KCompositeJob::slotResult( job );
  
  if ( !hasSubjobs() ) {
    emitResult();
  }
}
