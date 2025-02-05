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

#include "folderscommand.h"

#include <qdir.h>
#include <qfileinfo.h>
#include <qsavefile.h>
#include <qstandardpaths.h>
#include <qtimer.h>
#include <qurl.h>

#include <kconfig.h>
#include <kconfiggroup.h>

#include <Akonadi/CollectionFetchJob>
#include <Akonadi/ServerManager>

#include <iostream>

#include "commandfactory.h"
#include "errorreporter.h"

using namespace Qt::Literals::StringLiterals;
DEFINE_COMMAND("folders", FoldersCommand, kli18nc("info:shell", "Save, check or restore Akonadi folder IDs"));

FoldersCommand::FoldersCommand(QObject *parent)
    : AbstractCommand(parent)
{
}

void FoldersCommand::setupCommandOptions(QCommandLineParser *parser)
{
    addOptionsOption(parser);
    parser->addOption(QCommandLineOption((QStringList() << "b"
                                                        << "backup"),
                                         i18n("Save current folder ID assignments")));
    parser->addOption(QCommandLineOption((QStringList() << "c"
                                                        << "check"),
                                         i18n("Check current against saved folder ID assignments")));
    parser->addOption(QCommandLineOption((QStringList() << "r"
                                                        << "restore"),
                                         i18n("Restore configuration files with changed folder ID assignments")));
    addDryRunOption(parser);
}

AbstractCommand::Error FoldersCommand::initCommand(QCommandLineParser *parser)
{
    if (!getCommonOptions(parser))
        return InvalidUsage;

    int modeCount = 0;
    if (parser->isSet("backup"_L1)) {
        ++modeCount;
        mOperationMode = ModeBackup;
    }
    if (parser->isSet("check"_L1)) {
        ++modeCount;
        mOperationMode = ModeCheck;
    }
    if (parser->isSet("restore"_L1)) {
        ++modeCount;
        mOperationMode = ModeRestore;
    }
    if (modeCount > 1) {
        emitErrorSeeHelp(i18nc("@info:shell", "Only one of the 'backup', 'check' or 'restore' options may be specified"));
        return (InvalidUsage);
    }
    if (modeCount == 0) {
        emitErrorSeeHelp(i18nc("@info:shell", "One of the 'backup', 'check' or 'restore' options must be specified"));
        return (InvalidUsage);
    }

    return (NoError);
}

// Find the full path for a save file, optionally creating the parent
// directory if required.
QString FoldersCommand::findSaveFile(const QString &name, bool createDir)
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

void FoldersCommand::start()
{
    populateChangeData(); // define change data

    if (mOperationMode == ModeRestore) {
        if (!isDryRun()) { // allow if not doing anything
            if (!allowDangerousOperation()) {
                Q_EMIT finished(RuntimeError);
                return;
            }
        }

        // For this operation there is no need to fetch the current
        // collections from the Akonadi server, and it should not
        // even be possible because the user has been advised to shut
        // down the server before running in restore mode.  So bypass
        // this step and go straight away to restore the saved files.
        processRestore();
        return;
    }

    if (mOperationMode == ModeCheck) {
        // Check mode.  First read the original list of folders that
        // was saved by a previous backup operation.
        const QString readFileName = findSaveFile("savedfolders.dat", false);
        qDebug() << "check from" << readFileName;
        QFile readFile(readFileName, this);
        if (!readFile.open(QIODevice::ReadOnly)) {
            Q_EMIT error(i18nc("@info:shell", "Cannot read saved list from '%1'", readFile.fileName()));
            Q_EMIT error(i18nc("@info:shell", "Run '%1 %2 --backup' first", QCoreApplication::applicationName(), name()));
            Q_EMIT finished(RuntimeError);
            return;
        }

        readSavedPaths(&readFile, &mOrigPathMap); // populates mOrigPathMap
    }

    // Backup or check mode, so read the current collections
    fetchCollections();
}

void FoldersCommand::fetchCollections()
{
    CollectionFetchJob *job = new CollectionFetchJob(Collection::root(), CollectionFetchJob::Recursive, this);
    connect(job, &KJob::result, this, &FoldersCommand::onCollectionsFetched);
}

