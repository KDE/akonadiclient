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

#include "addcommand.h"

#include "collectionresolvejob.h"
#include "errorreporter.h"

#include <Akonadi/CollectionCreateJob>
#include <Akonadi/CollectionFetchJob>
#include <Akonadi/CollectionFetchScope>
#include <Akonadi/Item>
#include <Akonadi/ItemCreateJob>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMimeDatabase>

#include <iostream>

#include "commandfactory.h"

using namespace Akonadi;

DEFINE_COMMAND("add", AddCommand, kli18nc("info:shell", "Add items to a collection"));

AddCommand::AddCommand(QObject *parent)
    : AbstractCommand(parent)
{
}

void AddCommand::start()
{
    connect(resolveJob(), &KJob::result, this, &AddCommand::onTargetFetched);
    resolveJob()->start();
}

void AddCommand::setupCommandOptions(QCommandLineParser *parser)
{
    addOptionsOption(parser);
    parser->addOption(QCommandLineOption((QStringList() << "b"
                                                        << "base"),
                                         i18n("Base directory for input files/directories, default is current"),
                                         i18n("directory")));
    parser->addOption(QCommandLineOption((QStringList() << "f"
                                                        << "flat"),
                                         i18n("Flat mode, do not duplicate subdirectory structure")));
    parser->addOption(QCommandLineOption((QStringList() << "m"
                                                        << "mime"),
                                         i18n("MIME type of added items, default is to auto-detect"),
                                         i18n("mimetype")));
    addDryRunOption(parser);

    parser->addPositionalArgument("collection", i18nc("@info:shell", "The collection to add to: an ID, path or Akonadi URL"));
    parser->addPositionalArgument("files", i18nc("@info:shell", "Files or directories to add to the collection"), i18n("files..."));
}

int AddCommand::initCommand(QCommandLineParser *parser)
{
    const QStringList args = parser->positionalArguments();
    if (!checkArgCount(args, 1, i18nc("@info:shell", "Missing collection argument")))
        return InvalidUsage;
    if (!checkArgCount(args, 2, i18nc("@info:shell", "No file or directory arguments")))
        return InvalidUsage;

    if (!getCommonOptions(parser))
        return InvalidUsage;
    mFlatMode = parser->isSet("flat");

    const QString collectionArg = args.first();
    if (!getResolveJob(collectionArg))
        return InvalidUsage;

    const QString mimeTypeArg = parser->value("mime");
    if (!mimeTypeArg.isEmpty()) { // MIME type is specified
        QMimeDatabase db;
        mMimeType = db.mimeTypeForName(mimeTypeArg);
        if (!mMimeType.isValid()) {
            Q_EMIT error(i18nc("@info:shell", "Invalid MIME type argument '%1'", mimeTypeArg));
            return InvalidUsage;
        }
    }

    mBasePath = parser->value("base");
    if (!mBasePath.isEmpty()) { // base directory is specified
        QDir dir(mBasePath);
        if (!dir.exists()) {
            Q_EMIT error(i18nc("@info:shell", "Base directory '%1' not found", mBasePath));
            return InvalidUsage;
        }
        mBasePath = dir.absolutePath();
    } else { // base is not specified
        mBasePath = QDir::currentPath();
    }

    const int parsedArgsCount = args.count();
    for (int i = 1; i < parsedArgsCount; ++i) { // process all file/dir arguments
        QString path = args.at(i);
        while (path.endsWith(QLatin1Char('/'))) { // gives null collection name later
            path.chop(1);
        }

        QFileInfo fileInfo(path);
        if (fileInfo.isRelative()) {
            fileInfo.setFile(mBasePath, path);
        }

        if (!fileInfo.exists()) {
            Q_EMIT error(i18n("File '%1' does not exist", path));
            return InvalidUsage;
        } else if (!fileInfo.isReadable()) {
            Q_EMIT error(i18n("Error accessing file '%1'", path));
            return InvalidUsage;
        }

        const QString absolutePath = fileInfo.absoluteFilePath();

        if (fileInfo.isDir()) {
            mDirectories[absolutePath] = AddRecursive;
        } else {
            mDirectories[fileInfo.absolutePath()] = AddDirOnly;
            mFiles.insert(absolutePath);
        }

        if (absolutePath.startsWith(mBasePath)) {
            if (fileInfo.isDir()) {
                mBasePaths.insert(absolutePath, fileInfo.absoluteFilePath());
            } else {
                mBasePaths.insert(absolutePath, fileInfo.absolutePath());
            }
        } else {
            if (fileInfo.isFile()) {
                mBasePaths.insert(fileInfo.absolutePath(), mBasePath);
            }
            mBasePaths.insert(absolutePath, mBasePath);
        }
    }

    if (mFiles.isEmpty() && mDirectories.isEmpty()) {
        emitErrorSeeHelp(i18nc("@info:shell", "No valid file or directory arguments"));
        return InvalidUsage;
    }

    return NoError;
}

