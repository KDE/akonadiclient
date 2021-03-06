/*
    Copyright (C) 2014  Jonathan Marten <jjm@keelhaul.me.uk>

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

#ifndef TAGSCOMMAND_H
#define TAGSCOMMAND_H

#include "abstractcommand.h"

class KJob;

class TagsCommand : public AbstractCommand
{
    Q_OBJECT

public:
    explicit TagsCommand(QObject *parent = nullptr);
    virtual ~TagsCommand() = default;

    QString name() const override;

public Q_SLOTS:
    void start() override;

protected:
    void setupCommandOptions(QCommandLineParser *parser) override;
    int initCommand(QCommandLineParser *parser) override;

private:
    bool mBriefOutput;
    bool mUrlsOutput;

private Q_SLOTS:
    void onTagsFetched(KJob *job);
};

#endif                          // TAGSCOMMAND_H
