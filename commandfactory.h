/*
    Copyright (C) 2012  Kevin Krammer <krammer@kde.org>

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

#ifndef COMMANDFACTORY_H
#define COMMANDFACTORY_H

#include <QHash>

class AbstractCommand;

class KCmdLineArgs;

class QString;

class CommandFactory
{
  public:
    explicit CommandFactory( KCmdLineArgs *parsedArgs );
    ~CommandFactory();
    
    AbstractCommand *createCommand();
    
  private:
    KCmdLineArgs *mParsedArgs;
    
    QHash<QString, AbstractCommand*> mCommands;
    
  private:
    void registerCommands();
    void checkAndHandleHelp();
};

#endif // COMMANDFACTORY_H
