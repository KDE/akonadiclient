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

#include <akonadi/item.h>
#include <akonadi/itemfetchjob.h>
#include <akonadi/itemfetchscope.h>
#include <akonadi/itemmodifyjob.h>
#include <kcmdlineargs.h>
#include <kprocess.h>
#include <qprocess.h>
#include <qtemporaryfile.h>

#include <iostream>

using namespace Akonadi;

EditCommand::EditCommand(QObject *parent)
    : AbstractCommand(parent),
      mTempFile(0),
      mDryRun(false)
{
    mShortHelp = ki18nc("@info:shell", "Opens the raw payload for the specified item in $EDITOR for editing").toString();
}

EditCommand::~EditCommand()
{
    delete mTempFile;
}

void EditCommand::setupCommandOptions(KCmdLineOptions &options)
{
    AbstractCommand::setupCommandOptions(options);

    options.add("+item", ki18nc("@info:shell", "The item to edit"));
    options.add(":", ki18nc("@info:shell", "Options for command"));
    options.add("n").add("dryrun", ki18nc("@info:shell", "Run without making any actual changes"));
}

int EditCommand::initCommand(KCmdLineArgs *parsedArgs)
{
    if (parsedArgs->count() < 2) {
        emitErrorSeeHelp(ki18nc("@info:shell", "No item specified"));
        return InvalidUsage;
    }

    mItemArg = parsedArgs->arg(1);
    mDryRun = parsedArgs->isSet("dryrun");

    return NoError;
}

void EditCommand::start()
{
    Item item;
    bool ok;
    int id = mItemArg.toInt(&ok);
    if (ok) {
        item = Item(id);
    } else {
        const KUrl url = QUrl::fromUserInput(mItemArg);
        if (url.isValid()  && url.scheme() == QLatin1String("akonadi")) {
            item = Item::fromUrl(url);
        } else {
            emit error(i18nc("@info:shell", "Invalid item syntax '%1'", mItemArg));
            return;
        }
    }

    ItemFetchJob *job = new ItemFetchJob(item, this);
    job->fetchScope().setFetchModificationTime(false);
    job->fetchScope().fetchAllAttributes(false);
    job->fetchScope().fetchFullPayload(true);
    connect(job, SIGNAL(result(KJob *)), SLOT(onItemFetched(KJob *)));
}

void EditCommand::onItemFetched(KJob *job)
{
    if (job->error() != 0) {
        emit error(job->errorString());
        emit finished(RuntimeError);
        return;
    }

    ItemFetchJob *fetchJob = qobject_cast<ItemFetchJob *>(job);
    Q_ASSERT(fetchJob != 0);

    Item::List items = fetchJob->items();

    if (items.count() < 1) {
        emit error(i18nc("@info:shell", "No result found for item '%1'", job->property("arg").toString()));
        emit finished(RuntimeError);
        return;
    }

    Akonadi::Item item = items.first();

    if (!item.hasPayload()) {
        emit error(i18nc("@info:shell", "Item '%1' has no payload", job->property("arg").toString()));
        emit finished(RuntimeError);
        return;
    }

    QString editor = QString::fromLocal8Bit(qgetenv("EDITOR"));

    if (editor.isEmpty()) {
        emit error(i18nc("@info:shell", "$EDITOR environment variable needs to be set"));
        emit finished(RuntimeError);
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
        emit error(i18nc("@info:shell", "Cannot launch text editor '%1'", editor));
        emit finished(RuntimeError);
        return;
    }

    if (!mDryRun) {
        mTempFile->open();
        mTempFile->seek(0); // Seek to the beginning of the file
        item.setPayloadFromData(mTempFile->readAll());
        ItemModifyJob *modifyJob = new ItemModifyJob(item);
        connect(modifyJob, SIGNAL(result(KJob *)), SLOT(onItemModified(KJob *)));
    } else {
        onItemModified(job);
    }
}

void EditCommand::onItemModified(KJob *job)
{
    if (job->error() != 0) {
        emit error(job->errorString());
        emit finished(RuntimeError);
        return;
    }

    std::cout << i18nc("@info:shell", "Changes to item '%1' have been saved", mItemArg).toLocal8Bit().constData() << std::endl;
    emit finished(NoError);
}