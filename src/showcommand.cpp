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

#include "showcommand.h"

#include "collectionresolvejob.h"

#include <AkonadiCore/itemfetchjob.h>
#include <AkonadiCore/itemfetchscope.h>

#include <kcmdlineargs.h>
#include <kurl.h>

#include <iostream>

#include "commandfactory.h"

using namespace Akonadi;


DEFINE_COMMAND("show", ShowCommand, "Show the raw payload of an item");


ShowCommand::ShowCommand(QObject *parent)
  : AbstractCommand(parent)
{
}


ShowCommand::~ShowCommand()
{
}


void ShowCommand::setupCommandOptions(KCmdLineOptions &options)
{
  AbstractCommand::setupCommandOptions(options);

  options.add("+item...", ki18nc("@info:shell", "The items to show"));
}


int ShowCommand::initCommand(KCmdLineArgs *parsedArgs)
{
  if (parsedArgs->count()<2)
  {
    emitErrorSeeHelp(ki18nc("@show:shell", "No items specified"));
    return InvalidUsage;
  }

  for (int i = 1; i<parsedArgs->count(); ++i)
  {
    mItemArgs.append(parsedArgs->arg(i));
  }

  return NoError;
}


void ShowCommand::start()
{
  mExitStatus = NoError;				// not yet, anyway
  processNextItem();					// start off the process
}


void ShowCommand::processNextItem()
{
  if (mItemArgs.isEmpty())				// any more items?
  {							// no, all done
    emit finished(mExitStatus);
    return;
  }
  QString arg = mItemArgs.takeFirst();

  Item item = CollectionResolveJob::parseItem(arg, true);
  if (!item.isValid())
  {
    mExitStatus = RuntimeError;
    QMetaObject::invokeMethod(this, "processNextItem", Qt::QueuedConnection);
    return;
  }

  ItemFetchJob *job = new ItemFetchJob(item, this);
  job->fetchScope().setFetchModificationTime(false);
  job->fetchScope().fetchAllAttributes(false);
  job->fetchScope().fetchFullPayload(true);
  job->setProperty("arg", arg);
  connect(job, SIGNAL(result(KJob *)), SLOT(onItemFetched(KJob *)));
}


void ShowCommand::onItemFetched(KJob *job)
{
  if (job->error()!=0)
  {
    emit error(job->errorString());
    mExitStatus = RuntimeError;
  }
  else
  {
    ItemFetchJob *fetchJob = qobject_cast<ItemFetchJob *>(job);
    Q_ASSERT(fetchJob!=nullptr);
    Item::List items = fetchJob->items();
    if (items.count()<1)
    {
      emit error(i18nc("@info:shell", "No result returned for item '%1'", job->property("arg").toString()));
      mExitStatus = RuntimeError;
    }
    else
    {
      Akonadi::Item item = items.first();
      if (!item.hasPayload())
      {
        emit error(i18nc("@info:shell", "Item '%1' has no payload", job->property("arg").toString()));
        mExitStatus = RuntimeError;
      }
      else
      {
        std::cout << item.payloadData().constData();	// output the raw payload
        if (!mItemArgs.isEmpty())			// not the last item
        {
          std::cout << "\n";				// blank line to separate
        }
      }
    }
  }

  QMetaObject::invokeMethod(this, "processNextItem", Qt::QueuedConnection);
}