void AddCommand::processNextDirectory()
{
    if (mDirectories.isEmpty()) {
        ErrorReporter::progress(i18n("No more directories to process"));
        processNextFile();
        return;
    }

    const QMap<QString, AddDirectoryMode>::iterator directoriesBegin = mDirectories.begin();
    const QString path = directoriesBegin.key();
    const AddDirectoryMode mode = directoriesBegin.value();
    mDirectories.erase(directoriesBegin);

    if (mFlatMode) {
        mCollectionsByPath[path] = mBaseCollection;
    }

    if (mCollectionsByPath.value(mBasePaths[path]).isValid()) {
        if (mode == AddDirOnly) {
            // already added
            QMetaObject::invokeMethod(this, "processNextDirectory", Qt::QueuedConnection);
            return;
        }

        // exists but needs recursion and items
        QDir dir(path);
        if (!dir.exists()) {
            ErrorReporter::warning(i18n("Directory ‘%1’ no longer exists", path));
            QMetaObject::invokeMethod(this, "processNextDirectory", Qt::QueuedConnection);
            return;
        }

        const QFileInfoList children = dir.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot);
        for (const QFileInfo &fileInfo : children) {
            if (fileInfo.isDir()) {
                mDirectories[fileInfo.absoluteFilePath()] = AddRecursive;
                mBasePaths[fileInfo.absoluteFilePath()] = fileInfo.absoluteFilePath();
            } else {
                mFiles.insert(fileInfo.absoluteFilePath());
                mBasePaths[fileInfo.absoluteFilePath()] = fileInfo.absolutePath();
            }
        }

        QMetaObject::invokeMethod(this, "processNextDirectory", Qt::QueuedConnection);
        return;
    }

    QDir dir(path);
    if (!dir.exists()) {
        ErrorReporter::warning(i18n("Directory ‘%1’ no longer exists", path));
        QMetaObject::invokeMethod(this, "processNextDirectory", Qt::QueuedConnection);
        return;
    }

    // re-examine again later
    mDirectories[path] = mode;

    dir.cdUp();

    const Collection parent = mCollectionsByPath.value(mBasePaths[dir.absolutePath()]);
    if (parent.isValid()) {
        Collection collection;
        collection.setName(QFileInfo(path).fileName());
        collection.setParentCollection(parent); // set parent
        collection.setContentMimeTypes(parent.contentMimeTypes()); // "inherit" mime types from parent

        ErrorReporter::progress(i18n("Fetching collection \"%3\" in parent %1 \"%2\"", QString::number(parent.id()), parent.name(), collection.name()));

        CollectionFetchJob *job = new CollectionFetchJob(parent, CollectionFetchJob::FirstLevel);
        job->fetchScope().setListFilter(CollectionFetchScope::NoFilter);
        job->setProperty("path", path);
        job->setProperty("collection", QVariant::fromValue(collection));
        connect(job, &KJob::result, this, &AddCommand::onCollectionFetched);
        return;
    }

    // parent doesn't exist, generate parent chain creation entries
    while (!mCollectionsByPath.value(mBasePaths[dir.absolutePath()]).isValid()) {
        ErrorReporter::progress(i18n("Need to create collection for '%1'", QDir(mBasePath).relativeFilePath(dir.absolutePath())));
        mDirectories[dir.absolutePath()] = AddDirOnly;
        mBasePaths[dir.absolutePath()] = dir.absolutePath();
        dir.cdUp();
    }

    QMetaObject::invokeMethod(this, "processNextDirectory", Qt::QueuedConnection);
}

void AddCommand::processNextFile()
{
    if (mFiles.isEmpty()) {
        ErrorReporter::progress(i18n("No more files to process"));
        Q_EMIT finished(NoError);
        return;
    }

    const QSet<QString>::iterator filesBegin = mFiles.begin();
    const QString fileName = *filesBegin;
    mFiles.erase(filesBegin);

    QFile file(fileName);
    if (!file.exists()) {
        Q_EMIT error(i18nc("@info:shell", "File ‘%1’ does not exist", fileName));
        QMetaObject::invokeMethod(this, "processNextFile", Qt::QueuedConnection);
        return;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        Q_EMIT error(i18nc("@info:shell", "File ‘%1’ cannot be read", fileName));
        QMetaObject::invokeMethod(this, "processNextFile", Qt::QueuedConnection);
        return;
    }

    QMimeDatabase db;
    QMimeType mimeType = mMimeType.isValid() ? mMimeType : db.mimeTypeForFileNameAndData(fileName, &file);
    if (!mimeType.isValid()) {
        Q_EMIT error(i18nc("@info:shell", "Cannot determine MIME type of file ‘%1’", fileName));
        QMetaObject::invokeMethod(this, "processNextFile", Qt::QueuedConnection);
        return;
    }

    const QFileInfo fileInfo(fileName);

    const Collection parent = mCollectionsByPath.value(mBasePaths[fileInfo.absolutePath()]);
    if (!parent.isValid()) {
        Q_EMIT error(i18nc("@info:shell", "Cannot determine parent collection for file ‘%1’", QDir(mBasePath).relativeFilePath(fileName)));
        QMetaObject::invokeMethod(this, "processNextFile", Qt::QueuedConnection);
        return;
    }

    const QString size = QLocale::system().formattedDataSize(fileInfo.size());
    ErrorReporter::progress(i18n("Creating item in collection %1 \"%2\" from '%3' size %4",
                                 QString::number(parent.id()),
                                 parent.name(),
                                 QDir(mBasePath).relativeFilePath(fileName),
                                 size));
    Item item;
    item.setMimeType(mimeType.name());

    file.reset();
    item.setPayloadFromData(file.readAll());

    if (!isDryRun()) {
        ItemCreateJob *job = new ItemCreateJob(item, parent);
        job->setProperty("fileName", fileName);
        connect(job, &KJob::result, this, &AddCommand::onItemCreated);
    } else {
        processNextFile();
    }
}

