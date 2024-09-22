/*
    Copyright (C) 2012  Kevin Krammer <krammer@kde.org>
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

#include <Akonadi/Collection>
#include <KCompositeJob>

class CollectionPathJob : public KCompositeJob
{
    Q_OBJECT

public:
    explicit CollectionPathJob(const Akonadi::Collection &collection, QObject *parent = nullptr);
    ~CollectionPathJob() override = default;

    void start() override;

    [[nodiscard]] const Akonadi::Collection &collection() const
    {
        return (mCollection);
    }
    [[nodiscard]] QString collectionPath() const;
    [[nodiscard]] QString formattedCollectionPath() const;

protected Q_SLOTS:
    void slotResult(KJob *job) override;

private:
    Akonadi::Collection mCollection;
    QString mPath;
};
