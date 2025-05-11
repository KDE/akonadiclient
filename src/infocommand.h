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

#pragma once

#include "abstractcommand.h"

namespace Akonadi
{
class Collection;
class Item;
class CollectionStatistics;
};

class KJob;

class InfoCommand : public AbstractCommand
{
    Q_OBJECT

public:
    explicit InfoCommand(QObject *parent = nullptr);
    ~InfoCommand() override;

    [[nodiscard]] QString name() const override;

public Q_SLOTS:
    void start() override;

protected:
    void setupCommandOptions(QCommandLineParser *parser) override;
    AbstractCommand::Error initCommand(QCommandLineParser *parser) override;

private:
    bool mJsonOutput = false;
    Akonadi::Collection *mInfoCollection = nullptr;
    Akonadi::Item *mInfoItem = nullptr;
    Akonadi::CollectionStatistics *mInfoStatistics = nullptr;

private:
    void fetchStatistics();
    void fetchItems();
    void fetchParentPath(const Akonadi::Collection &collection);

private Q_SLOTS:
    void infoForNext();

    void onBaseFetched(KJob *job);
    void onStatisticsFetched(KJob *job);
    void onItemsFetched(KJob *job);
    void onParentPathFetched(KJob *job);
};
