spiped
------

spiped (pronounced "ess-pipe-dee") is a utility for creating
symmetrically encrypted and authenticated pipes between socket
addresses, so that one may connect to one address (e.g., a UNIX socket
on localhost) and transparently have a connection established to another
address (e.g., a UNIX socket on a different system).  This is similar to
`ssh -L` functionality, but does not use SSH and requires a pre-shared
symmetric key.


Download from www.tarsnap.com/spiped.html
-----------------------------------------

:exclamation:
Official releases on www.tarsnap.com/spiped.html have POSIX-compliant
Makefiles; please use those.

:warning:
The git repository (including the tag-based "release" snapshots on
github) uses BSD Makefiles, which may or may not work on your operating
system.


Building
--------

The official releases should build and install on almost any POSIX-compliant
operating system, using the included Makefiles:

    make BINDIR=/path/to/target/directory install

See the BUILDING file for more details.


Updating build code and releasing
---------------------------------

The POSIX-compatible Makefiles are generated via `make Makefiles` from the
included (far more readable) BSD Makefiles.  To run this target, you will
need to have a BSD `make(1)` utility; NetBSD's `make(1)` is available for many
operating systems as `bmake`.

Release tarballs are generated via `make VERSION=x.y.z publish`, subject
to the same caveat of needing a BSD-compatible make.


More info
---------

For more details about spiped, read the README file.
