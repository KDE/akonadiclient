/*
    Copyright (C) 2014  Jonathan Marten <jjm@keelhaul.me.uk>

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

#include <AkonadiCore/tagfetchjob.h>

#include "commandfactory.h"

using namespace Akonadi;

DEFINE_COMMAND("tags", TagsCommand, "List all known tags");

TagsCommand::TagsCommand(QObject *parent)
    : AbstractCommand(parent),
      mBriefOutput(false),
      mUrlsOutput(false)
{
}

void TagsCommand::setupCommandOptions(QCommandLineParser *parser)
{
    addOptionsOption(parser);
    parser->addOption(QCommandLineOption((QStringList() << "b" << "brief"), i18n("Brief output, tag names only")));
    parser->addOption(QCommandLineOption((QStringList() << "u" << "urls"), i18n("Brief output, tag URLs only")));
}

int TagsCommand::initCommand(QCommandLineParser *parser)
{
    mBriefOutput = parser->isSet("brief");
    mUrlsOutput = parser->isSet("urls");

    if (mBriefOutput && mUrlsOutput) {
        emitErrorSeeHelp(i18nc("@info:shell", "The 'brief' and 'urls' options cannot both be specified"));
        return InvalidUsage;
    }

    return NoError;
}

void TagsCommand::start()
{
    TagFetchJob *job = new TagFetchJob(this);
    connect(job, &KJob::result, this, &TagsCommand::onTagsFetched);
}

static void writeColumn(const QString &data, int width = 0)
{
    std::cout << data.leftJustified(width).toLocal8Bit().constData() << "  ";
}

static void writeColumn(quint64 data, int width = 0)
{
    writeColumn(QString::number(data), width);
}

void TagsCommand::onTagsFetched(KJob *job)
{
    if (job->error() != 0) {
        emit error(job->errorString());
        emit finished(RuntimeError);
        return;
    }

    TagFetchJob *fetchJob = qobject_cast<TagFetchJob *>(job);
    Q_ASSERT(fetchJob != nullptr);

    Tag::List tags = fetchJob->tags();
    if (tags.count() < 1) {
        emit error(i18nc("@info:shell", "No tags found"));
        emit finished(NoError);
        return;
    }

    if (!mBriefOutput && !mUrlsOutput) {
        writeColumn(i18nc("@info:shell column header", "ID"), 8);
        writeColumn(i18nc("@info:shell column header", "URL"), 25);
        writeColumn(i18nc("@info:shell column header", "Name"));
        std::cout << std::endl;
    }

    for (Tag::List::const_iterator it = tags.constBegin(), end = tags.constEnd(); it != end; ++it) {
        const Tag tag = (*it);

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

    emit finished(NoError);
}
