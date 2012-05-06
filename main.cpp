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

int main( int argc, char **argv ) {
  KAboutData aboutData( "akonadiclient", "akonadiclient", ki18nc( "@title program name", "Akonadi Commandline Client" ),
                        "PROGRAM_VERSION", ki18nc( "@info:shell short description", "A commandline/shell client for Akonadi" ),
                        KAboutData::License_GPL );
  
  aboutData.addAuthor( ki18n( "Kevin Krammer" ), ki18nc( "@title about data task", "Original Author" ), "krammer@kde.org" );
    
  CommandRunner runner( argc, argv );
  return runner.exec();
}
