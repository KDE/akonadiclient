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

#ifndef COPYCOMMAND_H
#define COPYCOMMAND_H

#include "abstractcommand.h"

#include <akonadi/collection.h>

class KJob;

class CopyCommand : public AbstractCommand
{
    Q_OBJECT

public:
    explicit CopyCommand(QObject *parent = nullptr);
    virtual ~CopyCommand() = default;

    QString name() const override;

public Q_SLOTS:
    virtual void start() override;

protected:
    bool mMoving;

protected:
    virtual void setupCommandOptions(QCommandLineParser *parser) override;
    virtual int initCommand(QCommandLineParser *parser) override;

private:
    QString mDestinationArg;
    QStringList mSourceArgs;
    bool mAnyErrors;
    Akonadi::Collection mDestinationCollection;
    Akonadi::Collection mSourceCollection;
    Akonadi::Collection::List mSubCollections;

private:
    void doNextSource();
    void doNextSubcollection(const QString &sourceArg);
    void fetchItems(const QString &sourceArg);

private Q_SLOTS:
    void onTargetFetched(KJob *job);
    void onSourceResolved(KJob *job);
    void onRecursiveCopyFinished(KJob *job);
    void onCollectionCopyFinished(KJob *job);
    void onItemCopyFinished(KJob *job);
    void onCollectionsFetched(KJob *job);
    void onItemsFetched(KJob *job);

    void processNextSource();
    void processNextSubcollection(const QString &sourceArg);

};

#endif                          // COPYCOMMAND_H
