/*
    Copyright (C) 2013  Jonathan Marten <jjm@keelhaul.me.uk>

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

#include "copycommand.h"

#include "collectionresolvejob.h"
#include "collectionpathjob.h"
#include "errorreporter.h"

#include <AkonadiCore/CollectionCopyJob>
#include <AkonadiCore/CollectionMoveJob>
#include <AkonadiCore/CollectionFetchJob>
#include <AkonadiCore/ItemCopyJob>
#include <AkonadiCore/ItemMoveJob>
#include <AkonadiCore/ItemFetchJob>
#include <AkonadiCore/ItemFetchScope>

#include <KCmdLineOptions>
#include <KUrl>

#include <iostream>

#include "commandfactory.h"

using namespace Akonadi;

DEFINE_COMMAND("copy", CopyCommand, "Copy collections or items into a new collection");

CopyCommand::CopyCommand(QObject *parent)
    : AbstractCommand(parent),
      mResolveJob(nullptr)
{
    mMoving = false;
}

CopyCommand::~CopyCommand()
{
}

void CopyCommand::start()
{
    Q_ASSERT(mResolveJob != nullptr);

    mAnyErrors = false;                   // not yet, anyway

    connect(mResolveJob, SIGNAL(result(KJob*)), this, SLOT(onTargetFetched(KJob*)));
    mResolveJob->start();
}

void CopyCommand::setupCommandOptions(KCmdLineOptions &options)
{
    AbstractCommand::setupCommandOptions(options);

    addOptionsOption(options);
    options.add("+source...", ki18nc("@info:shell", "Existing collections or items to copy"));
    options.add("+destination", ki18nc("@info:shell", "Destination collection to copy into"));
    addOptionSeparator(options);
    addDryRunOption(options);
}

int CopyCommand::initCommand(KCmdLineArgs *parsedArgs)
{
    if (parsedArgs->count() < 3) {
        emitErrorSeeHelp(ki18nc("@info:shell", "Missing source/destination arguments"));
        return (InvalidUsage);
    }

    for (int i = 1; i < parsedArgs->count(); ++i) {   // save all the arguments
        // apart from command name
        mSourceArgs.append(parsedArgs->arg(i));
    }
    mDestinationArg = mSourceArgs.takeLast();     // extract the destination
    Q_ASSERT(!mSourceArgs.isEmpty());         // must have some left

    mDryRun = parsedArgs->isSet("dryrun");

    mResolveJob = new CollectionResolveJob(mDestinationArg, this);
    if (!mResolveJob->hasUsableInput()) {
        emit error(ki18nc("@info:shell",
                          "Invalid destination collection '%1', %2")
                   .subs(mDestinationArg)
                   .subs(mResolveJob->errorString()).toString());
        delete mResolveJob;
        mResolveJob = nullptr;
        return (InvalidUsage);
    }

    return (NoError);
}

void CopyCommand::onTargetFetched(KJob *job)
{
    if (job->error() != 0) {
        emit error(ki18nc("@info:shell",
                          "Cannot resolve destination collection '%1', %2")
                   .subs(mDestinationArg)
                   .subs(job->errorString()).toString());
        emit finished(RuntimeError);
        return;
    }

    Q_ASSERT(job == mResolveJob);
    mDestinationCollection = mResolveJob->collection();
    Q_ASSERT(mDestinationCollection.isValid());

    doNextSource();
}

inline void CopyCommand::doNextSource()
{
    QMetaObject::invokeMethod(this, "processNextSource", Qt::QueuedConnection);
}

void CopyCommand::processNextSource()
{
    if (mSourceArgs.isEmpty()) {              // no more to do
        ErrorReporter::progress(i18n("No more sources to process"));
        emit finished(!mAnyErrors ? NoError : RuntimeError);
        return;
    }

    const QString sourceArg = mSourceArgs.takeFirst();

    CollectionResolveJob *sourceJob = new CollectionResolveJob(sourceArg, this);
    sourceJob->setProperty("arg", sourceArg);
    connect(sourceJob, SIGNAL(result(KJob*)), SLOT(onSourceResolved(KJob*)));
    sourceJob->start();
}

void CopyCommand::onSourceResolved(KJob *job)
{
    Q_ASSERT(mResolveJob != nullptr);
    Q_ASSERT(mDestinationCollection.isValid());

    const QString sourceArg = job->property("arg").toString();
    if (job->error() != 0) {
        Item item;
        if (job->error() == Akonadi::Job::Unknown) {    // failed to resolve as collection
            // try as item instead
            item = CollectionResolveJob::parseItem(sourceArg);
        }

        if (!item.isValid()) {              // couldn't parse as item either
            ErrorReporter::error(i18nc("@info:shell", "Cannot resolve source '%1', %2",
                                       sourceArg, job->errorString()));
            mAnyErrors = true;                // note for exit status
            doNextSource();
            return;
        }

        ItemFetchJob *fetchJob = new ItemFetchJob(item, this);
        fetchJob->fetchScope().fetchFullPayload(false);
        fetchJob->setProperty("arg", sourceArg);
        connect(fetchJob, SIGNAL(result(KJob*)), SLOT(onItemsFetched(KJob*)));
        return;
    }

    CollectionResolveJob *sourceJob = qobject_cast<CollectionResolveJob *>(job);
    Q_ASSERT(sourceJob != nullptr);
    Akonadi::Collection sourceCollection = sourceJob->collection();

    if (sourceJob->hadSlash()) {
        // The source is specified as a collection that ends with a '/'.
        // This means that the contents of the source collection
        // (both items and sub-collections) are to be copied into
        // the destination collection, losing the original collection
        // name.  This interpretation is the same as that of the
        // source argument to rsync(1).

        if (mMoving) {
            ErrorReporter::progress(i18n("Moving contents of %1 -> %2",
                                         sourceJob->formattedCollectionName(),
                                         mResolveJob->formattedCollectionName()));
        } else {
            ErrorReporter::progress(i18n("Copying contents of %1 -> %2",
                                         sourceJob->formattedCollectionName(),
                                         mResolveJob->formattedCollectionName()));
        }

        mSourceCollection = sourceJob->collection();
        CollectionFetchJob *fetchJob = new CollectionFetchJob(mSourceCollection,
                CollectionFetchJob::FirstLevel, this);
        fetchJob->setProperty("arg", sourceArg);
        connect(fetchJob, SIGNAL(result(KJob*)), SLOT(onCollectionsFetched(KJob*)));
    } else {
        // The source is a collection that does not end with a '/'.
        // This means that the entire source collection is to be copied
        // recursively into the destination collection, under the
        // original collection name.  This case is simpler!

        Akonadi::Job *job;
        if (mMoving) {
            ErrorReporter::progress(i18n("Moving collection %1 -> %2",
                                         sourceJob->formattedCollectionName(),
                                         mResolveJob->formattedCollectionName()));
            if (!mDryRun) {
                job = new CollectionMoveJob(sourceCollection, mDestinationCollection, this);
            }
        } else {
            ErrorReporter::progress(i18n("Copying collection %1 -> %2",
                                         sourceJob->formattedCollectionName(),
                                         mResolveJob->formattedCollectionName()));

            if (!mDryRun) {
                job = new CollectionCopyJob(sourceCollection, mDestinationCollection, this);
            }
        }

        if (!mDryRun) {
            job->setProperty("arg", sourceArg);
            connect(job, SIGNAL(result(KJob*)), SLOT(onRecursiveCopyFinished(KJob*)));
        } else {
            doNextSource();
        }
    }
}

void CopyCommand::onRecursiveCopyFinished(KJob *job)
{
    const QString sourceArg = job->property("arg").toString();
    if (job->error() != 0) {
        ErrorReporter::error(ki18nc("@info:shell",
                                    "Cannot copy/move from '%1', %2")
                             .subs(sourceArg)
                             .subs(job->errorString()).toString());
        mAnyErrors = true;                  // note for exit status
    }

    doNextSource();
}

void CopyCommand::onCollectionsFetched(KJob *job)
{
    const QString sourceArg = job->property("arg").toString();
    if (job->error() != 0) {
        ErrorReporter::error(ki18nc("@info:shell",
                                    "Cannot fetch subcollections of '%1', %2")
                             .subs(sourceArg)
                             .subs(job->errorString()).toString());
        mAnyErrors = true;                  // note for exit status
        doNextSource();
        return;
    }

    CollectionFetchJob *fetchJob = qobject_cast<CollectionFetchJob *>(job);
    Q_ASSERT(fetchJob != nullptr);

    mSubCollections = fetchJob->collections();
    if (mSubCollections.isEmpty()) {          // no sub-collections, no problem
        ErrorReporter::progress(i18n("No sub-collections to copy/move"));
        fetchItems(sourceArg);              // go on to do items
        return;
    }

    if (mMoving) {
        ErrorReporter::progress(i18nc("@info:shell", "Moving %1 sub-collections", mSubCollections.count()));
    } else {
        ErrorReporter::progress(i18nc("@info:shell", "Copying %1 sub-collections", mSubCollections.count()));
    }
    doNextSubcollection(sourceArg);           // start processing them
}

inline void CopyCommand::doNextSubcollection(const QString &sourceArg)
{
    QMetaObject::invokeMethod(this, "processNextSubcollection", Qt::QueuedConnection,
                              Q_ARG(QString, sourceArg));
}

void CopyCommand::processNextSubcollection(const QString &sourceArg)
{
    if (mSubCollections.isEmpty()) {          // no more to do
        ErrorReporter::progress(i18n("No more sub-collections to process"));
        fetchItems(sourceArg);
        return;
    }

    Akonadi::Collection collection = mSubCollections.takeFirst();
    Akonadi::Job *job;

    if (mMoving) {
        if (!mDryRun) {
            job = new CollectionMoveJob(collection, mDestinationCollection, this);
        }
    } else {
        if (!mDryRun) {
            job = new CollectionCopyJob(collection, mDestinationCollection, this);
        }
    }

    if (!mDryRun) {
        job->setProperty("arg", sourceArg);
        job->setProperty("collection", collection.name());

        connect(job, SIGNAL(result(KJob*)), SLOT(onCollectionCopyFinished(KJob*)));
    } else {
        doNextSubcollection(sourceArg);
    }
}

void CopyCommand::onCollectionCopyFinished(KJob *job)
{
    const QString sourceArg = job->property("arg").toString();
    if (job->error() != 0) {
        ErrorReporter::error(ki18nc("@info:shell",
                                    "Cannot copy/move sub-collection '%2' from '%1', %3")
                             .subs(sourceArg)
                             .subs(job->property("collection").toString())
                             .subs(job->errorString()).toString());
        mAnyErrors = true;                  // note for exit status
    }

    doNextSubcollection(sourceArg);           // copy the next one
}

void CopyCommand::fetchItems(const QString &sourceArg)
{
    ItemFetchJob *fetchJob = new ItemFetchJob(mSourceCollection, this);
    fetchJob->fetchScope().fetchFullPayload(false);
    fetchJob->setProperty("arg", sourceArg);
    connect(fetchJob, SIGNAL(result(KJob*)), SLOT(onItemsFetched(KJob*)));
}

void CopyCommand::onItemsFetched(KJob *job)
{
    const QString sourceArg = job->property("arg").toString();
    if (job->error() != 0) {
        ErrorReporter::error(ki18nc("@info:shell",
                                    "Cannot fetch items of '%1', %2")
                             .subs(sourceArg)
                             .subs(job->errorString()).toString());
        mAnyErrors = true;                  // note for exit status
        doNextSource();
        return;
    }

    ItemFetchJob *fetchJob = qobject_cast<ItemFetchJob *>(job);
    Q_ASSERT(fetchJob != nullptr);

    Akonadi::Item::List items = fetchJob->items();
    if (items.isEmpty()) {                // no items, no problem
        ErrorReporter::progress(i18n("No items to process"));
        doNextSource();
        return;
    }

    Akonadi::Job *copyJob;
    if (mMoving) {
        ErrorReporter::progress(i18nc("@info:shell", "Moving %1 items", items.count()));
        if (!mDryRun) {
            copyJob = new ItemMoveJob(items, mDestinationCollection, this);
        }
    } else {
        ErrorReporter::progress(i18nc("@info:shell", "Copying %1 items", items.count()));
        if (!mDryRun) {
            copyJob = new ItemCopyJob(items, mDestinationCollection, this);
        }
    }
    if (!mDryRun) {
        copyJob->setProperty("arg", sourceArg);
        connect(copyJob, SIGNAL(result(KJob*)), SLOT(onItemCopyFinished(KJob*)));
    } else {
        doNextSource();
    }
}

void CopyCommand::onItemCopyFinished(KJob *job)
{
    const QString sourceArg = job->property("arg").toString();
    if (job->error() != 0) {
        ErrorReporter::error(ki18nc("@info:shell",
                                    "Cannot copy/move items from '%1', %2")
                             .subs(sourceArg)
                             .subs(job->errorString()).toString());
        mAnyErrors = true;                  // note for exit status
    }

    doNextSource();
}
