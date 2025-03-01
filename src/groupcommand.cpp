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

#include "groupcommand.h"

#include "collectionresolvejob.h"

#include <Akonadi/ContactSearchJob>
#include <Akonadi/ItemFetchJob>
#include <Akonadi/ItemFetchScope>
#include <Akonadi/ItemModifyJob>

#include <KContacts/Addressee>

#include <KEmailAddress>

#include <iostream>

#include "commandfactory.h"
#include "errorreporter.h"

using namespace Akonadi;
using namespace Qt::Literals::StringLiterals;
DEFINE_COMMAND("group", GroupCommand, kli18nc("info:shell", "Expand or modify a contact group"));

GroupCommand::GroupCommand(QObject *parent)
    : AbstractCommand(parent)
    , mBriefMode(false)
    , mOperationMode(ModeExpand)
{
}

GroupCommand::~GroupCommand()
{
    delete mGroupItem;
}

void GroupCommand::setupCommandOptions(QCommandLineParser *parser)
{
    addOptionsOption(parser);
    parser->addOption(QCommandLineOption((QStringList() << "e"
                                                        << "expand"),
                                         i18n("Show the expanded contact group (the default operation)")));
    parser->addOption(QCommandLineOption((QStringList() << "a"
                                                        << "add"),
                                         i18n("Add a contact to the group")));
    parser->addOption(QCommandLineOption((QStringList() << "d"
                                                        << "delete"),
                                         i18n("Delete a contact from the group")));
    parser->addOption(QCommandLineOption((QStringList() << "C"
                                                        << "clean"),
                                         i18n("Remove unknown item references from the group")));
    parser->addOption(QCommandLineOption((QStringList() << "c"
                                                        << "comment"),
                                         i18n("Email comment (name) for an added item"),
                                         i18n("name")));
    parser->addOption(QCommandLineOption((QStringList() << "b"
                                                        << "brief"),
                                         i18n("Brief output (for 'expand', email addresses only)")));
    addDryRunOption(parser);

    parser->addPositionalArgument("item", i18nc("@info:shell", "The contact group item, an ID or Akonadi URL"));
    parser->addPositionalArgument("args", i18nc("@info:shell", "Arguments for the operation"), i18n("args..."));
}

AbstractCommand::Error GroupCommand::initCommand(QCommandLineParser *parser)
{
    mItemArgs = parser->positionalArguments();
    if (!checkArgCount(mItemArgs, 1, i18nc("@info:shell", "Missing group argument")))
        return InvalidUsage;

    if (!getCommonOptions(parser))
        return InvalidUsage;

    int modeCount = 0;
    if (parser->isSet("expand"_L1)) {
        ++modeCount;
    }
    if (parser->isSet("add"_L1)) {
        ++modeCount;
    }
    if (parser->isSet("delete"_L1)) {
        ++modeCount;
    }
    if (parser->isSet("clean"_L1)) {
        ++modeCount;
    }
    if (modeCount > 1) {
        emitErrorSeeHelp(i18nc("@info:shell", "Only one of the 'expand', 'add', 'delete' or 'clean' options may be specified"));
        return (InvalidUsage);
    }

    if (parser->isSet("expand"_L1)) { // see if "Expand" mode
        // expand GROUP
        mOperationMode = ModeExpand;
    } else if (parser->isSet("add"_L1)) { // see if "Add" mode
        // add GROUP EMAIL|ID...
        // add GROUP -c NAME EMAIL|ID
        if (mItemArgs.count() < 2) { // missing GROUP was checked above
            emitErrorSeeHelp(i18nc("@info:shell", "No items specified to add"));
            return (InvalidUsage);
        }

        mNameArg = parser->value("comment"_L1);
        if (!mNameArg.isEmpty()) {
            // if the "comment" option is specified,
            // then only one EMAIL|ID argument may be given.
            if (mItemArgs.count() > 2) {
                emitErrorSeeHelp(i18nc("@info:shell", "Only one item may be specified to add with 'comment'"));
                return (InvalidUsage);
            }
        }

        mOperationMode = ModeAdd;
    } else if (parser->isSet("delete"_L1)) { // see if "Delete" mode
        // delete GROUP EMAIL|ID...
        if (mItemArgs.count() < 2) { // missing GROUP was checked above
            emitErrorSeeHelp(i18nc("@info:shell", "No items specified to delete"));
            return (InvalidUsage);
        }

        mOperationMode = ModeDelete;
    } else if (parser->isSet("clean"_L1)) { // see if "Clean" mode
        // clean GROUP
        mOperationMode = ModeClean;
    } else { // no mode option specified
        mOperationMode = ModeExpand;
    }

    if (mOperationMode != ModeAdd) {
        if (parser->isSet("comment"_L1)) {
            emitErrorSeeHelp(i18nc("@info:shell", "The 'comment' option is only allowed with 'add'"));
            return (InvalidUsage);
        }
    }

    mBriefMode = parser->isSet("brief"_L1); // brief/quiet output

    mGroupArg = mItemArgs.takeFirst(); // contact group collection

    Akonadi::Item item = CollectionResolveJob::parseItem(mGroupArg, true);
    if (!item.isValid()) {
        return (InvalidUsage);
    }

    return (NoError);
}

