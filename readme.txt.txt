
workflowLibrary: read data of german Personalausweis with selfauthentication from AusweisApp2
Version: 0.4
Author: buergerservice.org e.V. <KeePerso@buergerservice.org>
Language: English


-------------
requirements:
-------------
a 64bit Computer cause this Plugin is 64bit,
Visual C++ Redistributable for Visual Studio 2015/2017/2019,
AusweisApp2
cardreader (maybe you can use a new Android Handy as cardreader - connect in AusweisApp2)
for online identification ready Personalausweis - you can test it in AusweisApp2 with "Meine Daten einsehen"
internetaccess


-----------
how to use:
-----------
the workflowLibary is a library-project.
the workflowLibrary to use you need the workflowLibary.lib-file and the workflowLibary.h-file.

the workflowClient-project is a console-project in the workflowLibrary-folder. it is an example how you can use the library.
you use it from console usage workflowClient.exe <yourPersonalausweisPIN> for example workflowClient.exe 123456 


----------------------------
known problems and questions
----------------------------

errormessage:
	- is AusweisApp2 running ?
	- is cardreader connected ?
	- is Personalausweis on cardreader?
	- maybe disable firewall/viruswall/webfilter for test
	- maybe disable firewall/viruswall/webfilter for test

is my PIN safe?
	- the PIN is only sent to AusweisApp2 and not stored. you can use a cardreader with keypad, then the library 
	  cant see the PIN.


---------------
versionhistory:
---------------
0.4 added sha256
0.3 read certificate, german umlaute
0.2 better workflow
0.1 start pilotversion


---------
c-wrapper
---------
the c-wrapper is a wrapper for the selfauthentication functions for use in c


-------
license
-------
// Copyright (C) 2021 buergerservice.org e.V. <KeePerso@buergerservice.org>
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)
