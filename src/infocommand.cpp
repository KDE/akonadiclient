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

#include "collectionresolvejob.h"
#include "collectionpathjob.h"

#include <akonadi/itemfetchjob.h>
#include <akonadi/itemfetchscope.h>
#include <akonadi/collectionstatisticsjob.h>
#include <akonadi/collectionstatistics.h>

#include <akonadi/private/collectionpathresolver_p.h>	// just for error code

#include <qdatetime.h>

#include <kcmdlineargs.h>
#include <kglobal.h>
#include <kurl.h>

#include <iostream>

using namespace Akonadi;


InfoCommand::InfoCommand(QObject *parent)
  : AbstractCommand(parent),
    mResolveJob(0),
    mInfoCollection(0),
    mInfoItem(0),
    mInfoStatistics(0)
{
  mShortHelp = ki18nc("@info:shell", "Show full information for a collection or item").toString();
}

InfoCommand::~InfoCommand()
{
  delete mInfoItem;
  delete mInfoCollection;
  delete mInfoStatistics;
}


void InfoCommand::setupCommandOptions(KCmdLineOptions &options)
{
  AbstractCommand::setupCommandOptions(options);

  options.add("+[options]", ki18nc("@info:shell", "Options for command"));
  options.add("+collection|item", ki18nc("@info:shell", "The collection or item"));
  options.add(":", ki18nc("@info:shell", "Options for command:"));
  options.add("c").add("collection", ki18nc("@info:shell", "Assume that a collection is specified"));
  options.add("i").add("item", ki18nc("@info:shell", "Assume that an item is specified"));
}

int InfoCommand::initCommand(KCmdLineArgs *parsedArgs)
{
  if (parsedArgs->count()<2)
  {
    emitErrorSeeHelp(ki18nc("@info:shell", "Missing collection/item argument"));
    return InvalidUsage;
  }

  mIsItem = parsedArgs->isSet("item");
  mIsCollection = parsedArgs->isSet("collection");
  if (mIsItem && mIsCollection)
  {
    emit error(i18nc("@info:shell", "Cannot specify as both an item and collection"));
    return InvalidUsage;
  }

  mEntityArg = parsedArgs->arg(1);
  mResolveJob = new CollectionResolveJob(mEntityArg, this);

  if (!mResolveJob->hasUsableInput())
  {
    emit error(mResolveJob->errorString());
    delete mResolveJob;
    mResolveJob = 0;
    return InvalidUsage;
  }

  return NoError;
}


void InfoCommand::start()
{
  Q_ASSERT(mResolveJob!=0);

  if (mIsItem)						// user forced as an item
  {
    fetchItems();					// do this immediately
  }
  else
  {
    // User specified that the input is a collection, or
    // didn't specify at all what sort of entity it is.
    // First try to resolve it as a collection.
    connect(mResolveJob, SIGNAL(result(KJob *)), SLOT(onBaseFetched(KJob *)));
    mResolveJob->start();
  }
}


void InfoCommand::onBaseFetched(KJob *job)
{
  Q_ASSERT(job==mResolveJob);

  if (job->error()!=0)
  {
    if (job->error()==CollectionPathResolver::Unknown)
    {							// failed to resolve as collection
      if (!mIsCollection)				// not forced as a collection
      {
        fetchItems();					// try it as an item
        return;
      }
    }

    emit error(job->errorString());
    emit finished(RuntimeError);
    return;
  }

  if (mResolveJob->collection()==Collection::root())
  {
    emit error(i18nc("@info:shell", "No information available for collection root"));
    emit finished(RuntimeError);
    return;
  }

  fetchStatistics();
}


void InfoCommand::fetchStatistics()
{
  Q_ASSERT(mResolveJob!=0 && mResolveJob->collection().isValid());

  CollectionStatisticsJob *job = new CollectionStatisticsJob(mResolveJob->collection(), this);
  connect(job, SIGNAL(result(KJob *)), SLOT(onStatisticsFetched(KJob *)));
}