void GroupCommand::start()
{
    if (mOperationMode == ModeDelete || mOperationMode == ModeClean) {
        if (!isDryRun()) { // allow if not doing anything
            if (!allowDangerousOperation()) {
                Q_EMIT finished(RuntimeError);
                return;
            }
        }
    }

    fetchItems();
}

void GroupCommand::fetchItems()
{
    Item item = CollectionResolveJob::parseItem(mGroupArg);
    Q_ASSERT(item.isValid());

    ItemFetchJob *job = new ItemFetchJob(item, this);
    job->fetchScope().fetchFullPayload(true);
    connect(job, &KJob::result, this, &GroupCommand::onItemsFetched);
}

void GroupCommand::onItemsFetched(KJob *job)
{
    if (!checkJobResult(job))
        return;
    ItemFetchJob *fetchJob = qobject_cast<ItemFetchJob *>(job);
    Q_ASSERT(fetchJob != nullptr);

    Item::List items = fetchJob->items();
    if (items.count() < 1) {
        Q_EMIT error(i18nc("@info:shell", "Cannot find '%1' as an item", mGroupArg));
        Q_EMIT finished(RuntimeError);
        return;
    }

    mGroupItem = new Item(items.first());
    if (mGroupItem->mimeType() != KContacts::ContactGroup::mimeType()) {
        Q_EMIT error(i18nc("@info:shell", "Item '%1' is not a contact group", mGroupArg));
        Q_EMIT finished(RuntimeError);
        return;
    }

    if (!mGroupItem->hasPayload<KContacts::ContactGroup>()) { // should never happen?
        Q_EMIT error(i18nc("@info:shell", "Item '%1' has no contact group payload", mGroupArg));
        Q_EMIT finished(RuntimeError);
        return;
    }

    KContacts::ContactGroup group = mGroupItem->payload<KContacts::ContactGroup>();

    AbstractCommand::Error status = RuntimeError;
    switch (mOperationMode) { // perform the requested operation
    case ModeExpand:
        status = showExpandedGroup(group);
        break;

    case ModeAdd:
        status = addGroupItems(group);
        break;

    case ModeDelete:
        status = deleteGroupItems(group);
        break;

    case ModeClean:
        status = cleanGroupItems(group);
        break;
    }

    if (mOperationMode != ModeExpand) { // need to write back?
        if (status == NoError) { // only if there were no errors
            if (!isDryRun()) {
                mGroupItem->setPayload<KContacts::ContactGroup>(group);
                Akonadi::ItemModifyJob *modifyJob = new Akonadi::ItemModifyJob(*mGroupItem);
                modifyJob->exec();
                if (modifyJob->error() != 0) {
                    Q_EMIT error(modifyJob->errorString());
                    status = RuntimeError;
                }
            }
        } else {
            Q_EMIT error(i18nc("@info:shell", "Errors occurred, group not updated"));
        }
    }

    Q_EMIT finished(status);
}

