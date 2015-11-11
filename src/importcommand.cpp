/*
 * Copyright (C) 2014  Bhaskar Kandiyal <bkandiyal@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#include "importcommand.h"
#include "errorreporter.h"

#include <akonadi/collection.h>
#include <akonadi/xml/xmldocument.h>
#include <akonadi/collectioncreatejob.h>
#include <akonadi/collectionfetchjob.h>
#include <akonadi/itemcreatejob.h>

#include <klocalizedstring.h>
#include <kcmdlineargs.h>

#include <qfile.h>

#include "commandfactory.h"

using namespace Akonadi;


DEFINE_COMMAND("import", ImportCommand, "Import an XML file");


ImportCommand::ImportCommand(QObject *parent)
    : AbstractCommand(parent),
      mResolveJob(0),
      mDocument(0)
{
}

ImportCommand::~ImportCommand()
{
    delete mResolveJob;
    delete mDocument;
}

void ImportCommand::setupCommandOptions(KCmdLineOptions &options)
{
    AbstractCommand::setupCommandOptions(options);

    addOptionsOption(options);
    options.add("+parent", ki18nc("@info:shell", "The parent collection under which this file should be imported"));
    options.add("+file", ki18nc("@info:shell", "The file to import"));
    options.add(":", ki18nc("@info:shell", "Options for the command"));
    addOptionSeparator(options);
    addDryRunOption(options);
}

int ImportCommand::initCommand(KCmdLineArgs *parsedArgs)
{
    if (parsedArgs->count() < 2) {
        emitErrorSeeHelp(ki18nc("@info:shell", "No parent collection specified"));
        return InvalidUsage;
    }

    if (parsedArgs->count() == 2) {
        emitErrorSeeHelp(ki18nc("@info:shell", "No import file specified"));
        return InvalidUsage;
    }

    mDryRun = parsedArgs->isSet("dryrun");
    QString fileArg = parsedArgs->arg(2);

    mResolveJob = new CollectionResolveJob(parsedArgs->arg(1), this);
    if (!mResolveJob->hasUsableInput()) {
        emit error(ki18nc("@info:shell", "Invalid collection argument specified").toString());
        return InvalidUsage;
    }

    mDocument = new XmlDocument(fileArg);
    if (!mDocument->isValid()) {
        emit error(ki18nc("@info:shell", "Invalid XML file '%1'").subs(mDocument->lastError()).toString());
        return InvalidUsage;
    }

    mCollections = mDocument->collections();

    return NoError;
}

void ImportCommand::start()
{
    connect(mResolveJob, SIGNAL(result(KJob*)), SLOT(onParentFetched(KJob*)));
    mResolveJob->start();
}

void ImportCommand::onParentFetched(KJob *job)
{
    if (job->error() != 0) {
        emit error(ki18nc("@info:shell", "Unable to fetch parent collection: '%1'")
            .subs(job->errorString()).toString());
        emit finished(RuntimeError);
    }

    mParentCollection = mResolveJob->collection();
    QMetaObject::invokeMethod(this, "processNextCollection", Qt::QueuedConnection);
}

void ImportCommand::onChildrenFetched(KJob *job)
{
    if (job->error() != 0) {
        emit error(ki18nc("@info:shell", "Unable to fetch children of parent collection: '%1'")
            .subs(job->errorString()).toString());
        emit finished(RuntimeError);
        return;
    }

    QString rid = job->property("rid").toString();
    Collection parent = job->property("parent").value<Collection>();
    Collection collection = mDocument->collectionByRemoteId(rid);
    Collection newCol;
    Collection::List collections = qobject_cast<CollectionFetchJob*>(job)->collections();
    bool found = false;

    Q_FOREACH (const Collection &col, collections) {
        if (collection.name() == col.name()) {
            found = true;
            newCol = col;
            break;
        }
    }

    if (found) {
        ErrorReporter::progress(ki18nc("@info:shell", "Collection '%1' already exists")
            .subs(collection.name()).toString());
        mCollectionMap.insert(rid, newCol);
        QMetaObject::invokeMethod(this, "processNextCollection", Qt::QueuedConnection);
    } else {
        ErrorReporter::progress(ki18nc("@info:shell", "Creating collection '%1'")
            .subs(collection.name()).toString());
        collection.setParentCollection(parent);
        if (!mDryRun) {
            CollectionCreateJob *createJob = new CollectionCreateJob(collection, this);
            createJob->setProperty("rid", rid);
            connect(createJob, SIGNAL(result(KJob*)), SLOT(onCollectionCreated(KJob*)));
        } else {
            QMetaObject::invokeMethod(this, "processNextCollection", Qt::QueuedConnection);
        }
    }
}

void ImportCommand::processNextCollection()
{
    if (mCollections.isEmpty()) {
        processNextCollectionFromMap();
        return;
    }

    Collection collection = mCollections.takeFirst();
    Collection parent;

    ErrorReporter::progress(ki18nc("@info:shell", "Processing collection '%1'").subs(collection.name()).toString());

    if (collection.parentCollection().remoteId().isEmpty()) {
        parent = mParentCollection;
    } else {
        parent = mCollectionMap.value(collection.parentCollection().remoteId());
        if (!parent.isValid() && !mDryRun) {
            ErrorReporter::warning(ki18nc("@info:shell", "Invalid parent collection for collection with remote ID '%1'")
                .subs(collection.remoteId()).toString());
            QMetaObject::invokeMethod(this, "processNextCollection", Qt::QueuedConnection);
        }
    }

    if (!mDryRun) {
        CollectionFetchJob *fetchJob = new CollectionFetchJob(parent, CollectionFetchJob::FirstLevel, this);
        fetchJob->setProperty("rid", collection.remoteId());
        fetchJob->setProperty("parent", QVariant::fromValue<Collection>(parent));
        connect(fetchJob, SIGNAL(result(KJob*)), SLOT(onChildrenFetched(KJob*)));
    } else {
        QMetaObject::invokeMethod(this, "processNextCollection", Qt::QueuedConnection);
    }
}

void ImportCommand::onCollectionFetched(KJob *job)
{
    Collection collection = job->property("collection").value<Collection>();

    if (job->error() != 0) {
        ErrorReporter::warning(ki18nc("@info:shell", "Unable to fetch collection with remote ID '%1'. Error: '%2'")
            .subs(collection.remoteId()).subs(job->errorString()).toString());

        if (!mDryRun) {
            CollectionCreateJob *createJob = new CollectionCreateJob(collection, this);
            createJob->setProperty("rid", collection.remoteId());
            connect(createJob, SIGNAL(result(KJob*)), SLOT(onCollectionCreated(KJob*)));
        } else {
            QMetaObject::invokeMethod(this, "processNextCollection", Qt::QueuedConnection);
        }
    } else {
        CollectionFetchJob *fetchJob = qobject_cast<CollectionFetchJob*>(job);
        mCollectionMap.insert(collection.remoteId(), fetchJob->collections().first());
        QMetaObject::invokeMethod(this, "processNextCollection", Qt::QueuedConnection);
    }
}

void ImportCommand::onCollectionCreated(KJob *job)
{
    if (job->error() != 0) {
        emit error(ki18nc("@info:shell", "Unable to create collection with remote ID '%1'")
            .subs(job->property("rid").toString()).toString());
        emit finished(RuntimeError);
        return;
    }

    CollectionCreateJob *createJob = qobject_cast<CollectionCreateJob*>(job);
    mCollectionMap.insert(job->property("rid").toString(), createJob->collection());
    QMetaObject::invokeMethod(this, "processNextCollection", Qt::QueuedConnection);
}

void ImportCommand::processNextCollectionFromMap()
{
    if (mCollectionMap.isEmpty()) {
        emit finished(NoError);
        return;
    }

    QString rid = mCollectionMap.keys().at(0);
    Collection newCollection = mCollectionMap.take(rid);
    Collection oldCollection = mDocument->collectionByRemoteId(rid);

    ErrorReporter::progress(ki18nc("@info:shell", "Processing items for '%1'").subs(newCollection.name()).toString());

    mItemQueue = mDocument->items(oldCollection, true);
    mCurrentCollection = newCollection;

    QMetaObject::invokeMethod(this, "processNextItemFromQueue", Qt::QueuedConnection);
}

void ImportCommand::processNextItemFromQueue()
{
    if (mItemQueue.isEmpty()) {
        QMetaObject::invokeMethod(this, "processNextCollectionFromMap", Qt::QueuedConnection);
        return;
    }

    Item item = mItemQueue.takeFirst();
    ItemCreateJob *createJob = new ItemCreateJob(item, mCurrentCollection, this);
    connect(createJob, SIGNAL(result(KJob*)), SLOT(onItemCreated(KJob*)));
}

void ImportCommand::onItemCreated(KJob *job)
{
    if (job->error() != 0) {
        emit error(ki18nc("@info:shell", "Error creating item: '%1'").subs(job->errorString()).toString());
        emit finished(RuntimeError);
        return;
    }
    ItemCreateJob *itemCreateJob = qobject_cast<ItemCreateJob*>(job);
    ErrorReporter::progress(ki18nc("@info:shell", "Created item '%1'").subs(itemCreateJob->item().remoteId()).toString());
    QMetaObject::invokeMethod(this, "processNextItemFromQueue", Qt::QueuedConnection);
}
