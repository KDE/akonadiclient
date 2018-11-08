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

#ifndef ADDCOMMAND_H
#define ADDCOMMAND_H

#include "abstractcommand.h"

#include <AkonadiCore/Collection>

#include <QHash>
#include <QMap>
#include <QSet>
#include <QMimeType>

class CollectionResolveJob;
class KJob;

class AddCommand : public AbstractCommand
{
    Q_OBJECT

public:
    enum AddDirectoryMode {
        AddDirOnly = 0,
        AddRecursive
    };

    explicit AddCommand(QObject *parent = nullptr);
    virtual ~AddCommand() = default;

    QString name() const override;

public Q_SLOTS:
    void start() override;

protected:
    void setupCommandOptions(QCommandLineParser *parser) override;
    int initCommand(QCommandLineParser *parser) override;

private:
    CollectionResolveJob *mResolveJob;

    QSet<QString> mFiles;
    QMap<QString, AddDirectoryMode> mDirectories;
    QHash<QString, Akonadi::Collection> mCollectionsByPath;
    QString mBasePath;
    QHash<QString, QString> mBasePaths;
    bool mFlatMode;
    Akonadi::Collection mBaseCollection;
    QMimeType mMimeType;

private Q_SLOTS:
    void processNextDirectory();
    void processNextFile();
    void onTargetFetched(KJob *job);
    void onCollectionCreated(KJob *job);
    void onCollectionFetched(KJob *job);
    void onItemCreated(KJob *job);
};

#endif // ADDCOMMAND_H
