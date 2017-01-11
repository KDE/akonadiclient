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
#include <kcmdlineargs.h>

#include "commandfactory.h"

using namespace Akonadi;


DEFINE_COMMAND("export", ExportCommand, "Export a collection to an XML file");


ExportCommand::ExportCommand(QObject *parent)
    : AbstractCommand(parent),
      mDryRun(false),
      mResolveJob(0)
{
}

ExportCommand::~ExportCommand()
{
    delete mResolveJob;
}

void ExportCommand::setupCommandOptions(KCmdLineOptions& options)
{
    AbstractCommand::setupCommandOptions(options);

    addOptionsOption(options);
    options.add("+collection", ki18nc("@info:shell", "The collection to export"));
    options.add("+file", ki18nc("@info:shell", "The file to export to"));
    addOptionSeparator(options);
    addDryRunOption(options);
}

int ExportCommand::initCommand(KCmdLineArgs* parsedArgs)
{
    if (parsedArgs->count() < 2) {
        emitErrorSeeHelp(ki18nc("@info:shell", "No collection specified"));
        return InvalidUsage;
    }

    if (parsedArgs->count() == 2) {
        emitErrorSeeHelp(ki18nc("@info:shell", "No export file specified"));
        return InvalidUsage;
    }

    mDryRun = parsedArgs->isSet("dryrun");
    QString collectionArg = parsedArgs->arg(1);
    mFileArg = parsedArgs->arg(2);
    mResolveJob = new CollectionResolveJob(collectionArg, this);

    if (!mResolveJob->hasUsableInput()) {
        emit error(mResolveJob->errorString());
        return InvalidUsage;
    }

    return NoError;
}

void ExportCommand::start()
{
    connect(mResolveJob, SIGNAL(result(KJob*)), SLOT(onCollectionFetched(KJob*)));
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
        connect(writeJob, SIGNAL(result(KJob*)), SLOT(onWriteFinished(KJob*)));
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
