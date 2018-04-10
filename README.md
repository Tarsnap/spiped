spiped
======

Official signed releases are published at:
> www.tarsnap.com/spiped.html


**`spiped`** (pronounced "ess-pipe-dee") is a utility for creating
symmetrically encrypted and authenticated pipes between socket addresses, so
that one may connect to one address (e.g., a UNIX socket on localhost) and
transparently have a connection established to another address (e.g., a UNIX
socket on a different system).  This is similar to `ssh -L` functionality, but
does not use SSH and requires a pre-shared symmetric key.

**`spipe`** (pronounced "ess-pipe") is a utility which acts as an spiped
protocol client (i.e., connects to an spiped daemon), taking input from the
standard input and writing data read back to the standard output.

Note that spiped:

1. Requires a strong key file: The file specified via the `-k` option should
   have at least 256 bits of entropy.  (`dd if=/dev/urandom bs=32 count=1` is
   your friend.)

2. Requires strong entropy from `/dev/urandom`.  (Make sure your kernel's
   random number generator is seeded at boot time!)

3. Does not provide any protection against information leakage via packet
   timing: Running telnet over spiped will protect a password from being
   directly read from the network, but will not obscure the typing rhythm.

4. Can significantly increase bandwidth usage for interactive sessions: It
   sends data in packets of 1024 bytes, and pads smaller messages up to this
   length, so a 1 byte write could be expanded to 1024 bytes if it cannot be
   coalesced with adjacent bytes.

5. Uses a symmetric key -- so anyone who can connect to an spiped "server" is
   also able to impersonate it.

Example usage
-------------

Examples of spiped protecting SMTP and SSH are given on:
> https://www.tarsnap.com/spiped.html

For a detailed list of the command-line options to spiped and spipe, see the
man pages.


Security requirements
---------------------

The user is responsible for ensuring that:

1. The key file contains 256 or more bits of entropy.

2. The same key file is not used for more than 2^64 connections.

3. Any individual connection does not transmit more than 2^64 bytes.


Building
--------

The official releases should build and install on almost any POSIX-compliant
operating system, using the included Makefiles:

    make BINDIR=/path/to/target/directory install

See the [BUILDING](BUILDING) file for more details (e.g. how to install man pages).


Testing
-------

A small test suite can be run with:

    make test


Code layout
-----------

```
spiped/*        -- Code specific to the spiped utility.
  main.c        -- Command-line parsing, initialization, and event loop.
  dispatch.c    -- Accepts connections and hands them off to protocol code.
spipe/*	        -- Code specific to the spipe utility.
  main.c        -- Command-line parsing, initialization, and event loop.
  pushbits.c    -- Copies data between standard input/output and a socket.
proto/*	        -- Implements the spiped protocol.
  _conn.c       -- Manages the lifecycle of a connection.
  _handshake.c  -- Performs the handshaking portion of the protocol.
  _pipe.c       -- Performs the data-shuttling portion of the protocol.
  _crypt.c      -- Does the cryptographic bits needed by _handshake and _pipe.
lib/dnsthread   -- Spawns a thread for background DNS (re)resolution.
libcperciva/*   -- Library code from libcperciva
```

More info
---------

For more details about spiped, read the [DESIGN.md](DESIGN.md) file.
