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

#include <Akonadi/XmlWriteJob>
#include <Akonadi/XmlDocument>
#include <Akonadi/CollectionCreateJob>
#include <Akonadi/CollectionFetchJob>
#include <Akonadi/CollectionFetchScope>
#include <Akonadi/ItemCreateJob>

#include <klocalizedstring.h>

#include <qfile.h>

#include "commandfactory.h"
#include "errorreporter.h"
#include "collectionresolvejob.h"

using namespace Akonadi;

DEFINE_COMMAND("import", ImportCommand, I18N_NOOP("Import an XML file"));

ImportCommand::ImportCommand(QObject *parent)
    : AbstractCommand(parent),
      mDocument(nullptr)
{
}

ImportCommand::~ImportCommand()
{
    delete mDocument;
}

void ImportCommand::setupCommandOptions(QCommandLineParser *parser)
{
    addOptionsOption(parser);
    addDryRunOption(parser);

    parser->addPositionalArgument("parent", i18nc("@info:shell", "The parent collection under which to import the file"));
    parser->addPositionalArgument("file", i18nc("@info:shell", "The file to import"));
}

int ImportCommand::initCommand(QCommandLineParser *parser)
{
    const QStringList args = parser->positionalArguments();
    if (!checkArgCount(args, 1, i18nc("@info:shell", "No parent collection specified"))) return InvalidUsage;
    if (!checkArgCount(args, 2, i18nc("@info:shell", "No import file specified"))) return InvalidUsage;

    if (!getCommonOptions(parser)) return InvalidUsage;

    if (!getResolveJob(args.first())) return InvalidUsage;

    const QString fileArg = args.at(1);
    mDocument = new XmlDocument(fileArg);
    if (!mDocument->isValid()) {
        emit error(i18nc("@info:shell", "Invalid XML file, %1", mDocument->lastError()));
        return InvalidUsage;
    }

    mCollections = mDocument->collections();

    return NoError;
}

void ImportCommand::start()
{
    connect(resolveJob(), &KJob::result, this, &ImportCommand::onParentFetched);
    resolveJob()->start();
}

void ImportCommand::onParentFetched(KJob *job)
{
    if (!checkJobResult(job, i18nc("@info:shell", "Unable to fetch parent collection, %1", job->errorString()))) return;

    mParentCollection = resolveJob()->collection();
    QMetaObject::invokeMethod(this, "processNextCollection", Qt::QueuedConnection);
}

void ImportCommand::onChildrenFetched(KJob *job)
{
    if (!checkJobResult(job, i18nc("@info:shell", "Unable to fetch children of parent collection, %1", job->errorString()))) return;

    QString rid = job->property("rid").toString();
    Collection parent = job->property("parent").value<Collection>();
    Collection collection = mDocument->collectionByRemoteId(rid);
    Collection newCol;
    const Collection::List collections = qobject_cast<CollectionFetchJob *>(job)->collections();
    bool found = false;

    for (const Collection &col : collections) {
        if (collection.name() == col.name()) {
            found = true;
            newCol = col;
            break;
        }
    }

    if (found) {
        ErrorReporter::progress(i18nc("@info:shell", "Collection '%1' already exists", collection.name()));
        mCollectionMap.insert(rid, newCol);
        QMetaObject::invokeMethod(this, "processNextCollection", Qt::QueuedConnection);
    } else {
        ErrorReporter::progress(i18nc("@info:shell", "Creating collection '%1'", collection.name()));
        collection.setParentCollection(parent);
        if (!isDryRun()) {
            CollectionCreateJob *createJob = new CollectionCreateJob(collection, this);
            createJob->setProperty("rid", rid);
            connect(createJob, &KJob::result, this, &ImportCommand::onCollectionCreated);
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

    ErrorReporter::progress(i18nc("@info:shell", "Processing collection '%1'", collection.name()));

    if (collection.parentCollection().remoteId().isEmpty()) {
        parent = mParentCollection;
    } else {
        parent = mCollectionMap.value(collection.parentCollection().remoteId());
        if (!parent.isValid() && !isDryRun()) {
            ErrorReporter::warning(i18nc("@info:shell", "Invalid parent for collection with remote ID '%1'",
                                         collection.remoteId()));
            QMetaObject::invokeMethod(this, "processNextCollection", Qt::QueuedConnection);
        }
    }

    if (!isDryRun()) {
        CollectionFetchJob *fetchJob = new CollectionFetchJob(parent, CollectionFetchJob::FirstLevel, this);
        fetchJob->fetchScope().setListFilter(CollectionFetchScope::NoFilter);
        fetchJob->setProperty("rid", collection.remoteId());
        fetchJob->setProperty("parent", QVariant::fromValue<Collection>(parent));
        connect(fetchJob, &KJob::result, this, &ImportCommand::onChildrenFetched);
    } else {
        QMetaObject::invokeMethod(this, "processNextCollection", Qt::QueuedConnection);
    }
}

void ImportCommand::onCollectionFetched(KJob *job)
{
    Collection collection = job->property("collection").value<Collection>();

    if (job->error() != 0) {
        ErrorReporter::warning(i18nc("@info:shell", "Unable to fetch collection with remote ID '%1', %2",
                                     collection.remoteId(), job->errorString()));

        if (!isDryRun()) {
            CollectionCreateJob *createJob = new CollectionCreateJob(collection, this);
            createJob->setProperty("rid", collection.remoteId());
            connect(createJob, &KJob::result, this, &ImportCommand::onCollectionCreated);
        } else {
            QMetaObject::invokeMethod(this, "processNextCollection", Qt::QueuedConnection);
        }
    } else {
        CollectionFetchJob *fetchJob = qobject_cast<CollectionFetchJob *>(job);
        mCollectionMap.insert(collection.remoteId(), fetchJob->collections().first());
        QMetaObject::invokeMethod(this, "processNextCollection", Qt::QueuedConnection);
    }
}

void ImportCommand::onCollectionCreated(KJob *job)
{
    if (!checkJobResult(job, i18nc("@info:shell", "Unable to create collection with remote ID '%1'", job->property("rid").toString()))) return;
    CollectionCreateJob *createJob = qobject_cast<CollectionCreateJob *>(job);
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

    ErrorReporter::progress(i18nc("@info:shell", "Processing items for '%1'", newCollection.name()));

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
    connect(createJob, &KJob::result, this, &ImportCommand::onItemCreated);
}

void ImportCommand::onItemCreated(KJob *job)
{
    if (!checkJobResult(job, i18nc("@info:shell", "Error creating item, %1", job->errorString()))) return;
    ItemCreateJob *itemCreateJob = qobject_cast<ItemCreateJob *>(job);
    ErrorReporter::progress(i18nc("@info:shell", "Created item '%1'", itemCreateJob->item().remoteId()));
    QMetaObject::invokeMethod(this, "processNextItemFromQueue", Qt::QueuedConnection);
}