void FoldersCommand::onCollectionsFetched(KJob *job)
{
    Q_ASSERT(mOperationMode != ModeRestore);

    if (!checkJobResult(job))
        return;
    CollectionFetchJob *fetchJob = qobject_cast<CollectionFetchJob *>(job);
    Q_ASSERT(fetchJob != nullptr);

    const Collection::List colls = fetchJob->collections();
    if (colls.count() < 1) {
        Q_EMIT error(i18nc("@info:shell", "Cannot list any collections"));
        Q_EMIT finished(RuntimeError);
        return;
    }

    ErrorReporter::progress(i18nc("@info:shell", "Found %1 current Akonadi collections", colls.count()));

    getCurrentPaths(colls); // populates mCurPathMap
    processChanges(); // do the processing
}

bool FoldersCommand::readOrSaveLists()
{
    if (mOperationMode == ModeBackup) // creating backup, create save file
    {
        // Backup mode, so save the current list of folders to the "saved"
        // data file.  After that there is no more to do.
        const QString saveFileName = findSaveFile("savedfolders.dat", true);
        qDebug() << "backup to" << saveFileName;
        QSaveFile saveFile(saveFileName, this);
        if (!saveFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            Q_EMIT error(i18nc("@info:shell", "Cannot save backup list to '%1'", saveFile.fileName()));
            Q_EMIT finished(RuntimeError);
            return (false);
        }

        saveCurrentPaths(&saveFile); // saves mCurPathMap
        Q_EMIT finished(NoError); // no more to do
        return (false);
    }

    // Save the current list of folders that was obtained from the
    // Akonadi server to a "current" data file.  This will later be
    // used by a restore operation, because by then the list will not
    // be available from the server.
    const QString currFileName = findSaveFile("currentfolders.dat", true);
    qDebug() << "current to" << currFileName;
    QSaveFile currFile(currFileName, this);
    if (!currFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        Q_EMIT error(i18nc("@info:shell", "Cannot write current list to '%1'", currFile.fileName()));
        Q_EMIT finished(RuntimeError);
        return (false);
    }

    saveCurrentPaths(&currFile); // saves mCurPathMap
    return (true); // continue with checks
}

// Build a new value.  The match regular expression is not in the format
// that would be required to use QString::replace(QRegularExpression), so build
// up the replacement manually.
static QString updateValue(const QRegularExpressionMatch &match, const QString &oldValue, Collection::Id id)
{
    int idx = match.capturedStart(1); // index of start of match
    QString newValue = oldValue.left(idx); // up to start of match
    newValue += QString::number(id); // the updated folder ID
    idx += match.capturedLength(1); // past the old folder ID
    newValue += oldValue.mid(idx); // from end of match onwards
    return (newValue);
}

QList<ChangeData> FoldersCommand::findChangesFor(const QString &file)
{
    QList<ChangeData> result;

    const QStringList configNames = mChangeData.uniqueKeys();
    for (const QString &configName : std::as_const(configNames)) {
        const QRegularExpression rx = QRegularExpression::fromWildcard(configName, Qt::CaseSensitive);
        if (file.contains(rx)) {
            result = mChangeData.values(configName);
            break;
        }
    }

    return (result);
}

static QString newConfigFileName(const QString &name)
{
    return (name + "_" + QCoreApplication::applicationName() + "_new");
}

static void sleepFor(int ms)
{
    QEventLoop eventLoop;

    QTimer timer;
    timer.setInterval(ms);
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, &eventLoop, &QEventLoop::quit);
    timer.start();
    eventLoop.exec();
}

