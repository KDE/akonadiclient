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

#include <Akonadi/CollectionCopyJob>
#include <Akonadi/CollectionMoveJob>
#include <Akonadi/CollectionFetchJob>
#include <Akonadi/CollectionFetchScope>
#include <Akonadi/ItemCopyJob>
#include <Akonadi/ItemMoveJob>
#include <Akonadi/ItemFetchJob>
#include <Akonadi/ItemFetchScope>

#include <iostream>

#include "commandfactory.h"

using namespace Akonadi;

DEFINE_COMMAND("copy", CopyCommand, I18N_NOOP("Copy collections or items into a new collection"));

CopyCommand::CopyCommand(QObject *parent)
    : AbstractCommand(parent)
{
    mMoving = false;
}

void CopyCommand::start()
{
    mAnyErrors = false;                   // not yet, anyway

    connect(resolveJob(), &KJob::result, this, &CopyCommand::onTargetFetched);
    resolveJob()->start();
}

void CopyCommand::setupCommandOptions(QCommandLineParser *parser)
{
    addOptionsOption(parser);
    addDryRunOption(parser);

    parser->addPositionalArgument("source", i18nc("@info:shell", "Existing collections or items to copy"), i18n("source..."));
    parser->addPositionalArgument("destination", i18nc("@info:shell", "Destination collection to copy into"));
}

int CopyCommand::initCommand(QCommandLineParser *parser)
{
    mSourceArgs = parser->positionalArguments();
    if (!checkArgCount(mSourceArgs, 2, i18nc("@info:shell", "Missing source/destination arguments"))) return InvalidUsage;

    if (!getCommonOptions(parser)) return InvalidUsage;

    mDestinationArg = mSourceArgs.takeLast();		// extract the destination
    Q_ASSERT(!mSourceArgs.isEmpty());			// must have some left
    if (!getResolveJob(mDestinationArg)) return InvalidUsage;

    return NoError;
}

void CopyCommand::onTargetFetched(KJob *job)
{
    if (job->error() != 0) {
        emit error(i18nc("@info:shell", "Cannot resolve destination collection '%1', %2",
                         mDestinationArg, job->errorString()));
        emit finished(RuntimeError);
        return;
    }

    CollectionResolveJob *res = resolveJob();
    Q_ASSERT(job == res);
    mDestinationCollection = res->collection();
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
    connect(sourceJob, &KJob::result, this, &CopyCommand::onSourceResolved);
    sourceJob->start();
}

void CopyCommand::onSourceResolved(KJob *job)
{
    Q_ASSERT(mDestinationCollection.isValid());

    const QString sourceArg = job->property("arg").toString();
    if (job->error() != 0) {
        Item item;
        if (job->error() == Akonadi::Job::Unknown) {    // failed to resolve as collection
            // try as item instead
            item = CollectionResolveJob::parseItem(sourceArg);
        }

        if (!item.isValid()) {              // couldn't parse as item either
            emit error(i18nc("@info:shell", "Cannot resolve source '%1', %2",
                             sourceArg, job->errorString()));
            mAnyErrors = true;                // note for exit status
            doNextSource();
            return;
        }

        ItemFetchJob *fetchJob = new ItemFetchJob(item, this);
        fetchJob->fetchScope().fetchFullPayload(false);
        fetchJob->setProperty("arg", sourceArg);
        connect(fetchJob, &KJob::result, this, &CopyCommand::onItemsFetched);
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
                                         resolveJob()->formattedCollectionName()));
        } else {
            ErrorReporter::progress(i18n("Copying contents of %1 -> %2",
                                         sourceJob->formattedCollectionName(),
                                         resolveJob()->formattedCollectionName()));
        }

        mSourceCollection = sourceJob->collection();
        CollectionFetchJob *fetchJob = new CollectionFetchJob(mSourceCollection,
                CollectionFetchJob::FirstLevel, this);
        fetchJob->fetchScope().setListFilter(CollectionFetchScope::NoFilter);
        fetchJob->setProperty("arg", sourceArg);
        connect(fetchJob, &KJob::result, this, &CopyCommand::onCollectionsFetched);
    } else {
        // The source is a collection that does not end with a '/'.
        // This means that the entire source collection is to be copied
        // recursively into the destination collection, under the
        // original collection name.  This case is simpler!

        Akonadi::Job *copyMovejob;
        if (mMoving) {
            ErrorReporter::progress(i18n("Moving collection %1 -> %2",
                                         sourceJob->formattedCollectionName(),
                                         resolveJob()->formattedCollectionName()));
            if (!isDryRun()) {
                copyMovejob = new CollectionMoveJob(sourceCollection, mDestinationCollection, this);
            }
        } else {
            ErrorReporter::progress(i18n("Copying collection %1 -> %2",
                                         sourceJob->formattedCollectionName(),
                                         resolveJob()->formattedCollectionName()));

            if (!isDryRun()) {
                copyMovejob = new CollectionCopyJob(sourceCollection, mDestinationCollection, this);
            }
        }

        if (!isDryRun()) {
            copyMovejob->setProperty("arg", sourceArg);
            connect(copyMovejob, &KJob::result, this, &CopyCommand::onRecursiveCopyFinished);
        } else {
            doNextSource();
        }
    }
}

