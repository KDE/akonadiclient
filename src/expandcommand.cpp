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

#include "expandcommand.h"

#include "collectionresolvejob.h"

#include <akonadi/itemfetchjob.h>
#include <akonadi/itemfetchscope.h>

#include <kabc/addressee.h>
#include <kabc/contactgroup.h>

#include <kcmdlineargs.h>
#include <kglobal.h>
#include <kurl.h>

#include <iostream>

using namespace Akonadi;


ExpandCommand::ExpandCommand(QObject *parent)
  : AbstractCommand(parent),
    mResolveJob(0),
    mExpandItem(0),
    mBriefMode(false)
{
  mShortHelp = ki18nc("@info:shell", "Expand a contact group item").toString();
}


ExpandCommand::~ExpandCommand()
{
  delete mExpandItem;
}


void ExpandCommand::setupCommandOptions(KCmdLineOptions &options)
{
  AbstractCommand::setupCommandOptions(options);

  addOptionsOption(options);
  options.add("+item", ki18nc("@info:shell", "The contact group item"));
  addOptionSeparator(options);
  options.add("b").add("brief", ki18nc("@info:shell", "Brief output (email addresses only)"));
}


int ExpandCommand::initCommand(KCmdLineArgs *parsedArgs)
{
  if (parsedArgs->count()<2)
  {
    emitErrorSeeHelp(ki18nc("@info:shell", "Missing item argument"));
    return (InvalidUsage);
  }

  mBriefMode = parsedArgs->isSet("brief");

  mItemArg = parsedArgs->arg(1);
  mResolveJob = new CollectionResolveJob(mItemArg, this);

  if (!mResolveJob->hasUsableInput())
  {
    emit error(mResolveJob->errorString());
    delete mResolveJob;
    mResolveJob = 0;
    return (InvalidUsage);
  }

  return (NoError);
}


void ExpandCommand::start()
{
  Q_ASSERT(mResolveJob!=0);
  fetchItems();
}


void ExpandCommand::fetchItems()
{
  Item item;
  // See if user input is a valid integer as an item ID
  bool ok;
  int id = mItemArg.toInt(&ok);
  if (ok) item = Item(id);				// conversion succeeded
  else
  {
    // Otherwise check if we have an Akonadi URL
    const KUrl url = QUrl::fromUserInput(mItemArg);
    if (url.isValid() && url.scheme()==QLatin1String("akonadi"))
    {
      item = Item::fromUrl(url);
    }
    else
    {
      emit error(i18nc("@info:shell", "Invalid item syntax '%1'", mItemArg));
      emit finished(RuntimeError);
      return;
    }
  }

  ItemFetchJob *job = new ItemFetchJob(item, this);
  job->fetchScope().fetchFullPayload(true);

  connect(job, SIGNAL(result(KJob *)), SLOT(onItemsFetched(KJob *)));
}


static void writeColumn(const QString &data, int width = 0)
{
  std::cout << data.leftJustified(width).toLocal8Bit().constData() << "  ";
}

static void writeColumn(quint64 data, int width = 0)
{
    writeColumn(QString::number(data), width);
}


