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

#ifndef DELETECOMMAND_H
#define DELETECOMMAND_H

#include "abstractcommand.h"

namespace Akonadi
{
class Collection;
class Item;
class CollectionDeleteJob;
};

class CollectionResolveJob;
class KJob;

class DeleteCommand :  public AbstractCommand
{
    Q_OBJECT

public:
    explicit DeleteCommand(QObject *parent = nullptr);
    virtual ~DeleteCommand();

    QString name() const override;

public Q_SLOTS:
    void start() override;

protected:
    int initCommand(QCommandLineParser *parser) override;
    void setupCommandOptions(QCommandLineParser *parser) override;

private:
    Akonadi::CollectionDeleteJob *mDeleteJob;
    CollectionResolveJob *mResolveJob;
    bool mDryRun;
    bool mIsCollection;
    bool mIsItem;
    QString mEntityArg;

private:
    void fetchItems();

private Q_SLOTS:
    void onBaseFetched(KJob *job);
    void onCollectionDeleted(KJob *job);
    void onItemsDeleted(KJob *job);
    void onItemsFetched(KJob *job);
};

#endif // DELETECOMMAND_H
