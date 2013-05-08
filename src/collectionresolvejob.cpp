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

  // First see if user input is a valid integer as a collection ID
  bool ok;
  int id = userInput.toInt( &ok );
  if ( ok ) {						// conversion succeeded
    if ( id == 0 ) mCollection = Collection::root();	// the root collection
    else mCollection = Collection( id );		// the specified collection
  }
  else {
    // Then quickly check for a path of "/", meaning the root
    if ( userInput == QLatin1String( "/" ) )
    {
      mCollection = Collection::root();
    }
    else {
      // Next check if we have an Akonadi URL
      const KUrl url = QUrl::fromUserInput( userInput );
      if ( url.isValid() && url.scheme() == QLatin1String( "akonadi" ) ) {
        mCollection = Collection::fromUrl( url );
      }
    }
  }
  // If neither of these, assume that we have a path
}

CollectionResolveJob::~CollectionResolveJob()
{
}

void CollectionResolveJob::start()
{
  if ( !hasUsableInput() ) {
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

bool CollectionResolveJob::hasUsableInput()
{
  if ( mCollection.isValid() || mUserInput.startsWith( CollectionPathResolver::pathDelimiter() ) ) {
    return true;
  }

  setError( -1 ); // TODO better error code
  setErrorText( i18nc( "@info:shell", "Unknown Akonadi collection format '%1'", mUserInput ) );
  return false;
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

  bool willEmitResult = ( job->error() && !error() );
  // If willEmitResult is true, then emitResult() will be
  // done inside the call of KCompositeJob::slotResult() below.
  // So there will be no need for us to do it again.
  KCompositeJob::slotResult( job );

  if ( !hasSubjobs() && !willEmitResult ) {
    emitResult();
  }
}


QString CollectionResolveJob::formattedCollectionName() const
{
  if ( mCollection == Collection::root() ) {
    return ( i18nc( "@info:shell 1=collection ID",
                    "%1 (root)", mCollection.id() ) );
  }
  else {
    return ( i18nc( "@info:shell 1=collection ID, 2=collection name",
                    "%1 (\"%2\")", mCollection.id(), mCollection.name() ) );
  }
}
