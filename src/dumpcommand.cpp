/*
 * Copyright (C) 2017 Jonathan Marten <jjm@keelhaul.me.uk>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#include "dumpcommand.h"

#include <iostream>
#ifdef Q_OS_UNIX
#include <utime.h>
#endif

#include <qdir.h>
#include <qfile.h>
#include <qmimetype.h>
#include <qmimedatabase.h>

#include <klocalizedstring.h>

#include <Akonadi/ItemFetchScope>
#include <Akonadi/RecursiveItemFetchJob>
#include <Akonadi/TagFetchJob>
#include <Akonadi/TagFetchScope>

#include "commandfactory.h"
#include "errorreporter.h"
#include "collectionpathjob.h"
#include "collectionresolvejob.h"

using namespace Akonadi;

DEFINE_COMMAND("dump", DumpCommand,
               kli18nc("info:shell",
                       "Dump a collection to a directory structure"));

DumpCommand::DumpCommand(QObject *parent)
    : AbstractCommand(parent)
{
}


void DumpCommand::setupCommandOptions(QCommandLineParser *parser)
{
    addOptionsOption(parser);
    parser->addOption(QCommandLineOption((QStringList() << "m" << "maildir"), i18n("Dump email messages in maildir directory structure")));
    parser->addOption(QCommandLineOption((QStringList() << "a" << "akonadi-categories"), i18n("Dump items with Akonadi category URLs, otherwise text names")));
    parser->addOption(QCommandLineOption((QStringList() << "f" << "force"), i18n("Operate even if destination directory is not empty")));
    addDryRunOption(parser);

    parser->addPositionalArgument("collection", i18nc("@info:shell", "The collection to dump: an ID, path or Akonadi URL"));
    parser->addPositionalArgument("directory", i18nc("@info:shell", "The destination directory to dump to"));
}


int DumpCommand::initCommand(QCommandLineParser *parser)
{
    const QStringList args = parser->positionalArguments();
    if (!checkArgCount(args, 1, i18nc("@info:shell", "No collection specified"))) return InvalidUsage;
    if (!checkArgCount(args, 2, i18nc("@info:shell", "No destination directory specified"))) return InvalidUsage;

    if (!getCommonOptions(parser)) return InvalidUsage;
    mMaildir = parser->isSet("maildir");
    mAkonadiCategories = parser->isSet("akonadi-categories");

    const QString collectionArg = args.at(0);
    if (!getResolveJob(collectionArg)) return (InvalidUsage);

    mDirectoryArg = args.at(1);
    QDir dir(mDirectoryArg);
    if (!dir.exists())
    {
        Q_EMIT error(i18nc("@info:shell", "Directory '%1' not found or not a directory", mDirectoryArg));
        return (InvalidUsage);
    }

    mDirectoryArg = dir.canonicalPath();
    if (!parser->isSet("force") &&
        !dir.entryList(QDir::AllEntries|QDir::Hidden|QDir::System|QDir::NoDotAndDotDot).isEmpty())
    {
        Q_EMIT error(i18nc("@info:shell", "Directory '%1' is not empty (use '-f' to force operation)", mDirectoryArg));
        return (InvalidUsage);
    }

    return (NoError);
}


void DumpCommand::start()
{
    connect(resolveJob(), &KJob::result, this, &DumpCommand::onCollectionFetched);
    resolveJob()->start();
}


void DumpCommand::onCollectionFetched(KJob *job)
{
    if (!checkJobResult(job)) return;
    CollectionResolveJob *res = resolveJob();
    Q_ASSERT(job==res);

    // only attempt item listing if collection has non-collection content MIME types
    QStringList contentMimeTypes = res->collection().contentMimeTypes();
    contentMimeTypes.removeAll(Collection::mimeType());
    if (contentMimeTypes.isEmpty())
    {
        ErrorReporter::fatal(i18nc("@info:shell", "Collection %1 cannot contain items",
                                   res->formattedCollectionName()));
        return;
    }

    RecursiveItemFetchJob *fetchJob = new RecursiveItemFetchJob(res->collection(), QStringList(), this);
    fetchJob->fetchScope().setFetchModificationTime(true);
    fetchJob->fetchScope().fetchAllAttributes(false);
    fetchJob->fetchScope().fetchFullPayload(true);
    // Need this so that parentCollection() will be valid
    fetchJob->fetchScope().setAncestorRetrieval(ItemFetchScope::Parent);

    connect(fetchJob, &KJob::result, this, &DumpCommand::onItemsFetched);
    fetchJob->start();
}


void DumpCommand::onItemsFetched(KJob *job)
{
    if (!checkJobResult(job)) return;
    RecursiveItemFetchJob *fetchJob = qobject_cast<RecursiveItemFetchJob *>(job);
    Q_ASSERT(fetchJob!=NULL);

    Item::List items = fetchJob->items();
    if (items.isEmpty())
    {
        ErrorReporter::fatal(i18nc("@info:shell", "Collection %1 contains no items",
                                   resolveJob()->formattedCollectionName()));
        Q_EMIT finished(RuntimeError);
    }

    std::sort(items.begin(), items.end());		// for predictable ordering
    mItemList = items;					// save items as fetched

    TagFetchJob *tagJob = new TagFetchJob(this);
    tagJob->fetchScope().setFetchIdOnly(false);

    connect(tagJob, &KJob::result, this, &DumpCommand::onTagsFetched);
    tagJob->start();
}


void DumpCommand::onTagsFetched(KJob *job)
{
    if (!checkJobResult(job)) return;
    TagFetchJob *tagJob = qobject_cast<TagFetchJob *>(job);
    Q_ASSERT(tagJob!=NULL);

    mTagList = tagJob->tags();				// save tags as fetched
							// now can start processing
    QMetaObject::invokeMethod(this, "processNextItem", Qt::QueuedConnection);
}


void DumpCommand::processNextItem()
{
    if (mItemList.isEmpty())				// everything done
    {
        Q_EMIT finished(NoError);
        return;
    }

    CollectionPathJob *job = new CollectionPathJob(mItemList.first().parentCollection());
    connect(job, &KJob::result, this, &DumpCommand::onParentPathFetched);
    job->start();
}


void DumpCommand::onParentPathFetched(KJob *job)
{
    if (!checkJobResult(job)) return;
    CollectionPathJob *pathJob = qobject_cast<CollectionPathJob *>(job);
    Q_ASSERT(pathJob!=NULL);

    writeItem(mItemList.first(), pathJob->collectionPath());
    mItemList.removeFirst();

    QMetaObject::invokeMethod(this, "processNextItem", Qt::QueuedConnection);
}


void DumpCommand::writeItem(const Akonadi::Item &item, const QString &parent)
{
    if (!item.hasPayload())
    {
        ErrorReporter::warning(i18nc("@info:shell", "Item '%1' has no payload", item.id()));
        return;
    }

    const QString mimeType = item.mimeType();
    QMimeDatabase db;
    QMimeType mime = db.mimeTypeForName(mimeType);
    QString ext = mime.preferredSuffix();
    // No extension is registered for contact groups
    if (ext.isEmpty() && mimeType=="application/x-vnd.kde.contactgroup") ext = "group";

    QString destDir = mDirectoryArg+"/";
    if (mMaildir && mimeType=="message/rfc822")		// an email message,
    {							// replicate maildir structure
        const QStringList dirs = parent.split('/');
        Q_ASSERT(!dirs.isEmpty());
        for (const QString &dir : dirs)
        {
            destDir += "." + dir + ".directory/";
        }
        destDir += "cur";
    }
    else						// not an email message
    {							// just use plain directories
        destDir += parent;
    }

    QDir dir(destDir);					// containing directory
    if (!dir.exists())					// ensure that it exists
    {
        if (!mCreatedDirs.contains(destDir))		// if not already reported
        {						// in dry run mode
            std::cout << "mkdir       " << qPrintable(destDir) << std::endl;
        }

        if (isDryRun())					// not actually doing anything
        {
            mCreatedDirs.append(destDir);		// so that only reported once
        }
        else
        {
            const QString dirName = dir.dirName();	// this is awkward, why doesn't
            QDir parentDir(destDir+"/..");		// Qt just have QDir::mkpath()
            if (!parentDir.mkpath(dirName))		// with no subdirectory name?
            {
                ErrorReporter::fatal(i18nc("@info:shell", "Cannot create directory '%1/%2'",
                                           parentDir.canonicalPath(), dirName));
                Q_EMIT finished(RuntimeError);
                return;
            }
        }
    }

    QString destPath = destDir+"/";			// make path of item file
    destPath += QString("%1").arg(item.id(), 8, 10, QLatin1Char('0'));
    if (!ext.isEmpty()) destPath += '.'+ext;

    std::cout << qPrintable(QString("%1").arg(item.id(), -8)) << " -> " << qPrintable(destPath) << std::endl;
    if (!isDryRun())
    {
        QByteArray data = item.payloadData();		// the raw item payload
        if (mimeType=="text/directory" || mimeType=="text/vcard")
        {						// need to fix up tags?
            data.replace('\r', "");			// remove trailing CR from lines

            // Rewrite the "CATGEORIES" line to use the external tag names
            // as opposed to the internal Akonadi URLs.  Also hide any
            // "UID" lines so as not to confuse the receiver.

            bool changed = false;			// not yet, anyway
            const QList<QByteArray> oldLines = data.split('\n');
            QStringList newLines;
            for (const QByteArray &line : oldLines)
            {
                if (line.startsWith("UID:"))		// hide internal details
                {
                    newLines.append(QStringLiteral("X-AKONADI-") + QString::fromUtf8(line));
                    newLines.append(QStringLiteral("X-AKONADI-ITEM:%1").arg(item.id()));
                    changed = true;
                    continue;
                }
							// ignore old data from these
                if (line.startsWith("X-AKONADI-UID:")) continue;
                if (line.startsWith("X-AKONADI-ITEM:")) continue;

                if (!line.startsWith("CATEGORIES:"))
                {					// not interested in this line
                    newLines.append(QString::fromUtf8(line));
                    continue;
                }

                if (mAkonadiCategories)			// don't want to rewrite
                {
                    newLines.append(QString::fromUtf8(line));
                    continue;
                }

                const QStringList oldCats = QString::fromUtf8(line.constData()).mid(11).split(',');
                QStringList newCats;
                for (const QString &cat : oldCats)
                {
                    QString newCat = cat;
                    const Akonadi::Tag::Id catId = Akonadi::Tag::fromUrl(QUrl(cat)).id();
							// category tag ID from URL
                    for (const Akonadi::Tag &tag : std::as_const(mTagList))
                    {
                        if (tag.id()==catId)
                        {
                            newCat = tag.name();
                            changed = true;
                            break;
                        }
                    }

                    newCats.append(newCat);
                }

                QString newLine = QString("CATEGORIES:")+newCats.join(",");
                newLines.append(newLine);		// format new "CATGEORIES" line
            }
							// only if something changed
            if (changed) data = newLines.join("\n").toUtf8();
        }

        QFile file(destPath);
        if (!file.open(QIODevice::WriteOnly|QIODevice::Truncate))
        {
            ErrorReporter::fatal(i18nc("@info:shell", "Cannot save file '%1'", destPath));
            Q_EMIT finished(RuntimeError);
            return;
        }

        file.write(data);				// output the raw payload
        file.close();

#ifdef Q_OS_UNIX
        struct utimbuf times;				// stamp with item modification time
        times.modtime = item.modificationTime().toSecsSinceEpoch();
        times.actime = time(NULL);
        utime(QFile::encodeName(destPath).constData(), &times);
#endif
    }
}

#include "moc_dumpcommand.cpp"
