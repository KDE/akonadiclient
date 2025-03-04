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

#include "listcommand.h"

#include "collectionpathjob.h"
#include "collectionresolvejob.h"

#include <Akonadi/CollectionFetchJob>
#include <Akonadi/CollectionFetchScope>
#include <Akonadi/ItemFetchJob>
#include <Akonadi/ItemFetchScope>

#include <QDateTime>

#include <iostream>

#include "commandfactory.h"

using namespace Akonadi;

DEFINE_COMMAND("list", ListCommand, kli18nc("info:shell", "List sub-collections and/or items in a specified collection"));

ListCommand::ListCommand(QObject *parent)
    : AbstractCommand(parent)
{
}

void ListCommand::setupCommandOptions(QCommandLineParser *parser)
{
    addOptionsOption(parser);
    parser->addOption(QCommandLineOption((QStringList() << "l"
                                                        << "details"),
                                         i18n("List more detailed information")));
    parser->addOption(QCommandLineOption((QStringList() << "c"
                                                        << "collections"),
                                         i18n("List only sub-collections")));
    parser->addOption(QCommandLineOption((QStringList() << "i"
                                                        << "items"),
                                         i18n("List only contained items")));
    parser->addOption(QCommandLineOption((QStringList() << "R"
                                                        << "recursive"),
                                         i18n("List sub-collections recursively")));

    parser->addPositionalArgument("collection", i18nc("@info:shell", "The collection to list: an ID, path or Akonadi URL"));
}

AbstractCommand::Error ListCommand::initCommand(QCommandLineParser *parser)
{
    const QStringList args = parser->positionalArguments();
    if (!checkArgCount(args, 1, i18nc("@info:shell", "Missing collection argument")))
        return InvalidUsage;

    mListItems = parser->isSet("items"); // selection options specified
    mListCollections = parser->isSet("collections");
    if (!mListCollections && !mListItems) { // if none given, then
        mListCollections = mListItems = true; // list both by default
    }
    mListDetails = parser->isSet("details"); // detailed option specified
    mListRecursive = parser->isSet("recursive"); // recursive option specified
    if (mListRecursive && mListItems) {
        Q_EMIT error(i18nc("@info:shell", "Recursive list can only be used for collections"));
        return InvalidUsage;
    }

    const QString collectionArg = args.first();
    if (!getResolveJob(collectionArg))
        return InvalidUsage;

    return NoError;
}

void ListCommand::start()
{
    connect(resolveJob(), &KJob::result, this, &ListCommand::onBaseResolved);
    resolveJob()->start();
}

static void writeColumn(const QString &data, int width = 0)
{
    std::cout << data.leftJustified(width).toLocal8Bit().constData() << "  ";
}

static void writeColumn(quint64 data, int width = 0)
{
    writeColumn(QString::number(data), width);
}

void ListCommand::onBaseResolved(KJob *job)
{
    if (job->error() != 0) {
        Q_EMIT error(job->errorString());
        Q_EMIT finished(RuntimeError);
        return;
    }

    CollectionResolveJob *res = resolveJob();

    CollectionPathJob *pathJob = new CollectionPathJob(res->collection(), this);
    connect(pathJob, &KJob::result, this, &ListCommand::onBaseFetched);
    pathJob->start();
}

void ListCommand::onBaseFetched(KJob *job)
{
    if (job->error() != 0) {
        Q_EMIT error(job->errorString());
        Q_EMIT finished(RuntimeError);
        return;
    }

    CollectionPathJob *pathJob = qobject_cast<CollectionPathJob *>(job);
    Q_ASSERT(pathJob != nullptr);

    mBasePath = pathJob->collectionPath(); // for making relative paths later

    if (mListCollections) {
        fetchCollections();
    } else {
        fetchItems();
    }
}

void ListCommand::fetchCollections()
{
    CollectionResolveJob *res = resolveJob();
    Q_ASSERT(res->collection().isValid());

    CollectionFetchJob::Type listOption = (mListRecursive ? CollectionFetchJob::Recursive : CollectionFetchJob::FirstLevel);
    CollectionFetchJob *job = new CollectionFetchJob(res->collection(), listOption, this);
    job->fetchScope().setListFilter(CollectionFetchScope::NoFilter);
    connect(job, &KJob::result, this, &ListCommand::onCollectionsFetched);
}

