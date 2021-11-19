/*
    Copyright (C) 2013  Jonathan Marten <jjm@keelhaul.me.uk>

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

#include "showcommand.h"

#include "collectionresolvejob.h"

#include <akonadi/itemfetchjob.h>
#include <akonadi/itemfetchscope.h>

#include <KMime/Message>

#include <iostream>

#include "commandfactory.h"
#include "errorreporter.h"

using namespace Akonadi;

DEFINE_COMMAND("show", ShowCommand, I18N_NOOP("Show the raw payload of an item"));

ShowCommand::ShowCommand(QObject *parent)
    : AbstractCommand(parent)
{
}

void ShowCommand::setupCommandOptions(QCommandLineParser *parser)
{
    parser->addPositionalArgument("item", i18nc("@info:shell", "The items to show"), i18nc("@info:shell", "item..."));
    parser->addOption(QCommandLineOption((QStringList() << QStringLiteral("r") << QStringLiteral("raw")), i18nc("@info:shell", "Use raw payload (disables quoted-printable decoding)")));
}

int ShowCommand::initCommand(QCommandLineParser *parser)
{
    const QStringList itemArgs = parser->positionalArguments();
    if (!checkArgCount(itemArgs, 1, i18nc("@info:shell", "No items specified"))) return InvalidUsage;

    mRaw = parser->isSet("raw");

    initProcessLoop(itemArgs);
    return NoError;
}

void ShowCommand::start()
{
    startProcessLoop("processNextItem");
}

void ShowCommand::processNextItem()
{
    Item item = CollectionResolveJob::parseItem(currentArg(), true);
    if (!item.isValid()) {
        processNext();
        return;
    }

    ItemFetchJob *job = new ItemFetchJob(item, this);
    job->fetchScope().setFetchModificationTime(false);
    job->fetchScope().fetchAllAttributes(false);
    job->fetchScope().fetchFullPayload(true);
    connect(job, &KJob::result, this, &ShowCommand::onItemFetched);
}

void ShowCommand::onItemFetched(KJob *job)
{
    if (!checkJobResult(job)) {
        return;
    } else {
        ItemFetchJob *fetchJob = qobject_cast<ItemFetchJob *>(job);
        Q_ASSERT(fetchJob != nullptr);
        Item::List items = fetchJob->items();
        if (items.isEmpty()) {
            emit error(i18nc("@info:shell", "No result returned for item '%1'", currentArg()));
        } else {
            Akonadi::Item item = items.first();
            if (!item.hasPayload()) {
                emit error(i18nc("@info:shell", "Item '%1' has no payload", currentArg()));
            } else if (mRaw) {
                std::cout << item.payloadData().constData();    // output the raw payload
            } else {
                if (item.hasPayload<KMime::Message::Ptr>()) {
                    const KMime::Message::Ptr mail = item.payload<KMime::Message::Ptr>();
                    std::cout << qPrintable(mail->head());
                    const auto mainPart = mail->mainBodyPart();
                    if (mainPart) {
                        std::cout << qPrintable(mainPart->decodedText());
                    } else {
                        ErrorReporter::warning(i18n("Item has no main body part"));
                    }
                } else {
                    std::cout << qPrintable(QString::fromUtf8(item.payloadData().constData()));
                }
            }

            if (!isProcessLoopFinished()) {		// not the last item
                std::cout << "\n";			// blank line to separate
            }
        }
    }

    processNext();
}
