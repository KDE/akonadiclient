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


#ifndef INFOCOMMAND_H
#define INFOCOMMAND_H

#include "abstractcommand.h"

namespace Akonadi
{
  class Collection;
  class Item;
  class CollectionStatistics;
};


class CollectionResolveJob;
class KJob;

class InfoCommand : public AbstractCommand
{
  Q_OBJECT

  public:
    explicit InfoCommand(QObject *parent = 0);
    ~InfoCommand();

    QString name() const	{ return (QLatin1String("info")); }

  public Q_SLOTS:
    void start();

  protected:
    void setupCommandOptions(KCmdLineOptions &options);
    int initCommand(KCmdLineArgs *parsedArgs);

  private:
    CollectionResolveJob *mResolveJob;
    bool mIsCollection;
    bool mIsItem;

    QString mEntityArg;
    Akonadi::Collection *mInfoCollection;
    Akonadi::Item *mInfoItem;
    Akonadi::CollectionStatistics *mInfoStatistics;

  private:
    void fetchStatistics();
    void fetchItems();
    void fetchParentPath(const Akonadi::Collection &collection);

  private Q_SLOTS:
    void onBaseFetched(KJob *job);
    void onStatisticsFetched(KJob *job);
    void onItemsFetched(KJob *job);
    void onParentPathFetched(KJob *job);
};

#endif							// INFOCOMMAND_H
