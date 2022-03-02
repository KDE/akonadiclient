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

#include "collectionresolvejob.h"

#include <Akonadi/CollectionFetchJob>
#include <Akonadi/CollectionFetchScope>

#include <Akonadi/CollectionPathResolver>

#include <KLocalizedString>

#include "errorreporter.h"

using namespace Akonadi;

HackedCollectionPathResolver::HackedCollectionPathResolver(const QString &path, QObject *parent)
    : CollectionPathResolver(path, parent)
{}

HackedCollectionPathResolver::HackedCollectionPathResolver(const Collection &col, QObject *parent)
    : CollectionPathResolver(col, parent)
{}

bool HackedCollectionPathResolver::addSubjob(KJob *job)
{
    if (auto akjob = qobject_cast<Akonadi::Job*>(job)) {
        connect(akjob, &Job::aboutToStart,
                [](Akonadi::Job *subjob) {
                    if (auto fetchJob = qobject_cast<CollectionFetchJob*>(subjob)) {
                        fetchJob->fetchScope().setListFilter(CollectionFetchScope::NoFilter);
                    }
                });
    }
    return CollectionPathResolver::addSubjob(job);
}


CollectionResolveJob::CollectionResolveJob(const QString &userInput, QObject *parent)
    : KCompositeJob(parent),
      mUserInput(userInput),
      mHadSlash(false)
{
    setAutoDelete(false);

    // A collection path ending with a '/' is accepted by a
    // CollectionPathResolver.  However, we strip any slash here
    // so that an ID or a URL can also be parsed (but, obviously,
    // not from a single slash meaning the root).  Note whether
    // any slash was removed, so that the caller can act on it
    // if required.

    QString in = userInput;
    if (in.length() > 1 && in.endsWith(QLatin1Char('/'))) {
        in.chop(1);
        mHadSlash = true;
    }

    mCollection = parseCollection(in);
}

void CollectionResolveJob::start()
{
    if (!hasUsableInput()) {
        emitResult();
        return;
    }

    if (mCollection.isValid()) {
        fetchBase();
    } else {
        CollectionPathResolver *resolver = new HackedCollectionPathResolver(mUserInput, this);
        addSubjob(resolver);
        resolver->start();
    }
}

bool CollectionResolveJob::hasUsableInput()
{
    if (mCollection.isValid() || mUserInput.startsWith(CollectionPathResolver::pathDelimiter())) {
        return true;
    }

    setError(Akonadi::Job::UserError);
    setErrorText(i18nc("@info:shell", "Unknown Akonadi collection format '%1'", mUserInput));
    return false;
}

void CollectionResolveJob::fetchBase()
{
    if (mCollection == Collection::root()) {
        emitResult();
        return;
    }

    CollectionFetchJob *job = new CollectionFetchJob(mCollection, CollectionFetchJob::Base, this);
    job->fetchScope().setListFilter(CollectionFetchScope::NoFilter);
    addSubjob(job);
}

void CollectionResolveJob::slotResult(KJob *job)
{
    if (job->error() == 0) {
        CollectionFetchJob *fetchJob = qobject_cast<CollectionFetchJob *>(job);
        if (fetchJob != nullptr) {
            mCollection = fetchJob->collections().first();
        } else {
            CollectionPathResolver *resolver = qobject_cast<CollectionPathResolver *>(job);
            mCollection = Collection(resolver->collection());
            fetchBase();
        }
    }

    bool willEmitResult = (job->error() && !error());
    // If willEmitResult is true, then emitResult() will be
    // done inside the call of KCompositeJob::slotResult() below.
    // So there will be no need for us to do it again.
    KCompositeJob::slotResult(job);

    if (!hasSubjobs() && !willEmitResult) {
        emitResult();
    }
}

QString CollectionResolveJob::formattedCollectionName() const
{
    if (mCollection == Collection::root()) {
        return (i18nc("@info:shell 1=collection ID",
                      "%1 (root)", QString::number(mCollection.id())));
    } else {
        return (i18nc("@info:shell 1=collection ID, 2=collection name",
                      "%1 (\"%2\")",
                      QString::number(mCollection.id()), mCollection.name()));
    }
}

Akonadi::Item CollectionResolveJob::parseItem(const QString &userInput, bool verbose)
{
    Item item;

    // See if user input is a valid integer as an item ID
    bool ok;
    unsigned int id = userInput.toUInt(&ok);
    if (ok) {
        item = Item(id);    // conversion succeeded
    } else {
        // Otherwise check if we have an Akonadi URL
        const QUrl url = QUrl::fromUserInput(userInput);
        if (url.isValid() && url.scheme() == QLatin1String("akonadi")) {
            // valid Akonadi URL
            item = Item::fromUrl(url);
        }
    }
    // Otherwise return an invalid Item

    if (!item.isValid() && verbose) {         // report error if required
        ErrorReporter::error(i18nc("@info:shell", "Invalid item syntax '%1'", userInput));
    }

    return (item);
}

Akonadi::Collection CollectionResolveJob::parseCollection(const QString &userInput)
{
    Collection coll;

    // First see if user input is a valid integer as a collection ID
    bool ok;
    unsigned int id = userInput.toUInt(&ok);
    if (ok) {                     // conversion succeeded
        if (id == 0) {
            coll = Collection::root();    // the root collection
        } else {
            coll = Collection(id);    // the specified collection
        }
    } else {
        // Then quickly check for a path of "/", meaning the root
        if (userInput == QLatin1String("/")) {
            coll = Collection::root();
        } else {
            // Next check if we have an Akonadi URL
            const QUrl url = QUrl::fromUserInput(userInput);
            if (url.isValid() && url.scheme() == QLatin1String("akonadi")) {
                // valid Akonadi URL
                coll = Collection::fromUrl(url);
            }
        }
    }
    // If none of these applied, assume that we have a path
    // and return an invalid Collection.

    return (coll);
}