void CopyCommand::onRecursiveCopyFinished(KJob *job)
{
    const QString sourceArg = job->property("arg").toString();
    if (job->error() != 0) {
        emit error(i18nc("@info:shell", "Cannot copy/move from '%1', %2",
                         sourceArg, job->errorString()));
        mAnyErrors = true;                  // note for exit status
    }

    doNextSource();
}

void CopyCommand::onCollectionsFetched(KJob *job)
{
    const QString sourceArg = job->property("arg").toString();
    if (job->error() != 0) {
        emit error(i18nc("@info:shell", "Cannot fetch subcollections of '%1', %2",
                         sourceArg, job->errorString()));
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
        if (!isDryRun()) {
            job = new CollectionMoveJob(collection, mDestinationCollection, this);
        }
    } else {
        if (!isDryRun()) {
            job = new CollectionCopyJob(collection, mDestinationCollection, this);
        }
    }

    if (!isDryRun()) {
        job->setProperty("arg", sourceArg);
        job->setProperty("collection", collection.name());

        connect(job, &KJob::result, this, &CopyCommand::onCollectionCopyFinished);
    } else {
        doNextSubcollection(sourceArg);
    }
}

void CopyCommand::onCollectionCopyFinished(KJob *job)
{
    const QString sourceArg = job->property("arg").toString();
    if (job->error() != 0) {
        ErrorReporter::error(i18nc("@info:shell", "Cannot copy/move sub-collection '%2' from '%1', %3",
                                   sourceArg,
                                   job->property("collection").toString(),
                                   job->errorString()));
        mAnyErrors = true;                  // note for exit status
    }

    doNextSubcollection(sourceArg);           // copy the next one
}

void CopyCommand::fetchItems(const QString &sourceArg)
{
    ItemFetchJob *fetchJob = new ItemFetchJob(mSourceCollection, this);
    fetchJob->fetchScope().fetchFullPayload(false);
    fetchJob->setProperty("arg", sourceArg);
    connect(fetchJob, &KJob::result, this, &CopyCommand::onItemsFetched);
}

void CopyCommand::onItemsFetched(KJob *job)
{
    const QString sourceArg = job->property("arg").toString();
    if (job->error() != 0) {
        ErrorReporter::error(i18nc("@info:shell", "Cannot fetch items of '%1', %2",
                                   sourceArg, job->errorString()));
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
        if (!isDryRun()) {
            copyJob = new ItemMoveJob(items, mDestinationCollection, this);
        }
    } else {
        ErrorReporter::progress(i18nc("@info:shell", "Copying %1 items", items.count()));
        if (!isDryRun()) {
            copyJob = new ItemCopyJob(items, mDestinationCollection, this);
        }
    }
    if (!isDryRun()) {
        copyJob->setProperty("arg", sourceArg);
        connect(copyJob, &KJob::result, this, &CopyCommand::onItemCopyFinished);
    } else {
        doNextSource();
    }
}

void CopyCommand::onItemCopyFinished(KJob *job)
{
    const QString sourceArg = job->property("arg").toString();
    if (job->error() != 0) {
        ErrorReporter::error(i18nc("@info:shell", "Cannot copy/move items from '%1', %2",
                                   sourceArg, job->errorString()));
        mAnyErrors = true;                  // note for exit status
    }

    doNextSource();
}
