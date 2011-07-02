#ifndef _PCRYPT_H_
#define _PCRYPT_H_

#include <stdint.h>
#include <unistd.h>

#include "crypto_dh.h"

struct proto_keys;

/* Size of nonce. */
#define PCRYPT_NONCE_LEN 32

/* Size of temporary MAC keys used for Diffie-Hellman parameters. */
#define PCRYPT_DHMAC_LEN 32

/* Size of private Diffie-Hellman value. */
#define PCRYPT_X_LEN CRYPTO_DH_PRIVLEN

/* Size of MACed Diffie-Hellman parameter. */
#define PCRYPT_YH_LEN (CRYPTO_DH_PUBLEN + 32)

/**
 * pcrypt_dhmac(K, nonce_l, nonce_r, dhmac_l, dhmac_r, decr):
 * Using the key file hash ${K}, and the local and remote nonces ${nonce_l}
 * and ${nonce_r}, compute the local and remote diffie-hellman parameter MAC
 * keys ${dhmac_l} and ${dhmac_r}.  If ${decr} is non-zero, "local" == "S"
 * and "remote" == "C"; otherwise the assignments are opposite.
 */
void pcrypt_dhmac(const uint8_t[32],
    const uint8_t[PCRYPT_NONCE_LEN], const uint8_t[PCRYPT_NONCE_LEN],
    uint8_t[PCRYPT_DHMAC_LEN], uint8_t[PCRYPT_DHMAC_LEN], int);

/**
 * pcrypt_dh_validate(yh_r, dhmac_r):
 * Return non-zero if the value ${yh_r} received from the remote party is not
 * correctly MACed using the diffie-hellman parameter MAC key ${dhmac_r}, or
 * if the included y value is >= the diffie-hellman group modulus.
 */
int pcrypt_dh_validate(const uint8_t[PCRYPT_YH_LEN],
    const uint8_t[PCRYPT_DHMAC_LEN]);

/**
 * pcrypt_dh_generate(yh_l, x, dhmac_l, nofps):
 * Using the MAC key ${dhmac_l}, generate the MACed diffie-hellman handshake
 * parameter ${yh_l}.  Store the diffie-hellman private value in ${x}.  If
 * ${nofps} is non-zero, skip diffie-hellman generation and use y = 1.
 */
int pcrypt_dh_generate(uint8_t[PCRYPT_YH_LEN], uint8_t[PCRYPT_X_LEN],
    const uint8_t[PCRYPT_DHMAC_LEN], int);

/**
 * pcrypt_mkkeys(K, nonce_l, nonce_r, yh_r, x, nofps, decr, eh_c, eh_s):
 * Using the key file hash ${K}, the local and remote nonces ${nonce_l} and
 * ${nonce_r}, the remote MACed diffie-hellman handshake paramter ${yh_r},
 * and the local diffie-hellman secret ${x}, generate the keys ${eh_c} and
 * ${eh_s}.  If ${nofps} is non-zero, we are performing weak handshaking and
 * y_SC is set to 1 rather than being computed.  If ${decr} is non-zero,
 * "local" == "S" and "remote" == "C"; otherwise the assignments are opposite.
 */
int pcrypt_mkkeys(const uint8_t[32],
    const uint8_t[PCRYPT_NONCE_LEN], const uint8_t[PCRYPT_NONCE_LEN],
    const uint8_t[PCRYPT_YH_LEN], const uint8_t[PCRYPT_X_LEN], int, int,
    struct proto_keys **, struct proto_keys **);

/* Maximum size of an unencrypted packet. */
#define PCRYPT_MAXDSZ 1024

/* Size of an encrypted packet. */
#define PCRYPT_ESZ (PCRYPT_MAXDSZ + 4 /* len */ + 32 /* hmac */)

/**
 * pcrypt_enc(ibuf, len, obuf, k):
 * Encrypt ${len} bytes from ${ibuf} into PCRYPT_ESZ bytes using the keys in
 * ${k}, and write the result into ${obuf}.
 */
void pcrypt_enc(uint8_t *, size_t, uint8_t[PCRYPT_ESZ], struct proto_keys *);

/**
 * pcrypt_dec(ibuf, obuf, k):
 * Decrypt PCRYPT_ESZ bytes from ${ibuf} using the keys in ${k}.  If the data
 * is valid, write it into ${obuf} and return the length; otherwise, return
 * -1.
 */
ssize_t pcrypt_dec(uint8_t[PCRYPT_ESZ], uint8_t *, struct proto_keys *);

/**
 * pcrypt_free(k):
 * Free the protocol key structure ${k}.
 */
void pcrypt_free(struct proto_keys *);

#endif /* !_PCRYPT_H_ */
