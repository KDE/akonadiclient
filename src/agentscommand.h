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

#pragma once

#include "abstractcommand.h"

namespace Akonadi
{
class AgentInstance;
};

class AgentsCommand : public AbstractCommand
{
    Q_OBJECT

public:
    explicit AgentsCommand(QObject *parent = nullptr);
    virtual ~AgentsCommand() = default;

    QString name() const override;

public Q_SLOTS:
    void start() override;

protected:
    int initCommand(QCommandLineParser *parser) override;
    void setupCommandOptions(QCommandLineParser *parser) override;

private:
    void getState();
    void showInfo();
    void printAgentStatus(const QVector<Akonadi::AgentInstance> &agents);
    void restartAgents();
    void setState();

private:
    enum Option { LIST = 0, SETSTATE = 1, GETSTATE = 2, INFO = 3, RESTART = 4 };

    enum AgentState { OFFLINE = 0, ONLINE = 1 };

    AgentState mStateArg;
    Option mOption;
    QStringList mArguments;
};
