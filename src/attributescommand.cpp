/*
    Copyright (C) 2024  Jonathan Marten <jjm@keelhaul.me.uk>

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

#include "attributescommand.h"

#include <Akonadi/CollectionModifyJob>

#include <iostream>

#include "collectionpathjob.h"
#include "collectionresolvejob.h"
#include "commandfactory.h"

using namespace Akonadi;

// This attribute class allows both the type (name) and value to be set
// to arbitrary data, as specified for an "add" or "modify" command.
// This is needed because it is not possible with the Akonadi API to
// just add a collection attribute with a specified name, it must be an
// object which is a subclass of Akonadi::Attribute.  However Akonadi
// does not care about the actual C++ class of the attribute, as long as
// its type() returns the expected name.

class SyntheticAttribute : public Akonadi::Attribute
{
public:
    SyntheticAttribute(const QByteArray &type = QByteArray(), const QByteArray &value = QByteArray())
    {
        mType = type;
        mValue = value;
    }

    QByteArray type() const override
    {
        return (mType);
    }
    Akonadi::Attribute *clone() const override
    {
        return (new SyntheticAttribute(mType, mValue));
    }
    QByteArray serialized() const override
    {
        return (mValue);
    }
    void deserialize(const QByteArray &data) override
    {
        mValue = data;
    }

private:
    QByteArray mType;
    QByteArray mValue;
};

// AttributesCommand

DEFINE_COMMAND("attributes", AttributesCommand, kli18nc("info:shell", "Show or update attributes for a collection"));

AttributesCommand::AttributesCommand(QObject *parent)
    : AbstractCommand(parent)
{
}

AttributesCommand::~AttributesCommand()
{
}

void AttributesCommand::setupCommandOptions(QCommandLineParser *parser)
{
    addOptionsOption(parser);

    parser->addOption(QCommandLineOption((QStringList() << "s"
                                                        << "show"),
                                         i18n("Show the collection attributes (the default operation)")));
    parser->addOption(QCommandLineOption((QStringList() << "a"
                                                        << "add"),
                                         i18n("Add an attribute")));
    parser->addOption(QCommandLineOption((QStringList() << "m"
                                                        << "modify"),
                                         i18n("Modify an existing attribute")));
    parser->addOption(QCommandLineOption((QStringList() << "d"
                                                        << "delete"),
                                         i18n("Delete an existing attribute")));
    parser->addOption(QCommandLineOption((QStringList() << "x"
                                                        << "hex"),
                                         i18n("Show or interpret the attribute value as hex encoded")));

    parser->addPositionalArgument("collection", i18nc("@info:shell", "The collection: an ID, path or Akonadi URL"));
    parser->addPositionalArgument("args", i18nc("@info:shell", "Arguments for the operation"), i18n("args..."));
}

bool AttributesCommand::parseValue(const QString &arg, bool isHex)
{
    mCommandValue.clear(); // start off with it clean
    if (arg.isEmpty()) { // simple check for empty value
        return (true); // result is just empty also
    }

    if (isHex) { // interpret as hex string
        QByteArray hexString = arg.toLatin1().simplified();
        hexString.replace(' ', ""); // simplify and remove all spaces

        const int len = hexString.length();
        if ((len & 1) != 0) { // length must be even
            Q_EMIT error(i18nc("@info:shell", "Hex encoded string expected, but length %1 is not even", len));
            return (false);
        }

        QByteArray hexDecoded = QByteArray::fromHex(hexString);
        if (hexDecoded.length() < (len / 2) || // must have skipped something
            hexString.compare(hexDecoded.toHex(), Qt::CaseInsensitive) != 0) {
            Q_EMIT error(i18nc("@info:shell", "Hex encoded value contained invalid characters"));
            return (false);
        }

        mCommandValue = hexDecoded;
        return (true);
    }

    mCommandValue = arg.toLatin1();
    return (true);
}

int AttributesCommand::initCommand(QCommandLineParser *parser)
{
    QStringList args = parser->positionalArguments();

    if (!getCommonOptions(parser))
        return InvalidUsage;

    mOperationMode = ModeShow;
    mHexOption = parser->isSet("hex");

    int modeCount = 0;
    if (parser->isSet("show"))
        ++modeCount;
    if (parser->isSet("add"))
        ++modeCount;
    if (parser->isSet("delete"))
        ++modeCount;
    if (parser->isSet("modify"))
        ++modeCount;

    if (modeCount > 1) {
        emitErrorSeeHelp(i18nc("@info:shell", "Only one of the 'show', 'add', 'modify' or 'delete' options may be specified"));
        return (InvalidUsage);
    }

    if (!checkArgCount(args, 1, i18nc("@info:shell", "No collection specified")))
        return InvalidUsage;

    const QString collectionArg = args.takeFirst();
    if (!getResolveJob(collectionArg))
        return (InvalidUsage);

    if (parser->isSet("add")) {
        if (!checkArgCount(args, 2, i18nc("@info:shell", "No attribute name/value specified to add")))
            return (InvalidUsage);

        mOperationMode = ModeAdd;
        mCommandType = args.takeFirst().toLatin1();
        if (!parseValue(args.takeFirst(), mHexOption))
            return (InvalidUsage);
    }
    if (parser->isSet("delete")) {
        if (!checkArgCount(args, 1, i18nc("@info:shell", "No attribute name specified to delete")))
            return (InvalidUsage);

        mOperationMode = ModeDelete;
        mCommandType = args.takeFirst().toLatin1();
    }
    if (parser->isSet("modify")) {
        if (!checkArgCount(args, 2, i18nc("@info:shell", "No attribute name/value specified to modify")))
            return (InvalidUsage);

        mOperationMode = ModeModify;
        mCommandType = args.takeFirst().toLatin1();
        if (!parseValue(args.takeFirst(), mHexOption))
            return (InvalidUsage);
    }

    if (!args.isEmpty()) { // should be by now
        emitErrorSeeHelp(i18nc("@info:shell", "Too many arguments specified, from '%1'", args.first()));
        return (InvalidUsage);
    }

    return (NoError);
}

void AttributesCommand::start()
{
    connect(resolveJob(), &KJob::result, this, &AttributesCommand::onCollectionResolved);
    resolveJob()->start();
}

void AttributesCommand::onCollectionResolved(KJob *job)
{
    if (!checkJobResult(job))
        return;
    CollectionResolveJob *res = resolveJob();
    Q_ASSERT(job == res);
    mAttributesCollection = new Collection(res->collection());

    const Attribute::List &attrs = mAttributesCollection->attributes();

    if (mOperationMode == ModeShow) {
        CollectionPathJob *pathJob = new CollectionPathJob(*mAttributesCollection);
        connect(pathJob, &KJob::result, this, &AttributesCommand::onPathFetched);
        pathJob->start();
        return;
    }

    if (!allowDangerousOperation()) {
        Q_EMIT finished(RuntimeError);
        return;
    }

    bool attributeExists = false;
    for (const Attribute *attr : std::as_const(attrs)) {
        if (attr->type() == mCommandType) {
            attributeExists = true;
            break;
        }
    }

    if (mOperationMode == ModeAdd) { // add, must not already exist
        if (attributeExists) {
            Q_EMIT error(i18nc("@info:shell", "Collection already has an attribute '%1'", mCommandType));
            Q_EMIT finished(RuntimeError);
            return;
        }

        SyntheticAttribute *newAttr = new SyntheticAttribute(mCommandType, mCommandValue);
        SyntheticAttribute *collectionAttr = mAttributesCollection->attribute<SyntheticAttribute>(Collection::AddIfMissing);
        Q_ASSERT(collectionAttr != nullptr);
        *collectionAttr = *newAttr;
    } else { // delete/modify, must already exist
        if (!attributeExists) {
            Q_EMIT error(i18nc("@info:shell", "Collection has no attribute '%1'", mCommandType));
            Q_EMIT finished(RuntimeError);
            return;
        }

        if (mOperationMode == ModeDelete) { // delete, remove from colelction
            mAttributesCollection->removeAttribute(mCommandType);
        } else { // modify, set new value
            SyntheticAttribute *newAttr = new SyntheticAttribute(mCommandType, mCommandValue);
            SyntheticAttribute *collectionAttr = mAttributesCollection->attribute<SyntheticAttribute>(Collection::AddIfMissing);
            Q_ASSERT(collectionAttr != nullptr);
            *collectionAttr = *newAttr;
        }
    }

    CollectionModifyJob *modifyJob = new Akonadi::CollectionModifyJob(*mAttributesCollection);
    connect(modifyJob, &KJob::result, this, &AttributesCommand::onCollectionModified);
    modifyJob->start();
}

void AttributesCommand::onCollectionModified(KJob *job)
{
    if (!checkJobResult(job))
        return;
    Q_EMIT finished(NoError);
}

void AttributesCommand::onPathFetched(KJob *job)
{
    if (!checkJobResult(job))
        return;
    CollectionPathJob *pathJob = qobject_cast<CollectionPathJob *>(job);
    Q_ASSERT(pathJob != nullptr);
    const QString pathString = pathJob->formattedCollectionPath();

    const Attribute::List &attrs = mAttributesCollection->attributes();
    if (attrs.isEmpty()) {
        std::cout << std::endl << "Collection " << qPrintable(pathString) << " has no attributes" << std::endl;
    } else {
        int maxLength = 2;
        for (const Attribute *attr : std::as_const(attrs)) {
            maxLength = qMax(maxLength, attr->type().length());
        }

        std::cout << std::endl << "Collection " << qPrintable(pathString) << " has " << attrs.count() << " attributes:" << std::endl;
        for (const Attribute *attr : std::as_const(attrs)) {
            std::cout << "  ";
            std::cout << qPrintable(attr->type().leftJustified(maxLength));
            std::cout << "  ";

            const QByteArray value = attr->serialized();
            if (value.isEmpty()) { // attribute exists but empty value
                if (!mHexOption) {
                    std::cout << "\"\"";
                }
            } else {
                bool isPrintable = true; // assume so to start, anyway
                for (const char &ch : std::as_const(value)) {
                    if (!QChar::isPrint(ch)) {
                        isPrintable = false;
                        break;
                    }
                }

                if (isPrintable && !mHexOption) {
                    std::cout << '\"';
                    for (const char &ch : std::as_const(value)) {
                        if (ch == '\"') {
                            std::cout << "\\\"";
                        } else {
                            std::cout << ch;
                        }
                    }
                    std::cout << '\"';
                } else {
                    std::cout << qPrintable(value.toHex(' '));
                }
            }
            std::cout << std::endl;
        }
    }

    Q_EMIT finished(NoError);
}
