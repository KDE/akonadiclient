/*
    Copyright (C) 2025  Jonathan Marten <jjm@keelhaul.me.uk>

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

#pragma once

#include "abstractcommand.h"

#include <Akonadi/Collection>
#include <Akonadi/CollectionFetchJob>
using namespace Akonadi;

class KJob;

class CollectionListCommand : public AbstractCommand
{
    Q_OBJECT

public:
    explicit CollectionListCommand(QObject *parent = nullptr);
    ~CollectionListCommand() override = default;

protected:
    void listCollections(CollectionFetchJob::Type type);
    void getCurrentPaths();

protected Q_SLOTS:
    virtual void onCollectionsListed() = 0;

private Q_SLOTS:
    void onCollectionsListedInternal(KJob *job);

protected:
    Collection::List mCollections;
    QMap<Collection::Id, QString> mCurPathMap; // coll ID -> current path
};
