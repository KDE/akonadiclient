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

#include "collectionlistcommand.h"

#include <klocalizedstring.h>

using namespace Akonadi;

CollectionListCommand::CollectionListCommand(QObject *parent)
    : AbstractCommand(parent)
{
}

void CollectionListCommand::listCollections(CollectionFetchJob::Type type)
{
    CollectionFetchJob *job = new CollectionFetchJob(Collection::root(), type, this);
    connect(job, &KJob::result, this, &CollectionListCommand::onCollectionsListedInternal);
}

void CollectionListCommand::onCollectionsListedInternal(KJob *job)
{
    if (!checkJobResult(job))
        return;
    CollectionFetchJob *fetchJob = qobject_cast<CollectionFetchJob *>(job);
    Q_ASSERT(fetchJob != nullptr);

    mCollections = fetchJob->collections();
    if (mCollections.isEmpty()) {
        Q_EMIT error(i18nc("@info:shell", "Cannot list any collections"));
        Q_EMIT finished(RuntimeError);
        return;
    }

    onCollectionsListed();
}

void CollectionListCommand::getCurrentPaths()
{
    QMap<Collection::Id, Collection> curCollMap;
    for (const Collection &coll : std::as_const(mCollections)) {
        curCollMap[coll.id()] = coll;
    }

    for (const Collection &coll : std::as_const(mCollections)) {
        QStringList path(coll.name());
        Collection::Id parentId = coll.parentCollection().id();
        while (parentId != 0) {
            const Collection &parentColl = curCollMap[parentId];
            path.prepend(parentColl.name());
            parentId = parentColl.parentCollection().id();
        }

        path.prepend(""); // to get root at beginning
        mCurPathMap[coll.id()] = path.join('/');
    }
}