static void writeColumn(const QString &data, int width = 0)
{
    std::cout << qPrintable(data.leftJustified(width)) << "  ";
}

static void writeColumn(quint64 data, int width = 0)
{
    writeColumn(QString::number(data), width);
}

void GroupCommand::displayContactData(const KContacts::ContactGroup::Data &data)
{
    if (mBriefMode) {
        return;
    }

    writeColumn("  D", 5);
    writeColumn("", 8);
    writeColumn(data.email(), 30);
    writeColumn(data.name());
    std::cout << std::endl;
}

void GroupCommand::displayContactReference(Akonadi::Item::Id id)
{
    if (mBriefMode) {
        return;
    }

    writeColumn("  R", 5);
    writeColumn(id, 8);

    Akonadi::Item item(id);
    Akonadi::ItemFetchJob *job = new Akonadi::ItemFetchJob(item);
    job->fetchScope().fetchFullPayload();

    if (!job->exec() || job->count() == 0) {
        writeColumn(i18nc("@info:shell", "(unknown referenced item)"));
    } else {
        const Akonadi::Item::List fetchedItems = job->items();
        Q_ASSERT(fetchedItems.count() > 0);
        const Akonadi::Item fetchedItem = fetchedItems.first();

        if (!fetchedItem.hasPayload<KContacts::Addressee>()) {
            writeColumn(i18nc("@info:shell", "(item has no Addressee payload)"));
        } else {
            KContacts::Addressee addr = fetchedItem.payload<KContacts::Addressee>();
            writeColumn(addr.preferredEmail(), 30);
            writeColumn(addr.formattedName());
        }
    }

    std::cout << std::endl;
}

void GroupCommand::displayContactReference(const Akonadi::Item &item, const QString &email)
{
    if (mBriefMode) {
        return;
    }

    writeColumn("  R", 5);
    writeColumn(item.id(), 8);

    if (!item.hasPayload<KContacts::Addressee>()) {
        writeColumn(i18nc("@info:shell", "(item has no Addressee payload)"));
    } else {
        KContacts::Addressee addr = item.payload<KContacts::Addressee>();
        writeColumn(!email.isEmpty() ? email : addr.preferredEmail(), 30);
        writeColumn(addr.formattedName());
    }

    std::cout << std::endl;
}

void GroupCommand::displayReferenceError(Akonadi::Item::Id id)
{
    if (mBriefMode) {
        return;
    }

    writeColumn("  E", 5);
    writeColumn(id, 8);
    if (id == -1) {
        std::cout << qPrintable(i18nc("@item:shell", "(invalid referenced item)"));
    } else {
        std::cout << qPrintable(i18nc("@item:shell", "(unknown referenced item)"));
    }
    std::cout << std::endl;
}

bool GroupCommand::removeReferenceById(KContacts::ContactGroup &group, const QString &id, bool verbose)
{
    bool somethingFound = false;

    // Remove any existing reference with the same ID from the group.
    for (int i = 0; i < group.contactReferenceCount();) {
        KContacts::ContactGroup::ContactReference existingRef = group.contactReference(i);
        if (existingRef.uid() == id) {
            group.remove(existingRef);
            somethingFound = true;
            if (verbose) {
                displayContactReference(id.toUInt());
            }
        } else {
            ++i;
        }
    }

    return (somethingFound);
}

bool GroupCommand::removeDataByEmail(KContacts::ContactGroup &group, const QString &email, bool verbose)
{
    bool somethingFound = false;

    // Remove any existing data with the same email address from the group.
    for (int i = 0; i < group.dataCount();) {
        const KContacts::ContactGroup::Data data = group.data(i);
        if (QString::compare(data.email(), email, Qt::CaseInsensitive) == 0) {
            group.remove(data);
            somethingFound = true;
            if (verbose) {
                displayContactData(data);
            }
        } else {
            ++i;
        }
    }

    return (somethingFound);
}

