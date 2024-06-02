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

#include "updatecommand.h"

#include <Akonadi/ItemFetchJob>
#include <Akonadi/ItemFetchScope>
#include <Akonadi/ItemModifyJob>

#include <qfile.h>

#include <iostream>

#include "collectionresolvejob.h"
#include "commandfactory.h"

using namespace Akonadi;

DEFINE_COMMAND("update", UpdateCommand, I18N_NOOP("Update an item's payload from a file"));

UpdateCommand::UpdateCommand(QObject *parent)
    : AbstractCommand(parent),
      mFile(nullptr)
{
}

UpdateCommand::~UpdateCommand()
{
    delete mFile;
}

void UpdateCommand::setupCommandOptions(QCommandLineParser *parser)
{
    addOptionsOption(parser);
    addDryRunOption(parser);

    parser->addPositionalArgument("item", i18nc("@info:shell", "The item to update, an ID or Akonadi URL"));
    parser->addPositionalArgument("file", i18nc("@info:shell", "File to update the item from"));
}

int UpdateCommand::initCommand(QCommandLineParser *parser)
{
    const QStringList args = parser->positionalArguments();
    if (!checkArgCount(args, 1, i18nc("@info:shell", "No item specified"))) return InvalidUsage;
    if (!checkArgCount(args, 2, i18nc("@info:shell", "No file specified"))) return InvalidUsage;

    if (!getCommonOptions(parser)) return InvalidUsage;

    mItemArg = args.at(0);
    mFileArg = args.at(1);

    return NoError;
}

void UpdateCommand::start()
{
    if (!allowDangerousOperation()) {
        Q_EMIT finished(RuntimeError);
    }

    Item item = CollectionResolveJob::parseItem(mItemArg, true);
    if (!item.isValid()) {
        Q_EMIT finished(RuntimeError);
        return;
    }

    if (!QFile::exists(mFileArg)) {
        Q_EMIT error(i18nc("@info:shell", "File ‘%1’ does not exist", mFileArg));
        Q_EMIT finished(RuntimeError);
        return;
    }

    // TODO: report strerror(errno), then above is superfluous
    mFile = new QFile(mFileArg);
    if (!mFile->open(QIODevice::ReadOnly)) {
        Q_EMIT error(i18nc("@info:shell", "File ‘%1’ cannot be read", mFileArg));
        Q_EMIT finished(RuntimeError);
        delete mFile;
        mFile = nullptr;
        return;
    }

    ItemFetchJob *job = new ItemFetchJob(item, this);
    job->fetchScope().setFetchModificationTime(false);
    job->fetchScope().fetchAllAttributes(false);
    connect(job, &KJob::result, this, &UpdateCommand::onItemFetched);
}

void UpdateCommand::onItemFetched(KJob *job)
{
    if (!checkJobResult(job)) return;
    ItemFetchJob *fetchJob = qobject_cast<ItemFetchJob *>(job);
    Q_ASSERT(fetchJob != nullptr);

    Item::List items = fetchJob->items();
    if (items.count() < 1) {
        Q_EMIT error(i18nc("@info:shell", "No result returned for item '%1'", job->property("arg").toString()));
        Q_EMIT finished(RuntimeError);
        return;
    }

    if (!isDryRun()) {
        Item item = items.first();
        QByteArray data = mFile->readAll();
        item.setPayloadFromData(data);
        ItemModifyJob *modifyJob = new ItemModifyJob(item, this);
        connect(modifyJob, &KJob::result, this, &UpdateCommand::onItemUpdated);
    } else {
        onItemUpdated(job);
    }
}

void UpdateCommand::onItemUpdated(KJob *job)
{
    if (!checkJobResult(job)) return;
    std::cout << qPrintable(i18nc("@info:shell", "Item updated successfully")) << std::endl;
    Q_EMIT finished(NoError);
}
