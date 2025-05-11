/*
    Copyright (C) 2025 Daniel Vrátil <dvratil@kde.org>

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

#include "jsonformatter.h"

#include <QJsonArray>
#include <QJsonObject>

#include <iostream>

QJsonObject JsonFormatter::itemToJson(const Akonadi::Item &item)
{
    QJsonObject obj;
    obj["id"] = item.id();
    obj["revision"] = item.revision();
    obj["remoteId"] = item.remoteId().isEmpty() ? QJsonValue::Null : QJsonValue(item.remoteId());
    obj["remoteRevision"] = item.remoteRevision().isEmpty() ? QJsonValue::Null : QJsonValue(item.remoteRevision());
    obj["gid"] = item.gid().isEmpty() ? QJsonValue::Null : QJsonValue(item.gid());
    obj["mimeType"] = item.mimeType();
    obj["size"] = item.size();
    obj["modificationTime"] = item.modificationTime().toString(Qt::ISODate);
    const auto flags = item.flags();
    QList<QString> flagList;
    for (const auto &flag : flags) {
        flagList.append(QString::fromLatin1(flag));
    }
    obj["flags"] = QJsonArray::fromStringList(flagList);
    return obj;
}

QJsonObject JsonFormatter::collectionToJson(const Akonadi::Collection &collection,
                                            const QHash<Akonadi::Collection::Id, Akonadi::Collection::List> &collectionTree,
                                            const QHash<Akonadi::Collection::Id, Akonadi::Item::List> &collectionItems)
{
    QJsonObject obj;
    obj["id"] = collection.id();
    obj["name"] = collection.name();
    obj["remoteId"] = collection.remoteId();
    obj["parentId"] = collection.parentCollection().id();
    obj["resource"] = collection.resource();
    obj["contentMimeTypes"] = QJsonArray::fromStringList(collection.contentMimeTypes());
    QJsonArray rights;
    if (collection.rights() == Akonadi::Collection::ReadOnly) {
        rights.append("ReadOnly");
    } else {
        if (collection.rights().testFlag(Akonadi::Collection::Right::CanChangeItem)) {
            rights.append("CanChangeItem");
        }
        if (collection.rights().testFlag(Akonadi::Collection::Right::CanCreateItem)) {
            rights.append("CanCreateItem");
        }
        if (collection.rights().testFlag(Akonadi::Collection::Right::CanDeleteItem)) {
            rights.append("CanDeleteItem");
        }
        if (collection.rights().testFlag(Akonadi::Collection::Right::CanChangeCollection)) {
            rights.append("CanChangeCollection");
        }

        if (collection.rights().testFlag(Akonadi::Collection::Right::CanCreateCollection)) {
            rights.append("CanCreateCollection");
        }
        if (collection.rights().testFlag(Akonadi::Collection::Right::CanDeleteCollection)) {
            rights.append("CanDeleteCollection");
        }
        if (collection.rights().testFlag(Akonadi::Collection::Right::CanLinkItem)) {
            rights.append("CanLinkItem");
        }
        if (collection.rights().testFlag(Akonadi::Collection::Right::CanUnlinkItem)) {
            rights.append("CanUnlinkItem");
        }
    }
    obj["rights"] = rights;
    obj["isVirtual"] = collection.isVirtual();
    QJsonArray childCollections;
    for (const auto &child : collectionTree.value(collection.id())) {
        childCollections.append(collectionToJson(child, collectionTree, collectionItems));
    }
    obj["childCollections"] = childCollections;
    // std::cout << "Child collections: " << mCollectionItems.begin().key() << std::endl;
    QJsonArray childItems;
    for (const auto &item : collectionItems.value(collection.id())) {
        childItems.append(itemToJson(item));
    }
    obj["childItems"] = childItems;
    return obj;
}

void JsonFormatter::writeDocument(const QJsonObject &root)
{
    std::cout << QJsonDocument(root).toJson(QJsonDocument::Indented).toStdString() << std::endl;
}

void JsonFormatter::writeDocument(const QJsonArray &root)
{
    std::cout << QJsonDocument(root).toJson(QJsonDocument::Indented).toStdString() << std::endl;
}
