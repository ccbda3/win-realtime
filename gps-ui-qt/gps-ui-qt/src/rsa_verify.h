/* rsa_verify.h */
#ifndef RSA_VERIFY_H
#define RSA_VERIFY_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Verify an RSA-2048 PKCS#1 v1.5 SHA-256 signature.
 * Returns 1 if valid, 0 if invalid.
 * public_key_der and pubkey_len are ignored (embedded key used).
 * signature: 256 bytes
 * message: data that was signed
 */
int rsa_verify(const uint8_t *public_key_der, size_t pubkey_len,
               const uint8_t *signature,
               const uint8_t *message, size_t message_len);

#ifdef __cplusplus
}
#endif

#endif
