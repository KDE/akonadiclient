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

#include "collectionresolvejob.h"

#include <AkonadiCore/itemfetchjob.h>
#include <AkonadiCore/itemfetchscope.h>
#include <AkonadiCore/itemmodifyjob.h>

#include <kcmdlineargs.h>
#include <kurl.h>
#include <qfile.h>

#include <iostream>

#include "commandfactory.h"

using namespace Akonadi;

DEFINE_COMMAND("update", UpdateCommand, "Update an item's payload with the file specified");

UpdateCommand::UpdateCommand(QObject *parent)
    : AbstractCommand(parent),
      mDryRun(false),
      mFile(nullptr)
{
}

UpdateCommand::~UpdateCommand()
{
    delete mFile;
}

void UpdateCommand::setupCommandOptions(KCmdLineOptions &options)
{
    options.add("+item", ki18nc("@info:shell", "The item to update"));
    options.add("+file", ki18nc("@info:shell", "File to update the item from"));
    addOptionSeparator(options);
    addDryRunOption(options);
}

int UpdateCommand::initCommand(KCmdLineArgs *parsedArgs)
{
    if (parsedArgs->count() < 2) {
        emitErrorSeeHelp(ki18nc("@info:shell", "No item specified"));
        return InvalidUsage;
    }

    if (parsedArgs->count() == 2) {
        emitErrorSeeHelp(ki18nc("@info:shell", "No file specified"));
        return InvalidUsage;
    }

    mItemArg = parsedArgs->arg(1);
    mFileArg = parsedArgs->arg(2);
    mDryRun = parsedArgs->isSet("dryrun");

    return NoError;
}

void UpdateCommand::start()
{
    if (!allowDangerousOperation()) {
        emit finished(RuntimeError);
    }

    Item item = CollectionResolveJob::parseItem(mItemArg, true);
    if (!item.isValid()) {
        emit finished(RuntimeError);
        return;
    }

    if (!QFile::exists(mFileArg)) {
        emit error(i18nc("@info:shell", "File <filename>%1</filename> does not exist", mFileArg));
        emit finished(RuntimeError);
        return;
    }

    // TODO: report strerror(errno), then above is superfluous
    mFile = new QFile(mFileArg);
    if (!mFile->open(QIODevice::ReadOnly)) {
        emit error(i18nc("@info:shell", "File <filename>%1</filename> cannot be read", mFileArg));
        emit finished(RuntimeError);
        delete mFile;
        mFile = nullptr;
        return;
    }

    ItemFetchJob *job = new ItemFetchJob(item, this);
    job->fetchScope().setFetchModificationTime(false);
    job->fetchScope().fetchAllAttributes(false);
    connect(job, SIGNAL(result(KJob*)), SLOT(onItemFetched(KJob*)));
}

void UpdateCommand::onItemFetched(KJob *job)
{
    if (job->error() != 0) {
        emit error(job->errorString());
        emit finished(RuntimeError);
        return;
    }

    ItemFetchJob *fetchJob = qobject_cast<ItemFetchJob *>(job);
    Q_ASSERT(fetchJob != nullptr);

    Item::List items = fetchJob->items();
    if (items.count() < 1) {
        emit error(i18nc("@info:shell", "No result returned for item '%1'", job->property("arg").toString()));
        emit finished(RuntimeError);
        return;
    }

    if (!mDryRun) {
        Item item = items.first();
        QByteArray data = mFile->readAll();
        item.setPayloadFromData(data);
        ItemModifyJob *modifyJob = new ItemModifyJob(item, this);
        connect(modifyJob, SIGNAL(result(KJob*)), SLOT(onItemUpdated(KJob*)));
    } else {
        onItemUpdated(job);
    }
}

void UpdateCommand::onItemUpdated(KJob *job)
{
    if (job->error() != 0) {
        emit error(job->errorString());
        emit finished(RuntimeError);
        return;
    }

    std::cout << i18nc("@info:shell", "Item updated successfully").toLocal8Bit().data() << std::endl;
    emit finished(NoError);
}