AbstractCommand::Error GroupCommand::showExpandedGroup(const KContacts::ContactGroup &group)
{
    if (!mBriefMode) {
        std::cout << qPrintable(i18nc("@info:shell section header 1=item 2=groupref count 3=ref count 4=data count 5=name",
                                      "Group %1 \"%5\" has %2 groups, %3 references and %4 data items:",
                                      QString::number(mGroupItem->id()),
                                      group.contactGroupReferenceCount(),
                                      group.contactReferenceCount(),
                                      group.dataCount(),
                                      group.name()));
        std::cout << std::endl;

        std::cout << " ";
        writeColumn(i18nc("@info:shell column header", "Type"), 4);
        writeColumn(i18nc("@info:shell column header", "ID"), 8);
        writeColumn(i18nc("@info:shell column header", "Email"), 30);
        writeColumn(i18nc("@info:shell column header", "Name"));
        std::cout << std::endl;
    }

    int c = group.contactGroupReferenceCount();
    if (!mBriefMode) {
        for (int i = 0; i < c; ++i) {
            writeColumn("  G", 5);
            writeColumn(group.contactGroupReference(i).uid(), 8);
            std::cout << std::endl;
        }
    }

    c = group.contactReferenceCount();
    QList<Item::Id> fetchIds;
    for (int i = 0; i < c; ++i) {
        Item::Id id = group.contactReference(i).uid().toInt();
        fetchIds.append(id);
    }

    if (!fetchIds.isEmpty()) {
        ItemFetchJob *itemJob = new ItemFetchJob(fetchIds, this);
        itemJob->fetchScope().setFetchModificationTime(false);
        itemJob->fetchScope().fetchAllAttributes(false);
        itemJob->fetchScope().fetchFullPayload(true);

        itemJob->exec();
        if (itemJob->error() != 0) {
            std::cout << std::endl;
            Q_EMIT error(itemJob->errorString());
        }

        Item::List fetchedItems = itemJob->items();
        if (fetchedItems.isEmpty()) {
            Q_EMIT error(i18nc("@info:shell", "No items could be fetched"));
        }

        for (Item::List::const_iterator it = fetchedItems.constBegin(); it != fetchedItems.constEnd(); ++it) {
            const Item item = (*it);
            fetchIds.removeAll(item.id()); // note that we've fetched this

            QString email;
            if (item.hasPayload<KContacts::Addressee>()) {
                KContacts::Addressee addr = item.payload<KContacts::Addressee>();
                email = addr.preferredEmail();

                // Retrieve the original preferred email from the contact group reference.
                // If there is one, display that;  if not, the contact's preferred email.
                for (int i = 0; i < c; ++i) {
                    Item::Id id = group.contactReference(i).uid().toInt();
                    if (id == item.id()) {
                        const QString prefEmail = group.contactReference(i).preferredEmail();
                        if (!prefEmail.isEmpty()) {
                            email = prefEmail;
                        }
                        break;
                    }
                }
            }

            if (mBriefMode) { // only show email
                std::cout << qPrintable(email);
                std::cout << std::endl;
            } else {
                displayContactReference(item, email);
            }
        }

        for (const Item::Id id : std::as_const(fetchIds)) { // error for any that remain
            displayReferenceError(id);
        }
    }

    c = group.dataCount();
    for (int i = 0; i < c; ++i) {
        const KContacts::ContactGroup::Data data = group.data(i);
        if (mBriefMode) { // only show email
            std::cout << qPrintable(data.email());
            std::cout << std::endl;
        } else {
            displayContactData(data); // show full information
        }
    }

    return (!fetchIds.isEmpty() ? RuntimeError : NoError);
}