void FoldersCommand::processChanges()
{
    // First of all read or save the folder lists, as appropriate for the
    // operation mode.
    if (!readOrSaveLists())
        return;

    // Here the operation mode is either Check or Restore, and the
    // lists have been read correctly.  Based on those current and original
    // path mappings, either read from the Akonadi server or saved files,
    // check whether anything actually needs to be done.
    const int pathsChanged = checkForChanges();
    if (pathsChanged == 0) {
        ErrorReporter::progress(i18nc("@info:shell", "No folder IDs have changed"));
        Q_EMIT finished(NoError);
        return;
    }

    ErrorReporter::progress(i18nc("@info:shell", "%1 folder IDs have changed", pathsChanged));

    // Some folder IDs have changed, so prepare to process the application
    // configuration files.
    int numRenamed = 0; // count of groups renamed
    int numChanged = 0; // count of settings changed

    const QStringList configFiles = allConfigFiles();
    if (configFiles.isEmpty())
        return;

    // Crude, but ensures that the updated configuration files have
    // later timestamps than the saved current folders file.
    sleepFor(1000);

    for (const QString &configFile : std::as_const(configFiles)) {
        std::cerr << std::endl;
        ErrorReporter::progress(i18nc("@info:shell", "Processing config file '%1'...", configFile));

        // Get the changes to be checked and applied to this file.
        // If there are none then do nothing.
        const QList<ChangeData> changes = findChangesFor(configFile);
        // const QList<ChangeData> changes = mChangeData.values(configFile);
        if (changes.isEmpty())
            continue;

        // Get the current application configuration.  This will not
        // be changed for a check operation, but will be done at a later
        // stage for a restore.
        const KConfig curConfig(configFile, KConfig::SimpleConfig);

        // Get a new file for the updated configuration.  It may already
        // exist, so clear out all existing settings from it.
        KConfig newConfig(newConfigFileName(configFile), KConfig::SimpleConfig);
        const QStringList origGroupList = newConfig.groupList();
        if (!origGroupList.isEmpty()) {
            qDebug() << origGroupList.count() << "groups already in new config";
            for (const QString &groupName : std::as_const(origGroupList))
                newConfig.deleteGroup(groupName);
            newConfig.sync();
        }

        // Get the group list from the old configuration.  Note that it is
        // important that any KConfig on which KConfigBase::groupList() is
        // to be used must have been created with KConfig::SimpleConfig so
        // that it does not cascade with the KDE defaults and globals.
        QStringList curGroupList = curConfig.groupList();
        qDebug() << curGroupList.count() << "groups in current config";
        std::sort(curGroupList.begin(), curGroupList.end());

        // Pass 1 over the configuration:  Look at each group name and, if
        // it refers to a folder, rename it if necessary.  Either the original
        // or the renamed group is copied to the new configuration with all
        // of its entries unchanged.
        for (const QString &curGroupName : std::as_const(curGroupList)) {
            // qDebug() << "grp" << curGroupName;
            const KConfigGroup curGroup = curConfig.group(curGroupName);

            QString newGroupName = curGroupName; // unchanged so far

            // Look at each applicable change data item in turn.
            for (const ChangeData &change : std::as_const(changes)) {
                // Only process change data which refers to a group name match.
                if (change.isValueChange())
                    continue;

                // Match the group name against the pattern.
                const QRegularExpression rx = change.groupPattern();
                const QRegularExpressionMatch match = rx.match(curGroupName);
                if (!match.hasMatch())
                    continue;

                // The group name pattern matched.  Extract the folder ID
                // from it and see if it needs to be changed.
                const Collection::Id curId = match.captured(1).toULong();
                if (!mChangeMap.contains(curId))
                    continue;

                // The folder ID has changed, so rewrite the group name.
                const Collection::Id newId = mChangeMap[curId];
                // qDebug() << "  curid" << curId << "-> newid" << newId;

                if (newId == 0) {
                    // There is no mapping to a new folder, which means that
                    // the original group refers to a folder which no longer
                    // exists.  Copy it unchanged, and hope that the group name
                    // does not clash with another one.
                    //
                    // TODO: maybe just drop the group?
                    std::cerr << "Cannot rename group \"" << qPrintable(curGroupName) << "\""
                              << " for folder \"" << qPrintable(mCurPathMap[curId]) << "\""
                              << " which no longer exists" << std::endl;
                } else {
                    // Build the renamed group name.
                    newGroupName = updateValue(match, curGroupName, newId);
                    ++numRenamed;

                    std::cerr << "Group \"" << qPrintable(curGroupName) << "\""
                              << " for folder \"" << qPrintable(mOrigPathMap[curId]) << "\""
                              << " renamed to \"" << qPrintable(newGroupName) << "\"" << std::endl;
                }

                // There can only be one rename, so exit from the loop
                // over the change data now.
                break;
            }

            // Copy the original or the renamed group to the new configuration.
            // qDebug() << "-> newgrp" << newGroupName;
            KConfigGroup newGroup = newConfig.group(newGroupName);
            curGroup.copyTo(&newGroup);
        }

        ErrorReporter::progress(i18nc("@info:shell", "%1 groups were renamed", numRenamed));

        // Get the group list from the new configuration, each group in which
        // was copied or modified in the first pass above.  This will therefore
        // reflect any renames made there.
        QStringList newGroupList = newConfig.groupList();
        qDebug() << newGroupList.count() << "groups in new config";
        std::sort(newGroupList.begin(), newGroupList.end());

        // Pass 2 over the configuration:  modify settings values within groups.
        for (const QString &newGroupName : std::as_const(newGroupList)) {
            // qDebug() << "grp" << newGroupName;
            KConfigGroup newGroup = newConfig.group(newGroupName);

            // Look at each applicable change data item in turn.
            for (const ChangeData &change : std::as_const(changes)) {
                // Only process change data which refers to a key/value match.
                if (!change.isValueChange())
                    continue;

                // These regular expression patterns are constant
                // for each change.
                const QRegularExpression rx1 = change.groupPattern();
                const QRegularExpression rx2 = change.keyPattern();
                const QRegularExpression rx3 = change.valuePattern();

                // Match the group name against its pattern.  This may be
                // a plain name or a regular expression;  the name is only
                // matched and not modified.  Note that here, as with all
                // regular expressions obtained from ChangeData, the pattern
                // is anchored to the start and end of the string.
                const QRegularExpressionMatch match1 = rx1.match(newGroupName);
                if (!match1.hasMatch())
                    continue;

                // qDebug() << "grp" << newGroupName;

                // The group name matches.  Get a list of the keys
                // contained in this group.
                QStringList newKeyList = newGroup.keyList();
                std::sort(newKeyList.begin(), newKeyList.end());

                // Match each key name against the key pattern.
                for (const QString &newKey : std::as_const(newKeyList)) {
                    // Match this key name against the key pattern.
                    const QRegularExpressionMatch match2 = rx2.match(newKey);
                    if (!match2.hasMatch())
                        continue;

                    // The key name pattern matched.  Get the corresponding
                    // value or list of values as appropriate.  For simplicitly
                    // a single value is treated here as a one-item list,
                    // although it must be read as a single string value
                    // because it may contain a comma.
                    QStringList newValues;
                    const bool isList = change.isListValue();
                    if (isList)
                        newValues = newGroup.readEntry(newKey, QStringList());
                    else
                        newValues.append(newGroup.readEntry(newKey, ""));
                    // qDebug() << "newValues" << newValues;

                    QStringList updValues;
                    for (const QString &newValue : std::as_const(newValues)) {
                        QString updValue = newValue;

                        // Match the value or value item against the value
                        // pattern.  In most cases this is expected to be
                        // a folder ID, the default pattern.
                        const QRegularExpressionMatch match3 = rx3.match(newValue);
                        if (match3.hasMatch()) {
                            // The value pattern matched.  Extract the folder ID
                            // from it and see if it needs to be changed.
                            const Collection::Id newId = match3.captured(1).toULong();
                            // qDebug() << "  newId" << newId;
                            if (mChangeMap.contains(newId)) {
                                // The folder ID has changed, so rewrite the value.
                                const Collection::Id updId = mChangeMap[newId];
                                // qDebug() << "  newid" << newId << "-> updid" << updId;

                                if (updId == 0) {
                                    // There is no mapping to a new folder, which means that
                                    // the original group refers to a folder which no longer
                                    // exists.  Leave the value unchanged.
                                    std::cerr << "Cannot update \"" << qPrintable(newKey) << "\""
                                              << " in group \"" << qPrintable(newGroupName) << "\""
                                              << " for folder \"" << qPrintable(mOrigPathMap[newId]) << "\""
                                              << " which no longer exists" << std::endl;
                                } else {
                                    // Build the updated value.
                                    updValue = updateValue(match3, newValue, updId);
                                    // qDebug() << "-> updvalue" << updValue;

                                    std::cerr << "Updated \"" << qPrintable(newKey) << "\""
                                              << " in group \"" << qPrintable(newGroupName) << "\""
                                              << " for folder \"" << qPrintable(mOrigPathMap[newId]) << "\""
                                              << " to " << updId << std::endl;
                                    ++numChanged;
                                }
                            } // if folder ID changed
                        } // if value matched

                        updValues.append(updValue); // add new item to list
                    } // loop over list values

                    // qDebug() << "-> updValues" << updValues;
                    if (isList)
                        newGroup.writeEntry(newKey, updValues);
                    else
                        newGroup.writeEntry(newKey, updValues.value(0));
                } // loop over group keys
            } // loop over changes
        } // loop over groups

        ErrorReporter::progress(i18nc("@info:shell", "%1 settings values were changed", numChanged));
        newConfig.sync();
    } // loop over config files

    // Evaluate the check results,
    if (numRenamed == 0 && numChanged == 0) {
        std::cerr << std::endl << qPrintable(xi18nc("@info:shell", "No configuration changes are required")) << std::endl;

        Q_EMIT finished(NoError);
        return;
    }

    // A restore is needed, so tell the user what to do next.
    std::cerr << std::endl
              << qPrintable(xi18nc("@info:shell",
                                   "Configuration changes are required (%1 group renames and %2 settings changes).<nl/>"
                                   "Quit all running PIM applications and stop the Akonadi server, then<nl/>"
                                   "execute the command:<nl/>"
                                   "<bcode>"
                                   "  %3 %4 --restore"
                                   "</bcode>"
                                   "<nl/>"
                                   "to implement the changes.",
                                   numRenamed,
                                   numChanged,
                                   QCoreApplication::applicationName(),
                                   name()))
              << std::endl;

    Q_EMIT finished(NoError);
}

