==============
akonadiclient
==============
--------------------------------
A commandline client for Akonadi
--------------------------------

:Author: Bhaskar Kandiyal <bkandiyal@gmail.com>
:Date:	 August 12 2014
:Copyright: Copyright (C) 2014 Bhaskar Kandiyal. Free use of this software is granted under the terms of the GNU General Public License version 2 or any later version.
:Version: 0.1
:Manual section: 1

SYNOPSIS
========
**akonadiclient** command [options] [arguments]

DESCRIPTION
===========
**akonadiclient**  is a commandline client for manipulating KDE's Akonadi datastore. It provides an easy way to manipulate Akonadi's data through the commandline.
For example, adding items, collections, renaming or moving collections and also controlling the Akonadi agents.

COMMANDS
========

Filesystem Commands
-------------------

**copy**

    akonadiclient copy [OPTIONS] SOURCE DESTINATION

    Used to copy collections or items from one collection to another.

    Options:


    **-n, --dryrun**

        Run without making any actual changes

**create**

    akonadiclient create OPTIONS COLLECTION

    Creates a new collection. Please note that top-level collections can only be created by an Akonadi resource.

    Options:

    **-p, --parent <collection>**

        Parent collection to create the new collection in.

    **-n, --dryrun**

        Run without making any actual changes

**delete**

    akonadiclient delete [OPTIONS] COLLECTION | ITEM

    Delete a collection or an item.

    Options:

    **-c, --collection**

        Assume that a collection is specified as an argument

    **-i, --item**

        Assume that an item is specified as an argument

    **-n, --dryrun**

        Run without making any actual changes

**list**

    akonadiclient list [OPTIONS] COLLECTION

    List sub-collections and / or items inside the collection specified by COLLECTION.

    Options:

    **-l, --details**

        List more detailed information

    **-c, --collections**

        List only sub-collections

    **-i, --items**

        List only contained items

**move**

    akonadiclient move [OPTIONS] SOURCE DESTINATION

    Move collections or items into another collection

    Options:

    **-n, --dryrun**

        Run without making any actual changes

**rename**

    akonadiclient rename [OPTIONS] COLLECTION NAME

    Renames a the specified collection to NAME

    Options:

    **-n, --dryrun**

        Run without making any actual changes

Data Commands
-------------

**add**

    akonadiclient add [OPTIONS] COLLECTION FILES

    Add items to a specified collection


    Options:

    **-b, --base <dir>**

        Base directory for input files / directories, default is current

    **-f, --flat**

        Flat mode, do not duplicate subdirectory structure

    **-n, --dryrun**

        Run without making any actual changes


**edit**


    akonadiclient edit [OPTIONS] ITEM

    Opens up the payload of ITEM in a text editor specified by the environment variable $EDITOR

    Options:

    **-n, --dryrun**

        Run without making any actual changes

**expand**


    akonadiclient expand [OPTIONS] ITEM

    Expand a contact group item

    Options:

    **-b, --brief**

        Brief output (email addresses only)

**export**


    akonadiclient export [OPTIONS] COLLECTION FILE

    Exports COLLECTION to an XML file specified by FILE

    Options:

    **-n, --dryrun**

        Run without making any actual changes

**import**

    akonadiclient import [OPTIONS] PARENT FILE

    Imports an XML file inside the collection specified by PARENT. If the collection already exists, it's contents are merged with the contents of the collection in the XML file.

    Options:

    **-n, --dryrun**

        Run without making any actual changes

**info**

    akonadiclient info [OPTIONS] COLLECTION | ITEM

    Show full information about a collection or item

    Options:

    **-c, --collection**

        Assume that a collection is specified

    **-i, --item**

        Assume that an item is specified

**show**

    akonadiclient show ITEM

    Show the raw payload of an item


**tags**

    akonadiclient tags [OPTIONS]

    List all known tags

    Options:

    **-b, --brief**

        Brief output - tag names only

    **-u, --urls**

        Brief output - tag URLs only

**update**

    akonadiclient update ITEM FILE

    Updates the raw payload of an item specified by ITEM with the contents of FILE

    Options:

    **-n, --dryrun**

        Run without making any actual changes

Miscellaneous Commands
----------------------

**agents**

    akonadiclient agents OPTIONS [AGENTS...]

    Allows managing of Akonadi agents like changing the state of an agent, restarting an agent or listing all agents and thier state.

    Options:

    **-l, --list**

            List all agents

    **-s, --setstate <state>**

            Set <state> for specified agents. Valid states are "online" and "offline".

    **-g, --getstate**

            Get state for the specified agents

    **-i, --info**
            Shows information about the specified agents

    **-r, --restart**
            Restarts the specified agents

    **-n, --dryrun**
            Run without making any actual changes

**help**

    akonadiclient help [COMMAND]

    Displays help for COMMAND. If COMMAND is not specified then it lists all the available commands.
