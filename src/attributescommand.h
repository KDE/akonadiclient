/*
    Copyright (C) 2024-2025  Jonathan Marten <jjm@keelhaul.me.uk>

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

#include <Akonadi/Collection>
using namespace Akonadi;

class QFileDevice;
class KJob;

class AttributesCommand : public CollectionListCommand
{
    Q_OBJECT

public:
    explicit AttributesCommand(QObject *parent = nullptr);
    ~AttributesCommand() override = default;

    [[nodiscard]] QString name() const override;

public Q_SLOTS:
    void start() override;

protected:
    void setupCommandOptions(QCommandLineParser *parser) override;
    AbstractCommand::Error initCommand(QCommandLineParser *parser) override;

private:
    bool parseValue(const QString &arg, bool isHex);
    void saveCollectionAttributes(QFileDevice *file);
    void readSavedAttributes(QFileDevice *file);
    void processChanges();
    void processNextChange();

private:
    enum Mode {
        ModeShow,
        ModeAdd,
        ModeDelete,
        ModeModify,
        ModeBackup,
        ModeCheck,
        ModeRestore
    };

private Q_SLOTS:
    void onCollectionResolved(KJob *job);
    void onPathFetched(KJob *job);
    void onCollectionModified(KJob *job);
    void onAttributesModified(KJob *job);

    void onCollectionFetched(KJob *job);
    void processCollection();
    void onPathResolved(KJob *job);

    void onCollectionsListed() override;

private:
    AttributesCommand::Mode mOperationMode;

    QByteArray mCommandType;
    QByteArray mCommandValue;
    bool mHexOption;

    int mUpdatedCollectionCount;

    QMap<Collection::Id, QString> mOrigPathMap; // coll ID -> original path
    QMap<QPair<Collection::Id, QByteArray>, QByteArray> mOrigAttrMap; // (coll ID, attr name) -> attr value
    QList<QPair<Collection::Id, QByteArray>> mOrigAttrKeys; // keys of above
};