void FoldersCommand::getCurrentPaths(const Collection::List &colls)
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

void FoldersCommand::saveCurrentPaths(QSaveFile *file)
{
    ErrorReporter::progress(i18nc("@info:shell", "Saving folder paths to '%1'", file->fileName()));
    QTextStream ts(file);
    for (const Collection::Id &id : mCurPathMap.keys()) {
        ts << QString::asprintf("%-15lld  ", id) << mCurPathMap.value(id) << '\n';
    }

    file->commit();
    ErrorReporter::progress(i18nc("@info:shell", "Saved %1 collection folder paths", mCurPathMap.count()));
}

void FoldersCommand::readSavedPaths(QFileDevice *file, QMap<Collection::Id, QString> *pathMap)
{
    ErrorReporter::progress(i18nc("@info:shell", "Reading saved folder paths from '%1'", file->fileName()));
    QTextStream ts(file);
    const QRegularExpression rx("^(\\d+)\\s+(.+)$");
    while (!ts.atEnd()) {
        const QString line = ts.readLine();
        const QRegularExpressionMatch match = rx.match(line);
        if (!match.hasMatch())
            continue;
        pathMap->insert(static_cast<Collection::Id>(match.captured(1).toULong()), match.captured(2));
    }

    ErrorReporter::progress(i18nc("@info:shell", "Read %1 saved collection paths", pathMap->count()));
}

