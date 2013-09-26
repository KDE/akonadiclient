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


#ifndef EXPANDCOMMAND_H
#define EXPANDCOMMAND_H

#include "abstractcommand.h"

namespace Akonadi
{
  class Item;
};


class CollectionResolveJob;
class KJob;

class ExpandCommand : public AbstractCommand
{
  Q_OBJECT

  public:
    explicit ExpandCommand(QObject *parent = 0);
    ~ExpandCommand();

    QString name() const	{ return (QLatin1String("expand")); }

  public Q_SLOTS:
    void start();

  protected:
    void setupCommandOptions(KCmdLineOptions &options);
    int initCommand(KCmdLineArgs *parsedArgs);

  private:
    CollectionResolveJob *mResolveJob;

    QString mItemArg;
    Akonadi::Item *mExpandItem;
    bool mBriefMode;

  private:
    void fetchItems();

  private Q_SLOTS:
    void onItemsFetched(KJob *job);
};

#endif							// EXPANDCOMMAND_H