void ExpandCommand::onItemsFetched(KJob *job)
{
  if (job->error() != 0)
  {
    emit error(job->errorString());
    emit finished(RuntimeError);
    return;
  }

  ItemFetchJob *fetchJob = qobject_cast<ItemFetchJob *>(job);
  Q_ASSERT(fetchJob!=0);
  Item::List items = fetchJob->items();
  if (items.count()<1)
  {
    emit error(i18nc("@info:shell", "Cannot find '%1' as an item", mItemArg));
    emit finished(RuntimeError);
    return;
  }

  mExpandItem = new Item(items.first());
  if (mExpandItem->mimeType()!=KABC::ContactGroup::mimeType())
  {
    emit error(i18nc("@info:shell", "Item '%1' is not a contact group", mItemArg));
    emit finished(RuntimeError);
    return;
  }

  if (!mExpandItem->hasPayload<KABC::ContactGroup>())	// should never happen?
  {
    emit error(i18nc("@info:shell", "Item '%1' has no contact group payload", mItemArg));
    emit finished(RuntimeError);
    return;
  }

  KABC::ContactGroup group = mExpandItem->payload<KABC::ContactGroup>();

  if (!mBriefMode)
  {
    std::cout << i18nc( "@info:shell section header 1=item 2=groupref count 3=ref count 4=data count 5=name",
                        "Item %1 \"%5\" has %2 groups, %3 references and %4 data items:",
                        QString::number(mExpandItem->id()),
                        group.contactGroupReferenceCount(),
                        group.contactReferenceCount(),
                        group.dataCount(),
                        group.name()).toLocal8Bit().constData()
              << std::endl;

    std::cout << " ";
    writeColumn(i18nc("@info:shell column header", "Type"), 4);
    writeColumn(i18nc("@info:shell column header", "ID"), 8);
    writeColumn(i18nc("@info:shell column header", "Email"), 30);
    writeColumn(i18nc("@info:shell column header", "Name"));
    std::cout << std::endl;
  }

  int c = group.contactGroupReferenceCount();
  for (int i = 0; i<c; ++i)
  {
    if (mBriefMode)
    {
      continue;
    }
    else
    {
      writeColumn("  G", 5);
      writeColumn(group.contactGroupReference(i).uid(), 8);
    }
    std::cout << std::endl;
  }

  c = group.contactReferenceCount();
  QList<Item::Id> fetchIds;
  for (int i = 0; i<c; ++i)
  {
    Item::Id id = group.contactReference(i).uid().toInt();
    fetchIds.append(id);
  }

  if (!fetchIds.isEmpty())
  {
    ItemFetchJob *itemJob = new ItemFetchJob(fetchIds, this);
    itemJob->fetchScope().setFetchModificationTime(false);
    itemJob->fetchScope().fetchAllAttributes(false);
    itemJob->fetchScope().fetchFullPayload(true);

    itemJob->exec();
    if (itemJob->error()!=0)
    {
      std::cout << std::endl;
      emit error(itemJob->errorString());
      emit finished(RuntimeError);
      return;
    }
    else
    {
      Item::List fetchedItems = itemJob->items();
      if (fetchedItems.isEmpty())
      {
        emit error(i18nc("@info:shell", "No items could be fetched"));
        emit finished(RuntimeError);
        return;
      }
      else
      {
        for (Item::List::const_iterator it = fetchedItems.constBegin();
             it != fetchedItems.constEnd(); ++it)
        {
          const Item item = (*it);

          if (!mBriefMode)
          {
            writeColumn("  R", 5);
            writeColumn(item.id(), 8);
          }

          if (!item.hasPayload<KABC::Addressee>())
          {
            std::cout << i18nc("@info:shell", "Item has no Addressee payload").toLocal8Bit().constData();
          }
          else
          {
            KABC::Addressee addr = item.payload<KABC::Addressee>();
            QString email = addr.preferredEmail();

            // Retrieve the original preferred email from the contact group reference.
            // If there is one, display that;  if not, the contact's preferred email.
            for (int i = 0; i<c; ++i)
            {
              Item::Id id = group.contactReference(i).uid().toInt();
              if (id==item.id())
              {
                const QString prefEmail = group.contactReference(i).preferredEmail();
                if (!prefEmail.isEmpty()) email = prefEmail;
                break;
              }
            }

            if (mBriefMode)
            {
              std::cout << email.toLocal8Bit().constData();
            }
            else
            {
              writeColumn(email, 30);
              writeColumn(addr.formattedName());
            }
          }
          std::cout << std::endl;
        }
      }
    }
  }

  c = group.dataCount();
  for (int i = 0; i<c; ++i)
  {
    if (!mBriefMode)
    {
      writeColumn("  D", 5);
      writeColumn("", 8);
    }

    const KABC::ContactGroup::Data data = group.data(i);
    if (mBriefMode)
    {
      std::cout << data.email().toLocal8Bit().constData();
    }
    else
    {
      writeColumn(data.email(), 30);
      writeColumn(data.name());
    }
    std::cout << std::endl;
  }

  emit finished(NoError);
}
