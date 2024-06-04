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

#include <Akonadi/AgentInstance>
#include <Akonadi/AgentManager>

#include <QVector>
#include <qstringlist.h>

#include <iomanip>
#include <iostream>

#include "commandfactory.h"

using namespace Akonadi;

DEFINE_COMMAND("agents", AgentsCommand, kli18nc("info:shell", "Manage Akonadi agents"));

AgentsCommand::AgentsCommand(QObject *parent)
    : AbstractCommand(parent)
    , mStateArg(ONLINE)
    , mOption(LIST)
{
}

void AgentsCommand::setupCommandOptions(QCommandLineParser *parser)
{
    addOptionsOption(parser);
    parser->addOption(QCommandLineOption((QStringList() << "l"
                                                        << "list"),
                                         i18n("List all agents")));
    parser->addOption(QCommandLineOption((QStringList() << "s"
                                                        << "setstate"),
                                         i18n("Set state \"online\" or \"offline\" for specified agents"),
                                         i18nc("@info:shell", "state")));
    parser->addOption(QCommandLineOption((QStringList() << "g"
                                                        << "getstate"),
                                         i18n("Get state for the specified agent")));
    parser->addOption(QCommandLineOption((QStringList() << "i"
                                                        << "info"),
                                         i18n("Show information about the specified agent")));
    parser->addOption(QCommandLineOption((QStringList() << "r"
                                                        << "restart"),
                                         i18n("Restart the specified agent")));
    addDryRunOption(parser);

    parser->addPositionalArgument("agents", i18nc("@info:shell", "Agents to operate on"), i18nc("@info:shell", "[agents...]"));
}

int AgentsCommand::initCommand(QCommandLineParser *parser)
{
    mArguments = parser->positionalArguments();

    if (!getCommonOptions(parser))
        return InvalidUsage;

    if (parser->isSet("list")) {
        mOption = LIST;
    } else {
        if (!checkArgCount(mArguments, 1, i18nc("@info:shell", "No agents or options specified")))
            return InvalidUsage;

        if (parser->isSet("info")) {
            mOption = INFO;
        } else if (parser->isSet("restart")) {
            mOption = RESTART;
        } else if (parser->isSet("getstate")) {
            mOption = GETSTATE;
        } else if (parser->isSet("setstate")) {
            mOption = SETSTATE;
            const QString state = parser->value("setstate");

            if (state.length() >= 2 && state.compare(QStringLiteral("online").left(state.length())) == 0) {
                mStateArg = ONLINE;
            } else if (state.length() >= 2 && state.compare(QStringLiteral("offline").left(state.length())) == 0) {
                mStateArg = OFFLINE;
            } else {
                emitErrorSeeHelp(i18nc("@info:shell", "Invalid state '%1'", state));
                return InvalidUsage;
            }
        } else {
            emitErrorSeeHelp(i18nc("@info:shell", "No option specified"));
            return InvalidUsage;
        }
    }

    return NoError;
}

static bool instanceLessThan(const AgentInstance &a, const AgentInstance &b)
{
    return (a.identifier() < b.identifier());
}

void AgentsCommand::start()
{
    switch (mOption) {
    case LIST: {
        QVector<AgentInstance> instances = AgentManager::self()->instances();
        std::sort(instances.begin(), instances.end(), &instanceLessThan);
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
        emitErrorSeeHelp(i18nc("@info:shell", "Invalid parameters"));
        Q_EMIT finished(InvalidUsage);
        break;
    }
    }

    Q_EMIT finished(NoError);
}

void AgentsCommand::printAgentStatus(const QVector<AgentInstance> &agents)
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
            Q_EMIT error(i18nc("@info:shell", "No agent exists with the identifier '%1'", mArguments.at(i)));
            Q_EMIT finished(RuntimeError);
            return;
        }
        agentList.append(instance);
    }

    if (!isDryRun()) {
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
    QVector<AgentInstance> agentList;

    for (int i = 0; i < mArguments.length(); i++) {
        AgentInstance instance = manager->instance(mArguments.at(i));
        if (!instance.isValid()) {
            Q_EMIT error(i18nc("@info:shell", "No agent exists with the identifier '%1'", mArguments.at(i)));
            Q_EMIT finished(RuntimeError);
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
            Q_EMIT error(i18nc("@info:shell", "No agent exists with the identifier '%1'", mArguments.at(i)));
            Q_EMIT finished(RuntimeError);
            return;
        }
        agentList.append(instance);
    }

    for (int i = 0; i < agentList.length(); i++) {
        AgentInstance instance = agentList.at(i);
        std::cout << qPrintable(i18nc("@info:shell", "ID:      ")) << qPrintable(instance.identifier()) << std::endl;
        std::cout << qPrintable(i18nc("@info:shell", "Name:    ")) << qPrintable(instance.name()) << std::endl;
        std::cout << qPrintable(i18nc("@info:shell", "Status:  ")) << qPrintable(instance.statusMessage()) << std::endl;
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
            Q_EMIT error(i18nc("@info:shell", "No agent exists with the identifier '%1'", mArguments.at(i)));
            Q_EMIT finished(RuntimeError);
            return;
        }
        agentList.append(instance);
    }

    if (!isDryRun()) {
        for (int i = 0; i < agentList.length(); i++) {
            AgentInstance instance = agentList.at(i);
            instance.restart();
        }
    }
}

#include "moc_agentscommand.cpp"
