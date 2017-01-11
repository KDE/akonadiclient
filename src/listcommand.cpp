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

#include "collectionresolvejob.h"

#include <AkonadiCore/CollectionFetchJob>
#include <AkonadiCore/ItemFetchJob>
#include <AkonadiCore/ItemFetchScope>

#include <QDateTime>

#include <KCmdLineOptions>
#include <KGlobal>

#include <iostream>

#include "commandfactory.h"

using namespace Akonadi;

DEFINE_COMMAND("list", ListCommand, "List sub-collections and/or items in a specified collection");

ListCommand::ListCommand(QObject *parent)
    : AbstractCommand(parent),
      mResolveJob(nullptr)
{
}

ListCommand::~ListCommand()
{
}

void ListCommand::setupCommandOptions(KCmdLineOptions &options)
{
    AbstractCommand::setupCommandOptions(options);

    addOptionsOption(options);
    options.add("+collection", ki18nc("@info:shell", "The collection to list, either as a path or akonadi URL"));
    addOptionSeparator(options);
    options.add("l").add("details", ki18n("List more detailed information"));
    options.add("c").add("collections", ki18n("List only sub-collections"));
    options.add("i").add("items", ki18n("List only contained items"));
}

int ListCommand::initCommand(KCmdLineArgs *parsedArgs)
{
    if (parsedArgs->count() < 2) {
        emitErrorSeeHelp(ki18nc("@info:shell", "Missing collection argument"));
        return InvalidUsage;
    }

    mListItems = parsedArgs->isSet("items");       // selection options specified
    mListCollections = parsedArgs->isSet("collections");
    if (!mListCollections && !mListItems) {        // if none given, then
        mListCollections = mListItems = true;       // list both by default
    }
    mListDetails = parsedArgs->isSet("details");   // listing option specified

    const QString collectionArg = parsedArgs->arg(1);
    mResolveJob = new CollectionResolveJob(collectionArg, this);

    if (!mResolveJob->hasUsableInput()) {
        emit error(mResolveJob->errorString());
        delete mResolveJob;
        mResolveJob = nullptr;

        return InvalidUsage;
    }

    return NoError;
}

void ListCommand::start()
{
    Q_ASSERT(mResolveJob != nullptr);

    connect(mResolveJob, SIGNAL(result(KJob*)), this, SLOT(onBaseFetched(KJob*)));
    mResolveJob->start();
}

static void writeColumn(const QString &data, int width = 0)
{
    std::cout << data.leftJustified(width).toLocal8Bit().constData() << "  ";
}

static void writeColumn(quint64 data, int width = 0)
{
    writeColumn(QString::number(data), width);
}

void ListCommand::onBaseFetched(KJob *job)
{
    if (job->error() != 0) {
        emit error(job->errorString());
        emit finished(RuntimeError);
        return;
    }

    Q_ASSERT(job == mResolveJob);

    if (mListCollections) {
        fetchCollections();
    } else {
        fetchItems();
    }
}

void ListCommand::fetchCollections()
{
    Q_ASSERT(mResolveJob != nullptr  && mResolveJob->collection().isValid());

    CollectionFetchJob *job = new CollectionFetchJob(mResolveJob->collection(), CollectionFetchJob::FirstLevel, this);
    connect(job, SIGNAL(result(KJob*)), this, SLOT(onCollectionsFetched(KJob*)));
}

