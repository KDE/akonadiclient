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

#include <Akonadi/CollectionModifyJob>

#include <klocalizedstring.h>

#include <iostream>

#include "collectionresolvejob.h"
#include "commandfactory.h"

using namespace Akonadi;

DEFINE_COMMAND("rename", RenameCommand, I18N_NOOP("Rename a collection"));

RenameCommand::RenameCommand(QObject *parent)
    : AbstractCommand(parent)
{
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
    if (!checkArgCount(args, 1, i18nc("@info:shell", "Missing collection argument"))) return InvalidUsage;
    if (!checkArgCount(args, 2, i18nc("@info:shell", "New name not specified"))) return InvalidUsage;

    if (!getCommonOptions(parser)) return InvalidUsage;

    QString oldCollectionNameArg = args.first();
    mNewCollectionNameArg = args.at(1);

    if (!getResolveJob(oldCollectionNameArg)) return InvalidUsage;

    return NoError;
}

void RenameCommand::start()
{
    connect(resolveJob(), &KJob::result, this, &RenameCommand::onCollectionFetched);
    resolveJob()->start();
}

void RenameCommand::onCollectionFetched(KJob *job)
{
    if (!checkJobResult(job)) return;
    CollectionResolveJob *res = resolveJob();
    Q_ASSERT(job == res && res->collection().isValid());

    if (!isDryRun()) {
        Collection collection = res->collection();
        collection.setName(mNewCollectionNameArg);

        CollectionModifyJob *modifyJob = new CollectionModifyJob(collection);
        connect(modifyJob, &KJob::result, this, &RenameCommand::onCollectionModified);
    } else {
        onCollectionModified(job);
    }
}

void RenameCommand::onCollectionModified(KJob *job)
{
    if (!checkJobResult(job)) return;
    std::cout << qPrintable(i18nc("@info:shell", "Collection renamed successfully")) << std::endl;
    emit finished(NoError);
}