int FoldersCommand::checkForChanges()
{
    QStringList origPaths = mOrigPathMap.values();
    std::sort(origPaths.begin(), origPaths.end());
    int changed = 0;

    for (const QString &origPath : std::as_const(origPaths)) {
        const Collection::Id origId = mOrigPathMap.key(origPath, 0);
        Q_ASSERT(origId != 0);

        const QString curPath = mCurPathMap.value(origId);
        if (origPath != curPath) {
            ++changed;
            const Collection::Id curId = mCurPathMap.key(origPath, 0);
            std::cerr << "Folder \"" << qPrintable(origPath) << "\" changed ID "
                      << "from " << origId << " to " << curId << std::endl;
            mChangeMap[origId] = curId;
        }
    }

    return (changed);
}

// Set up the change data, listing all known references to folder IDs
// in Akonadi and PIM application configuration files.
void FoldersCommand::populateChangeData()
{
    // KMail configuration
    ChangeData d = ChangeData("Folder-(\\d+)");
    mChangeData.insert("kmail2rc", d);
    d = ChangeData("Search", "LastSearchCollectionId");
    mChangeData.insert("kmail2rc", d);
    d = ChangeData("Composer", "previous-fcc");
    mChangeData.insert("kmail2rc", d);

    // [FavoriteCollectionsOrder]
    // 0=c389,c407,c635,c428,c438,c404,c403,c654
    //
    // The key value is the model column, see EntityOrderProxyModel::lessThan()
    // in akonadi/src/core/models/entityorderproxymodel.cpp
    // For the KMail favourites list this is always 0.
    d = ChangeData("FavoriteCollectionsOrder", "0", "c(\\d+)");
    d.setIsListValue(true);
    mChangeData.insert("kmail2rc", d);

    // [FavoriteCollections]
    // FavoriteCollectionIds=35,266,31,32,285,389,407,635,428,438,404,654,403
    //
    // Loaded and saved in akonadi/src/core/models/favoritecollectionsmodel.cpp
    d = ChangeData("FavoriteCollections", "FavoriteCollectionIds");
    d.setIsListValue(true);
    mChangeData.insert("kmail2rc", d);

    // POP3 resource
    d = ChangeData("General", "targetCollection");
    mChangeData.insert("akonadi_pop3_resource_*rc", d);
    // IMAP resource
    d = ChangeData("cache", "TrashCollection");
    mChangeData.insert("akonadi_imap_resource_*rc", d);

    // Identities
    d = ChangeData("Identity #\\d+", "Drafts");
    mChangeData.insert("emailidentities", d);
    d = ChangeData("Identity #\\d+", "Fcc");
    mChangeData.insert("emailidentities", d);
    d = ChangeData("Identity #\\d+", "Templates");
    mChangeData.insert("emailidentities", d);

    // Filters
    //
    // In theory the "action-args-N" is only a destination folder ID if
    // the corresponding "action-name-N" is either "transfer" or "copy",
    // see mailcommon/src/filter/filteractions/filteractionmove.cpp and
    // mailcommon/src/filter/filteractions/filteractioncopy.cpp for those
    // strings.  However, any sensible status, header or email address
    // argument is unlikely to match the anchored regular expression.
    d = ChangeData("Filter #\\d+", "action-args-\\d+");
    mChangeData.insert("akonadi_mailfilter_agentrc", d);

    ErrorReporter::progress(
        i18nc("@info:shell", "Defined %1 change data patterns for %2 config file names", mChangeData.count(), mChangeData.uniqueKeys().count()));
}

