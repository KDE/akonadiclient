/*
    Copyright (C) 2014-2021  Jonathan Marten <jjm@keelhaul.me.uk>

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

#include "tagscommand.h"

#include <qfile.h>
#include <qregularexpression.h>
#include <qsavefile.h>
#include <qvariant.h>

#include <iostream>

#include <Akonadi/TagCreateJob>
#include <Akonadi/TagDeleteJob>
#include <Akonadi/TagFetchJob>
#include <Akonadi/TagModifyJob>

#include "commandfactory.h"
#include "errorreporter.h"

using namespace Akonadi;
using namespace Qt::Literals::StringLiterals;
DEFINE_COMMAND("tags", TagsCommand, kli18nc("info:shell", "List or modify tags"));

TagsCommand::TagsCommand(QObject *parent)
    : AbstractCommand(parent)
    , mBriefOutput(false)
    , mUrlsOutput(false)
    , mOperationMode(ModeList)
    , mAddForceId(0)
    , mAddForceRetain(false)
{
}

void TagsCommand::setupCommandOptions(QCommandLineParser *parser)
{
    addOptionsOption(parser);
    parser->addOption(QCommandLineOption((QStringList() << "b"
                                                        << "brief"),
                                         i18n("Brief output, tag names or IDs only")));
    parser->addOption(QCommandLineOption((QStringList() << "u"
                                                        << "urls"),
                                         i18n("Brief output, tag URLs only")));

    parser->addOption(QCommandLineOption((QStringList() << "l"
                                                        << "list"),
                                         i18n("List all known tags (the default operation)")));
    parser->addOption(QCommandLineOption((QStringList() << "a"
                                                        << "add"),
                                         i18n("Add named tags")));
    parser->addOption(QCommandLineOption((QStringList() << "d"
                                                        << "delete"),
                                         i18n("Delete tags")));
    parser->addOption(QCommandLineOption((QStringList() << "s"
                                                        << "backup"),
                                         i18n("Save the current tags")));
    parser->addOption(QCommandLineOption((QStringList() << "r"
                                                        << "restore"),
                                         i18n("Restore the saved tags (into a new database)")));
    parser->addOption(QCommandLineOption((QStringList() << "I" << "id"), i18n("ID for a tag to be added (default automatic)"), i18n("id")));
    parser->addOption(QCommandLineOption((QStringList() << "K" << "retain"), i18n("Retain intermediate tags added when the 'id' option is used")));
    parser->addPositionalArgument(i18n("TAG"), i18n("The name of a tag to add, or the name, ID or URL of a tag to delete"), i18n("[TAG...]"));
}

AbstractCommand::Error TagsCommand::initCommand(QCommandLineParser *parser)
{
    mBriefOutput = parser->isSet("brief"_L1);
    mUrlsOutput = parser->isSet("urls"_L1);

    if (parser->isSet("id")) {
        bool ok;
        mAddForceId = parser->value("id"_L1).toUInt(&ok);
        if (!ok || (mAddForceId == 0)) {
            emitErrorSeeHelp(i18nc("@info:shell", "Invalid value for the 'id' option"));
            return (InvalidUsage);
        }
    }

    if (mBriefOutput && mUrlsOutput) {
        emitErrorSeeHelp(i18nc("@info:shell", "The 'brief' and 'urls' options cannot both be specified"));
        return InvalidUsage;
    }

    int modeCount = 0;
    if (parser->isSet("list"_L1)) {
        ++modeCount;
    }
    if (parser->isSet("add"_L1)) {
        ++modeCount;
    }
    if (parser->isSet("delete"_L1)) {
        ++modeCount;
    }
    if (parser->isSet("backup"_L1)) {
        ++modeCount;
    }
    if (parser->isSet("restore"_L1)) {
        ++modeCount;
    }
    if (modeCount > 1) {
        emitErrorSeeHelp(i18nc("@info:shell", "Only one of the 'list', 'add', 'delete', or 'backup'/'restore' options may be specified"));
        return (InvalidUsage);
    }

    const QStringList tagArgs = parser->positionalArguments();

    if (parser->isSet("list"_L1)) { // see if "List" mode
        // expand
        mOperationMode = ModeList;
    } else if (parser->isSet("add"_L1)) { // see if "Add" mode
        // add [-i ID] NAME
        // add NAME...
        mOperationMode = ModeAdd;

        if (tagArgs.isEmpty()) {
            emitErrorSeeHelp(i18nc("@info:shell", "No tags specified to add"));
            return (InvalidUsage);
        }

        if (tagArgs.count() > 1 && mAddForceId != 0) {
            emitErrorSeeHelp(i18nc("@info:shell", "Multiple tags cannot be specified to add with 'id'"));
            return (InvalidUsage);
        }
    } else if (parser->isSet("delete"_L1)) // see if "Delete" mode
    {
        // delete NAME|ID|URL...
        mOperationMode = ModeDelete;

        if (tagArgs.isEmpty()) {
            emitErrorSeeHelp(i18nc("@info:shell", "No tags specified to delete"));
            return (InvalidUsage);
        }
    } else if (parser->isSet("backup"_L1)) { // see if "Backup" mode
        mOperationMode = ModeBackup;
    } else if (parser->isSet("restore"_L1)) { // see if "Restore" mode
        mOperationMode = ModeRestore;
    }

    mAddForceRetain = parser->isSet("retain"_L1);

    if (mAddForceRetain && (mOperationMode != ModeAdd) && (mOperationMode != ModeRestore)) {
        emitErrorSeeHelp(i18nc("@info:shell", "The 'retain' option can only be used with 'add' or 'restore'"));
        return (InvalidUsage);
    }
    if ((mAddForceId != 0) && (mOperationMode != ModeAdd)) {
        emitErrorSeeHelp(i18nc("@info:shell", "The 'id' option can only be used with 'add'"));
        return (InvalidUsage);
    }

    initProcessLoop(tagArgs);
    return NoError;
}

void TagsCommand::start()
{
    // Deleting a tag is obviously a dangerous operation.  Adding a tag with
    // a forced ID is also considered to be so, because it permanently assigns
    // not only the requested tag ID but also any intermediate numbered ones.
    // The same happens, of course, with adding a tag as normal without a
    // forced ID, but in this case it can be assumed that the user is not
    // actually interested in the assigned ID.
    if ((mOperationMode == ModeDelete) || (mOperationMode == ModeRestore) || ((mOperationMode == ModeAdd) && (mAddForceId != 0))) {
        if (!isDryRun()) { // allow if not doing anything
            if (!allowDangerousOperation()) {
                Q_EMIT finished(RuntimeError);
                return;
            }
        }
    }

    if (mOperationMode == ModeRestore) {
        // In this mode, first read the original list of tags that was
        // saved by a previous backup operation.
        const QString readFileName = findSaveFile("savedtags.dat", false);
        qDebug() << "read from" << readFileName;
        QFile readFile(readFileName, this);
        if (!readFile.open(QIODevice::ReadOnly)) {
            Q_EMIT error(i18nc("@info:shell", "Cannot read saved tags from '%1'", readFile.fileName()));
            Q_EMIT error(i18nc("@info:shell", "Run '%1 %2 --backup' first", QCoreApplication::applicationName(), name()));
            Q_EMIT finished(RuntimeError);
            return;
        }

        readSavedTags(&readFile); // populates mOrigTagMap
        if (mOrigTagMap.isEmpty()) {
            ErrorReporter::notice(i18nc("@info:shell", "No saved tags to restore"));
            return;
        }
    }

    TagFetchJob *job = new TagFetchJob(this); // always need current tag list
    connect(job, &KJob::result, this, &TagsCommand::onTagsFetched);
}

void TagsCommand::addNextTag()
{
    Tag::Id id;
    QString name;

    if (mOperationMode == ModeRestore) { // restore, currentArg() is tag ID
        id = static_cast<Tag::Id>(currentArg().toULong());
        name = mOrigTagMap[id];
    } else { // add, currentArg() is name
        name = currentArg();
        id = mAddForceId;
    }

    qDebug() << id << name;

    // See whether a tag with that name or ID already exists
    for (const Tag &tag : mFetchedTags) {
        if (tag.name() == name || (id != 0 && tag.id() == id)) {
            Q_EMIT error(i18nc("@info:shell", "A tag named '%1' ID %2 already exists", tag.name(), QString::number(tag.id())));
            processNext(); // ignore the conflicting tag
            return;
        }
    }

    // Try to add the tag.  The construction must be done in this order
    // (using the constructor taking a name and then setting the ID), because
    // constructing a tag using the ID and then setting the name does not set
    // the tag GID.
    Tag newTag(name);
    if (id != 0)
        newTag.setId(id);
    TagCreateJob *createJob = new TagCreateJob(newTag, this);
    if (id != 0)
        createJob->setProperty("requested", id);
    connect(createJob, &KJob::result, this, &TagsCommand::onTagAdded);
    createJob->start();
}

void TagsCommand::onTagAdded(KJob *job)
{
    if (!checkJobResult(job))
        return;
    TagCreateJob *createJob = qobject_cast<TagCreateJob *>(job);
    Q_ASSERT(createJob != nullptr);

    Tag addedTag = createJob->tag();
    const Tag::Id requestedId = createJob->property("requested").value<Tag::Id>();
    const Tag::Id addedId = addedTag.id();

    if (!addedTag.isValid()) {
        // It was not possible to add the tag.  Since the presence of an
        // already existing name or ID has already checked, this is unlikely
        // to happen unless there is a serious problem with the Akonadi
        // server or database.
        if (requestedId != 0) {
            Q_EMIT error(i18nc("@info:shell", "Failed to add tag '%1' ID %2", currentArg(), QString::number(requestedId)));
        } else {
            Q_EMIT error(i18nc("@info:shell", "Failed to add tag '%1'", currentArg()));
        }

        processNext(); // try again with next
        return;
    }

    // The tag was successfully added.  If a particular ID was requested,
    // then check the three possible cases for the ID of the tag that
    // was actually added.
    if ((requestedId != 0) && (addedId != requestedId)) { // didn't create the one requested

        if (addedId > requestedId) { // too high

            // A tag with the requested ID could not and will never be able
            // to be created, because the ID sequence is already past that.
            // Delete the tag that was just added, and give up.  There is
            // no need to call processNext() because initCommand() above
            // checked that there is only one tag argument.
            ErrorReporter::error(
                xi18nc("@info:shell", "Cannot create a tag with ID %1, sequence already at ID %2", QString::number(requestedId), QString::number(addedId)));
            if (!mAddForceRetain) {
                qDebug() << "delete unwanted tag" << addedTag.id();
                TagDeleteJob *deleteJob = new TagDeleteJob(addedTag, this);
                deleteJob->setProperty("toohigh", true);
                deleteJob->setProperty("requested", createJob->property("requested"));
                connect(deleteJob, &KJob::result, this, &TagsCommand::onTagDeleted);
                deleteJob->start();
            } else {
                const QString newName = QCoreApplication::applicationName() + "-" + QString::number(addedTag.id());
                qDebug() << "rename unwanted tag" << addedTag.id() << "->" << newName;
                addedTag.setName(newName);
                TagModifyJob *modifyJob = new TagModifyJob(addedTag, this);
                modifyJob->setProperty("toohigh", true);
                modifyJob->setProperty("requested", createJob->property("requested"));
                connect(modifyJob, &KJob::result, this, &TagsCommand::onTagDeleted);
                modifyJob->start();
            }
        } else { // not high enough

            // The tag ID allocated was numerically less than the requested
            // one.  Delete the tag that was just added and repeat the operation -
            // as many times as is necessary, until the allocated tag ID
            // reaches the requested one.
            if (!mAddForceRetain) {
                qDebug() << "delete temp tag" << addedTag.id();
                TagDeleteJob *deleteJob = new TagDeleteJob(addedTag, this);
                deleteJob->setProperty("toohigh", false);
                deleteJob->setProperty("requested", createJob->property("requested"));
                connect(deleteJob, &KJob::result, this, &TagsCommand::onTagDeleted);
                deleteJob->start();
            } else {
                const QString newName = QCoreApplication::applicationName() + "-" + QString::number(addedTag.id());
                qDebug() << "rename temp tag" << addedTag.id() << "->" << newName;
                addedTag.setName(newName);
                TagModifyJob *modifyJob = new TagModifyJob(addedTag, this);
                modifyJob->setProperty("toohigh", false);
                modifyJob->setProperty("requested", createJob->property("requested"));
                connect(modifyJob, &KJob::result, this, &TagsCommand::onTagDeleted);
                modifyJob->start();
            }
        }

        return;
    }

    // If we get here the tag was successfully added and, if an ID was forced,
    // that ID was allocated as intended.  Report the result and go on to the
    // next argument if there is one.
    if (mBriefOutput)
        std::cout << addedTag.id() << std::endl;
    else if (mUrlsOutput)
        std::cout << qPrintable(addedTag.url().toDisplayString()) << std::endl;
    else
        ErrorReporter::success(xi18nc("@info:shell", "Added tag '%1' ID %2", addedTag.name(), QString::number(addedId)));

    processNext(); // continue to do next
}

void TagsCommand::deleteNextTag()
{
    Tag delTag;
    // See if user input is a valid integer as a tag ID
    bool ok;
    unsigned int id = currentArg().toUInt(&ok);
    if (ok)
        delTag = Tag(id); // conversion succeeded
    else {
        // Otherwise check if we have an Akonadi URL
        const QUrl url = QUrl::fromUserInput(currentArg());
        if (url.isValid() && url.scheme() == QLatin1String("akonadi")) { // valid Akonadi URL
            delTag = Tag::fromUrl(url);
        } else {
            // Otherwise assume a tag name.  Unfortunately this means that
            // a tag with a name that looks like an integer or a URL cannot be
            // deleted.  This is not very likely.
            //
            // A tag can only be deleted by ID, so find the corresponding
            // tag with that name in the fetched list and use its ID.
            for (const Tag &tag : mFetchedTags) {
                if (tag.name() == currentArg()) {
                    delTag = Tag(tag.id());
                    break;
                }
            }

            // Check now whether the named tag currently exists.
            if (delTag.id() == -1) // no tag found by loop above
            {
                Q_EMIT error(i18nc("@info:shell", "Tag to delete '%1' does not exist", currentArg()));
                processNext(); // ignore the missing tag
                return;
            }
        }
    }

    // Need to verify that a tag with the specified ID currently exists
    // in the Akonadi database.  Otherwise attempting to delete a tag
    // with a nonexistent ID crashes the server with the assert:
    //
    //   qt_assert_x(where="QueryBuilder::buildWhereCondition()", what="No values given for IN condition.",
    //               file="akonadi/src/server/storage/querybuilder.cpp"
    //   Akonadi::Server::QueryBuilder::buildWhereCondition() at akonadi/src/server/storage/querybuilder.cpp:536
    //   Akonadi::Server::QueryBuilder::buildWhereCondition() at akonadi/src/server/storage/querybuilder.cpp:522
    //   Akonadi::Server::QueryBuilder::buildQuery() at akonadi/src/server/storage/querybuilder.cpp:319
    //   Akonadi::Server::QueryBuilder::exec() at akonadi/src/server/storage/querybuilder.cpp:366
    //   Akonadi::Server::DataStore::removeTags() at akonadi/src/server/storage/datastore.cpp:670
    //   Akonadi::Server::TagDeleteHandler::parseStream() at akonadi/src/server/handler/tagdeletehandler.cpp:40
    //   Akonadi::Server::Connection::parseStream() at akonadi/src/server/connection.cpp:162
    //
    // Look for a matching tag by ID, if a name has been specified then
    // it will have been resolved to an existing ID above.
    Q_ASSERT(delTag.isValid());
    for (const Tag &tag : mFetchedTags) {
        if (delTag.id() == tag.id()) // tag specified by ID
        {
            // It is now safe to delete the tag.  Use the fetched tag,
            // since that will have both its ID and name available to
            // be reported later.
            TagDeleteJob *deleteJob = new TagDeleteJob(tag, this);
            connect(deleteJob, &KJob::result, this, &TagsCommand::onTagDeleted);
            deleteJob->start();
            return;
        }
    }

    Q_EMIT error(i18nc("@info:shell", "Tag to delete ID %1 does not exist", QString::number(delTag.id())));
    processNext(); // ignore the missing tag
}

void TagsCommand::onTagDeleted(KJob *job)
{
    if (!checkJobResult(job))
        return;

    // This may be called on completion of either a TagDeleteJob or a
    // TagModifyJob.  So do not try to cast or check the type of the 'job'
    // parameter until which of those has been resolved.  A TagModifyJob
    // will only have been created by onTagAdded() and will always have
    // the "requested" property set.
    const Tag::Id requestedId = job->property("requested").value<Tag::Id>();
    if (requestedId != 0) {
        qDebug() << "requested" << requestedId << "toohigh?" << job->property("toohigh").toBool();

        // This deletion or renaming was done as a result of trying to force a
        // tag to be created with a specific ID, and that was not possible in
        // some way.
        if (job->property("toohigh").toBool()) {
            // The tag could not be created because the creation sequence is
            // already past that point.  There is no point in trying again for
            // that particular tag, but if restoring then try subsequent ones
            // because there may be a gap.
            ErrorReporter::warning(xi18nc("@info:shell", "Tag could not be created with the requested ID"));
            if (mOperationMode == ModeRestore) {
                processNext();
            } else {
                Q_EMIT finished(RuntimeError);
            }
            return;
        }

        // Try again to add the tag, which this time will get the next ID
        // in sequence.
        addNextTag();
        return;
    }

    TagDeleteJob *deleteJob = qobject_cast<TagDeleteJob *>(job);
    Q_ASSERT(deleteJob != nullptr);
    const Tag deletedTag = deleteJob->tags().first(); // must be one and only one

    // The tag has been deleted as requested by a user action, so acknowledge it.
    if (mBriefOutput)
        std::cout << deletedTag.id() << std::endl;
    else if (mUrlsOutput)
        std::cout << qPrintable(deletedTag.url().toDisplayString()) << std::endl;
    else
        ErrorReporter::success(i18nc("@info:shell", "Deleted tag '%1' ID %2", deletedTag.name(), QString::number(deletedTag.id())));

    processNext(); // continue to do next
}

static void writeColumn(const QString &data, int width = 0)
{
    std::cout << qPrintable(data.leftJustified(width)) << "  ";
}

static void writeColumn(quint64 data, int width = 0)
{
    writeColumn(QString::number(data), width);
}

void TagsCommand::onTagsFetched(KJob *job)
{
    if (!checkJobResult(job))
        return;
    TagFetchJob *fetchJob = qobject_cast<TagFetchJob *>(job);
    Q_ASSERT(fetchJob != nullptr);
    mFetchedTags = fetchJob->tags();

    // Now that the current tag list has been fetched,
    // look at what to do.
    if ((mOperationMode == ModeList) || (mOperationMode == ModeBackup)) {
        if (mFetchedTags.isEmpty()) {
            Q_EMIT error(i18nc("@info:shell", "No tags found"));
            Q_EMIT finished(NoError);
        }
    }

    if (mOperationMode == ModeList)
        listTags();
    else if (mOperationMode == ModeAdd)
        startProcessLoop("addNextTag");
    else if (mOperationMode == ModeDelete)
        startProcessLoop("deleteNextTag");
    else if (mOperationMode == ModeBackup)
        backupTags();
    else if (mOperationMode == ModeRestore)
        restoreTags();
}

void TagsCommand::listTags()
{
    if (!mBriefOutput && !mUrlsOutput) {
        writeColumn(i18nc("@info:shell column header", "ID"), 8);
        writeColumn(i18nc("@info:shell column header", "URL"), 25);
        writeColumn(i18nc("@info:shell column header", "Name"));
        std::cout << std::endl;
    }

    for (const Tag &tag : std::as_const(mFetchedTags)) {
        if (!mBriefOutput && !mUrlsOutput) {
            writeColumn(tag.id(), 8);
        }
        if (!mBriefOutput || mUrlsOutput) {
            writeColumn(tag.url().toDisplayString(), 25);
        }
        if (!mUrlsOutput) {
            writeColumn(tag.name());
        }
        std::cout << std::endl;
    }

    Q_EMIT finished();
}

void TagsCommand::backupTags()
{
    ErrorReporter::info(i18nc("@info:shell", "Found %1 current Akonadi tags", mFetchedTags.count()));

    // Save the current list of tags to the "saved"
    // data file.  After that there is no more to do.
    const QString saveFileName = findSaveFile("savedtags.dat", true);
    qDebug() << "backup to" << saveFileName;
    QSaveFile saveFile(saveFileName);
    if (!saveFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        Q_EMIT error(i18nc("@info:shell", "Cannot save backup list to '%1'", saveFile.fileName()));
        Q_EMIT finished(RuntimeError);
        return;
    }

    QTextStream ts(&saveFile);
    for (const Tag &tag : std::as_const(mFetchedTags)) {
        ts << Qt::left << qSetFieldWidth(8) << QString::number(tag.id()) << qSetFieldWidth(0) << "  " << qSetFieldWidth(20) << tag.url().toDisplayString()
           << qSetFieldWidth(0) << "  " << qSetFieldWidth(0) << tag.name() << Qt::endl;
    }

    ts.flush();
    saveFile.commit(); // finished with save file

    ErrorReporter::success(i18nc("@info:shell", "Saved tags to '%1'", saveFile.fileName()));
    Q_EMIT finished();
}

void TagsCommand::readSavedTags(QFileDevice *file)
{
    ErrorReporter::progress(i18nc("@info:shell", "Reading saved tags from '%1'", file->fileName()));
    QTextStream ts(file);
    const QRegularExpression rx("^(\\d+)\\s+(\\S+)\\s+(.+)$");
    while (!ts.atEnd()) {
        const QString line = ts.readLine().trimmed();
        const QRegularExpressionMatch match = rx.match(line);
        if (!match.hasMatch())
            continue;

        // It is not necessary to parse and restore the tag URL, since it
        // can be obtained simply by creating a Tag with a given ID, with
        // no Akonadi operations required.
        const Tag::Id id = static_cast<Tag::Id>(match.captured(1).toULong());
        const QString name = match.captured(3);
        mOrigTagMap.insert(id, name);
    }

    ErrorReporter::info(i18nc("@info:shell", "Read %1 saved tags", mOrigTagMap.count()));
}

void TagsCommand::restoreTags()
{
    // Get the list of tag IDs to be restored, in numerical order.  The new
    // tags must be created in this order, but there may be gaps in the
    // sequence.
    QList<Tag::Id> tagIds = mOrigTagMap.keys();
    std::sort(tagIds.begin(), tagIds.end());
    qDebug() << "to restore" << tagIds;

    QStringList tagArgs;
    // The process loop arguments need to be a list of strings.  Yes, this
    // does involve a round trip ID -> string -> ID conversion, but the
    // total number of tags to restore is expected to be fairly small.
    for (const Tag::Id &id : std::as_const(tagIds)) {
        tagArgs.append(QString::number(id));
    }

    initProcessLoop(tagArgs, i18n("Tags restored"));
    startProcessLoop("addNextTag");
}
