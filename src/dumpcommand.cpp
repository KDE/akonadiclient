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

#include <klocalizedstring.h>
#include <kcmdlineargs.h>
#include <kmimetype.h>

#include <akonadi/recursiveitemfetchjob.h>
#include <akonadi/tagfetchjob.h>
#include <akonadi/itemfetchscope.h>
#include <akonadi/tagfetchscope.h>

#include "commandfactory.h"
#include "errorreporter.h"
#include "collectionpathjob.h"

using namespace Akonadi;


DEFINE_COMMAND("dump", DumpCommand, "Dump a collection to a directory structure");


DumpCommand::DumpCommand(QObject *parent)
    : AbstractCommand(parent),
      mResolveJob(NULL)
{
}


DumpCommand::~DumpCommand()
{
    delete mResolveJob;
}


void DumpCommand::setupCommandOptions(KCmdLineOptions& options)
{
    AbstractCommand::setupCommandOptions(options);

    addOptionsOption(options);
    options.add("+collection", ki18nc("@info:shell", "The collection to dump"));
    options.add("+directory", ki18nc("@info:shell", "The directory to dump to"));
    addOptionSeparator(options);
    options.add("m").add("maildir", ki18nc("@info:shell", "Dump email messages in maildir directory structure"));
    options.add("f").add("force", ki18nc("@info:shell", "Operate even if destination directory is not empty"));
    addDryRunOption(options);
}


int DumpCommand::initCommand(KCmdLineArgs *parsedArgs)
{
    if (parsedArgs->count()<2)
    {
        emitErrorSeeHelp(ki18nc("@info:shell", "No collection specified"));
        return (InvalidUsage);
    }

    if (parsedArgs->count()==2)
    {
        emitErrorSeeHelp(ki18nc("@info:shell", "No dump directory specified"));
        return (InvalidUsage);
    }

    QString collectionArg = parsedArgs->arg(1);
    mResolveJob = new CollectionResolveJob(collectionArg, this);
    if (!mResolveJob->hasUsableInput())
    {
        emit error(mResolveJob->errorString());
        return (InvalidUsage);
    }

    mDryRun = parsedArgs->isSet("dryrun");
    mMaildir = parsedArgs->isSet("maildir");

    mDirectoryArg = parsedArgs->arg(2);
    QDir dir(mDirectoryArg);
    if (!dir.exists())
    {
        emit error(ki18nc("@info:shell", "Directory '%1' not found or is not a directory").subs(mDirectoryArg).toString());
        return (InvalidUsage);
    }

    mDirectoryArg = dir.canonicalPath();
    if (!parsedArgs->isSet("force") &&
        !dir.entryList(QDir::AllEntries|QDir::Hidden|QDir::System|QDir::NoDotAndDotDot).isEmpty())
    {
        emit error(ki18nc("@info:shell", "Directory '%1' is not empty (use '-f' to force operation)").subs(mDirectoryArg).toString());
        return (InvalidUsage);
    }

    return (NoError);
}


void DumpCommand::start()
{
    connect(mResolveJob, SIGNAL(result(KJob *)), SLOT(onCollectionFetched(KJob *)));
    mResolveJob->start();
}


void DumpCommand::onCollectionFetched(KJob *job)
{
    if (job->error()!=0)
    {
        emit error(job->errorString());
        emit finished(RuntimeError);
        return;
    }

    Q_ASSERT(qobject_cast<CollectionResolveJob *>(job)==mResolveJob);

    // only attempt item listing if collection has non-collection content MIME types
    QStringList contentMimeTypes = mResolveJob->collection().contentMimeTypes();
    contentMimeTypes.removeAll(Collection::mimeType());
    if (contentMimeTypes.isEmpty())
    {
        ErrorReporter::fatal(i18nc("@info:shell", "Collection %1 cannot contain items",
                                   mResolveJob->formattedCollectionName()));
        return;
    }

    RecursiveItemFetchJob *fetchJob = new RecursiveItemFetchJob(mResolveJob->collection(), QStringList(), this);
    fetchJob->fetchScope().setFetchModificationTime(true);
    fetchJob->fetchScope().fetchAllAttributes(false);
    fetchJob->fetchScope().fetchFullPayload(true);
    // Need this so that parentCollection() will be valid
    fetchJob->fetchScope().setAncestorRetrieval(ItemFetchScope::Parent);

    connect(fetchJob, SIGNAL(result(KJob *)), SLOT(onItemsFetched(KJob *)));
    fetchJob->start();
}


