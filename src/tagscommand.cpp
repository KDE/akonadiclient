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

#include <iostream>

#include <Akonadi/TagFetchJob>
#include <Akonadi/TagCreateJob>
#include <Akonadi/TagDeleteJob>

#include "commandfactory.h"

using namespace Akonadi;

DEFINE_COMMAND("tags", TagsCommand, I18N_NOOP("List or modify tags"));

TagsCommand::TagsCommand(QObject *parent)
    : AbstractCommand(parent),
      mBriefOutput(false),
      mUrlsOutput(false),
      mOperationMode(ModeList),
      mAddForceId(0)
{
}

void TagsCommand::setupCommandOptions(QCommandLineParser *parser)
{
    addOptionsOption(parser);
    parser->addOption(QCommandLineOption((QStringList() << "b" << "brief"), i18n("Brief output, tag names or IDs only")));
    parser->addOption(QCommandLineOption((QStringList() << "u" << "urls"), i18n("Brief output, tag URLs only")));

    parser->addOption(QCommandLineOption((QStringList() << "l" << "list"), i18n("List all known tags (the default operation)")));
    parser->addOption(QCommandLineOption((QStringList() << "a" << "add"), i18n("Add named tags")));
    parser->addOption(QCommandLineOption((QStringList() << "d" << "delete"), i18n("Delete tags")));
    // This option would be nice to have but will not work, an ID is
    // automatically assigned by the Akonadi server when a tag is created.
    //parser->addOption(QCommandLineOption((QStringList() << "i" << "id"), i18n("ID for a tag to be added (default automatic)"), i18n("id")));

    parser->addPositionalArgument(i18n("TAG"), i18n("The name of a tag to add, or the name, ID or URL of a tag to delete"), i18n("[TAG...]"));
}

int TagsCommand::initCommand(QCommandLineParser *parser)
{
    mBriefOutput = parser->isSet("brief");
    mUrlsOutput = parser->isSet("urls");

    //if (parser->isSet("id"))
    //{
    //    bool ok;
    //    mAddForceId = parser->value("id").toUInt(&ok);
    //    if (!ok || mAddForceId==0)
    //    {
    //        emitErrorSeeHelp(i18nc("@info:shell", "Invalid value for the 'id' option"));
    //        return (InvalidUsage);
    //    }
    //}

    if (mBriefOutput && mUrlsOutput) {
        emitErrorSeeHelp(i18nc("@info:shell", "The 'brief' and 'urls' options cannot both be specified"));
        return InvalidUsage;
    }

    int modeCount = 0;
    if (parser->isSet("list")) {
        ++modeCount;
    }
    if (parser->isSet("add")) {
        ++modeCount;
    }
    if (parser->isSet("delete")) {
        ++modeCount;
    }
    if (modeCount > 1) {
        emitErrorSeeHelp(i18nc("@info:shell", "Only one of the 'list', 'add' or 'delete' options may be specified"));
        return (InvalidUsage);
    }

    const QStringList tagArgs = parser->positionalArguments();

    if (parser->isSet("list")) {			// see if "List" mode
        // expand
        mOperationMode = ModeList;
    } else if (parser->isSet("add")) {			// see if "Add" mode
        // add [-i ID] NAME
        // add NAME...
        mOperationMode = ModeAdd;

        if (tagArgs.isEmpty())
        {
            emitErrorSeeHelp(i18nc("@info:shell", "No tags specified to add"));
            return (InvalidUsage);
        }

        if (tagArgs.count()>1 && mAddForceId!=0)
        {
            emitErrorSeeHelp(i18nc("@info:shell", "Multiple tags cannot be specified to add with 'id'"));
            return (InvalidUsage);
        }
    }
    else if (parser->isSet("delete"))			// see if "Delete" mode
    {
        // delete NAME|ID|URL...
        mOperationMode = ModeDelete;

        if (tagArgs.isEmpty())
        {
            emitErrorSeeHelp(i18nc("@info:shell", "No tags specified to delete"));
            return (InvalidUsage);
        }
    }

    initProcessLoop(tagArgs);
    return NoError;
}

void TagsCommand::start()
{
    if (mOperationMode == ModeDelete) {
        if (!isDryRun()) {				// allow if not doing anything
            if (!allowDangerousOperation()) {
                Q_EMIT finished(RuntimeError);
                return;
            }
        }
    }

    TagFetchJob *job = new TagFetchJob(this);		// always need tag list
    connect(job, &KJob::result, this, &TagsCommand::onTagsFetched);
}

void TagsCommand::addNextTag()
{
    // See whether a tag with that name or ID already exists
    for (const Tag &tag : mFetchedTags)
    {
        if (tag.name()==currentArg() || (mAddForceId!=0 && tag.id()==mAddForceId))
        {
            Q_EMIT error(i18nc("@info:shell", "A tag named '%1' ID %2 already exists",
                             tag.name(), QString::number(tag.id())));
            processNext();				// ignore the conflicting tag
            return;
        }
    }

    Tag newTag(currentArg());
    if (mAddForceId!=0) newTag.setId(mAddForceId);
    TagCreateJob *createJob = new TagCreateJob(newTag, this);
    connect(createJob, &KJob::result, this, &TagsCommand::onTagAdded);
}

