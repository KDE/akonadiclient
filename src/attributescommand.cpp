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

#include <qdir.h>
#include <qfile.h>
#include <qfileinfo.h>
#include <qregularexpression.h>
#include <qsavefile.h>
#include <qstandardpaths.h>
#include <qtextstream.h>

#include <Akonadi/CollectionFetchJob>
#include <Akonadi/CollectionModifyJob>

#include <iostream>

#include "collectionpathjob.h"
#include "collectionresolvejob.h"
#include "commandfactory.h"
#include "errorreporter.h"

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
    parser->addOption(QCommandLineOption((QStringList() << "b"
                                                        << "backup"),
                                         i18n("Save current folder attributes")));
    parser->addOption(QCommandLineOption((QStringList() << "c"
                                                        << "check"),
                                         i18n("Check current against saved folder attributes")));
    parser->addOption(QCommandLineOption((QStringList() << "r"
                                                        << "restore"),
                                         i18n("Restore changed folder attributes")));

    // This command does not provide a "dry run" mode, even for the Backup,
    // Check or Restore operations, for two reasons:
    //
    //   1.  The original operations (Show/Add/Modify/Delete) didn't have
    //       a dry run mode either.
    //
    //   2.  A dry run for Backup doesn't make sense, and Check is just
    //       like a dry run for Restore.

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

