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

#include <akonadi/item.h>
#include <akonadi/itemdeletejob.h>
#include <akonadi/itemfetchjob.h>

#include <akonadi/collectiondeletejob.h>
#include <akonadi/private/collectionpathresolver_p.h>

#include <kcmdlineargs.h>
#include <klocalizedstring.h>

#include <QUrl>
#include <iostream>

using namespace Akonadi;

DeleteCommand::DeleteCommand(QObject *parent)
    : AbstractCommand(parent),
      mDeleteJob(0),
      mResolveJob(0),
      mDryRun(false),
      mIsCollection(false),
      mIsItem(false)
{
    mShortHelp = ki18nc("@info:shell", "Delete a collection or an item").toString();
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
    if (!mResolveJob->hasUsableInput()) {
        emit error(mResolveJob->errorString());
        delete mResolveJob;
        mResolveJob = 0;
        return InvalidUsage;
    }

    return NoError;
}

void DeleteCommand::start()
{
    Q_ASSERT(mResolveJob != 0);

    if (mIsItem) {
        fetchItems();
    } else {
        connect(mResolveJob, SIGNAL(result(KJob *)), SLOT(onBaseFetched(KJob *)));
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
        connect(mDeleteJob, SIGNAL(result(KJob *)), SLOT(onCollectionDeleted(KJob *)));
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
    Item item;
    // See if user input is a valid integer as an item ID
    bool ok;
    int id = mEntityArg.toInt(&ok);
    if (ok) {
        item = Item(id);        // conversion succeeded
    } else {
        // Otherwise check if we have an Akonadi URL
        const KUrl url = QUrl::fromUserInput(mEntityArg);
        if (url.isValid() && url.scheme() == QLatin1String("akonadi")) {
            item = Item::fromUrl(url);
        } else {
            emit error(i18nc("@info:shell", "Invalid item/collection syntax"));
            emit finished(RuntimeError);
            return;
        }
    }

    if (!mDryRun) {
        ItemDeleteJob *deleteJob = new ItemDeleteJob(item, this);
        connect(deleteJob, SIGNAL(result(KJob *)), SLOT(onItemsDeleted(KJob *)));
    } else {
        ItemFetchJob *fetchJob = new ItemFetchJob(item, this);
        connect(fetchJob, SIGNAL(result(KJob *)), SLOT(onItemsFetched(KJob *)));
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
    Q_ASSERT(fetchJob!=0);
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
        emit error(job->errorString());
        emit finished(RuntimeError);
        return;
    }

    std::cout << i18n("Item deleted successfully").toLocal8Bit().constData() << std::endl;
    emit finished(NoError);
}