void FoldersCommand::processRestore()
{
    const QStringList configFiles = allConfigFiles();
    if (configFiles.isEmpty())
        return;

    // Check that a previous 'check' operation has been done.  This means
    // that the check results "currentfolders" file must exist and be newer
    // than the backed up "savedfolders" file.
    bool ok = false;

    QFileInfo savedFileInfo(findSaveFile("savedfolders.dat", false));
    if (!savedFileInfo.exists()) {
        Q_EMIT error(xi18nc("@info:shell", "Saved folder list '%1' does not exist", savedFileInfo.absoluteFilePath()));
    } else {
        QFileInfo currFileInfo(findSaveFile("currentfolders.dat", false));
        if (!currFileInfo.exists()) {
            Q_EMIT error(xi18nc("@info:shell", "Current folder list '%1' does not exist", currFileInfo.absoluteFilePath()));
        } else {
            if (currFileInfo.lastModified() < savedFileInfo.lastModified()) {
                Q_EMIT error(i18nc("@info:shell",
                                   "Current folder list '%1' is older than saved folder list '%2'",
                                   currFileInfo.absoluteFilePath(),
                                   savedFileInfo.absoluteFilePath()));
                Q_EMIT error(i18nc("@info:shell", "Run '%1 %2 --check' first", QCoreApplication::applicationName(), name()));
            } else {
                // Check that the new configuration files generated by
                // the 'check' operation all exist and are not older than
                // the "currentfolders" file.
                ok = true;
                for (const QString &configFile : std::as_const(configFiles)) {
                    const QString newConfigFile = newConfigFileName(configFile);
                    QFileInfo confFileInfo(newConfigFile);

                    if (!confFileInfo.exists() || confFileInfo.lastModified() < currFileInfo.lastModified()) {
                        Q_EMIT error(i18nc("@info:shell",
                                           "New configuration file '%1' does not exist, or is older than folder list '%2'",
                                           confFileInfo.absoluteFilePath(),
                                           currFileInfo.absoluteFilePath()));
                        ok = false;
                    }
                }
            }
        }
    }

    if (!ok) {
        Q_EMIT finished(RuntimeError);
        return;
    }

    // Check that the Akonadi server and agents are stopped.
    if (!isDryRun()) {
        ErrorReporter::progress(i18nc("@info:shell", "Checking whether the Akonadi server is running..."));
        bool firstTime = true;

        while (true) {
            if (ServerManager::state() == ServerManager::NotRunning) {
                std::cerr << qPrintable(i18nc("@info:shell", "Akonadi server is stopped")) << std::endl;
                break;
            }

            if (firstTime) {
                ErrorReporter::progress(i18nc("@info:shell", "Shutting down Akonadi server..."));
                ServerManager::stop();
                firstTime = false;
            }

            sleepFor(1000);
        }
    }

    ErrorReporter::progress(i18nc("@info:shell", "Updating %1 configuration files in '%2'", configFiles.count(), QDir::currentPath()));

    for (const QString &configFile : std::as_const(configFiles)) {
        const QString newConfigFile = newConfigFileName(configFile);

        QFile newFile(newConfigFile);
        if (!newFile.exists())
            continue; // should never happen

        QFile oldFile(configFile);
        if (oldFile.exists()) {
            std::cerr << qPrintable(i18nc("@info:shell", "Removing old '%1'", configFile)) << std::endl;
            if (!isDryRun()) {
                if (!oldFile.remove()) {
                    ErrorReporter::warning(i18nc("@info:shell", "Cannot remove old '%1'", configFile));
                }
            }
        }

        std::cerr << qPrintable(i18nc("@info:shell", "Copying new '%2' to '%1'", configFile, newConfigFile)) << std::endl;
        if (!isDryRun()) {
            if (!newFile.copy(configFile)) {
                ErrorReporter::warning(i18nc("@info:shell", "Cannot copy new '%2'to '%1'", configFile, newConfigFile));
            }
        }
    }

    ErrorReporter::progress(i18nc("@info:shell", "Restore finished"));
    Q_EMIT finished(NoError);
}