void DumpCommand::onItemsFetched(KJob *job)
{
    if (job->error()!=0)
    {
        emit error(job->errorString());
        emit finished(RuntimeError);
        return;
    }

    RecursiveItemFetchJob *fetchJob = qobject_cast<RecursiveItemFetchJob *>(job);
    Q_ASSERT(fetchJob!=NULL);
    Item::List items = fetchJob->items();
    if (items.isEmpty())
    {
        ErrorReporter::fatal(i18nc("@info:shell", "Collection %1 contains no items",
                                   mResolveJob->formattedCollectionName()));
        emit finished(RuntimeError);
    }

    qSort(items);					// for predictable ordering
    mItemList = items;					// save items as fetched

    TagFetchJob *tagJob = new TagFetchJob(this);
    tagJob->fetchScope().setFetchIdOnly(false);

    connect(tagJob, SIGNAL(result(KJob *)), SLOT(onTagsFetched(KJob *)));
    tagJob->start();
}


void DumpCommand::onTagsFetched(KJob *job)
{
    if (job->error()!=0)
    {
        emit error(job->errorString());
        emit finished(RuntimeError);
        return;
    }

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
        emit finished(NoError);
        return;
    }

    CollectionPathJob *job = new CollectionPathJob(mItemList.first().parentCollection());
    connect(job, SIGNAL(result(KJob *)), SLOT(onParentPathFetched(KJob *)));
    job->start();
}


void DumpCommand::onParentPathFetched(KJob *job)
{
    if (job->error()!=0)
    {
        emit error(job->errorString());
        emit finished(RuntimeError);
        return;
    }

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
    KMimeType::Ptr mime = KMimeType::mimeType(mimeType);
    QString ext = mime->mainExtension();
    // No extension is registered for contact groups
    if (ext.isEmpty() && mimeType=="application/x-vnd.kde.contactgroup") ext = ".group";

    // std::cout << "  " << qPrintable(QString::number(item.id()))
              // << "  " << qPrintable(mimeType)
              // << "  " << qPrintable(ext)
              // << "  " << qPrintable(parent)
              // << std::endl;

    QString destDir = mDirectoryArg+"/";
    if (mMaildir && mimeType=="message/rfc822")		// an email message,
    {							// replicate maildir structure
        QStringList dirs = parent.split('/');
        Q_ASSERT(!dirs.isEmpty());
        foreach (const QString &dir, dirs)
        {
            destDir += "."+dir+".directory/";
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

        if (mDryRun)					// not actually doing anything
        {
            mCreatedDirs.append(destDir);		// so that only reported once
        }
        else
        {
            const QString dirName = dir.dirName();	// this is awkward, why doesn't
            QDir parent(destDir+"/..");			// Qt just have QDir::mkpath()
            if (!parent.mkpath(dirName))		// with no subdirectory name?
            {
                ErrorReporter::fatal(i18nc("@info:shell", "Cannot create directory '%1/%2'",
                                           parent.canonicalPath(), dirName));
                emit finished(RuntimeError);
                return;
            }
        }
    }

    QString destPath = destDir+"/";			// make path of item file
    destPath += QString("%1").arg(item.id(), 8, 10, QLatin1Char('0'));
    destPath += ext;

    std::cout << qPrintable(QString("%1").arg(item.id(), -8)) << " -> " << qPrintable(destPath) << std::endl;
    if (!mDryRun)
    {
        QByteArray data = item.payloadData();		// the raw item payload
        if (mimeType=="text/directory")			// need to fix up tags?
        {
            // Rewrite the "CATGEORIES" line to use the external tag names
            // as opposed to the internal Akonadi URLs.

            bool changed = false;			// not yet, anyway
            QList<QByteArray> oldLines = data.split('\n');
            QStringList newLines;
            foreach (const QByteArray &line, oldLines)
            {
                if (!line.startsWith("CATEGORIES:"))	// not interested in this line
                {
                    newLines.append(QString::fromUtf8(line));
                    continue;
                }

                QStringList oldCats = QString::fromUtf8(line.constData()).mid(11).split(',');
                QStringList newCats;
                foreach (const QString &cat, oldCats)
                {
                    QString newCat = cat;
                    const Akonadi::Tag::Id catId = Akonadi::Tag::fromUrl(cat).id();
							// category tag ID from URL
                    foreach (const Akonadi::Tag &tag, mTagList)
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
            emit finished(RuntimeError);
            return;
        }

        file.write(data);				// output the raw payload
        file.close();

#ifdef Q_OS_UNIX
        struct utimbuf times;				// stamp with item modification time
        times.modtime = item.modificationTime().toTime_t();
        times.actime = time(NULL);
        utime(QFile::encodeName(destPath), &times);
#endif
    }
}
