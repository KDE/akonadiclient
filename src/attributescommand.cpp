/*
    Copyright (C) 2024-2025  Jonathan Marten <jjm@keelhaul.me.uk>

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

#include <qfile.h>
#include <qregularexpression.h>
#include <qsavefile.h>
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
// to arbitrary values, as specified for an "add" or "modify" command.
// This is needed because it is not possible with the Akonadi API to
// simply add a collection attribute with a specified name and value, it
// must be an object which is a subclass of Akonadi::Attribute and whose
// type corresponds to the required name.  However Akonadi does not care
// about the actual C++ class of the attribute, as long as its type()
// returns the name.

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
    : CollectionListCommand(parent)
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
    //   1.  The original operations (Show/Add/Modify/Delete) don't have
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

void AttributesCommand::start()
{
    if (mOperationMode == ModeRestore || mOperationMode == ModeAdd || mOperationMode == ModeModify || mOperationMode == ModeDelete) {
        if (!allowDangerousOperation()) {
            Q_EMIT finished(RuntimeError);
            return;
        }
    }

    if (mOperationMode == ModeBackup) {
        // In this mode, just list and then save the current collections.
        listCollections(CollectionFetchJob::Recursive);
    } else if (mOperationMode == ModeCheck || mOperationMode == ModeRestore) {
        // In these modes, first read the original list of collection
        // attributes that was saved by a previous backup operation.
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
        processChanges(); // scan for and report/fix changes
    } else {
        // Operating on one collection, so first resolve the collection
        // argument specified.
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
    Collection *coll = new Collection(res->collection());
    const Attribute::List &attrs = coll->attributes();

    if (mOperationMode == ModeShow) {
        CollectionPathJob *pathJob = new CollectionPathJob(*coll);
        connect(pathJob, &KJob::result, this, &AttributesCommand::onPathFetched);
        pathJob->start();
        return;
    }

    const bool attributeExists = (coll->attribute(mCommandType)!=nullptr);
    if (mOperationMode == ModeAdd) { // add, must not already exist
        if (attributeExists) {
            Q_EMIT error(i18nc("@info:shell", "Collection %2 already has an attribute '%1'", mCommandType, coll->id()));
            Q_EMIT finished(RuntimeError);
            return;
        }
    } else { // delete/modify, must already exist
        if (!attributeExists) {
            Q_EMIT error(i18nc("@info:shell", "Collection %2 has no attribute '%1'", mCommandType, coll->id()));
            Q_EMIT finished(RuntimeError);
            return;
        }
    }

    if (mOperationMode == ModeDelete) { // delete, remove from collection
        coll->removeAttribute(mCommandType);
    } else { // add/modify, set new value
        SyntheticAttribute *newAttr = new SyntheticAttribute(mCommandType, mCommandValue);
        coll->addAttribute(newAttr);
    }

    CollectionModifyJob *modifyJob = new CollectionModifyJob(*coll);
    connect(modifyJob, &KJob::result, this, &AttributesCommand::onCollectionModified);
    modifyJob->start();
}

void AttributesCommand::onCollectionModified(KJob *job)
{
    if (!checkJobResult(job))
        return;
    Q_EMIT finished(NoError);
}

static bool allPrintable(const QByteArray &str)
{
    for (const char &ch : std::as_const(str)) {
        if (!QChar::isPrint(ch))
            return (false);
    }
    return (true);
}

static QByteArray printableForMessage(const QByteArray &str)
{
    if (str.isEmpty())
        return ("(null)");
    else if (allPrintable(str)) {
        QByteArray ret = str;
        ret.replace('"', "\\\"");
        return ("\"" + str + "\"");
    } else
        return (str.toHex('\0'));
}

static QByteArray printableForDisplay(const QByteArray &str, bool forceHex)
{
    if (str.isEmpty())
        return (forceHex ? "" : "\"\"");
    else if (!forceHex && allPrintable(str)) {
        QByteArray ret = str;
        return ("\"" + ret.replace('"', "\\\"") + "\"");
    } else
        return (str.toHex(' '));
}

void AttributesCommand::onPathFetched(KJob *job)
{
    if (!checkJobResult(job))
        return;
    CollectionPathJob *pathJob = qobject_cast<CollectionPathJob *>(job);
    Q_ASSERT(pathJob != nullptr);
    const QString pathString = pathJob->formattedCollectionPath();
    const Collection &coll = pathJob->collection();

    std::cout << std::endl;
    const Attribute::List &attrs = coll.attributes();
    if (attrs.isEmpty()) {
        std::cout << qPrintable(xi18nc("@info:shell", "Collection %1 has no attributes", pathString));
        std::cout << std::endl;
    } else {
        int maxLength = 2;
        for (const Attribute *attr : std::as_const(attrs)) {
            maxLength = qMax(maxLength, attr->type().length());
        }

        std::cout << qPrintable(xi18ncp("@info:shell", "Collection %2 has %1 attribute:", "Collection %2 has %1 attributes:", attrs.count(), pathString));
        std::cout << std::endl;
        for (const Attribute *attr : std::as_const(attrs)) {
            std::cout << "  ";
            std::cout << qPrintable(attr->type().leftJustified(maxLength));
            std::cout << "  ";
            std::cout << qPrintable(printableForDisplay(attr->serialized(), mHexOption));
            std::cout << std::endl;
        }
    }

    Q_EMIT finished(NoError);
}

void AttributesCommand::onCollectionsListed()
{
    ErrorReporter::progress(i18nc("@info:shell", "Found %1 current Akonadi collections", mCollections.count()));

    getCurrentPaths(); // populates mCurPathMap

    // Save the current list of folders to the "saved"
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
}

void AttributesCommand::saveCollectionAttributes(QFileDevice *file)
{
    ErrorReporter::progress(i18nc("@info:shell", "Saving collection attributes to '%1'", file->fileName()));

    // We already have all of the collections and their attributes
    // available in 'mCollections'.  So there is no need to run any
    // more asynchronous jobs for this operation, it can just be a
    // simple loop.

    int saveCount = 0;
    for (const Collection &collection : std::as_const(mCollections)) {
        const Attribute::List &attrs = collection.attributes();

        if (attrs.isEmpty()) { // only save for folders with attributes
            continue;
        }

        // Creating a new QTextStream each time works, and simply writes
        // more data to the QSaveFile.  Doing it this way to avoid needing
        // yet another member variable.
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

void AttributesCommand::processChanges()
{
    Q_ASSERT(mOperationMode == ModeCheck || mOperationMode == ModeRestore);

    // Do the loop over the original (backed up) collections, so that if any
    // attributes are present but the folder they applied to has disappeared
    // then the user can be warned.
    //
    // The full list of collections which were present in Akonadi when the
    // backup was saved is not needed, only those which had attributes which
    // may be a far smaller number.  The values of 'mOrigPathMap' are the
    // folder paths for those, which are assumed to be still present in the
    // current database under the same paths.  They can therefore be resolved
    // to the appropriate current collection even if the collection ID has
    // changed from when they were backed up.
    initProcessLoop(mOrigPathMap.values());
    mUpdatedCollectionCount = 0;

    mOrigAttrKeys = mOrigAttrMap.keys();
    qDebug() << mOrigPathMap.count() << "collections with attributes," << mOrigAttrKeys.count() << "saved attributes";

    startProcessLoop("processCollection");
}

void AttributesCommand::processCollection()
{
    const QString collPath = currentArg();
    qDebug() << collPath;

    // Resolve the path into a current collection ID.  The path as saved
    // in the backup file is always absolute.
    CollectionPathResolver *resolveJob = new CollectionPathResolver(collPath, this);
    connect(resolveJob, &KJob::result, this, &AttributesCommand::onPathResolved);
    resolveJob->start();
}

void AttributesCommand::onPathResolved(KJob *job)
{
    CollectionPathResolver *resolveJob = qobject_cast<CollectionPathResolver *>(job);
    Q_ASSERT(resolveJob != nullptr);

    const Collection::Id collId = resolveJob->collection();
    // qDebug() << "collection ID" << collId;
    if (job->error() || collId == -1) {
        ErrorReporter::warning(xi18nc("@info:shell", "Folder '%1' had attributes but is no longer present", resolveJob->path()));
        processNextChange();
        return;
    }

    // Fetch the collection together with its current attributes.
    CollectionFetchJob *fetchJob = new CollectionFetchJob((QList<Collection::Id>() << collId), CollectionFetchJob::Base, this);
    fetchJob->setProperty("path", resolveJob->path());
    fetchJob->setProperty("id", collId);
    connect(fetchJob, &KJob::result, this, &AttributesCommand::onCollectionFetched);
    fetchJob->start();
}

void AttributesCommand::onCollectionFetched(KJob *job)
{
    CollectionFetchJob *fetchJob = qobject_cast<CollectionFetchJob *>(job);
    Q_ASSERT(fetchJob != nullptr);
    Collection::List colls = fetchJob->collections();

    const QString collPath = '/' + fetchJob->property("path").toString();
    const Collection::Id collId = fetchJob->property("id").value<Collection::Id>();

    if (!checkJobResult(job) || colls.isEmpty()) {
        Q_EMIT error(xi18nc("@info:shell", "Cannot fetch collection %1 \"%2\"", collId, collPath));
        processNextChange();
        return;
    }

    const Collection::Id origCollId = mOrigPathMap.key(collPath);
    if (origCollId != collId) {
        ErrorReporter::progress(
            xi18nc("@info:shell", "Collection \"%2\" changed ID from %1 to %3", QString::number(origCollId), collPath, QString::number(collId)));
    }

    // Get the list of attributes that the old collection had.
    QMap<QByteArray, QByteArray> origAttrs;
    bool isSpecialCollection = false;

    for (const QPair<Collection::Id, QByteArray> &attrKey : std::as_const(mOrigAttrKeys)) {
        if (attrKey.first != origCollId)
            continue;
        const QByteArray &attrName = attrKey.second;
        const QByteArray &attrValue = mOrigAttrMap[attrKey];

        // This attribute value is only maintained and used internally
        // by Akonadi.  It is not a user setting, so it is ignored and
        // never restored.
        if (attrName == "AccessRights")
            continue;

        // qDebug() << "    " << attrName << "=" << attrValue;

        // This attribute value is set by Akonadi when the special collections
        // are created, and never changed.  Therefore it is not restored here,
        // but it is taken into account to see whether certain other values
        // should be restored.
        if (attrName == "SpecialCollectionAttribute") {
            isSpecialCollection = true;
            continue;
        }

        // For any other attribute save the name and value, for now anyway.
        origAttrs[attrName] = attrValue;
    }

    // qDebug() << "  " << origAttrs.count() << "attrs saved";

    // If this is a special collection, then do not restore the ENTITYDISPLAY
    // attribute which is again set when they are created.  There is no GUI
    // for the user to change the icon for special collections, so it is safe
    // to assume that the default values are correct.
    if (isSpecialCollection && origAttrs.contains("ENTITYDISPLAY")) {
        origAttrs.remove("ENTITYDISPLAY");
    }

    // qDebug() << "  " << origAttrs.count() << "attrs to be restored";

    if (origAttrs.isEmpty()) { // nothing to restore
        processNextChange();
        return;
    }

    // Get the current collection, with its attributes.
    Collection &currColl = colls.first();
    const Attribute::List &currAttrs = currColl.attributes();
    // qDebug() << "  currently has" << currAttrs.count() << "attrs";

    int updateCount = 0; // how many updates are needed

    const QList<QByteArray> origAttrNames = origAttrs.keys();
    for (const QByteArray &origAttrName : std::as_const(origAttrNames)) {
        // qDebug() << "   trying" << origAttrName;

        bool needsUpdate = true; // assume so for now

        const Attribute *currAttr = currColl.attribute(origAttrName);
        if (currAttr != nullptr) // see if attribute present
        { // and if so, check its value
            // qDebug() << "    found with value" << currAttr->serialized();
            if (currAttr->serialized() == origAttrs[origAttrName]) {
                // qDebug() << "    same value, no change";
                needsUpdate = false; // not this attribute, anyway
            } else {
                ErrorReporter::progress(xi18nc("@info:shell",
                                               "Folder  \"%1\" attribute \"%2\" changed from %3 to %4",
                                               currColl.displayName(),
                                               origAttrName,
                                               printableForMessage(currAttr->serialized()),
                                               printableForMessage(origAttrs[origAttrName])));
            }
        } else {
            // qDebug() << "    not found";
            ErrorReporter::progress(xi18nc("@info:shell",
                                           "Folder \"%1\" attribute \"%2\" added %3",
                                           currColl.displayName(),
                                           origAttrName,
                                           printableForMessage(origAttrs[origAttrName])));
        }

        // qDebug() << "  needs update?" << needsUpdate;

        if (needsUpdate) {
            // qDebug() << "  set new attr" << origAttrName;
            SyntheticAttribute *newAttr = new SyntheticAttribute(origAttrName, origAttrs[origAttrName]);
            currColl.addAttribute(newAttr);
            ++updateCount;
        }

    } // end loop over attrs

    // qDebug() << "  update count" << updateCount;
    if (updateCount > 0) // need to update collection
    {
        ++mUpdatedCollectionCount;
        if (mOperationMode == ModeCheck) { // or maybe just report
            ErrorReporter::progress(xi18ncp("@info:shell",
                                            "Folder \"%2\" needs update of %1 attribute",
                                            "Folder \"%2\" needs update of %1 attributes",
                                            updateCount,
                                            currColl.displayName()));
        } else {
            ErrorReporter::progress(
                xi18ncp("@info:shell", "Folder \"%2\" updating %1 attribute", "Folder \"%2\" updating %1 attributes", updateCount, currColl.displayName()));

            qDebug() << "starting CollectionModifyJob for" << currColl.id();
            CollectionModifyJob *modifyJob = new CollectionModifyJob(currColl);
            connect(modifyJob, &KJob::result, this, &AttributesCommand::onAttributesModified);
            modifyJob->start();
            return;
        }
    }

    processNextChange();
}

void AttributesCommand::onAttributesModified(KJob *job)
{
    if (!checkJobResult(job))
        return;
    processNextChange();
}

void AttributesCommand::processNextChange()
{
    if (isProcessLoopFinished()) { // that was the last collection
        if (mUpdatedCollectionCount == 0) {
            if (mOperationMode == ModeCheck) {
                ErrorReporter::progress(xi18nc("@info:shell", "No collection attributes need to be restored"));
            } else {
                ErrorReporter::progress(xi18nc("@info:shell", "No collection attributes needed to be restored"));
            }
        } else {
            if (mOperationMode == ModeCheck) {
                ErrorReporter::progress(xi18ncp("@info:shell",
                                                "Attributes need to be restored for %1 collection",
                                                "Attributes need to be restored for %1 collections",
                                                mUpdatedCollectionCount));

                // A restore is needed, so tell the user what to do next.
                std::cerr << std::endl
                          << qPrintable(xi18nc("@info:shell",
                                               "Attribute changes are required, execute the command:<nl/>"
                                               "<bcode>"
                                               "  %1 %2 --restore"
                                               "</bcode>"
                                               "<nl/>"
                                               "to implement the changes.",
                                               QCoreApplication::applicationName(),
                                               name()))
                          << std::endl;
            } else {
                ErrorReporter::progress(xi18ncp("@info:shell",
                                                "Attributes were restored for %1 collection",
                                                "Attributes were restored for %1 collections",
                                                mUpdatedCollectionCount));
            }
        }
    }

    processNext(); // next collection or finish
}
