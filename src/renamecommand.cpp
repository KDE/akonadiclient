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

#include "renamecommand.h"

#include <AkonadiCore/collectionmodifyjob.h>

#include <klocalizedstring.h>

#include <iostream>

#include "collectionresolvejob.h"
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

void RenameCommand::setupCommandOptions(QCommandLineParser *parser)
{
    addOptionsOption(parser);
    addDryRunOption(parser);

    parser->addPositionalArgument("collection", i18nc("@info:shell", "The collection to rename"));
    parser->addPositionalArgument("name", i18nc("@info:shell", "New name for collection"));
}

int RenameCommand::initCommand(QCommandLineParser *parser)
{
    const QStringList args = parser->positionalArguments();
    if (args.isEmpty()) {
        emitErrorSeeHelp(i18nc("@info:shell", "No collection specified"));
        return InvalidUsage;
    }

    if (args.count()<2) {
        emitErrorSeeHelp(i18nc("@info:shell", "New collection name not specified"));
        return InvalidUsage;
    }

    mDryRun = parser->isSet("dryrun");
    QString oldCollectionNameArg = args.first();
    mNewCollectionNameArg = args.at(1);

    mResolveJob = new CollectionResolveJob(oldCollectionNameArg, this);
    if (!mResolveJob->hasUsableInput()) {
        emit error(i18nc("@info:shell", "Invalid collection argument '%1', '%2'",
                         oldCollectionNameArg, mResolveJob->errorString()));
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
        connect(modifyJob, &KJob::result, this, &RenameCommand::onCollectionModified);
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
