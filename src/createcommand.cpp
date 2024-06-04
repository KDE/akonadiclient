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

#include "createcommand.h"

#include "collectionpathjob.h"
#include "collectionresolvejob.h"
#include "errorreporter.h"

#include <Akonadi/Collection>
#include <Akonadi/CollectionCreateJob>
#include <Akonadi/CollectionFetchJob>

#include <iostream>

#include "commandfactory.h"

using namespace Akonadi;

DEFINE_COMMAND("create", CreateCommand, kli18nc("info:shell", "Create a new collection"));
using namespace Qt::Literals::StringLiterals;
CreateCommand::CreateCommand(QObject *parent)
    : AbstractCommand(parent)
{
}

void CreateCommand::start()
{
    connect(resolveJob(), &KJob::result, this, &CreateCommand::onTargetFetched);
    resolveJob()->start();
}

void CreateCommand::setupCommandOptions(QCommandLineParser *parser)
{
    addOptionsOption(parser);
    parser->addOption(QCommandLineOption((QStringList() << "p"
                                                        << "parent"),
                                         i18n("Parent collection to create in"),
                                         i18n("collection")));
    addDryRunOption(parser);

    parser->addPositionalArgument("collection", i18nc("@info:shell", "The collection to create, either as a path or a name (with a parent specified)"));
}

int CreateCommand::initCommand(QCommandLineParser *parser)
{
    const QStringList args = parser->positionalArguments();
    if (!checkArgCount(args, 1, i18nc("@info:shell", "Missing collection argument")))
        return InvalidUsage;

    if (!getCommonOptions(parser))
        return InvalidUsage;

    const QString collectionArg = args.first();
    if (parser->isSet("parent")) {
        // A parent collection is specified.  That must already exist,
        // and the new collection must be a plain name (not containing
        // any '/'es).

        if (collectionArg.contains(QLatin1Char('/'))) {
            Q_EMIT error(i18nc("@info:shell", "Collection argument (with parent) cannot be a path"));
            return InvalidUsage;
        }

        mNewCollectionName = collectionArg;
        mParentCollection = parser->value("parent");
    } else {
        // No parent collection is specified.  The new collection name
        // is the last part of the specified argument, and the remainder
        // is taken as the parent collection which must already exist.
        // Note that this means that an argument like "33/newname" is
        // acceptable, where the number is resolved as a collection ID.
        // Even something like "akonadi://?collection=33/newname" is
        // acceptable.

        int i = collectionArg.lastIndexOf(QLatin1Char('/'));
        if (i == -1) {
            Q_EMIT error(i18nc("@info:shell", "Collection argument (without parent) must be a path"));
            return InvalidUsage;
        }

        mNewCollectionName = collectionArg.mid(i + 1);
        mParentCollection = collectionArg.left(i);
    }

    if (mNewCollectionName.isEmpty()) {
        emitErrorSeeHelp(i18nc("@info:shell", "New collection name not specified"));
        return InvalidUsage;
    }

    if (!getResolveJob(mParentCollection))
        return InvalidUsage;

    return NoError;
}

void CreateCommand::onTargetFetched(KJob *job)
{
    if (!checkJobResult(job, i18nc("@info:shell", "Cannot fetch parent collection '%1', %2", mParentCollection, job->errorString())))
        return;
    CollectionResolveJob *res = resolveJob();
    Q_ASSERT(job == res);
    Akonadi::Collection parentCollection = res->collection();
    Q_ASSERT(parentCollection.isValid());

    // Warning for bug 319513
    if ((mNewCollectionName == "cur"_L1) || (mNewCollectionName == "new"_L1) || (mNewCollectionName == "tmp"_L1)) {
        QString parentResource = parentCollection.resource();
        if (parentResource.startsWith(QLatin1String("akonadi_maildir_resource"))) {
            ErrorReporter::warning(i18n("Creating a maildir folder named '%1' may not work", mNewCollectionName));
        }
    }

    Akonadi::Collection newCollection;
    newCollection.setParentCollection(parentCollection);
    newCollection.setName(mNewCollectionName);
    newCollection.setContentMimeTypes(parentCollection.contentMimeTypes());
    if (!isDryRun()) {
        CollectionCreateJob *createJob = new CollectionCreateJob(newCollection);
        connect(createJob, &KJob::result, this, &CreateCommand::onCollectionCreated);
    } else {
        Q_EMIT finished(NoError);
    }
}

void CreateCommand::onCollectionCreated(KJob *job)
{
    if (!checkJobResult(job,
                        i18n("Error creating collection '%1' under '%2', %3", mNewCollectionName, resolveJob()->formattedCollectionName(), job->errorString())))
        return;
    CollectionCreateJob *createJob = qobject_cast<CollectionCreateJob *>(job);
    Q_ASSERT(createJob != nullptr);

    CollectionPathJob *pathJob = new CollectionPathJob(createJob->collection());
    connect(pathJob, &KJob::result, this, &CreateCommand::onPathFetched);
    pathJob->start();
}

void CreateCommand::onPathFetched(KJob *job)
{
    if (!checkJobResult(job, i18n("Error getting path of new collection, %1", job->errorString())))
        return;
    CollectionPathJob *pathJob = qobject_cast<CollectionPathJob *>(job);
    Q_ASSERT(pathJob != nullptr);

    std::cout << qPrintable(i18n("Created new collection '%1'", pathJob->formattedCollectionPath())) << std::endl;

    Q_EMIT finished(NoError);
}
