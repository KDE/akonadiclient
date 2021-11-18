/*
    Copyright (C) 2013  Jonathan Marten <jjm@keelhaul.me.uk>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include "infocommand.h"

#include <akonadi/itemfetchjob.h>
#include <akonadi/itemfetchscope.h>
#include <akonadi/collectionstatisticsjob.h>
#include <akonadi/collectionstatistics.h>

#include <akonadi/collectionpathresolver.h>		// just for error code

#include <qdatetime.h>

#ifdef USE_KIO_CONVERTSIZE
#include <kio/global.h>
#endif

#include <iostream>

#include "commandfactory.h"
#include "collectionresolvejob.h"
#include "collectionpathjob.h"

using namespace Akonadi;

DEFINE_COMMAND("info", InfoCommand, I18N_NOOP("Show full information for a collection or item"));

InfoCommand::InfoCommand(QObject *parent)
    : AbstractCommand(parent),
      mInfoCollection(nullptr),
      mInfoItem(nullptr),
      mInfoStatistics(nullptr)
{
}

InfoCommand::~InfoCommand()
{
    delete mInfoItem;
    delete mInfoCollection;
    delete mInfoStatistics;
}

void InfoCommand::setupCommandOptions(QCommandLineParser *parser)
{
    addOptionsOption(parser);
    addCollectionItemOptions(parser);

    parser->addPositionalArgument("entity", i18nc("@info:shell", "Collections or items to display"), i18n("entity..."));
}

int InfoCommand::initCommand(QCommandLineParser *parser)
{
    const QStringList args = parser->positionalArguments();
    if (!checkArgCount(args, 1, i18nc("@info:shell", "No collections or items specified"))) return InvalidUsage;

    if (!getCommonOptions(parser)) return InvalidUsage;

    initProcessLoop(args);
    return NoError;
}

void InfoCommand::start()
{
    startProcessLoop("infoForNext");
}

void InfoCommand::infoForNext()
{
    if (wantItem()) {					// user forced as an item
        fetchItems();					// do this immediately
    } else
    {
        if (!getResolveJob(currentArg()))
        {
            processNext();
            return;
        }

        // User specified that the input is a collection, or
        // didn't specify at all what sort of entity it is.
        // First try to resolve it as a collection.
        CollectionResolveJob *res = resolveJob();
        connect(res, &KJob::result, this, &InfoCommand::onBaseFetched);
        res->start();
    }
}

void InfoCommand::onBaseFetched(KJob *job)
{
    CollectionResolveJob *res = resolveJob();
    Q_ASSERT(job == res);

    if (job->error() != 0) {
        if (job->error() == CollectionPathResolver::Unknown) {
            // failed to resolve as collection
            if (!wantCollection()) {			// not forced as a collection
                fetchItems();				// try it as an item
                return;
            }
        }

        emit error(job->errorString());
        processNext();
        return;
    }

    if (res->collection() == Collection::root()) {
        emit error(i18nc("@info:shell", "No information available for collection root"));
        processNext();
        return;
    }

    fetchStatistics();
}

void InfoCommand::fetchStatistics()
{
    Q_ASSERT(resolveJob()->collection().isValid());
    CollectionStatisticsJob *job = new CollectionStatisticsJob(resolveJob()->collection(), this);
    connect(job, &KJob::result, this, &InfoCommand::onStatisticsFetched);
}

void InfoCommand::onStatisticsFetched(KJob *job)
{
    if (!checkJobResult(job)) return;
    CollectionStatisticsJob *statsJob = qobject_cast<CollectionStatisticsJob *>(job);
    Q_ASSERT(statsJob != nullptr);
    mInfoStatistics = new CollectionStatistics(statsJob->statistics());

    mInfoCollection = new Collection(resolveJob()->collection());
    fetchParentPath(mInfoCollection->parentCollection());
}

void InfoCommand::fetchItems()
{
    Item item = CollectionResolveJob::parseItem(currentArg());
    if (!item.isValid()) {
        emit error(i18nc("@info:shell", "Invalid item/collection syntax '%1'", currentArg()));
        processNext();
        return;
    }

    ItemFetchJob *job = new ItemFetchJob(item, this);
    job->fetchScope().setFetchModificationTime(true);
    job->fetchScope().setFetchTags(true);
    job->fetchScope().fetchAllAttributes(true);

    // Need this so that parentCollection() will be valid.
    job->fetchScope().setAncestorRetrieval(ItemFetchScope::Parent);

    // Not actually going to use the payload here, but if we don't set it
    // to be fetched then hasPayload() will not return a meaningful result.
    job->fetchScope().fetchFullPayload(true);

    connect(job, &KJob::result, this, &InfoCommand::onItemsFetched);
}

void InfoCommand::onItemsFetched(KJob *job)
{
    if (!checkJobResult(job)) return;
    ItemFetchJob *fetchJob = qobject_cast<ItemFetchJob *>(job);
    Q_ASSERT(fetchJob != nullptr);
    Item::List items = fetchJob->items();
    if (items.count() < 1) {
        emit error(i18nc("@info:shell", "Cannot find '%1' as a collection or item", currentArg()));
        processNext();
        return;
    }

    mInfoItem = new Item(items.first());
    fetchParentPath(mInfoItem->parentCollection());
}

void InfoCommand::fetchParentPath(const Akonadi::Collection &collection)
{
    Q_ASSERT(mInfoCollection != nullptr || mInfoItem != nullptr);

    CollectionPathJob *job = new CollectionPathJob(collection);
    connect(job, &KJob::result, this, &InfoCommand::onParentPathFetched);
    job->start();
}

static void writeInfo(const QString &tag, const QString &data)
{
    std::cout << qPrintable(QString(tag + ":").leftJustified(10));
    std::cout << "  ";
    std::cout << qPrintable(data);
    std::cout << std::endl;
}

static void writeInfo(const QString &tag, quint64 data)
{
    writeInfo(tag, QString::number(data));
}

void InfoCommand::onParentPathFetched(KJob *job)
{
    if (!checkJobResult(job)) return;
    CollectionPathJob *pathJob = qobject_cast<CollectionPathJob *>(job);
    Q_ASSERT(pathJob != nullptr);
    const QString parentString = pathJob->formattedCollectionPath();

    // Finally we have fetched all of the information to display.

    std::cout << std::endl;
    if (mInfoCollection != nullptr) {			// for a collection
        Q_ASSERT(mInfoCollection->isValid());

        writeInfo(i18nc("@info:shell", "ID"), QString::number(mInfoCollection->id()));
        writeInfo(i18nc("@info:shell", "URL"), mInfoCollection->url().toDisplayString());
        writeInfo(i18nc("@info:shell", "Parent"), parentString);
        writeInfo(i18nc("@info:shell", "Type"), i18nc("@info:shell entity type", "Collection"));
        writeInfo(i18nc("@info:shell", "Name"), mInfoCollection->name());
        writeInfo(i18nc("@info:shell", "Owner"), mInfoCollection->resource());
        writeInfo(i18nc("@info:shell", "MIME"), mInfoCollection->contentMimeTypes().join(" "));
        writeInfo(i18nc("@info:shell", "Remote ID"), mInfoCollection->remoteId());

        QStringList rightsList;
        Collection::Rights rights = mInfoCollection->rights();
        if (rights & Collection::ReadOnly) {
            rightsList << i18nc("@info:shell", "ReadOnly");
        }
        if (rights & Collection::CanChangeItem) {
            rightsList << i18nc("@info:shell", "ChangeItem");
        }
        if (rights & Collection::CanCreateItem) {
            rightsList << i18nc("@info:shell", "CreateItem");
        }
        if (rights & Collection::CanDeleteItem) {
            rightsList << i18nc("@info:shell", "DeleteItem");
        }
        if (rights & Collection::CanChangeCollection) {
            rightsList << i18nc("@info:shell", "ChangeColl");
        }
        if (rights & Collection::CanCreateCollection) {
            rightsList << i18nc("@info:shell", "CreateColl");
        }
        if (rights & Collection::CanDeleteCollection) {
            rightsList << i18nc("@info:shell", "DeleteColl");
        }
        if (rights & Collection::CanLinkItem) {
            rightsList << i18nc("@info:shell", "LinkItem");
        }
        if (rights & Collection::CanUnlinkItem) {
            rightsList << i18nc("@info:shell", "UnlinkItem");
        }
        writeInfo(i18nc("@info:shell", "Rights"), rightsList.join(" "));

        Q_ASSERT(mInfoStatistics != nullptr);
        writeInfo(i18nc("@info:shell", "Count"), QLocale::system().toString(mInfoStatistics->count()));
        writeInfo(i18nc("@info:shell", "Unread"), QLocale::system().toString(mInfoStatistics->unreadCount()));
#ifdef USE_KIO_CONVERTSIZE
        const QString size = KIO::convertSize(mInfoStatistics->size());
#else
        const QString size = QLocale::system().formattedDataSize(mInfoStatistics->size());
#endif
        writeInfo(i18nc("@info:shell", "Size"), size);
    } else if (mInfoItem != nullptr) {			// for an item
        writeInfo(i18nc("@info:shell", "ID"), QString::number(mInfoItem->id()));
        writeInfo(i18nc("@info:shell", "URL"), mInfoItem->url().toDisplayString());
        writeInfo(i18nc("@info:shell", "Parent"), parentString);
        writeInfo(i18nc("@info:shell", "Type"), i18nc("@info:shell entity type", "Item"));
        writeInfo(i18nc("@info:shell", "MIME"), mInfoItem->mimeType());
        // from kdepim/akonadiconsole/browserwidget.cpp BrowserWidget::setItem()
        writeInfo(i18nc("@info:shell", "Modified"), (mInfoItem->modificationTime().toString() + " UTC"));
        writeInfo(i18nc("@info:shell", "Revision"), mInfoItem->revision());
        writeInfo(i18nc("@info:shell", "Remote ID"), mInfoItem->remoteId());
        writeInfo(i18nc("@info:shell", "Payload"), (mInfoItem->hasPayload() ? i18nc("@info:shell", "yes") : i18nc("@info:shell", "no")));

        Item::Flags flags = mInfoItem->flags();
        QStringList flagDisp;
        foreach (const QByteArray &flag, flags) {
            flagDisp << flag;
        }
        if (flagDisp.isEmpty()) {
            flagDisp << i18nc("@info:shell", "(none)");
        }
        writeInfo(i18nc("@info:shell", "Flags"), flagDisp.join(" "));

        Tag::List tags = mInfoItem->tags();
        QStringList tagDisp;
        foreach (const Akonadi::Tag &tag, tags) {
            tagDisp << tag.url().url();
        }
        if (tagDisp.isEmpty()) {
            tagDisp << i18nc("@info:shell", "(none)");
        }
        writeInfo(i18nc("@info:shell", "Tags"), tagDisp.join(" "));

#ifdef USE_KIO_CONVERTSIZE
        const QString size = KIO::convertSize(mInfoItem->size());
#else
        const QString size = QLocale::system().formattedDataSize(mInfoItem->size());
#endif
        writeInfo(i18nc("@info:shell", "Size"), size);
    } else {                      // neither collection nor item?
        // should never happen
        writeInfo(i18nc("@info:shell", "Type"), i18nc("@info:shell entity type", "Unknown"));
    }

    processNext();
}
