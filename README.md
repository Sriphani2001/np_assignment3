# np_assignment3

Requirements (Ubuntu 20.04LTS, adapt for your version): 


libncurses-dev libswitch-perl libio-socket-timeout-perl libstring-random-perl \\
libstring-diff-perl libexpect-perl libdata-dump-perl libdata-dump-oneline-perl \\
libtest-hexstring-perl


Sadly the 'String::Dump' (used by test_server) perl module, isnt found in a
standard package, and hence needs to be installed via CPAN, Perl's package
manager. The operation to install any perl module, is in theory principle, start
CPAN (the command is cpan). The first time you run cpan, you will be asked a set
of questions, unless you know some specifics, just use the proposed default
values. Also, note that installing Perl modules may require some softwar to be
built, from source. Hence, having the basic C/C++ developer tools
(build-essential), is required. Additional development source and header files,
may also be required, depending on what will be built. Second note, if you run
the cpan as root (sudo cpan), all installed modules will be availible to all
users on the host. If you just run it as your user, the modules will only be
availible to your user. My recommendation is to run the command as root. 

Once cpan has been configured, updated (if needed), you will be presented with a
prompt;
cpan[1]>

We can now search for the module name, or keywords in the module. This is done
using the 'i' command. In this case, we will issue;
cpan[2]> i /string::dump/                                                                                   
Reading '/root/.cpan/Metadata'
  Database was generated on Wed, 14 Apr 2021 06:29:02 GMT
Module  < Metabrik::String::Dump (GOMOR/Metabrik-Repository-1.41.tar.gz)
Module  = String::Dump           (PATCH/String-Dump-0.09.tar.gz)
2 items found

The second module is the one we are looking for; and to install it we just
issue; 
cpan[3]> install String::Dump

This will trigger a lot of activitiy, hopefully it will be sucessfull. If not,
look through the log and try to identify why it failed. Usually, it will be due
to some missing requirement. If so, just install that module and try again.
Note,that in some cases, that may require you to exit cpan, and install some
library. 



--------------------------------------------------------------------------------
General usage: 
	Adapt the Makefile to suit your solution, i.e. C or C++ compiler.
	Requirement is that make and make clean operates.

	Server binary must be called cserverd.
	Client binary must be called cchat.


--------------------------------------------------------------------------------
Files & Short descriptions: 
main_curses.c
	Contains examples on how to use regexec and ncurses.

test_client.pl
	Is a tool to assist in the client testing.
	Requires a proper server, use reference server.

	Usage: test_client.pl serverIP serverPort logfile.

	Operations:
	1. Start 'logger' client (nc serverIP serverPort),
	to log output from server and clients.
	
	2. Start test_client.
	It launches N clients towards the <serverIP>:<serverPort>.
	These send messages peridodically, from Ipsum Loreum text.
	Reads replies, then sends again (after a timeout).

	3. Verify that 'logger' sees the messages properly from
	the test_client.

	4. Start your client. Send some messages. See that it works.

	Note; in my 'test' I have access to the server as well, and
	I see the messages, as well as a hex dump of them. So, its
	EASY to see if you are sending a full message (padded with
	zeros), cf. 'Hello!\n\x00\x00\x00\x00......\x00'.
	
	Important part, client should be able to read and send
	messages according to standard. React properly, if the
	server sends an ERROR. 



test_server.pl
	Basic test of your server.
	At the moment it tests NICK name handling and multiple
	clients (configurable).

	Usage: test_server.pl serverIP serverPort logfile.

	Operations:
	1. Start your server, cf. ./cserverd 127.0.0.1:5000

	2. Start the test_server.pl 127.0.0.1 5000 myLog

	The tool writes parts of the result in STDOUT, some in
	the log file (myLog in example).

	The output should in the best of worlds be, something
	like:
	"Im the departing subroutinge, Errors: 0"
	"<nothing printed here>".

	But the log should also be tested.

--------------------------------------------------------------------------------
Detailed Description for test_client and test_server.
