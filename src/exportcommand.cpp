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

#include <Akonadi/XmlWriteJob>

#include <klocalizedstring.h>

#include "commandfactory.h"
#include "collectionresolvejob.h"

using namespace Akonadi;

DEFINE_COMMAND("export", ExportCommand,
               kli18nc("info:shell", "Export a collection to an XML file"));

ExportCommand::ExportCommand(QObject *parent)
    : AbstractCommand(parent)
{
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
    if (!checkArgCount(args, 1, i18nc("@info:shell", "No collection specified"))) return InvalidUsage;
    if (!checkArgCount(args, 2, i18nc("@info:shell", "No export file specified"))) return InvalidUsage;

    if (!getCommonOptions(parser)) return InvalidUsage;

    QString collectionArg = args.at(0);
    mFileArg = args.at(1);

    if (!getResolveJob(collectionArg)) return InvalidUsage;

    return NoError;
}

void ExportCommand::start()
{
    connect(resolveJob(), &KJob::result, this, &ExportCommand::onCollectionFetched);
    resolveJob()->start();
}

void ExportCommand::onCollectionFetched(KJob *job)
{
    if (!checkJobResult(job)) return;

    if (!isDryRun()) {
        XmlWriteJob *writeJob = new XmlWriteJob(resolveJob()->collection(), mFileArg);
        connect(writeJob, &KJob::result, this, &ExportCommand::onWriteFinished);
    } else {
        Q_EMIT finished(NoError);
    }
}

void ExportCommand::onWriteFinished(KJob *job)
{
    if (!checkJobResult(job)) return;
    Q_EMIT finished(NoError);
}

#include "moc_exportcommand.cpp"