AbstractCommand::Error AttributesCommand::initCommand(QCommandLineParser *parser)
{
    QStringList args = parser->positionalArguments();

    if (!getCommonOptions(parser))
        return InvalidUsage;

    mOperationMode = ModeShow;
    mHexOption = parser->isSet("hex");

    int modeCount = 0;
    bool needCollection = true;
    if (parser->isSet("show")) {
        ++modeCount;
        mOperationMode = ModeShow;
    }
    if (parser->isSet("add")) {
        ++modeCount;
        mOperationMode = ModeAdd;
    }
    if (parser->isSet("delete")) {
        ++modeCount;
        mOperationMode = ModeDelete;
    }
    if (parser->isSet("modify")) {
        ++modeCount;
        mOperationMode = ModeModify;
    }
    if (parser->isSet("backup")) {
        ++modeCount;
        mOperationMode = ModeBackup;
        needCollection = false;
    }
    if (parser->isSet("check")) {
        ++modeCount;
        mOperationMode = ModeCheck;
        needCollection = false;
    }
    if (parser->isSet("restore")) {
        ++modeCount;
        mOperationMode = ModeRestore;
        needCollection = false;
    }

    if (modeCount > 1) {
        emitErrorSeeHelp(i18nc("@info:shell", "Only one operation mode option may be specified"));
        return (InvalidUsage);
    }

    if (needCollection) {
        if (!checkArgCount(args, 1, i18nc("@info:shell", "No collection specified")))
            return InvalidUsage;

        const QString collectionArg = args.takeFirst();
        if (!getResolveJob(collectionArg))
            return (InvalidUsage);
    }

    if (mOperationMode == ModeAdd) {
        if (!checkArgCount(args, 2, i18nc("@info:shell", "No attribute name/value specified to add")))
            return (InvalidUsage);

        mCommandType = args.takeFirst().toLatin1();
        if (!parseValue(args.takeFirst(), mHexOption))
            return (InvalidUsage);
    }
    if (mOperationMode == ModeDelete) {
        if (!checkArgCount(args, 1, i18nc("@info:shell", "No attribute name specified to delete")))
            return (InvalidUsage);

        mCommandType = args.takeFirst().toLatin1();
    }
    if (mOperationMode == ModeModify) {
        if (!checkArgCount(args, 2, i18nc("@info:shell", "No attribute name/value specified to modify")))
            return (InvalidUsage);

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

// TODO: common with FoldersCommand
// Find the full path for a save file, optionally creating the parent
// directory if required.
QString AttributesCommand::findSaveFile(const QString &name, bool createDir)
{
    const QString saveDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + '/';
    QFileInfo info(saveDir);
    if (!info.isDir()) {
        if (info.exists()) {
            Q_EMIT error(i18nc("@info:shell", "Save location '%1' exists but is not a directory", info.absoluteFilePath()));
            Q_EMIT finished(RuntimeError);
            return (QString());
        }

        if (createDir) {
            QDir d(info.dir());
            if (!d.mkpath(saveDir)) {
                Q_EMIT error(i18nc("@info:shell", "Cannot create save directory '%1'", info.absoluteFilePath()));
                Q_EMIT finished(RuntimeError);
                return (QString());
            }
        }
    }

    info.setFile(info.dir(), name);
    qDebug() << info.absoluteFilePath();
    return (info.absoluteFilePath());
}

void AttributesCommand::start()
{
    if (mOperationMode == ModeBackup || mOperationMode == ModeCheck || mOperationMode == ModeRestore) {
        if (mOperationMode == ModeRestore) {
            if (!allowDangerousOperation()) {
                Q_EMIT finished(RuntimeError);
                return;
            }
        }

        if (mOperationMode == ModeCheck || mOperationMode == ModeRestore) {
            // Check or restore mode.  First read the original list of
            // folder attributes that was saved by a previous backup operation.
            const QString readFileName = findSaveFile("savedattributes.dat", false);
            qDebug() << "read from" << readFileName;
            QFile readFile(readFileName, this);
            if (!readFile.open(QIODevice::ReadOnly)) {
                Q_EMIT error(i18nc("@info:shell", "Cannot read saved attributes from '%1'", readFile.fileName()));
                Q_EMIT error(i18nc("@info:shell", "Run '%1 %2 --backup' first", QCoreApplication::applicationName(), name()));
                Q_EMIT finished(RuntimeError);
                return;
            }

            readSavedAttributes(&readFile); // populates mOrigPathMap and mOrigAttrMap
        }

        // In any of those modes, read the current collections.
        fetchCollections();
    } else {
        // Resolve the collection argument specified.
        connect(resolveJob(), &KJob::result, this, &AttributesCommand::onCollectionResolved);
        resolveJob()->start();
    }
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

        if (mOperationMode == ModeDelete) { // delete, remove from collection
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

void AttributesCommand::fetchCollections()
{
    CollectionFetchJob *job = new CollectionFetchJob(Collection::root(), CollectionFetchJob::Recursive, this);
    connect(job, &KJob::result, this, &AttributesCommand::onCollectionsFetched);
}

void AttributesCommand::onCollectionsFetched(KJob *job)
{
    Q_ASSERT(mOperationMode != ModeRestore);

    if (!checkJobResult(job))
        return;
    CollectionFetchJob *fetchJob = qobject_cast<CollectionFetchJob *>(job);
    Q_ASSERT(fetchJob != nullptr);

    mCollections = fetchJob->collections();
    if (mCollections.isEmpty()) {
        Q_EMIT error(i18nc("@info:shell", "Cannot list any collections"));
        Q_EMIT finished(RuntimeError);
        return;
    }

    ErrorReporter::progress(i18nc("@info:shell", "Found %1 current Akonadi collections", mCollections.count()));

    getCurrentPaths(mCollections); // populates mCurPathMap

    if (mOperationMode == ModeBackup) {
        // Backup mode.  Save the current list of folders to the "saved"
        // data file.  After that there is no more to do.
        const QString saveFileName = findSaveFile("savedattributes.dat", true);
        qDebug() << "backup to" << saveFileName;
        QSaveFile saveFile(saveFileName);
        if (!saveFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            Q_EMIT error(i18nc("@info:shell", "Cannot save backup list to '%1'", saveFile.fileName()));
            Q_EMIT finished(RuntimeError);
            return;
        }

        saveCollectionAttributes(&saveFile);
        saveFile.commit(); // finished with save file
        return;
    }

    // Check or restore mode, check and set the attributes.
    // processChanges();
}

// TODO: common with FoldersCommand
void AttributesCommand::getCurrentPaths(const Collection::List &colls)
{
    QMap<Collection::Id, Collection> curCollMap;
    for (const Collection &coll : std::as_const(colls)) {
        curCollMap[coll.id()] = coll;
    }

    for (const Collection &coll : std::as_const(colls)) {
        QStringList path(coll.displayName());
        Collection::Id parentId = coll.parentCollection().id();
        while (parentId != 0) {
            const Collection &parentColl = curCollMap[parentId];
            path.prepend(parentColl.displayName());
            parentId = parentColl.parentCollection().id();
        }

        path.prepend(""); // to get root at beginning
        const QString p = path.join('/');
        mCurPathMap[coll.id()] = p;
    }
}

void AttributesCommand::saveCollectionAttributes(QFileDevice *file)
{
    ErrorReporter::progress(i18nc("@info:shell", "Saving collection attributes to '%1'", file->fileName()));

    // We already have all of the collections and their attributes
    // available in 'mCollections'.  There is no need to run any more
    // asynchronous jobs for this operation, so it can just be a
    // simple loop.

    int saveCount = 0;
    for (const Collection &collection : std::as_const(mCollections)) {
        const Attribute::List &attrs = collection.attributes();

        if (attrs.isEmpty()) { // only save for folders with attributes
            continue;
        }

        // Creating a new QTextStream each time simply writes more data
        // to the QSaveFile.  Doing it this way to avoid yet needing yet
        // another member variable.
        QTextStream ts(file);

        ts << Qt::left << qSetFieldWidth(8) << QString::number(collection.id()) << qSetFieldWidth(0) << "  " << qSetFieldWidth(30) << "=" << qSetFieldWidth(0)
           << "  " << QUrl::toPercentEncoding(mCurPathMap.value(collection.id()), "/") << Qt::endl;

        for (const Attribute *attr : std::as_const(attrs)) {
            ts << Qt::left << qSetFieldWidth(8) << QString::number(collection.id()) << qSetFieldWidth(0) << "  " << qSetFieldWidth(30) << attr->type()
               << qSetFieldWidth(0) << "  " << attr->serialized().toPercentEncoding("()") << Qt::endl;
        }

        ts << Qt::endl;
        ++saveCount;
    }

    ErrorReporter::progress(i18nc("@info:shell", "Saved attributes for %1 collections", saveCount));
    Q_EMIT finished(NoError);
}

void AttributesCommand::readSavedAttributes(QFileDevice *file)
{
    ErrorReporter::progress(i18nc("@info:shell", "Reading saved attributes from '%1'", file->fileName()));
    QTextStream ts(file);
    const QRegularExpression rx("^(\\d+)\\s+(\\S+)\\s+(.+)$");
    while (!ts.atEnd()) {
        const QString line = ts.readLine().trimmed();
        const QRegularExpressionMatch match = rx.match(line);
        if (!match.hasMatch())
            continue;

        const Collection::Id id = static_cast<Collection::Id>(match.captured(1).toULong());
        const QByteArray attrName = match.captured(2).toLatin1();
        const QByteArray attrValue = QByteArray::fromPercentEncoding(match.captured(3).toLatin1());

        if (attrName == '=') { // this is the folder path
            mOrigPathMap.insert(id, attrValue);
        } else { // this is an attribute name/value
            QPair<Collection::Id, QByteArray> key(id, attrName);
            mOrigAttrMap.insert(key, attrValue);
        }
    }

    ErrorReporter::progress(i18nc("@info:shell", "Read attributes for %1 collections", mOrigPathMap.count()));
}