AbstractCommand::Error GroupCommand::addGroupItems(KContacts::ContactGroup &group)
{
    Q_ASSERT(!mItemArgs.isEmpty());

    if (!mBriefMode) {
        std::cout << qPrintable(i18nc("@info:shell section header 1=item 2=groupref count 3=ref count 4=data count 5=name",
                                      "Adding to group %1 \"%2\":",
                                      QString::number(mGroupItem->id()),
                                      group.name()));
        std::cout << std::endl;
    }

    bool hadError = false; // not yet, anyway
    for (const QString &arg : std::as_const(mItemArgs)) {
        // Look to see whether the argument is an email address
        if (KEmailAddress::isValidSimpleAddress(arg)) {
            const QString email = arg.toLower(); // email addresses are case-insensitive

            Akonadi::ContactSearchJob *job = new Akonadi::ContactSearchJob();
            job->setQuery(Akonadi::ContactSearchJob::Email, email);
            if (!job->exec()) {
                ErrorReporter::error(i18nc("@info:shell", "Cannot search for email '%1', %2", email, job->errorString()));
                hadError = true;
                continue;
            }

            const Item::List resultItems = job->items();
            if (resultItems.count() > 0) {
                // If the query returned a known Akonadi contact, add that
                // (or the first, if there are more than one) as a reference.

                if (resultItems.count() > 1) {
                    ErrorReporter::warning(i18nc("@info:shell", "Multiple contacts found for '%1', using the first one", email));
                }

                Akonadi::Item item = resultItems.first();
                Q_ASSERT(item.hasPayload<KContacts::Addressee>());
                KContacts::ContactGroup::ContactReference ref(QString::number(item.id()));

                // Not really equivalent to "only add the new reference if it
                // doesn't exist already", because e.g. the old reference may have
                // a preferred email set whereas the new one may be different.

                removeReferenceById(group, ref.uid()); // remove any existing
                group.append(ref); // then add new reference
                displayContactReference(item); // report what was added
            } else {
                // If no Akonadi contact matching the specified email could be found,
                // add the email (with the comment, if specified) as contact data.

                removeDataByEmail(group, email); // remove existing with that email
                QString name = (mNameArg.isEmpty() ? arg : mNameArg);
                KContacts::ContactGroup::Data data(name, email);
                group.append(data); // add new contact data
                displayContactData(data); // report what was added
            }
        } else {
            // Not an email address, see if an Akonadi URL or numeric item ID
            Item item = CollectionResolveJob::parseItem(arg, true);
            if (!item.isValid()) {
                hadError = true;
                continue;
            }

            Akonadi::ItemFetchJob *job = new Akonadi::ItemFetchJob(item);
            job->fetchScope().fetchFullPayload();
            if (!job->exec()) {
                ErrorReporter::error(i18nc("@info:shell", "Cannot fetch requested item %1, %2", QString::number(item.id()), job->errorString()));
                hadError = true;
                continue;
            }

            const Akonadi::Item::List fetchedItems = job->items();
            Q_ASSERT(fetchedItems.count() > 0);
            const Akonadi::Item fetchedItem = fetchedItems.first();

            if (!fetchedItem.hasPayload<KContacts::Addressee>()) {
                ErrorReporter::error(i18nc("@info:shell", "Item %1 is not a contact item", QString::number(item.id())));
                hadError = true;
                continue;
            }

            KContacts::ContactGroup::ContactReference ref(QString::number(fetchedItem.id()));

            removeReferenceById(group, ref.uid()); // remove any existing
            group.append(ref); // then add new reference
            displayContactReference(fetchedItem); // report what was added
        }
    }

    if (!mBriefMode) {
        std::cout << qPrintable(i18nc("@info:shell section header 1=item 2=groupref count 3=ref count 4=data count 5=name",
                                      "Group %1 \"%5\" now has %2 groups, %3 references and %4 data items",
                                      QString::number(mGroupItem->id()),
                                      group.contactGroupReferenceCount(),
                                      group.contactReferenceCount(),
                                      group.dataCount(),
                                      group.name()));
        std::cout << std::endl;
    }

    return (hadError ? RuntimeError : NoError);
}

