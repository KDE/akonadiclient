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

#include "editcommand.h"

#include <Akonadi/Item>
#include <Akonadi/ItemFetchJob>
#include <Akonadi/ItemFetchScope>
#include <Akonadi/ItemModifyJob>

#include <qprocess.h>
#include <qtemporaryfile.h>

#include <iostream>

#include "collectionresolvejob.h"
#include "commandfactory.h"

using namespace Akonadi;

DEFINE_COMMAND("edit", EditCommand, kli18nc("info:shell", "Edit the raw payload for the specified item using $EDITOR"));

EditCommand::EditCommand(QObject *parent)
    : AbstractCommand(parent)
{
}

EditCommand::~EditCommand()
{
    delete mTempFile;
}

void EditCommand::setupCommandOptions(QCommandLineParser *parser)
{
    addOptionsOption(parser);
    addDryRunOption(parser);

    parser->addPositionalArgument("item", i18nc("@info:shell", "The item to edit"));
}

int EditCommand::initCommand(QCommandLineParser *parser)
{
    const QStringList args = parser->positionalArguments();
    if (!checkArgCount(args, 1, i18nc("@info:shell", "No item specified")))
        return InvalidUsage;

    if (!getCommonOptions(parser))
        return InvalidUsage;

    mItemArg = args.first();

    return NoError;
}

void EditCommand::start()
{
    Item item = CollectionResolveJob::parseItem(mItemArg, true);
    if (!item.isValid()) {
        Q_EMIT finished(InvalidUsage);
    }

    ItemFetchJob *job = new ItemFetchJob(item, this);
    job->fetchScope().setFetchModificationTime(false);
    job->fetchScope().fetchAllAttributes(false);
    job->fetchScope().fetchFullPayload(true);
    connect(job, &ItemFetchJob::result, this, &EditCommand::onItemFetched);
}

void EditCommand::onItemFetched(KJob *job)
{
    if (!checkJobResult(job))
        return;
    ItemFetchJob *fetchJob = qobject_cast<ItemFetchJob *>(job);
    Q_ASSERT(fetchJob != nullptr);

    Item::List items = fetchJob->items();

    if (items.count() < 1) {
        Q_EMIT error(i18nc("@info:shell", "No result found for item '%1'", job->property("arg").toString()));
        Q_EMIT finished(RuntimeError);
        return;
    }

    Akonadi::Item item = items.first();

    if (!item.hasPayload()) {
        Q_EMIT error(i18nc("@info:shell", "Item '%1' has no payload", job->property("arg").toString()));
        Q_EMIT finished(RuntimeError);
        return;
    }

    QString editor = QString::fromLocal8Bit(qgetenv("EDITOR"));
    if (editor.isEmpty()) {
        Q_EMIT error(i18nc("@info:shell", "EDITOR environment variable needs to be set"));
        Q_EMIT finished(RuntimeError);
        return;
    }

    mTempFile = new QTemporaryFile(this);
    mTempFile->open();
    mTempFile->write(item.payloadData());
    mTempFile->flush(); // Make sure the data is written to the file before opening up the editor
    mTempFile->close();

    /* Using system() because KProcess / QProcess does not behave properly with console commands that expect input from stdin */
    int ret = system(QString(editor + " " + mTempFile->fileName()).toLocal8Bit().data());
    if (ret != 0) {
        Q_EMIT error(i18nc("@info:shell", "Cannot launch text editor '%1'", editor));
        Q_EMIT finished(RuntimeError);
        return;
    }

    if (!isDryRun()) {
        mTempFile->open();
        mTempFile->seek(0); // Seek to the beginning of the file
        item.setPayloadFromData(mTempFile->readAll());
        ItemModifyJob *modifyJob = new ItemModifyJob(item);
        connect(modifyJob, &ItemModifyJob::result, this, &EditCommand::onItemModified);
    } else {
        onItemModified(job);
    }
}

void EditCommand::onItemModified(KJob *job)
{
    if (!checkJobResult(job))
        return;
    std::cout << i18nc("@info:shell", "Changes to item '%1' have been saved", mItemArg).toLocal8Bit().constData() << std::endl;
    Q_EMIT finished(NoError);
}

#include "moc_editcommand.cpp"
