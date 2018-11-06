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

#include "exportcommand.h"

#include <AkonadiXml/XmlWriteJob>

#include <klocalizedstring.h>

#include "commandfactory.h"
#include "collectionresolvejob.h"

using namespace Akonadi;

DEFINE_COMMAND("export", ExportCommand, "Export a collection to an XML file");

ExportCommand::ExportCommand(QObject *parent)
    : AbstractCommand(parent),
      mDryRun(false),
      mResolveJob(nullptr)
{
}

ExportCommand::~ExportCommand()
{
    delete mResolveJob;
}

void ExportCommand::setupCommandOptions(QCommandLineParser *parser)
{
    addOptionsOption(parser);
    addDryRunOption(parser);

    parser->addPositionalArgument("collection", i18nc("@info:shell", "The collection to export: an ID, path or Akonadi URL"));
    parser->addPositionalArgument("file", i18nc("@info:shell", "The file to export to"));
}

int ExportCommand::initCommand(QCommandLineParser *parser)
{
    const QStringList args = parser->positionalArguments();
    if (args.isEmpty()) {
        emitErrorSeeHelp(i18nc("@info:shell", "No collection specified"));
        return InvalidUsage;
    }

    if (args.count()<2) {
        emitErrorSeeHelp(i18nc("@info:shell", "No export file specified"));
        return InvalidUsage;
    }

    mDryRun = parser->isSet("dryrun");
    QString collectionArg = args.at(0);
    mFileArg = args.at(1);

    mResolveJob = new CollectionResolveJob(collectionArg, this);
    if (!mResolveJob->hasUsableInput()) {
        emit error(mResolveJob->errorString());
        return InvalidUsage;
    }

    return NoError;
}

void ExportCommand::start()
{
    connect(mResolveJob, &KJob::result, this, &ExportCommand::onCollectionFetched);
    mResolveJob->start();
}

void ExportCommand::onCollectionFetched(KJob *job)
{
    if (job->error() != 0) {
        emit error(job->errorString());
        emit finished(RuntimeError);
        return;
    }

    if (!mDryRun) {
        XmlWriteJob *writeJob = new XmlWriteJob(mResolveJob->collection(), mFileArg);
        connect(writeJob, &KJob::result, this, &ExportCommand::onWriteFinished);
    } else {
        emit finished(NoError);
    }
}

void ExportCommand::onWriteFinished(KJob *job)
{
    if (job->error() != 0) {
        emit error(job->errorString());
        emit finished(RuntimeError);
        return;
    }

    emit finished(NoError);
}
