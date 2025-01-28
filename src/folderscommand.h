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

#include "abstractcommand.h"

#include <qregularexpression.h>

#include <Akonadi/Collection>
using namespace Akonadi;

class QSaveFile;
class QFileDevice;

class KJob;

///////////////////////////////////////////////////////////////////////////

class ChangeData
{
public:
    /**
     * Construct a change data item intended to match a group name.
     **/
    explicit ChangeData(const QString &groupPattern)
        : mGroupPattern(QRegularExpression::anchoredPattern(groupPattern))
        , mIsList(false)
    {
    }

    /**
     * Construct a change data item intended to match a key and value
     * within a group.
     **/
    explicit ChangeData(const QString &groupPattern, const QString &keyPattern, const QString &valuePattern = QString())
        : mGroupPattern(QRegularExpression::anchoredPattern(groupPattern))
        , mKeyPattern(QRegularExpression::anchoredPattern(keyPattern))
        , mValuePattern(QRegularExpression::anchoredPattern(!valuePattern.isEmpty() ? valuePattern : "(\\d+)"))
        , mIsList(false)
    {
    }

    ~ChangeData() = default;

    void setIsListValue(bool isList)
    {
        mIsList = isList;
    }
    bool isListValue() const
    {
        return (mIsList);
    }

    QRegularExpression groupPattern() const
    {
        return (mGroupPattern);
    }
    QRegularExpression keyPattern() const
    {
        return (mKeyPattern);
    }
    QRegularExpression valuePattern() const
    {
        return (mValuePattern);
    }

    // Not sure whether a default-constructed QRegularExpresssion
    // or one constructed with an empty string counts as invalid,
    // so test for the presence of a pattern.
    bool isValueChange() const
    {
        return (!mKeyPattern.pattern().isEmpty());
    }

private:
    QRegularExpression mGroupPattern;
    QRegularExpression mKeyPattern;
    QRegularExpression mValuePattern;
    bool mIsList;
};

///////////////////////////////////////////////////////////////////////////

class FoldersCommand : public AbstractCommand
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
    void fetchCollections();
    void processChanges();
    bool readOrSaveLists();
    void processRestore();

    void getCurrentPaths(const Collection::List &colls);
    void saveCurrentPaths(QSaveFile *file);
    void readSavedPaths(QFileDevice *file, QMap<Collection::Id, QString> *pathMap);
    int checkForChanges();

    QString findSaveFile(const QString &name, bool createDir);
    QStringList allConfigFiles();

    void populateChangeData();
    QList<ChangeData> findChangesFor(const QString &file);

private:
    FoldersCommand::Mode mOperationMode;

    QMap<Collection::Id, QString> mOrigPathMap; // coll ID -> original path
    QMap<Collection::Id, QString> mCurPathMap; // coll ID -> current path
    QMap<Collection::Id, Collection::Id> mChangeMap; // original coll -> current coll

    QMultiMap<QString, ChangeData> mChangeData;

private Q_SLOTS:
    void onCollectionsFetched(KJob *job);
};
