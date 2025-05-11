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

#pragma once

#include <QDomDocument>
#include <QJsonObject>

#include <Akonadi/Collection>
#include <Akonadi/Item>

class JsonFormatter
{
public:
    static QJsonObject itemToJson(const Akonadi::Item &item);
    static QJsonObject collectionToJson(const Akonadi::Collection &collection,
                                        const QHash<Akonadi::Collection::Id, Akonadi::Collection::List> &collectionTree = {},
                                        const QHash<Akonadi::Collection::Id, Akonadi::Item::List> &collectionItems = {});

    static void writeDocument(const QJsonObject &root);
    static void writeDocument(const QJsonArray &root);
};