void TagsCommand::onTagAdded(KJob *job)
{
    if (!checkJobResult(job)) return;
    TagCreateJob *createJob = qobject_cast<TagCreateJob *>(job);
    Q_ASSERT(createJob != nullptr);

    Tag addedTag = createJob->tag();
    if (!addedTag.isValid())
    {
        if (mAddForceId!=0)
        {
            Q_EMIT error(i18nc("@info:shell", "Cannot add tag '%1' ID %2",
                             currentArg(), QString::number(mAddForceId)));
        }
        else
        {
            Q_EMIT error(i18nc("@info:shell", "Cannot add tag '%1'", currentArg()));
        }
    }
    else
    {
        if (mBriefOutput) std::cout << addedTag.id() << std::endl;
        else if (mUrlsOutput) std::cout << qPrintable(addedTag.url().toDisplayString()) << std::endl;
        else std::cout << "Added tag '" << qPrintable(addedTag.name()) << "' ID " << addedTag.id() << std::endl;
    }

    processNext();					// continue to do next
}

void TagsCommand::deleteNextTag()
{
    Tag delTag;
    // See if user input is a valid integer as a tag ID
    bool ok;
    unsigned int id = currentArg().toUInt(&ok);
    if (ok) delTag = Tag(id);				// conversion succeeded
    else
    {
        // Otherwise check if we have an Akonadi URL
        const QUrl url = QUrl::fromUserInput(currentArg());
        if (url.isValid() && url.scheme() == QLatin1String("akonadi"))
        {						// valid Akonadi URL
            delTag = Tag::fromUrl(url);
        }
        else
        {
            // Otherwise assume a tag name.  Unfortunately this means that
            // a tag with a name that looks like an integer or a URL cannot be
            // deleted.  This is not very likely.
            //
            // A tag can only be deleted by ID, so find the corresponding
            // tag with that name in the fetched list and use its ID.
            for (const Tag &tag : mFetchedTags)
            {
                if (tag.name() == currentArg())
                {
                    delTag = Tag(tag.id());
                    break;
                }
            }

            // Check now whether the named tag currently exists.
            if (delTag.id()==-1)			// no tag found by loop above
            {
                Q_EMIT error(i18nc("@info:shell", "Tag to delete '%1' does not exist", currentArg()));
                processNext();				// ignore the missing tag
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
    for (const Tag &tag : mFetchedTags)
    {
        if (delTag.id()==tag.id())			// tag specified by ID
        {
            // It is now safe to delete the tag.  Use the fetched tag,
            // since that will have both its ID and name available to
            // be reported later.
            TagDeleteJob *deleteJob = new TagDeleteJob(tag, this);
            connect(deleteJob, &KJob::result, this, &TagsCommand::onTagDeleted);
            return;
        }
    }

    Q_EMIT error(i18nc("@info:shell", "Tag to delete ID %1 does not exist", QString::number(delTag.id())));
    processNext();					// ignore the missing tag
}

void TagsCommand::onTagDeleted(KJob *job)
{
    if (!checkJobResult(job)) return;
    TagDeleteJob *deleteJob = qobject_cast<TagDeleteJob *>(job);
    Q_ASSERT(deleteJob != nullptr);

    Tag deletedTag = deleteJob->tags().first();		// must be one and only one
    if (mBriefOutput) std::cout << deletedTag.id() << std::endl;
    else if (mUrlsOutput) std::cout << qPrintable(deletedTag.url().toDisplayString()) << std::endl;
    else std::cout << "Deleted tag '" << qPrintable(deletedTag.name()) << "' ID " << deletedTag.id() << std::endl;
    processNext();					// continue to do next
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
    if (!checkJobResult(job)) return;
    TagFetchJob *fetchJob = qobject_cast<TagFetchJob *>(job);
    Q_ASSERT(fetchJob != nullptr);
    mFetchedTags = fetchJob->tags();

    // Now that the current tag list has been fetched,
    // look at what to do.
    if (mOperationMode == ModeList)
    {
        if (mFetchedTags.isEmpty())
        {
            Q_EMIT error(i18nc("@info:shell", "No tags found"));
            Q_EMIT finished(NoError);
        }
        else listTags();
    }
    else if (mOperationMode == ModeAdd) startProcessLoop("addNextTag");
    else if (mOperationMode == ModeDelete) startProcessLoop("deleteNextTag");
}

void TagsCommand::listTags()
{
    if (!mBriefOutput && !mUrlsOutput) {
        writeColumn(i18nc("@info:shell column header", "ID"), 8);
        writeColumn(i18nc("@info:shell column header", "URL"), 25);
        writeColumn(i18nc("@info:shell column header", "Name"));
        std::cout << std::endl;
    }

    for (const Tag &tag : qAsConst(mFetchedTags))
    {
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