void AddCommand::onTargetFetched(KJob *job)
{
    if (!checkJobResult(job, i18nc("@info:shell", "Cannot fetch target collection, %1", job->errorString())))
        return;
    CollectionResolveJob *res = resolveJob();
    Q_ASSERT(job == res);

    mBaseCollection = res->collection();
    Q_ASSERT(mBaseCollection.isValid());
    mCollectionsByPath[mBasePath] = mBaseCollection;
    mBasePaths[mBasePath] = mBasePath;

    ErrorReporter::progress(i18n("Root folder is %1 \"%2\"", QString::number(mBaseCollection.id()), mBaseCollection.name()));
    processNextDirectory();
}

void AddCommand::onCollectionCreated(KJob *job)
{
    const QString path = job->property("path").toString();
    Q_ASSERT(!path.isEmpty());

    if (job->error() != 0) {
        qWarning() << "error=" << job->error() << "errorString=" << job->errorString();

        mDirectories.remove(path);
    } else {
        CollectionCreateJob *createJob = qobject_cast<CollectionCreateJob *>(job);
        Q_ASSERT(createJob != nullptr);

        QFileInfo fileInfo(path);
        mBasePaths[path] = fileInfo.absoluteFilePath();

        mCollectionsByPath[path] = createJob->collection();
    }

    QMetaObject::invokeMethod(this, "processNextDirectory", Qt::QueuedConnection);
}

void AddCommand::onCollectionFetched(KJob *job)
{
    const QString path = job->property("path").toString();
    Q_ASSERT(!path.isEmpty());

    Akonadi::Collection newCollection = job->property("collection").value<Collection>();

    CollectionFetchJob *fetchJob = qobject_cast<CollectionFetchJob *>(job);
    Q_ASSERT(fetchJob != nullptr);

    bool found = false;
    const Collection::List collections = fetchJob->collections();
    for (const Collection &col : collections) {
        if (col.name() == newCollection.name()) {
            found = true;
            newCollection = col;
        }
    }

    if (!found) {
        if (mFlatMode) { // not creating any collections
            ErrorReporter::error(
                i18n("Error fetching collection %1 \"%2\", %3", QString::number(newCollection.id()), newCollection.name(), job->errorString()));
            QMetaObject::invokeMethod(this, "processNextDirectory", Qt::QueuedConnection);
            return;
        }

        // no such collection, try creating it
        QString name = newCollection.name();
        // Workaround for bug 319513
        if ((name == "cur") || (name == "new") || (name == "tmp")) {
            QString parentResource = newCollection.parentCollection().resource();
            if (parentResource.startsWith(QLatin1String("akonadi_maildir_resource"))) {
                name += "_";
                newCollection.setName(name);
                ErrorReporter::warning(i18n("Changed maildir folder name to '%1'", name));
            }
        }

        if (!isDryRun()) {
            CollectionCreateJob *createJob = new CollectionCreateJob(newCollection);
            createJob->setProperty("path", path);

            Akonadi::Collection parent = newCollection.parentCollection();
            ErrorReporter::progress(
                i18n("Creating collection \"%3\" under parent %1 \"%2\"", QString::number(parent.id()), parent.name(), newCollection.name()));

            connect(createJob, &KJob::result, this, &AddCommand::onCollectionCreated);
        } else {
            QMetaObject::invokeMethod(this, "processNextDirectory", Qt::QueuedConnection);
        }
        return;
    }

    mCollectionsByPath[path] = newCollection;

    QMetaObject::invokeMethod(this, "processNextDirectory", Qt::QueuedConnection);
}

void AddCommand::onItemCreated(KJob *job)
{
    const QString fileName = job->property("fileName").toString();

    if (checkJobResult(job, i18nc("@info:shell", "Failed to add ‘%1’, %2", fileName, job->errorString()))) {
        ItemCreateJob *createJob = qobject_cast<ItemCreateJob *>(job);
        Q_ASSERT(createJob != nullptr);

        ErrorReporter::progress(i18n("Added file '%2' as item %1", QString::number(createJob->item().id()), QDir(mBasePath).relativeFilePath(fileName)));
    }

    processNextFile();
}
