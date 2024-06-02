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

#include <Akonadi/Item>
#include <Akonadi/ItemDeleteJob>
#include <Akonadi/ItemFetchJob>
#include <Akonadi/CollectionDeleteJob>
#include <Akonadi/CollectionPathResolver>

#include <klocalizedstring.h>

#include <qurl.h>

#include <iostream>

#include "commandfactory.h"
#include "collectionresolvejob.h"


using namespace Akonadi;

DEFINE_COMMAND("delete", DeleteCommand,
               kli18nc("info:shell", "Delete a collection or an item"));

DeleteCommand::DeleteCommand(QObject *parent)
    : AbstractCommand(parent),
      mDeleteJob(nullptr)
{
}

void DeleteCommand::setupCommandOptions(QCommandLineParser *parser)
{
    addOptionsOption(parser);
    addCollectionItemOptions(parser);
    addDryRunOption(parser);

    parser->addPositionalArgument("entity", i18nc("@info:shell", "The collection or item"));
}

int DeleteCommand::initCommand(QCommandLineParser *parser)
{
    const QStringList args = parser->positionalArguments();
    if (!checkArgCount(args, 1, i18nc("@info:shell", "Missing collection/item argument"))) return InvalidUsage;

    if (!getCommonOptions(parser)) return InvalidUsage;

    mEntityArg = args.first();
    if (!getResolveJob(mEntityArg)) return InvalidUsage;

    return NoError;
}

void DeleteCommand::start()
{
    if (!allowDangerousOperation()) {
        Q_EMIT finished(RuntimeError);
    }

    if (wantItem()) {
        fetchItems();
    } else {
        connect(resolveJob(), &KJob::result, this, &DeleteCommand::onBaseFetched);
        resolveJob()->start();
    }
}

void DeleteCommand::onBaseFetched(KJob *job)
{
    CollectionResolveJob *res = resolveJob();
    Q_ASSERT(job == res);

    if (job->error() != 0) {
        if (job->error() == CollectionPathResolver::Unknown) {
            if (!wantCollection()) {
                fetchItems();
                return;
            }
        }

        Q_EMIT error(job->errorString());
        Q_EMIT finished(RuntimeError);
        return;
    }

    if (res->collection() == Collection::root()) {
        Q_EMIT error(i18nc("@info:shell", "Cannot delete the root collection"));
        Q_EMIT finished(RuntimeError);
        return;
    }

    if (!isDryRun()) {
        mDeleteJob = new CollectionDeleteJob(res->collection(), this);
        connect(mDeleteJob, &KJob::result, this, &DeleteCommand::onCollectionDeleted);
    } else {
        onCollectionDeleted(job);
    }
}

void DeleteCommand::onCollectionDeleted(KJob *job)
{
    if (!checkJobResult(job)) return;
    std::cout << i18n("Collection deleted successfully").toLocal8Bit().constData() << std::endl;
    Q_EMIT finished(NoError);
}

void DeleteCommand::fetchItems()
{
    Item item = CollectionResolveJob::parseItem(mEntityArg);
    if (!item.isValid()) {
        Q_EMIT error(i18nc("@info:shell", "Invalid item/collection syntax '%1'", mEntityArg));
        Q_EMIT finished(RuntimeError);
        return;
    }

    if (!isDryRun()) {
        ItemDeleteJob *deleteJob = new ItemDeleteJob(item, this);
        connect(deleteJob, &KJob::result, this, &DeleteCommand::onItemsDeleted);
    } else {
        ItemFetchJob *fetchJob = new ItemFetchJob(item, this);
        connect(fetchJob, &KJob::result, this, &DeleteCommand::onItemsFetched);
    }
}

void DeleteCommand::onItemsFetched(KJob *job)
{
    if (!checkJobResult(job)) return;
    ItemFetchJob *fetchJob = qobject_cast<ItemFetchJob *>(job);
    Q_ASSERT(fetchJob != nullptr);

    Item::List items = fetchJob->items();
    if (items.count() < 0) {
        Q_EMIT error(i18nc("@info:shell", "Cannot find '%1' as a collection or item", mEntityArg));
        Q_EMIT finished(RuntimeError);
        return;
    }

    Q_EMIT finished(NoError);
}

void DeleteCommand::onItemsDeleted(KJob *job)
{
    if (!checkJobResult(job)) return;
    std::cout << i18n("Item deleted successfully").toLocal8Bit().constData() << std::endl;
    Q_EMIT finished(NoError);
}
