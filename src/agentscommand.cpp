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

#include "agentscommand.h"

#include <klocalizedstring.h>
#include <kcmdlineargs.h>
#include <akonadi/agentmanager.h>
#include <akonadi/agentinstance.h>

#include <qstringlist.h>

#include <iostream>
#include <iomanip>

using namespace Akonadi;

AgentsCommand::AgentsCommand(QObject *parent)
    : AbstractCommand(parent),
      mDryRun(false),
      mStateArg(ONLINE),
      mOption(LIST)
{
    mShortHelp = ki18nc("@info:shell", "Manage Akonadi agents").toString();
}

AgentsCommand::~AgentsCommand()
{
}

void AgentsCommand::setupCommandOptions(KCmdLineOptions &options)
{
    AbstractCommand::setupCommandOptions(options);

    addOptionsOption(options);
    options.add("+[agents]...", ki18nc("@info:shell", "Agents to operate on"));
    addOptionSeparator(options);
    options.add("l").add("list", ki18nc("@info:shell", "List all agents"));
    options.add("s").add("setstate <state>", ki18nc("@info:shell", "Set state for specified agents. Valid states are \"online\" and \"offline\"."));
    options.add("g").add("getstate", ki18nc("@info:shell", "Get state for the specified agent"));
    options.add("i").add("info", ki18nc("@info:shell", "Show information about the specified agent"));
    options.add("r").add("restart", ki18nc("@info:shell", "Restart the specified agent"));
    addDryRunOption(options);
}

int AgentsCommand::initCommand(KCmdLineArgs *parsedArgs)
{
    mDryRun = parsedArgs->isSet("dryrun");

    for (int i = 1; i < parsedArgs->count(); i++) {
        mArguments.append(parsedArgs->arg(i));
    }

    if (parsedArgs->isSet("list")) {
        mOption = LIST;
    } else {
        if (mArguments.length() == 0) {
            emitErrorSeeHelp(ki18nc("@info:shell", "No agents or options specified"));
            return InvalidUsage;
        }

        if (parsedArgs->isSet("info")) {
            mOption = INFO;
        } else if (parsedArgs->isSet("restart")) {
            mOption = RESTART;
        } else if (parsedArgs->isSet("getstate")) {
            mOption = GETSTATE;
        } else if (parsedArgs->isSet("setstate")) {
            mOption = SETSTATE;
            QString state = parsedArgs->getOption("setstate");

            if (state.compare("online") == 0) {
                mStateArg = ONLINE;
            } else if (state.compare("offline") == 0) {
                mStateArg = OFFLINE;
            } else {
                emitErrorSeeHelp(ki18nc("@info:shell", "Invalid state '%1'").subs(parsedArgs->getOption("setstate")));;
                return InvalidUsage;
            }
        } else {
            emitErrorSeeHelp(ki18nc("@info:shell", "No option specified"));
            return InvalidUsage;
        }
    }

    return NoError;
}

void AgentsCommand::start()
{
    switch (mOption) {
        case LIST: {
            QList<AgentInstance> instances = AgentManager::self()->instances();
            printAgentStatus(instances);
            break;
        }
        case SETSTATE: {
            setState();
            break;
        }
        case GETSTATE: {
            getState();
            break;
        }
        case INFO: {
            showInfo();
            break;
        }

        case RESTART: {
            restartAgents();
            break;
        }

        default: {
            emitErrorSeeHelp(ki18nc("@info:shell", "Invalid parameters"));
            emit finished(InvalidUsage);
            break;
        }
    }

    emit finished(NoError);
}

void AgentsCommand::printAgentStatus(const QList<AgentInstance> &agents)
{
    int max_width = 0;
    for (int i = 0; i < agents.length(); i++) {
        AgentInstance agent = agents.at(i);
        if (max_width < agent.identifier().length()) {
            max_width = agent.identifier().length();
        }
    }

    std::cout << "Name" << std::setw(max_width + 20) << "Status" << std::endl;
    for (int i = 0; i < agents.length(); i++) {
        AgentInstance agent = agents.at(i);
        int width = max_width - agent.identifier().length();
        std::cout << agent.identifier().toLocal8Bit().data() << std::setw(width + 18) << " ";
        std::cout << agent.statusMessage().toLocal8Bit().data() << std::endl;
    }
}

void AgentsCommand::setState()
{
    AgentManager *manager = AgentManager::self();
    QList<AgentInstance> agentList;

    for (int i = 0; i < mArguments.length(); i++) {
        AgentInstance instance = manager->instance(mArguments.at(i));
        if (!instance.isValid()) {
            emit error(ki18nc("@info:shell", "No agent exists with the identifier '%1'").subs(mArguments.at(i)).toString());
            emit finished(RuntimeError);
            return;
        }
        agentList.append(instance);
    }

    if (!mDryRun) {
        for (int i = 0; i < agentList.length(); i++) {
            AgentInstance instance = agentList.at(i);
            switch (mStateArg) {
                case ONLINE: {
                    instance.setIsOnline(true);
                    break;
                }
                case OFFLINE: {
                    instance.setIsOnline(false);
                    break;
                }
            }
        }
    }
}

void AgentsCommand::getState()
{
    AgentManager *manager = AgentManager::self();
    QList<AgentInstance> agentList;

    for (int i = 0; i < mArguments.length(); i++) {
        AgentInstance instance = manager->instance(mArguments.at(i));
        if (!instance.isValid()) {
            emit error(ki18nc("@info:shell", "No agent exists with the identifier '%1'").subs(mArguments.at(i)).toString());
            emit finished(RuntimeError);
            return;
        }
        agentList.append(instance);
    }

    printAgentStatus(agentList);
}

void AgentsCommand::showInfo()
{
    AgentManager *manager = AgentManager::self();
    QList<AgentInstance> agentList;

    for (int i = 0; i < mArguments.length(); i++) {
        AgentInstance instance = manager->instance(mArguments.at(i));
        if (!instance.isValid()) {
            emit error(ki18nc("@info:shell", "No agent exists with the identifier '%1'").subs(mArguments.at(i)).toString());
            emit finished(RuntimeError);
            return;
        }
        agentList.append(instance);
    }

    for (int i = 0; i < agentList.length(); i++) {
        AgentInstance instance = agentList.at(i);
        std::cout << ki18nc("@info:shell", "ID:      ").toString().toLocal8Bit().constData() << instance.identifier().toLocal8Bit().data() << std::endl;
        std::cout << ki18nc("@info:shell", "Name:    ").toString().toLocal8Bit().constData() << instance.name().toLocal8Bit().data() << std::endl;
        std::cout << ki18nc("@info:shell", "Status:  ").toString().toLocal8Bit().constData() << instance.statusMessage().toLocal8Bit().data() << std::endl;
        if ((i + 1) < mArguments.length()) {
            std::cout << std::endl;
        }
    }
}

void AgentsCommand::restartAgents()
{
    AgentManager *manager = AgentManager::self();
    QList<AgentInstance> agentList;

    for (int i = 0; i < mArguments.length(); i++) {
        AgentInstance instance = manager->instance(mArguments.at(i));
        if (!instance.isValid()) {
            emit error(ki18nc("@info:shell", "No agent exists with the identifier '%1'").subs(mArguments.at(i)).toString());
            emit finished(RuntimeError);
            return;
        }
        agentList.append(instance);
    }

    if (!mDryRun) {
        for (int i = 0; i < agentList.length(); i++) {
            AgentInstance instance = agentList.at(i);
            instance.restart();
        }
    }
}