void ListCommand::onCollectionsFetched(KJob *job)
{
    if (job->error() != 0) {
        emit error(job->errorString());
        emit finished(RuntimeError);
        return;
    }

    CollectionFetchJob *fetchJob = qobject_cast<CollectionFetchJob *>(job);
    Q_ASSERT(fetchJob != nullptr);

    Collection::List collections = fetchJob->collections();
    if (collections.isEmpty()) {
        if (mListCollections) {                  // message only if collections requested
            std::cout << i18nc("@info:shell",
                               "Collection %1 has no sub-collections",
                               mResolveJob->formattedCollectionName()).toLocal8Bit().constData() << std::endl;
        }
    } else {
        // This works because Akonadi::Entity implements operator<
        // which compares item IDs numerically
        qSort(collections);

        std::cout << i18nc("@info:shell output section header 1=count, 2=collection",
                           "Collection %2 has %1 sub-collections:",
                           collections.count(),
                           mResolveJob->formattedCollectionName()).toLocal8Bit().constData() << std::endl;
        if (mListDetails) {
            std::cout << "  ";
            writeColumn(i18nc("@info:shell column header", "ID"), 8);
            writeColumn(i18nc("@info:shell column header", "Name"));
            std::cout << std::endl;
        }

        Q_FOREACH (const Collection &collection, collections) {
            std::cout << "  ";
            if (mListDetails) {
                writeColumn(collection.id(), 8);
                writeColumn(collection.name());
            } else {
                std::cout << collection.name().toLocal8Bit().constData();
            }
            std::cout << std::endl;
        }
    }

    if (mListItems) {
        fetchItems();
    } else {
        emit finished(NoError);
    }
}

void ListCommand::fetchItems()
{
    Q_ASSERT(mResolveJob != nullptr  && mResolveJob->collection().isValid());

    // only attempt item listing if collection has non-collection content MIME types
    QStringList contentMimeTypes = mResolveJob->collection().contentMimeTypes();
    contentMimeTypes.removeAll(Collection::mimeType());
    if (!contentMimeTypes.isEmpty()) {
        ItemFetchJob *job = new ItemFetchJob(mResolveJob->collection(), this);
        job->fetchScope().setFetchModificationTime(true);
        job->fetchScope().fetchAllAttributes(false);
        job->fetchScope().fetchFullPayload(false);
        connect(job, SIGNAL(result(KJob*)), this, SLOT(onItemsFetched(KJob*)));
    } else {
        std::cout << i18nc("@info:shell",
                           "Collection %1 cannot contain items",
                           mResolveJob->formattedCollectionName()).toLocal8Bit().constData() << std::endl;
        emit finished(NoError);
    }
}

void ListCommand::onItemsFetched(KJob *job)
{
    if (job->error() != 0) {
        emit error(job->errorString());
        emit finished(RuntimeError);
        return;
    }

    ItemFetchJob *fetchJob = qobject_cast<ItemFetchJob *>(job);
    Q_ASSERT(fetchJob != nullptr);
    Item::List items = fetchJob->items();

    if (items.isEmpty()) {
        if (mListItems) {                    // message only if items requested
            std::cout << i18nc("@info:shell",
                               "Collection %1 has no items",
                               mResolveJob->formattedCollectionName()).toLocal8Bit().constData() << std::endl;
        }
    } else {
        qSort(items);

        std::cout << i18nc("@info:shell output section header 1=count, 2=collection",
                           "Collection %2 has %1 items:",
                           items.count(),
                           mResolveJob->formattedCollectionName()).toLocal8Bit().constData() << std::endl;
        if (mListDetails) {
            std::cout << "  ";
            writeColumn(i18nc("@info:shell column header", "ID"), 8);
            writeColumn(i18nc("@info:shell column header", "MIME type"), 20);
            writeColumn(i18nc("@info:shell column header", "Size"), 10);
            writeColumn(i18nc("@info:shell column header", "Modification Time"));
            std::cout << std::endl;
        }

        Q_FOREACH (const Item &item, items) {
            std::cout << "  ";
            if (mListDetails) {
                writeColumn(item.id(), 8);
                writeColumn(item.mimeType(), 20);
                writeColumn(KGlobal::locale()->formatByteSize(item.size()), 10);
                // from kdepim/akonadiconsole/browserwidget.cpp BrowserWidget::setItem()
                writeColumn((item.modificationTime().toString() + " UTC"));
            } else {
                std::cout << item.id();
            }
            std::cout << std::endl;
        }
    }

    emit finished(NoError);
}