QStringList FoldersCommand::allConfigFiles()
{
    QStringList result;

    // Each configuration file named in the change data may be a glob pattern,
    // so prepare to expand it into all matching files.
    const QString configPath = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    qDebug() << "configPath" << configPath;
    if (configPath.isEmpty()) {
        Q_EMIT error(i18nc("@info:shell", "Cannot locate user configuration directory"));
        Q_EMIT finished(RuntimeError);
        return result;
    }

    QDir configDir(configPath);
    if (!configDir.exists()) {
        Q_EMIT error(i18nc("@info:shell", "Configuration directory '%1' does not exist", configDir.canonicalPath()));
        Q_EMIT finished(RuntimeError);
        return result;
    }

    QDir::setCurrent(configPath); // go here for file access

    configDir.setFilter(QDir::Files | QDir::NoDotAndDotDot | QDir::NoSymLinks);
    configDir.setSorting(QDir::Name);

    // Expand each application configuration file in turn, as defined by the
    // change data.
    const QStringList configNames = mChangeData.uniqueKeys();
    for (const QString &configName : std::as_const(configNames)) {
        // Expand the glob pattern into all matching files.
        configDir.setNameFilters(QStringList() << configName);
        result.append(configDir.entryList());
    }

    qDebug() << "found" << result.count() << "config files";
    if (result.isEmpty()) {
        Q_EMIT error(i18nc("@info:shell", "No applicable configuration files found"));
        Q_EMIT finished(RuntimeError);
    }

    return result;
}
