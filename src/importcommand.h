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

#ifndef IMPORTCOMMAND_H
#define IMPORTCOMMAND_H

#include "abstractcommand.h"

#include <Akonadi/Collection>
#include <Akonadi/Item>

namespace Akonadi
{
class XmlDocument;
};

class QFile;
class KJob;

class ImportCommand : public AbstractCommand
{
    Q_OBJECT

public:
    explicit ImportCommand(QObject *parent = nullptr);
    ~ImportCommand() override;

    QString name() const override;

public Q_SLOTS:
    void start() override;

protected:
    int initCommand(QCommandLineParser *parser) override;
    void setupCommandOptions(QCommandLineParser *parser) override;

private:
    Akonadi::Collection mParentCollection;
    Akonadi::Collection mCurrentCollection;
    QVector<Akonadi::Item> mItemQueue;
    QVector<Akonadi::Collection> mCollections;
    QMap<QString, Akonadi::Collection> mCollectionMap;
    Akonadi::XmlDocument *mDocument;

private Q_SLOTS:
    void onCollectionCreated(KJob *job);
    void onCollectionFetched(KJob *job);
    void onChildrenFetched(KJob *job);
    void onItemCreated(KJob *job);
    void onParentFetched(KJob *job);
    void processNextCollectionFromMap();
    void processNextCollection();
    void processNextItemFromQueue();
};

#endif // IMPORTCOMMAND_H
