/*
    Copyright (C) 2024 Jonathan Marten <jjm@keelhaul.me.uk>

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

#include "collectionlistcommand.h"

#include <qregularexpression.h>

#include <Akonadi/Collection>
using namespace Akonadi;

class QSaveFile;
class QFileDevice;
class KJob;
class ChangeData;

class FoldersCommand : public CollectionListCommand
{
    Q_OBJECT

public:
    explicit FoldersCommand(QObject *parent = nullptr);
    ~FoldersCommand() override = default;

    [[nodiscard]] QString name() const override;

public Q_SLOTS:
    void start() override;

protected:
    void setupCommandOptions(QCommandLineParser *parser) override;
    AbstractCommand::Error initCommand(QCommandLineParser *parser) override;

private:
    enum Mode {
        ModeBackup,
        ModeCheck,
        ModeRestore
    };

private:
    void processChanges();
    bool readOrSaveLists();
    void processRestore();

    void saveCurrentPaths(QSaveFile *file);
    void readSavedPaths(QFileDevice *file, QMap<Collection::Id, QString> *pathMap);
    int checkForChanges();

    QStringList allConfigFiles();

    void populateChangeData();
    QList<const ChangeData *> findChangesFor(const QString &file);

private:
    FoldersCommand::Mode mOperationMode;

    QMap<Collection::Id, QString> mOrigPathMap; // coll ID -> original path
    QMap<Collection::Id, Collection::Id> mChangeMap; // original coll -> current coll

    QMultiMap<QString, const ChangeData *> mChangeData;

private Q_SLOTS:
    void onCollectionsListed() override;
};
