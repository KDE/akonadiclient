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

#include "addcommandtest.h"
#include "../addcommand.h"
#include "../collectionresolvejob.h"

#include <AkonadiCore/collectioncreatejob.h>
#include <AkonadiCore/collectionfetchjob.h>
#include <AkonadiCore/control.h>
#include <akonadi/qtest_akonadi.h>
#include <AkonadiCore/itemfetchjob.h>
#include <AkonadiCore/itemfetchscope.h>

#include <kstandarddirs.h>
#include <ktempdir.h>

#include <QtDebug>

using namespace Akonadi;

QTEST_AKONADIMAIN(AddCommandTest, NoGUI);

void AddCommandTest::initTestCase()
{
    AkonadiTest::checkTestIsIsolated();
    Control::start();
    mTempDir = new KTempDir(KStandardDirs::locateLocal("tmp", "akonadiclient"));
    mTempDir->setAutoRemove(false);

    QVERIFY(mTempDir->exists());
    QString path = mTempDir->name();
    QDir dir(mTempDir->name());

    QVERIFY(dir.mkpath("foo/bar"));

    QFile file(dir.absoluteFilePath("test.txt"));
    file.open(QIODevice::WriteOnly);
    file.write("Testing");
    file.close();

    dir.cd("foo");

    file.setFileName(dir.absoluteFilePath("hello.txt"));
    file.open(QIODevice::WriteOnly);
    file.write("Foo Item");
    file.close();

    dir.cd("bar");
    file.setFileName(dir.absoluteFilePath("world.txt"));
    file.open(QIODevice::WriteOnly);
    file.write("Bar Item");
    file.close();

    dir.cdUp(); // Change back to 'foo'

    QDir::setCurrent(dir.absolutePath());
}

void AddCommandTest::testAddItem()
{
    QDir dir(mTempDir->name());
    dir.cd("foo");
    QDir::setCurrent(dir.absolutePath());

    CollectionResolveJob *resolveJob = new CollectionResolveJob("/res3", this);
    QVERIFY(resolveJob->hasUsableInput());
    AKVERIFYEXEC(resolveJob);

    QVERIFY(resolveJob->collection().isValid());

    Collection col;
    col.setName("test");
    col.setParentCollection(resolveJob->collection());

    CollectionCreateJob *createJob = new CollectionCreateJob(col, this);
    AKVERIFYEXEC(createJob);

    col = createJob->collection();
    QVERIFY(col.isValid());

    const int numArgs = 5;
    QVector<QByteArray> args;
    args.reserve(numArgs);
    args << "akonadiclient" << "add" << "/res3/test" << "../test.txt" << "bar";

    const char *testArgs[numArgs];
    for (int i = 0; i < numArgs; i++) {
        testArgs[i] = args.at(i).data();
    }

    KCmdLineArgs *parsedArgs = getParsedArgs(numArgs, testArgs);

    AddCommand *command = new AddCommand(this);

    int ret = command->init(parsedArgs);
    QCOMPARE(ret, 0);

    ret = runCommand(command);
    QCOMPARE(ret, 0);

    ItemFetchJob *itemFetchJob1 = new ItemFetchJob(col, this);
    itemFetchJob1->fetchScope().fetchAllAttributes(false);
    itemFetchJob1->fetchScope().fetchFullPayload(true);

    AKVERIFYEXEC(itemFetchJob1);

    QCOMPARE(itemFetchJob1->items().count(), 1);
    QCOMPARE(itemFetchJob1->items().at(0).payloadData(), QByteArray("Testing"));

    CollectionFetchJob *fetchJob1 = new CollectionFetchJob(col, CollectionFetchJob::FirstLevel, this);
    AKVERIFYEXEC(fetchJob1);

    QCOMPARE(fetchJob1->collections().count(), 1);

    col = fetchJob1->collections().at(0);
    QCOMPARE(col.name(), QString("bar"));

    ItemFetchJob *itemFetchJob2 = new ItemFetchJob(col, this);
    itemFetchJob2->fetchScope().fetchAllAttributes(false);
    itemFetchJob2->fetchScope().fetchFullPayload(true);

    AKVERIFYEXEC(itemFetchJob2);
    QCOMPARE(itemFetchJob2->items().count(), 1);
    QCOMPARE(itemFetchJob2->items().at(0).payloadData(), QByteArray("Bar Item"));
}

