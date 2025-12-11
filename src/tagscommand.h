/*
    Copyright (C) 2014  Jonathan Marten <jjm@keelhaul.me.uk>

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

#include <qmap.h>

#include <akonadi/tag.h>
using namespace Akonadi;

class KJob;
class QFileDevice;

class TagsCommand : public AbstractCommand
{
    Q_OBJECT

public:
    explicit TagsCommand(QObject *parent = nullptr);
    ~TagsCommand() override = default;

    [[nodiscard]] QString name() const override;

public Q_SLOTS:
    void start() override;

protected:
    void setupCommandOptions(QCommandLineParser *parser) override;
    AbstractCommand::Error initCommand(QCommandLineParser *parser) override;

private:
    enum Mode {
        ModeList,
        ModeAdd,
        ModeDelete,
        ModeBackup,
        ModeRestore,
    };

    bool mBriefOutput;
    bool mUrlsOutput;
    TagsCommand::Mode mOperationMode;
    int mAddForceId;
    bool mAddForceRetain;
    Akonadi::Tag::List mFetchedTags;

private:
    void listTags();
    void backupTags();
    void restoreTags();
    void readSavedTags(QFileDevice *file);

private Q_SLOTS:
    void onTagsFetched(KJob *job);
    void onTagAdded(KJob *job);
    void onTagDeleted(KJob *job);

    void addNextTag();
    void deleteNextTag();

private:
    QMap<Tag::Id, QString> mOrigTagMap;
};
