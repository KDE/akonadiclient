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

#include "agentscommandtest.h"

#include "../agentscommand.h"

#include <AkonadiCore/control.h>
#include <akonadi/qtest_akonadi.h>

using namespace Akonadi;

QTEST_AKONADIMAIN(AgentsCommandTest, NoGUI);

void AgentsCommandTest::initTestCase()
{
    AkonadiTest::checkTestIsIsolated();
    Control::start();
}

void AgentsCommandTest::testAgentSetState()
{
    const char *args[4] = {
        "akonadiclient",
        "agents",
        "--setstate=offline",
        "akonadi_knut_resource_0"
    };

    AgentManager *manager = AgentManager::self();
    QSignalSpy instanceSpy(manager, SIGNAL(instanceOnline(Akonadi::AgentInstance,bool)));

    KCmdLineArgs *parsedArgs = getParsedArgs(4, args);
    AgentsCommand *command = new AgentsCommand(this);

    int ret = command->init(parsedArgs);
    QCOMPARE(ret, 0);

    ret = runCommand(command);
    QCOMPARE(ret, 0);

    for (int i = 0; i < 10 && instanceSpy.count() <= 0; i++) {
        QTest::qWait(100);
    }

    QCOMPARE(instanceSpy.count(), 1);

    AgentInstance instance = manager->instance(args[3]);
    QCOMPARE(instance.isOnline(), false);

    delete command;

    const char *newargs[4] = {
        "akonadiclient",
        "agents",
        "--setstate=online",
        "akonadi_knut_resource_0"
    };

    parsedArgs = getParsedArgs(4, newargs);
    command = new AgentsCommand(this);

    ret = command->init(parsedArgs);
    QCOMPARE(ret, 0);

    instanceSpy.clear();

    ret = runCommand(command);
    QCOMPARE(ret, 0);

    for (int i = 0; i < 10 && instanceSpy.count() <= 0; i++) {
        QTest::qWait(100);
    }

    QCOMPARE(instanceSpy.count(), 1);

    instance = manager->instance(newargs[3]);

    QCOMPARE(instance.isOnline(), true);
}