void InfoCommand::onStatisticsFetched(KJob *job)
{
  if (job->error() != 0)
  {
    emit error(job->errorString());
    emit finished(RuntimeError);
    return;
  }

  CollectionStatisticsJob *statsJob = qobject_cast<CollectionStatisticsJob *>(job);
  Q_ASSERT(statsJob!=0);
  mInfoStatistics = new CollectionStatistics(statsJob->statistics());

  mInfoCollection = new Collection(mResolveJob->collection());
  fetchParentPath(mInfoCollection->parentCollection());
}


void InfoCommand::fetchItems()
{
  Item item;
  // See if user input is a valid integer as an item ID
  bool ok;
  int id = mEntityArg.toInt(&ok);
  if (ok) item = Item(id);				// conversion succeeded
  else
  {
    // Otherwise check if we have an Akonadi URL
    const KUrl url = QUrl::fromUserInput(mEntityArg);
    if (url.isValid() && url.scheme()==QLatin1String("akonadi"))
    {
      item = Item::fromUrl(url);
    }
    else
    {
      emit error(i18nc("@info:shell", "Invalid item/collection syntax"));
      emit finished(RuntimeError);
      return;
    }
  }

  ItemFetchJob *job = new ItemFetchJob(item, this);
  job->fetchScope().setFetchModificationTime(true);
  job->fetchScope().fetchAllAttributes(true);

  // Need this so that parentCollection() will be valid.
  job->fetchScope().setAncestorRetrieval(ItemFetchScope::Parent);

  // Not actually going to use the payload here, but if we don't set it
  // to be fetched then hasPayload() will not return a meaningful result.
  job->fetchScope().fetchFullPayload(true);

  connect(job, SIGNAL(result(KJob *)), SLOT(onItemsFetched(KJob *)));
}

void InfoCommand::onItemsFetched(KJob *job)
{
  if (job->error() != 0) {
    emit error(job->errorString());
    emit finished(RuntimeError);
    return;
  }

  ItemFetchJob *fetchJob = qobject_cast<ItemFetchJob *>(job);
  Q_ASSERT(fetchJob!=0);
  Item::List items = fetchJob->items();
  if (items.count()<1)
  {
    emit error(i18nc("@info:shell", "Cannot find '%1' as a collection or item", mEntityArg));
    emit finished(RuntimeError);
    return;
  }

  mInfoItem = new Item(items.first());
  fetchParentPath(mInfoItem->parentCollection());
}


void InfoCommand::fetchParentPath(const Akonadi::Collection &collection)
{
  Q_ASSERT(mInfoCollection!=0 || mInfoItem!=0);

  CollectionPathJob *job = new CollectionPathJob(collection);
  connect(job, SIGNAL(result(KJob *)), SLOT(onParentPathFetched(KJob *)));
  job->start();
}


static void writeInfo(const QString &tag, const QString &data)
{
  std::cout << (tag+":").leftJustified(10).toLocal8Bit().constData();
  std::cout << "  ";
  std::cout << data.toLocal8Bit().constData();
  std::cout << std::endl;
}

static void writeInfo(const QString &tag, quint64 data)
{
  writeInfo(tag, QString::number(data));
}


