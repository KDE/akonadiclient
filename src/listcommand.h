/*
    Copyright (C) 2012  Kevin Krammer <krammer@kde.org>

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

class KJob;

class ListCommand : public AbstractCommand
{
    Q_OBJECT

public:
    explicit ListCommand(QObject *parent = nullptr);
    ~ListCommand() override = default;

    [[nodiscard]] QString name() const override;

public Q_SLOTS:
    void start() override;

protected:
    void setupCommandOptions(QCommandLineParser *parser) override;
    AbstractCommand::Error initCommand(QCommandLineParser *parser) override;

private:
    bool mListItems;
    bool mListCollections;
    bool mListDetails;
    bool mListRecursive;
    Akonadi::Collection::List mCollections; // needed for recursive list
    QString mBasePath; // only used for recursive list

private:
    void fetchCollections();
    void fetchItems();
    void processCollections();
    void writeCollection(const Akonadi::Collection::Id &id, const QString &nameOrPath);

private Q_SLOTS:
    void onBaseFetched(KJob *job);
    void onBaseResolved(KJob *job);
    void onCollectionsFetched(KJob *job);
    void onItemsFetched(KJob *job);
    void onParentPathFetched(KJob *job);
};