AbstractCommand::Error GroupCommand::deleteGroupItems(KContacts::ContactGroup &group)
{
    Q_ASSERT(!mItemArgs.isEmpty());

    if (!mBriefMode) {
        std::cout << qPrintable(i18nc("@info:shell section header 1=item 2=groupref count 3=ref count 4=data count 5=name",
                                      "Removing from group %1 \"%2\":",
                                      QString::number(mGroupItem->id()),
                                      group.name()));
        std::cout << std::endl;
    }

    bool hadError = false; // not yet, anyway
    for (const QString &arg : std::as_const(mItemArgs)) {
        // Look to see whether the argument is an email address
        if (KEmailAddress::isValidSimpleAddress(arg)) {
            bool somethingFound = false;

            // An email address is specified.  First remove any existing
            // data items having that email address.

            if (removeDataByEmail(group, arg, true)) {
                somethingFound = true; // note that did something
            }

            // Then remove any references to any item containing that email address.
            Akonadi::ContactSearchJob *job = new Akonadi::ContactSearchJob();
            job->setQuery(Akonadi::ContactSearchJob::Email, arg);
            if (!job->exec()) {
                ErrorReporter::error(i18nc("@info:shell", "Cannot search for email '%1', %2", arg, job->errorString()));
                hadError = true;
                continue;
            }

            const Item::List resultItems = job->items();
            for (int itemIndex = 0; itemIndex < resultItems.count(); ++itemIndex) {
                const Akonadi::Item item = resultItems[itemIndex];
                if (removeReferenceById(group, QString::number(item.id()), true)) {
                    somethingFound = true; // note that did something
                }
            }

            if (!somethingFound) { // nothing found to remove
                ErrorReporter::warning(i18nc("@info:shell", "Nothing to remove for email '%1'", arg));
                hadError = true;
                continue;
            }
        } else {
            // Not an email address, see if an Akonadi URL or numeric item ID
            Item item = CollectionResolveJob::parseItem(arg, true);
            if (!item.isValid()) {
                hadError = true;
                continue;
            }

            // Remove any references to that Akonadi ID
            const QString itemId = QString::number(item.id());
            for (int i = 0; i < group.contactReferenceCount();) {
                KContacts::ContactGroup::ContactReference existingRef = group.contactReference(i);
                if (existingRef.uid() == itemId) {
                    displayContactReference(item.id());
                    group.remove(existingRef);
                } else {
                    ++i;
                }
            }
        }
    }

    if (!mBriefMode) {
        std::cout << qPrintable(i18nc("@info:shell section header 1=item 2=groupref count 3=ref count 4=data count 5=name",
                                      "Group %1 \"%5\" now has %2 groups, %3 references and %4 data items",
                                      QString::number(mGroupItem->id()),
                                      group.contactGroupReferenceCount(),
                                      group.contactReferenceCount(),
                                      group.dataCount(),
                                      group.name()));
        std::cout << std::endl;
    }

    return (hadError ? RuntimeError : NoError);
}

AbstractCommand::Error GroupCommand::cleanGroupItems(KContacts::ContactGroup &group)
{
    if (!mBriefMode) {
        std::cout << qPrintable(i18nc("@info:shell section header 1=item 2=groupref count 3=ref count 4=data count 5=name",
                                      "Cleaning references from group %1 \"%2\":",
                                      QString::number(mGroupItem->id()),
                                      group.name()));
        std::cout << std::endl;
    }

    // Remove any reference items with an unknown or invalid ID from the group.
    for (int i = 0; i < group.contactReferenceCount();) {
        KContacts::ContactGroup::ContactReference ref = group.contactReference(i);

        bool doDelete = false;
        qint64 id = ref.uid().toLong();
        if (id == -1) {
            doDelete = true; // invalid, always remove this
        } else {
            Akonadi::Item item(id);
            Akonadi::ItemFetchJob *job = new Akonadi::ItemFetchJob(item);
            // no need for payload
            if (!job->exec() || job->count() == 0) { // if fetch failed or nothing returned
                doDelete = true;
            }
        }

        if (doDelete) { // reference is to be deleted
            group.remove(ref); // remove it from group
            displayReferenceError(id);
        } else {
            ++i;
        }
    }

    if (!mBriefMode) {
        std::cout << qPrintable(i18nc("@info:shell section header 1=item 2=groupref count 3=ref count 4=data count 5=name",
                                      "Group %1 \"%5\" now has %2 groups, %3 references and %4 data items",
                                      QString::number(mGroupItem->id()),
                                      group.contactGroupReferenceCount(),
                                      group.contactReferenceCount(),
                                      group.dataCount(),
                                      group.name()));
        std::cout << std::endl;
    }

    return (NoError);
}
