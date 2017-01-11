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

#include "createcommand.h"

#include "collectionresolvejob.h"
#include "collectionpathjob.h"
#include "errorreporter.h"

#include <AkonadiCore/Collection>
#include <AkonadiCore/CollectionCreateJob>
#include <AkonadiCore/CollectionFetchJob>

#include <KCmdLineOptions>

#include <iostream>

#include "commandfactory.h"

using namespace Akonadi;


DEFINE_COMMAND("create", CreateCommand, "Create a new collection");


CreateCommand::CreateCommand(QObject *parent)
  : AbstractCommand(parent),
    mResolveJob(0)
{
}


CreateCommand::~CreateCommand()
{
}


void CreateCommand::start()
{
  Q_ASSERT(mResolveJob!=0);

  connect(mResolveJob, SIGNAL(result(KJob *)), this, SLOT(onTargetFetched(KJob *)));
  mResolveJob->start();
}


void CreateCommand::setupCommandOptions(KCmdLineOptions &options)
{
  AbstractCommand::setupCommandOptions(options);

  addOptionsOption(options);
  options.add( "+collection", ki18nc("@info:shell", "The collection to create, either as a path or a name (with a parent specified)"));
  addOptionSeparator(options);
  options.add("p").add("parent <collection>", ki18nc("@info:shell", "Parent collection to create in"));
  addDryRunOption(options);
}


int CreateCommand::initCommand(KCmdLineArgs *parsedArgs)
{
  if (parsedArgs->count()<2)
  {
    emitErrorSeeHelp(ki18nc("@info:shell", "Missing collection argument"));
    return InvalidUsage;
  }

  const QString collectionArg = parsedArgs->arg(1);

  if (parsedArgs->isSet("parent"))
  {
    // A parent collection is specified.  That must already exist,
    // and the new collection must be a plain name (not containing
    // any '/'es).

    if (collectionArg.contains(QLatin1Char('/')))
    {
      emitErrorSeeHelp(ki18nc("@info:shell", "Collection argument (with parent) cannot be a path"));
      return InvalidUsage;
    }

    mNewCollectionName = collectionArg;
    mParentCollection = parsedArgs->getOption("parent");
  }
  else
  {
    // No parent collection is specified.  The new collection name
    // is the last part of the specified argument, and the remainder
    // is taken as the parent collection which must already exist.
    // Note that this means that an argument like "33/newname" is
    // acceptable, where the number is resolved as a collection ID.
    // Even something like "akonadi://?collection=33/newname" is
    // acceptable.

    int i = collectionArg.lastIndexOf(QLatin1Char('/'));
    if (i==-1)
    {
      emitErrorSeeHelp(ki18nc("@info:shell", "Collection argument (without parent) must be a path"));
      return InvalidUsage;
    }

    mNewCollectionName = collectionArg.mid(i+1);
    mParentCollection = collectionArg.left(i);
  }

  if (mNewCollectionName.isEmpty())
  {
    emitErrorSeeHelp(ki18nc("@info:shell", "New collection name not specified"));
    return InvalidUsage;
  }

  mDryRun = parsedArgs->isSet("dryrun");

  mResolveJob = new CollectionResolveJob(mParentCollection, this);
  if (!mResolveJob->hasUsableInput())
  {
    emit error(ki18nc("@info:shell",
                      "Invalid parent collection '%1', %2")
               .subs(mParentCollection)
               .subs(mResolveJob->errorString()).toString());
    delete mResolveJob;
    mResolveJob = 0;
    return InvalidUsage;
  }

  return NoError;
}


void CreateCommand::onTargetFetched(KJob *job)
{
  if (job->error()!=0)
  {
    emit error(ki18nc("@info:shell",
                      "Cannot fetch parent collection '%1', %2")
               .subs(mParentCollection)
               .subs(job->errorString()).toString());
    emit finished(RuntimeError);
    return;
  }

  Q_ASSERT(job==mResolveJob);
  Akonadi::Collection parentCollection = mResolveJob->collection();
  Q_ASSERT(parentCollection.isValid());

  // Warning for bug 319513
  if ((mNewCollectionName=="cur") || (mNewCollectionName=="new") || (mNewCollectionName=="tmp"))
  {
    QString parentResource = parentCollection.resource();
    if (parentResource.startsWith(QLatin1String("akonadi_maildir_resource")))
    {
      ErrorReporter::warning(i18n("Creating a maildir folder named '%1' may not work",
                                  mNewCollectionName));
    }
  }

  Akonadi::Collection newCollection;
  newCollection.setParentCollection(parentCollection);
  newCollection.setName(mNewCollectionName);
  newCollection.setContentMimeTypes(parentCollection.contentMimeTypes());
  if (!mDryRun)
  {
    CollectionCreateJob *createJob = new CollectionCreateJob(newCollection);
    connect(createJob, SIGNAL(result(KJob *)), this, SLOT(onCollectionCreated(KJob *)));
  }
  else
  {
    emit finished(NoError);
  }
}


void CreateCommand::onCollectionCreated(KJob *job)
{
  if (job->error()!=0)
  {
    Q_ASSERT(mResolveJob!=0);
    ErrorReporter::error(i18n("Error creating collection \"%1\" under \"%2\" , %3",
                              mNewCollectionName,
                              mResolveJob->formattedCollectionName(),
                              job->errorString()));
    emit finished(RuntimeError);
    return;
  }

  CollectionCreateJob *createJob = qobject_cast<CollectionCreateJob *>(job);
  Q_ASSERT(createJob!=0);

  CollectionPathJob *pathJob = new CollectionPathJob(createJob->collection());
  connect(pathJob, SIGNAL(result(KJob *)), SLOT(onPathFetched(KJob *)));
  pathJob->start();
}


void CreateCommand::onPathFetched(KJob *job)
{
  if (job->error()!=0)
  {
    Q_ASSERT(mResolveJob!=0);
    ErrorReporter::error(i18n("Error getting path of new collection, %1", job->errorString()));
    emit finished(RuntimeError);
    return;
  }

  CollectionPathJob *pathJob = qobject_cast<CollectionPathJob *>(job);
  Q_ASSERT(pathJob!=0);

  std::cout << i18n("Created new collection %1",
                    pathJob->formattedCollectionPath()).toLocal8Bit().constData()
            << std::endl;

  emit finished(NoError);
}