void InfoCommand::onParentPathFetched(KJob *job)
{
  if (job->error()!=0)
  {
    emit error(job->errorString());
    emit finished(RuntimeError);
    return;
  }

  // Finally we have fetched all of the information to display.

  CollectionPathJob *pathJob = qobject_cast<CollectionPathJob *>(job);
  Q_ASSERT(pathJob!=0);
  const QString parentString = pathJob->formattedCollectionPath();

  if (mInfoCollection!=0)				// for a collection
  {
    Q_ASSERT(mInfoCollection->isValid());

    writeInfo(i18nc("@info:shell", "ID"), mInfoCollection->id());
    writeInfo(i18nc("@info:shell", "URL"), mInfoCollection->url().pathOrUrl());
    writeInfo(i18nc("@info:shell", "Parent"), parentString);
    writeInfo(i18nc("@info:shell", "Type"), i18nc("@info:shell entity type", "Collection"));
    writeInfo(i18nc("@info:shell", "Name"), mInfoCollection->name());
    writeInfo(i18nc("@info:shell", "Owner"), mInfoCollection->resource());
    writeInfo(i18nc("@info:shell", "MIME"), mInfoCollection->contentMimeTypes().join(" "));

    QStringList rightsList;
    Collection::Rights rights = mInfoCollection->rights();
    if (rights & Collection::ReadOnly) rightsList << i18nc("@info:shell", "ReadOnly");
    if (rights & Collection::CanChangeItem) rightsList << i18nc("@info:shell", "ChangeItem");
    if (rights & Collection::CanCreateItem) rightsList << i18nc("@info:shell", "CreateItem");
    if (rights & Collection::CanDeleteItem) rightsList << i18nc("@info:shell", "DeleteItem");
    if (rights & Collection::CanChangeCollection) rightsList << i18nc("@info:shell", "ChangeColl");
    if (rights & Collection::CanCreateCollection) rightsList << i18nc("@info:shell", "CreateColl");
    if (rights & Collection::CanDeleteCollection) rightsList << i18nc("@info:shell", "DeleteColl");
    if (rights & Collection::CanLinkItem) rightsList << i18nc("@info:shell", "LinkItem");
    if (rights & Collection::CanUnlinkItem) rightsList << i18nc("@info:shell", "UnlinkItem");
    writeInfo(i18nc("@info:shell", "Rights"), rightsList.join(" "));

    Q_ASSERT(mInfoStatistics!=0);
    writeInfo(i18nc("@info:shell", "Count"), KGlobal::locale()->formatNumber(mInfoStatistics->count(), 0));
    writeInfo(i18nc("@info:shell", "Unread"), KGlobal::locale()->formatNumber(mInfoStatistics->unreadCount(), 0));
    writeInfo(i18nc("@info:shell", "Size"), KGlobal::locale()->formatByteSize(mInfoStatistics->size()));
  }
  else if (mInfoItem!=0)				// for an item
  {
    writeInfo(i18nc("@info:shell", "ID"), mInfoItem->id());
    writeInfo(i18nc("@info:shell", "URL"), mInfoItem->url().pathOrUrl());
    writeInfo(i18nc("@info:shell", "Parent"), parentString);
    writeInfo(i18nc("@info:shell", "Type"), i18nc("@info:shell entity type", "Item"));
    writeInfo(i18nc("@info:shell", "MIME"), mInfoItem->mimeType());
    // from kdepim/akonadiconsole/browserwidget.cpp BrowserWidget::setItem()
    writeInfo(i18nc("@info:shell", "Modified"), (mInfoItem->modificationTime().toString()+" UTC"));
    writeInfo(i18nc("@info:shell", "Revision"), mInfoItem->revision());
    writeInfo(i18nc("@info:shell", "Remote ID"), mInfoItem->remoteId());
    writeInfo(i18nc("@info:shell", "Payload"), (mInfoItem->hasPayload() ? i18nc("@info:shell", "yes") : i18nc("@info:shell", "no")));

    Item::Flags flags = mInfoItem->flags();
    QStringList flagDisp;
    foreach (const QByteArray &flag, flags)
    {
      flagDisp << flag;
    }
    if (flagDisp.isEmpty()) flagDisp << i18nc("@info:shell", "(none)");
    writeInfo(i18nc("@info:shell", "Flags"), flagDisp.join(" "));

    writeInfo(i18nc("@info:shell", "Size"), KGlobal::locale()->formatByteSize(mInfoItem->size()));
  }
  else							// neither collection nor item?
  {							// should never happen
    writeInfo(i18nc("@info:shell", "Type"), i18nc("@info:shell entity type", "Unknown"));
  }

  emit finished(NoError);
}
