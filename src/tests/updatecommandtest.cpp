/*
 * Copyright (C) 2014  Bhaskar Kandiyal <bkandiyal@gmail.com>
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

#include "updatecommandtest.h"

#include "../collectionresolvejob.h"
#include "../updatecommand.h"

#include <AkonadiCore/control.h>
#include <AkonadiCore/itemcreatejob.h>
#include <AkonadiCore/itemfetchjob.h>
#include <akonadi/qtest_akonadi.h>
#include <AkonadiCore/itemfetchscope.h>

using namespace Akonadi;

QTEST_AKONADIMAIN(UpdateCommandTest, NoGUI);

void UpdateCommandTest::initTestCase()
{
    AkonadiTest::checkTestIsIsolated();
    Control::start();
}

void UpdateCommandTest::testUpdateItem()
{
    CollectionResolveJob *resolveJob = new CollectionResolveJob("/res3", this);
    QVERIFY(resolveJob->hasUsableInput());
    AKVERIFYEXEC(resolveJob);

    Item item;
    item.setMimeType("text/plain");
    item.setPayload<QByteArray>("Hello World");

    Collection collection = resolveJob->collection();

    ItemCreateJob *createJob = new ItemCreateJob(item, collection);
    AKVERIFYEXEC(createJob);

    item = createJob->item();

    QByteArray newContent = "Foo Bar!";

    QTemporaryFile tempFile;
    tempFile.setAutoRemove(true);
    tempFile.open();
    tempFile.write(newContent, newContent.size());
    tempFile.close();

    const int numArgs = 4;

    QVector<QByteArray> args;
    args.reserve(numArgs);
    args << "akonadiclient" << "update"
        << item.url().toEncoded()
        << tempFile.fileName().toLocal8Bit();

    const char *testArgs[4] = {
        args[0].data(),
        args[1].data(),
        args[2].data(),
        args[3].data()
    };

    KCmdLineArgs *parsedArgs = getParsedArgs(numArgs, testArgs);

    UpdateCommand *command = new UpdateCommand(this);

    int ret = command->init(parsedArgs);
    QCOMPARE(ret, 0);

    ret = runCommand(command);
    QCOMPARE(ret, 0);

    ItemFetchJob *fetchJob = new ItemFetchJob(item, this);
    fetchJob->fetchScope().setFetchModificationTime(false);
    fetchJob->fetchScope().fetchAllAttributes(false);
    fetchJob->fetchScope().fetchFullPayload(true);

    AKVERIFYEXEC(fetchJob);

    QCOMPARE(fetchJob->items().count(), 1);
    QByteArray payload = fetchJob->items().first().payload<QByteArray>();

    QCOMPARE(payload, newContent);
}