void ListCommand::onCollectionsFetched(KJob *job)
{
    if (!checkJobResult(job))
        return;
    CollectionFetchJob *fetchJob = qobject_cast<CollectionFetchJob *>(job);
    Q_ASSERT(fetchJob != nullptr);

    Collection::List collections = fetchJob->collections();
    if (collections.isEmpty()) {
        if (mListCollections) { // message only if collections requested
            std::cout << qPrintable(i18nc("@info:shell", "Collection %1 has no sub-collections", resolveJob()->formattedCollectionName())) << std::endl;
        }
    } else {
        // This works because Akonadi::Entity implements operator<
        // which compares item IDs numerically
        std::sort(collections.begin(), collections.end());

        if (mListRecursive) {
            std::cout << qPrintable(i18ncp("@info:shell output section header 1=count, 2=collection",
                                           "Collection %2 has %1 recursive sub-collection:",
                                           "Collection %2 has %1 recursive sub-collections:",
                                           collections.count(),
                                           resolveJob()->formattedCollectionName()));
        } else {
            std::cout << qPrintable(i18ncp("@info:shell output section header 1=count, 2=collection",
                                           "Collection %2 has %1 sub-collection:",
                                           "Collection %2 has %1 sub-collections:",
                                           collections.count(),
                                           resolveJob()->formattedCollectionName()));
        }
        std::cout << std::endl;

        if (mListDetails) {
            std::cout << "  ";
            writeColumn(i18nc("@info:shell column header", "ID"), 8);
            writeColumn(i18nc("@info:shell column header", "Name"));
            std::cout << std::endl;
        }

        mCollections = collections;
        processCollections();
        return;
    }

    if (mListItems) {
        fetchItems();
    } else {
        Q_EMIT finished(NoError);
    }
}

void ListCommand::processCollections()
{
    if (mCollections.isEmpty()) { // no more to do
        Q_EMIT finished(NoError);
        return;
    }

    const Collection collection = mCollections.takeFirst();
    if (mListRecursive) { // must show full collection path
        CollectionPathJob *job = new CollectionPathJob(collection, this);
        connect(job, &KJob::result, this, &ListCommand::onParentPathFetched);
        job->start();
        return;
    }

    writeCollection(collection.id(), collection.name());
    // can just list now
    processCollections(); // hopefully should tail recurse
}

void ListCommand::onParentPathFetched(KJob *job)
{
    if (!checkJobResult(job))
        return;
    CollectionPathJob *pathJob = qobject_cast<CollectionPathJob *>(job);
    Q_ASSERT(pathJob != nullptr);

    QString path = pathJob->collectionPath();
    if (!mBasePath.isEmpty())
        path = path.mid(mBasePath.length() + 1);
    writeCollection(pathJob->collection().id(), path);

    processCollections();
}

void ListCommand::writeCollection(const Akonadi::Collection::Id &id, const QString &nameOrPath)
{
    std::cout << "  ";
    if (mListDetails) {
        writeColumn(id, 8);
        writeColumn(nameOrPath);
    } else {
        std::cout << qPrintable(nameOrPath);
    }
    std::cout << std::endl;
}

void ListCommand::fetchItems()
{
    Collection coll = resolveJob()->collection();
    Q_ASSERT(coll.isValid());

    // only attempt item listing if collection has non-collection content MIME types
    QStringList contentMimeTypes = coll.contentMimeTypes();
    contentMimeTypes.removeAll(Collection::mimeType());
    if (!contentMimeTypes.isEmpty()) {
        ItemFetchJob *job = new ItemFetchJob(coll, this);
        job->fetchScope().setFetchModificationTime(true);
        job->fetchScope().fetchAllAttributes(false);
        job->fetchScope().fetchFullPayload(false);
        connect(job, &KJob::result, this, &ListCommand::onItemsFetched);
    } else {
        std::cout << qPrintable(i18nc("@info:shell", "Collection %1 cannot contain items", resolveJob()->formattedCollectionName())) << std::endl;
        Q_EMIT finished(NoError);
    }
}

void ListCommand::onItemsFetched(KJob *job)
{
    if (!checkJobResult(job))
        return;
    ItemFetchJob *fetchJob = qobject_cast<ItemFetchJob *>(job);
    Q_ASSERT(fetchJob != nullptr);
    Item::List items = fetchJob->items();

    if (items.isEmpty()) {
        if (mListItems) { // message only if items requested
            std::cout << qPrintable(i18nc("@info:shell", "Collection %1 has no items", resolveJob()->formattedCollectionName())) << std::endl;
        }
    } else {
        std::sort(items.begin(), items.end());

        std::cout << qPrintable(i18ncp("@info:shell output section header 1=count, 2=collection",
                                       "Collection %2 has %1 item:",
                                       "Collection %2 has %1 items:",
                                       items.count(),
                                       resolveJob()->formattedCollectionName()))
                  << std::endl;
        if (mListDetails) {
            std::cout << "  ";
            writeColumn(i18nc("@info:shell column header", "ID"), 8);
            writeColumn(i18nc("@info:shell column header", "MIME type"), 20);
            writeColumn(i18nc("@info:shell column header", "Size"), 10);
            writeColumn(i18nc("@info:shell column header", "Modification Time"));
            std::cout << std::endl;
        }

        for (const Item &item : std::as_const(items)) {
            std::cout << "  ";
            if (mListDetails) {
                writeColumn(item.id(), 8);
                writeColumn(item.mimeType(), 20);
                const QString size = QLocale::system().formattedDataSize(item.size());
                writeColumn(size, 10);
                // from kdepim/akonadiconsole/browserwidget.cpp BrowserWidget::setItem()
                writeColumn((item.modificationTime().toString() + " UTC"));
            } else {
                std::cout << item.id();
            }
            std::cout << std::endl;
        }
    }

    Q_EMIT finished(NoError);
}
