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

#ifndef EXPORTCOMMAND_H
#define EXPORTCOMMAND_H

#include "abstractcommand.h"

class CollectionResolveJob;
class KJob;

class ExportCommand : public AbstractCommand
{
    Q_OBJECT

public:
    explicit ExportCommand(QObject *parent = nullptr);
    virtual ~ExportCommand();

    QString name() const override;

public Q_SLOTS:
    void start() override;

protected:
    int initCommand(QCommandLineParser *parser) override;
    void setupCommandOptions(QCommandLineParser *parser) override;

private:
    bool mDryRun;
    CollectionResolveJob *mResolveJob;
    QString mFileArg;

private Q_SLOTS:
    void onCollectionFetched(KJob *);
    void onWriteFinished(KJob *);
};

#endif // EXPORTCOMMAND_H
