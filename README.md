spiped
------

spiped (pronounced "ess-pipe-dee") is a utility for creating
symmetrically encrypted and authenticated pipes between socket
addresses, so that one may connect to one address (e.g., a UNIX socket
on localhost) and transparently have a connection established to another
address (e.g., a UNIX socket on a different system).  This is similar to
'ssh -L' functionality, but does not use SSH and requires a pre-shared
symmetric key.


Building
--------

The Make logic in this tree is for FreeBSD `make`(1).  To create a
tarball containing portable Makefiles, run 'make publish'.

**Ubuntu**: install NetBSD make(1):

    sudo apt-get install bmake

then run:

    bmake publish VERSION=my.number

After that, you can untar the resulting `spiped-my.number.tgz` file and
compile the resulting directory with normal Linux `make`(1).


More info
---------

For more details about spiped, read the README file.
