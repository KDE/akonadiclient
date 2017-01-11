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

#ifndef GROUPCOMMAND_H
#define GROUPCOMMAND_H

#include "abstractcommand.h"

#include <qstringlist.h>
#include <AkonadiCore/item.h>
#include <kcontacts/contactgroup.h>

class KJob;

class GroupCommand : public AbstractCommand
{
    Q_OBJECT

public:
    explicit GroupCommand(QObject *parent = 0);
    ~GroupCommand();

    QString name() const;

public Q_SLOTS:
    void start();

protected:
    void setupCommandOptions(KCmdLineOptions &options);
    int initCommand(KCmdLineArgs *parsedArgs);

private:
    enum Mode {
        ModeExpand,
        ModeAdd,
        ModeDelete,
        ModeClean
    };

    Akonadi::Item *mGroupItem;

    QString mGroupArg;
    QString mNameArg;
    QStringList mItemArgs;
    bool mBriefMode;
    bool mDryRun;
    GroupCommand::Mode mOperationMode;

private:
    void fetchItems();

    void displayContactData(const KContacts::ContactGroup::Data &data);
    void displayContactReference(Akonadi::Item::Id id);
    void displayContactReference(const Akonadi::Item &item, const QString &email = QString::null);
    void displayReferenceError(Akonadi::Item::Id id);

    bool removeDataByEmail(KContacts::ContactGroup &group, const QString &email, bool verbose = false);
    bool removeReferenceById(KContacts::ContactGroup &group, const QString &id, bool verbose = false);

    AbstractCommand::Errors showExpandedGroup(const KContacts::ContactGroup &group);
    AbstractCommand::Errors addGroupItems(KContacts::ContactGroup &group);
    AbstractCommand::Errors deleteGroupItems(KContacts::ContactGroup &group);
    AbstractCommand::Errors cleanGroupItems(KContacts::ContactGroup &group);

private Q_SLOTS:
    void onItemsFetched(KJob *job);
};

#endif                          // GROUPCOMMAND_H
