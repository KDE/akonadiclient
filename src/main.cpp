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

#include "commandrunner.h"

#include <KAboutData>
#include <KCmdLineArgs>

#include "version.h"

const char *appname = "akonadiclient";

int main( int argc, char **argv )
{
  KAboutData aboutData( appname, 0, ki18nc( "@title program name", "Akonadi Client" ),
#ifdef VCS_HAVE_VERSION
                        ( VERSION " (" VCS_TYPE_STRING " " VCS_REVISION_STRING ")"),
#else
                        VERSION,
#endif
                        ki18nc( "@info:shell short description", "A command-line/shell client for Akonadi" ),
                        KAboutData::License_GPL );

  aboutData.addAuthor( ki18n( "Kevin Krammer" ), ki18nc( "@title about data task", "Original Author" ), "krammer@kde.org" );

  KCmdLineArgs::init( argc, argv, &aboutData );
  KCmdLineArgs::addStdCmdLineOptions( KCmdLineArgs::CmdLineArgNone );

  KCmdLineOptions options;
  options.add( "!+command", ki18nc( "@info:shell", "Command to execute" ) );
  options.add( "+[options]", ki18nc( "@info:shell", "Options for command" ) );
  options.add( "+[args]", ki18nc( "@info:shell", "Arguments for command" ) );
  options.add( "", ki18nc( "@info:shell",
                           "See '<application>%1</application> help'"
                           " for available commands"
                           "\n"
                           "See '<application>%1</application> help command'"
                           " for more information on a specific command." ).subs( appname ) );
  KCmdLineArgs::addCmdLineOptions( options );

  // call right away so standard options like --version can terminate the program right here
  KCmdLineArgs *parsedArgs = KCmdLineArgs::parsedArgs();

  CommandRunner runner( aboutData, parsedArgs );
  return runner.exec();
}
