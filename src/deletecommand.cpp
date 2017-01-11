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

#include "deletecommand.h"

#include "collectionresolvejob.h"

#include <AkonadiCore/item.h>
#include <AkonadiCore/itemdeletejob.h>
#include <AkonadiCore/itemfetchjob.h>

#include <AkonadiCore/collectiondeletejob.h>
#include <AkonadiCore/CollectionPathResolver>

#include <kcmdlineargs.h>
#include <klocalizedstring.h>

#include <QUrl>
#include <iostream>

#include "commandfactory.h"

using namespace Akonadi;

DEFINE_COMMAND("delete", DeleteCommand, "Delete a collection or an item");

DeleteCommand::DeleteCommand(QObject *parent)
    : AbstractCommand(parent),
      mDeleteJob(nullptr),
      mResolveJob(nullptr),
      mDryRun(false),
      mIsCollection(false),
      mIsItem(false)
{
}

DeleteCommand::~DeleteCommand()
{
    delete mResolveJob;
}

void DeleteCommand::setupCommandOptions(KCmdLineOptions &options)
{
    AbstractCommand::setupCommandOptions(options);

    addOptionsOption(options);
    options.add("+collection|item", ki18nc("@info:shell", "The collection or item"));
    addOptionSeparator(options);
    addCollectionItemOptions(options);
    addDryRunOption(options);
}

int DeleteCommand::initCommand(KCmdLineArgs *parsedArgs)
{
    if (parsedArgs->count() < 2) {
        emitErrorSeeHelp(ki18nc("@info:shell", "Missing collection/item argument"));
        return InvalidUsage;
    }

    mIsItem = parsedArgs->isSet("item");
    mIsCollection = parsedArgs->isSet("collection");
    mDryRun = parsedArgs->isSet("dryrun");

    if (mIsItem && mIsCollection) {
        emit error(i18nc("@info:shell", "Cannot specify as both an item and collection"));
        return InvalidUsage;
    }

    mEntityArg = parsedArgs->arg(1);
    mResolveJob = new CollectionResolveJob(mEntityArg, this);
    // TODO: does this work for deleting ITEMs?
    if (!mResolveJob->hasUsableInput()) {
        emit error(mResolveJob->errorString());
        delete mResolveJob;
        mResolveJob = nullptr;
        return InvalidUsage;
    }

    return NoError;
}

void DeleteCommand::start()
{
    if (!allowDangerousOperation()) {
        emit finished(RuntimeError);
    }

    Q_ASSERT(mResolveJob != nullptr);

    if (mIsItem) {
        fetchItems();
    } else {
        connect(mResolveJob, SIGNAL(result(KJob*)), SLOT(onBaseFetched(KJob*)));
        mResolveJob->start();
    }
}

void DeleteCommand::onBaseFetched(KJob *job)
{
    Q_ASSERT(job == mResolveJob);

    if (job->error() != 0) {
        if (job->error() == CollectionPathResolver::Unknown) {
            if (!mIsCollection) {
                fetchItems();
                return;
            }
        }

        emit error(job->errorString());
        emit finished(RuntimeError);
        return;
    }

    if (mResolveJob->collection() == Collection::root()) {
        emit error(i18nc("@info:shell", "Cannot delete the root collection"));
        emit finished(RuntimeError);
        return;
    }

    if (!mDryRun) {
        mDeleteJob = new CollectionDeleteJob(mResolveJob->collection());
        connect(mDeleteJob, SIGNAL(result(KJob*)), SLOT(onCollectionDeleted(KJob*)));
    } else {
        onCollectionDeleted(job);
    }
}

void DeleteCommand::onCollectionDeleted(KJob *job)
{
    if (job->error() != 0) {
        emit error(job->errorString());
        emit finished(RuntimeError);
        return;
    }

    std::cout << i18n("Collection deleted successfully").toLocal8Bit().constData() << std::endl;
    emit finished(NoError);
}

void DeleteCommand::fetchItems()
{
    Item item = CollectionResolveJob::parseItem(mEntityArg);
    if (!item.isValid()) {
        emit error(i18nc("@info:shell", "Invalid item/collection syntax '%1'", mEntityArg));
        emit finished(RuntimeError);
        return;
    }

    if (!mDryRun) {
        ItemDeleteJob *deleteJob = new ItemDeleteJob(item, this);
        connect(deleteJob, SIGNAL(result(KJob*)), SLOT(onItemsDeleted(KJob*)));
    } else {
        ItemFetchJob *fetchJob = new ItemFetchJob(item, this);
        connect(fetchJob, SIGNAL(result(KJob*)), SLOT(onItemsFetched(KJob*)));
    }
}

void DeleteCommand::onItemsFetched(KJob *job)
{
    if (job->error() != 0) {
        emit error(job->errorString());
        emit finished(RuntimeError);
        return;
    }

    ItemFetchJob *fetchJob = qobject_cast<ItemFetchJob *>(job);
    Q_ASSERT(fetchJob != nullptr);
    Item::List items = fetchJob->items();
    if (items.count() < 0) {
        emit error(i18nc("@info:shell", "Cannot find '%1' as a collection or item", mEntityArg));
        emit finished(RuntimeError);
        return;
    }

    emit finished(NoError);
}

void DeleteCommand::onItemsDeleted(KJob *job)
{
    if (job->error() != 0) {
        emit error(i18nc("@info:shell", "Error: %1", job->errorString()));
        emit finished(RuntimeError);
        return;
    }

    std::cout << i18n("Item deleted successfully").toLocal8Bit().constData() << std::endl;
    emit finished(NoError);
}