void AddCommandTest::testAddItemWithBase()
{
    CollectionResolveJob *resolveJob = new CollectionResolveJob("/res3", this);
    QVERIFY(resolveJob->hasUsableInput());
    AKVERIFYEXEC(resolveJob);

    QVERIFY(resolveJob->collection().isValid());

    Collection col;
    col.setName("temp");
    col.setParentCollection(resolveJob->collection());

    CollectionCreateJob *createJob = new CollectionCreateJob(col, this);
    AKVERIFYEXEC(createJob);

    col = createJob->collection();
    QVERIFY(col.isValid());

    const int numArgs = 8;
    QVector<QByteArray> args;
    args.reserve(numArgs);
    args << "akonadiclient" << "add" << "--base" << mTempDir->name().toLocal8Bit() << "/res3/temp"
         << "test.txt" << "foo/hello.txt" << "foo/bar";

    const char *testArgs[numArgs];
    for (int i = 0; i < numArgs; i++) {
        testArgs[i] = args.at(i).data();
    }

    KCmdLineArgs *parsedArgs = getParsedArgs(numArgs, testArgs);

    AddCommand *command = new AddCommand(this);

    int ret = command->init(parsedArgs);
    QCOMPARE(ret, 0);

    ret = runCommand(command);
    QCOMPARE(ret, 0);

    ItemFetchJob *itemFetchJob1 = new ItemFetchJob(col, this);
    itemFetchJob1->fetchScope().fetchAllAttributes(false);
    itemFetchJob1->fetchScope().fetchFullPayload(true);

    AKVERIFYEXEC(itemFetchJob1);

    QCOMPARE(itemFetchJob1->items().count(), 1);
    QCOMPARE(itemFetchJob1->items().at(0).payloadData(), QByteArray("Testing"));

    CollectionFetchJob *fetchJob1 = new CollectionFetchJob(col, CollectionFetchJob::FirstLevel, this);
    AKVERIFYEXEC(fetchJob1);

    QCOMPARE(fetchJob1->collections().count(), 1);

    col = fetchJob1->collections().at(0);
    QCOMPARE(col.name(), QString("foo"));

    ItemFetchJob *itemFetchJob2 = new ItemFetchJob(col, this);
    itemFetchJob2->fetchScope().fetchAllAttributes(false);
    itemFetchJob2->fetchScope().fetchFullPayload(true);

    AKVERIFYEXEC(itemFetchJob2);
    QCOMPARE(itemFetchJob2->items().count(), 1);
    QCOMPARE(itemFetchJob2->items().at(0).payloadData(), QByteArray("Foo Item"));

    CollectionFetchJob *fetchJob2 = new CollectionFetchJob(col, CollectionFetchJob::FirstLevel, this);
    AKVERIFYEXEC(fetchJob2);
    QCOMPARE(fetchJob2->collections().count(), 1);

    col = fetchJob2->collections().at(0);
    QCOMPARE(col.name(), QString("bar"));

    ItemFetchJob *itemFetchJob3 = new ItemFetchJob(col, this);
    itemFetchJob3->fetchScope().fetchAllAttributes(false);
    itemFetchJob3->fetchScope().fetchFullPayload(true);
    AKVERIFYEXEC(itemFetchJob3);

    QCOMPARE(itemFetchJob3->items().count(), 1);
    QCOMPARE(itemFetchJob3->items().at(0).payloadData(), QByteArray("Bar Item"));
}