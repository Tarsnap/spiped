spiped design
=============

Encrypted protocol
------------------

The client and server share a key file with 256 or more bits of entropy.  On
launch, they read the key file and compute
    K = SHA256(key file).

When a connection is established:
C1. The client generates a 256-bit random value nonce_C and sends it.
S1. The server generates a 256-bit random value nonce_S and sends it.

C2. The client receives a 256-bit value nonce_S.
S2. The server receives a 256-bit value nonce_C.

C3/S3. Both parties now compute the 512-bit value
    dk_1 = PBKDF2-SHA256(K, nonce_C || nonce_S, 1)
and parse it as a pair of 256-bit values
    dhmac_C || dhmac_S = dk_1.

C4. The client picks* a value x_C and computes** y_C = 2^x_C mod p, where p is
the Diffie-Hellman "group #14" modulus, and h_C = HMAC-SHA256(dhmac_C, y_C).
The client sends y_C || h_C to the server.
S4. The server receives a 2304-bit value which it parses as y_C || h_C, where
y_C is 2048 bits and h_C is 256 bits; and drops the connection if h_C is not
equal to HMAC-SHA256(dhmac_C, y_C) or y_C >= p.

S5. The server picks* a value x_S and computes** y_S = 2^x_S mod p and
h_S = HMAC-SHA256(dhmac_S, y_S).  The server sends y_S || h_S to the client.
C5. The client receives a 2304-bit value which it parses as y_S || h_S, where
y_S is 2048 bits and h_S is 256 bits; and drops the connection if h_S is not
equal to HMAC-SHA256(dhmac_S, y_S) or y_S >= p.

C6. The client computes** y_SC = y_S^x_C mod p.
S6. The server computes** y_SC = y_C^x_S mod p.
(Note that these two compute values are identical.)

C7/S7. Both parties now compute the 1024-bit value
    dk_2 = PBKDF2-SHA256(K, nonce_C || nonce_S || y_SC, 1)
and parse it as a 4-tuple of 256-bit values
    E_C || H_C || E_S || H_S.

Thereafter, the client and server exchange 1060-byte packets P generated from
plaintext messages M of 1--1024 bytes
    msg_padded = M || ( 0x00 x (1024 - length(M))) || bigendian32(length(M))
    msg_encrypted = AES256-CTR(E, msg_padded, packet#)
    P = msg_encrypted || HMAC-SHA256(H, msg_encrypted || bigendian64(packet#))
where E and H are E_C and H_C or E_S and H_S depending on whether the packet
is being sent by the client or the server, and AES256-CTR is computed with
nonce equal to the packet #, which starts at zero and increments for each
packet sent in the same direction.

* The values x_C, x_S picked must either be 0 (if forward perfect secrecy
is not desired) or have 256 bits of entropy (if forward perfect secrecy is
desired).

** The values y_C, y_S, and y_SC are 2048 bits and big-endian.

Security proof
--------------
1. Under the random oracle model, K has at least 255 bits of entropy (it's a
256-bit hash computed from a value with at least 256 bits of entropy).

2. Provided that at least one party is following the protocol and the key
file has been used for fewer than 2^64 connections, the probability that the
tuple (K, nonce_C, nonce_S) has occurred before is less than 2^(-192).

3. Under the random oracle model, the probability of an attacker without
access to K guessing either of dhmac_C and dhmac_S is less than
    P(attacker guesses K) +
    P(the tuple has been input to the oracle before) +
    P(the attacker directly guesses),
which is less than
    2^(-255) + 2^(-192) + 2^(-255) = 2^(-192) + 2^(-254).

4. Consequently, in order for an attacker to convince a protocol-obeying
party that a tuple (y, h) is legitimate, the attacker must do at least 2^190
expected work (which we consider to be computationally infeasible and do not
consider any further).

5. If one of the parties opts to not have perfect forward secrecy, then the
value y_SC will be equal to 1 and dk_2 will have the same security properties
as dk_1, i.e., it will be computationally infeasible for an attacker without
access to K to compute dk_2.

6. If both parties opt for perfect forward secrecy, an attacker who can
compute y_SC has solved a Diffie-Hellman problem over the 2048-bit group #14,
which is (under the CDH assumption) computationally infeasible.

7. Consequently, if both parties opt for perfect forward secrecy, an attacker
who obtains access to K after the handshake has completed will continue to be
unable to compute dk_2 from information exchanged during the handshake.

8. Under the random oracle model, the packets P are indistinguishable from
random 1060-byte packets; thus no information about the keys used or the
plaintext being transmitted is revealed by post-key-exchange communications.

9. Because the values (msg_encrypted || bigendian(packet#)) are distinct for
each packet, under the random oracle model it is infeasible for an attacker
without access to the value H to generate a packet which will be accepted as
valid.
