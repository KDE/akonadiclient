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

#include "collectionresolvejob.h"
#include "renamecommand.h"

#include <AkonadiCore/collectionmodifyjob.h>
#include <kcmdlineargs.h>
#include <klocalizedstring.h>

#include <iostream>

#include "commandfactory.h"

using namespace Akonadi;

DEFINE_COMMAND("rename", RenameCommand, "Rename a collection");

RenameCommand::RenameCommand(QObject *parent)
    : AbstractCommand(parent),
      mDryRun(false),
      mResolveJob(nullptr)
{
}

RenameCommand::~RenameCommand()
{
    delete mResolveJob;
}

void RenameCommand::setupCommandOptions(KCmdLineOptions &options)
{
    AbstractCommand::setupCommandOptions(options);

    options.add("+collection", ki18nc("@info:shell", "The collection to rename"));
    options.add("+name", ki18nc("@info:shell", "New name for collection"));
    addOptionSeparator(options);
    addDryRunOption(options);
}

int RenameCommand::initCommand(KCmdLineArgs *parsedArgs)
{
    if (parsedArgs->count() < 2) {
        emitErrorSeeHelp(ki18nc("@info:shell", "No collection specified"));
        return InvalidUsage;
    }

    if (parsedArgs->count() < 3) {
        emitErrorSeeHelp(ki18nc("@info:shell", "New collection name not specified"));
        return InvalidUsage;
    }

    mDryRun = parsedArgs->isSet("dryrun");
    QString oldCollectionNameArg = parsedArgs->arg(1);
    mNewCollectionNameArg = parsedArgs->arg(2);

    mResolveJob = new CollectionResolveJob(oldCollectionNameArg, this);
    if (!mResolveJob->hasUsableInput()) {
        emit error(ki18nc("@info:shell",
                          "Invalid collection argument '%1', '%2'")
                   .subs(oldCollectionNameArg)
                   .subs(mResolveJob->errorString()).toString());
        delete mResolveJob;
        mResolveJob = nullptr;

        return InvalidUsage;
    }

    return NoError;
}

void RenameCommand::start()
{
    Q_ASSERT(mResolveJob != nullptr);
    connect(mResolveJob, &KJob::result, this, &RenameCommand::onCollectionFetched);
    mResolveJob->start();
}

void RenameCommand::onCollectionFetched(KJob *job)
{
    if (job->error() != 0) {
        emit error(job->errorString());
        emit finished(RuntimeError);
        return;
    }

    Q_ASSERT(job == mResolveJob && mResolveJob->collection().isValid());

    if (!mDryRun) {
        Collection collection = mResolveJob->collection();
        collection.setName(mNewCollectionNameArg);

        CollectionModifyJob *modifyJob = new CollectionModifyJob(collection);
        connect(modifyJob, SIGNAL(result(KJob*)), SLOT(onCollectionModified(KJob*)));
    } else {
        onCollectionModified(job);
    }
}

void RenameCommand::onCollectionModified(KJob *job)
{
    if (job->error() != 0) {
        emit error(job->errorString());
        emit finished(RuntimeError);
    } else {
        std::cout << i18nc("@info:shell", "Collection renamed successfully").toLocal8Bit().data() << std::endl;
        emit finished(NoError);
    }
}
