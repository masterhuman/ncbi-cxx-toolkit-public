/*
 *  PSA crypto core internal interfaces
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

#ifndef PSA_CRYPTO_CORE_H
#define PSA_CRYPTO_CORE_H

#if !defined(MBEDTLS_CONFIG_FILE)
#include "mbedtls/config.h"
#else
#include MBEDTLS_CONFIG_FILE
#endif

#include "psa/crypto.h"
#include "psa/crypto_se_driver.h"

/** Constant-time buffer comparison
 *
 * \param[in]  a    Left-hand buffer for comparison.
 * \param[in]  b    Right-hand buffer for comparison.
 * \param n         Amount of bytes to compare.
 *
 * \return 0 if the buffer contents are equal, non-zero otherwise
 */
static inline int mbedtls_psa_safer_memcmp(
    const uint8_t *a, const uint8_t *b, size_t n)
{
    size_t i;
    unsigned char diff = 0;

    for (i = 0; i < n; i++) {
        diff |= a[i] ^ b[i];
    }

    return diff;
}

/** The data structure representing a key slot, containing key material
 * and metadata for one key.
 */
typedef struct {
    psa_core_key_attributes_t attr;

    /*
     * Number of locks on the key slot held by the library.
     *
     * This counter is incremented by one each time a library function
     * retrieves through one of the dedicated internal API a pointer to the
     * key slot.
     *
     * This counter is decremented by one each time a library function stops
     * accessing the key slot and states it by calling the
     * psa_unlock_key_slot() API.
     *
     * This counter is used to prevent resetting the key slot while the library
     * may access it. For example, such control is needed in the following
     * scenarios:
     * . In case of key slot starvation, all key slots contain the description
     *   of a key, and the library asks for the description of a persistent
     *   key not present in the key slots, the key slots currently accessed by
     *   the library cannot be reclaimed to free a key slot to load the
     *   persistent key.
     * . In case of a multi-threaded application where one thread asks to close
     *   or purge or destroy a key while it is in used by the library through
     *   another thread.
     */
    size_t lock_count;

    /* Dynamically allocated key data buffer.
     * Format as specified in psa_export_key(). */
    struct key_data {
        uint8_t *data;
        size_t bytes;
    } key;
} psa_key_slot_t;

/* A mask of key attribute flags used only internally.
 * Currently there aren't any. */
#define PSA_KA_MASK_INTERNAL_ONLY (     \
        0)

/** Test whether a key slot is occupied.
 *
 * A key slot is occupied iff the key type is nonzero. This works because
 * no valid key can have 0 as its key type.
 *
 * \param[in] slot      The key slot to test.
 *
 * \return 1 if the slot is occupied, 0 otherwise.
 */
static inline int psa_is_key_slot_occupied(const psa_key_slot_t *slot)
{
    return slot->attr.type != 0;
}

/** Test whether a key slot is locked.
 *
 * A key slot is locked iff its lock counter is strictly greater than 0.
 *
 * \param[in] slot  The key slot to test.
 *
 * \return 1 if the slot is locked, 0 otherwise.
 */
static inline int psa_is_key_slot_locked(const psa_key_slot_t *slot)
{
    return slot->lock_count > 0;
}

/** Retrieve flags from psa_key_slot_t::attr::core::flags.
 *
 * \param[in] slot      The key slot to query.
 * \param mask          The mask of bits to extract.
 *
 * \return The key attribute flags in the given slot,
 *         bitwise-anded with \p mask.
 */
static inline uint16_t psa_key_slot_get_flags(const psa_key_slot_t *slot,
                                              uint16_t mask)
{
    return slot->attr.flags & mask;
}

/** Set flags in psa_key_slot_t::attr::core::flags.
 *
 * \param[in,out] slot  The key slot to modify.
 * \param mask          The mask of bits to modify.
 * \param value         The new value of the selected bits.
 */
static inline void psa_key_slot_set_flags(psa_key_slot_t *slot,
                                          uint16_t mask,
                                          uint16_t value)
{
    slot->attr.flags = ((~mask & slot->attr.flags) |
                        (mask & value));
}

/** Turn on flags in psa_key_slot_t::attr::core::flags.
 *
 * \param[in,out] slot  The key slot to modify.
 * \param mask          The mask of bits to set.
 */
static inline void psa_key_slot_set_bits_in_flags(psa_key_slot_t *slot,
                                                  uint16_t mask)
{
    slot->attr.flags |= mask;
}

/** Turn off flags in psa_key_slot_t::attr::core::flags.
 *
 * \param[in,out] slot  The key slot to modify.
 * \param mask          The mask of bits to clear.
 */
static inline void psa_key_slot_clear_bits(psa_key_slot_t *slot,
                                           uint16_t mask)
{
    slot->attr.flags &= ~mask;
}

#if defined(MBEDTLS_PSA_CRYPTO_SE_C)
/** Get the SE slot number of a key from the key slot storing its description.
 *
 * \param[in]  slot  The key slot to query. This must be a key slot storing
 *                   the description of a key of a dynamically registered
 *                   secure element, otherwise the behaviour is undefined.
 */
static inline psa_key_slot_number_t psa_key_slot_get_slot_number(
    const psa_key_slot_t *slot)
{
    return *((psa_key_slot_number_t *) (slot->key.data));
}
#endif

/** Completely wipe a slot in memory, including its policy.
 *
 * Persistent storage is not affected.
 *
 * \param[in,out] slot  The key slot to wipe.
 *
 * \retval #PSA_SUCCESS
 *         Success. This includes the case of a key slot that was
 *         already fully wiped.
 * \retval #PSA_ERROR_CORRUPTION_DETECTED \emptydescription
 */
psa_status_t psa_wipe_key_slot(psa_key_slot_t *slot);

/** Try to allocate a buffer to an empty key slot.
 *
 * \param[in,out] slot          Key slot to attach buffer to.
 * \param[in] buffer_length     Requested size of the buffer.
 *
 * \retval #PSA_SUCCESS
 *         The buffer has been successfully allocated.
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY
 *         Not enough memory was available for allocation.
 * \retval #PSA_ERROR_ALREADY_EXISTS
 *         Trying to allocate a buffer to a non-empty key slot.
 */
psa_status_t psa_allocate_buffer_to_slot(psa_key_slot_t *slot,
                                         size_t buffer_length);

/** Wipe key data from a slot. Preserves metadata such as the policy. */
psa_status_t psa_remove_key_data_from_memory(psa_key_slot_t *slot);

/** Copy key data (in export format) into an empty key slot.
 *
 * This function assumes that the slot does not contain
 * any key material yet. On failure, the slot content is unchanged.
 *
 * \param[in,out] slot          Key slot to copy the key into.
 * \param[in] data              Buffer containing the key material.
 * \param data_length           Size of the key buffer.
 *
 * \retval #PSA_SUCCESS
 *         The key has been copied successfully.
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY
 *         Not enough memory was available for allocation of the
 *         copy buffer.
 * \retval #PSA_ERROR_ALREADY_EXISTS
 *         There was other key material already present in the slot.
 */
psa_status_t psa_copy_key_material_into_slot(psa_key_slot_t *slot,
                                             const uint8_t *data,
                                             size_t data_length);

/** Convert an Mbed TLS error code to a PSA error code
 *
 * \note This function is provided solely for the convenience of
 *       Mbed TLS and may be removed at any time without notice.
 *
 * \param ret           An Mbed TLS-thrown error code
 *
 * \return              The corresponding PSA error code
 */
psa_status_t mbedtls_to_psa_error(int ret);

/** Import a key in binary format.
 *
 * \note The signature of this function is that of a PSA driver
 *       import_key entry point. This function behaves as an import_key
 *       entry point as defined in the PSA driver interface specification for
 *       transparent drivers.
 *
 * \param[in]  attributes       The attributes for the key to import.
 * \param[in]  data             The buffer containing the key data in import
 *                              format.
 * \param[in]  data_length      Size of the \p data buffer in bytes.
 * \param[out] key_buffer       The buffer to contain the key data in output
 *                              format upon successful return.
 * \param[in]  key_buffer_size  Size of the \p key_buffer buffer in bytes. This
 *                              size is greater or equal to \p data_length.
 * \param[out] key_buffer_length  The length of the data written in \p
 *                                key_buffer in bytes.
 * \param[out] bits             The key size in number of bits.
 *
 * \retval #PSA_SUCCESS  The key was imported successfully.
 * \retval #PSA_ERROR_INVALID_ARGUMENT
 *         The key data is not correctly formatted.
 * \retval #PSA_ERROR_NOT_SUPPORTED \emptydescription
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED \emptydescription
 */
psa_status_t psa_import_key_into_slot(
    const psa_key_attributes_t *attributes,
    const uint8_t *data, size_t data_length,
    uint8_t *key_buffer, size_t key_buffer_size,
    size_t *key_buffer_length, size_t *bits);

/** Export a key in binary format
 *
 * \note The signature of this function is that of a PSA driver export_key
 *       entry point. This function behaves as an export_key entry point as
 *       defined in the PSA driver interface specification.
 *
 * \param[in]  attributes       The attributes for the key to export.
 * \param[in]  key_buffer       Material or context of the key to export.
 * \param[in]  key_buffer_size  Size of the \p key_buffer buffer in bytes.
 * \param[out] data             Buffer where the key data is to be written.
 * \param[in]  data_size        Size of the \p data buffer in bytes.
 * \param[out] data_length      On success, the number of bytes written in
 *                              \p data
 *
 * \retval #PSA_SUCCESS  The key was exported successfully.
 * \retval #PSA_ERROR_NOT_SUPPORTED \emptydescription
 * \retval #PSA_ERROR_COMMUNICATION_FAILURE \emptydescription
 * \retval #PSA_ERROR_HARDWARE_FAILURE \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED \emptydescription
 * \retval #PSA_ERROR_STORAGE_FAILURE \emptydescription
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY \emptydescription
 */
psa_status_t psa_export_key_internal(
    const psa_key_attributes_t *attributes,
    const uint8_t *key_buffer, size_t key_buffer_size,
    uint8_t *data, size_t data_size, size_t *data_length);

/** Export a public key or the public part of a key pair in binary format.
 *
 * \note The signature of this function is that of a PSA driver
 *       export_public_key entry point. This function behaves as an
 *       export_public_key entry point as defined in the PSA driver interface
 *       specification.
 *
 * \param[in]  attributes       The attributes for the key to export.
 * \param[in]  key_buffer       Material or context of the key to export.
 * \param[in]  key_buffer_size  Size of the \p key_buffer buffer in bytes.
 * \param[out] data             Buffer where the key data is to be written.
 * \param[in]  data_size        Size of the \p data buffer in bytes.
 * \param[out] data_length      On success, the number of bytes written in
 *                              \p data
 *
 * \retval #PSA_SUCCESS  The public key was exported successfully.
 * \retval #PSA_ERROR_NOT_SUPPORTED \emptydescription
 * \retval #PSA_ERROR_COMMUNICATION_FAILURE \emptydescription
 * \retval #PSA_ERROR_HARDWARE_FAILURE \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED \emptydescription
 * \retval #PSA_ERROR_STORAGE_FAILURE \emptydescription
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY \emptydescription
 */
psa_status_t psa_export_public_key_internal(
    const psa_key_attributes_t *attributes,
    const uint8_t *key_buffer, size_t key_buffer_size,
    uint8_t *data, size_t data_size, size_t *data_length);

/**
 * \brief Generate a key.
 *
 * \note The signature of the function is that of a PSA driver generate_key
 *       entry point.
 *
 * \param[in]  attributes         The attributes for the key to generate.
 * \param[out] key_buffer         Buffer where the key data is to be written.
 * \param[in]  key_buffer_size    Size of \p key_buffer in bytes.
 * \param[out] key_buffer_length  On success, the number of bytes written in
 *                                \p key_buffer.
 *
 * \retval #PSA_SUCCESS
 *         The key was generated successfully.
 * \retval #PSA_ERROR_INVALID_ARGUMENT \emptydescription
 * \retval #PSA_ERROR_NOT_SUPPORTED
 *         Key size in bits or type not supported.
 * \retval #PSA_ERROR_BUFFER_TOO_SMALL
 *         The size of \p key_buffer is too small.
 */
psa_status_t psa_generate_key_internal(const psa_key_attributes_t *attributes,
                                       uint8_t *key_buffer,
                                       size_t key_buffer_size,
                                       size_t *key_buffer_length);

/** Sign a message with a private key. For hash-and-sign algorithms,
 *  this includes the hashing step.
 *
 * \note The signature of this function is that of a PSA driver
 *       sign_message entry point. This function behaves as a sign_message
 *       entry point as defined in the PSA driver interface specification for
 *       transparent drivers.
 *
 * \note This function will call the driver for psa_sign_hash
 *       and go through driver dispatch again.
 *
 * \param[in]  attributes       The attributes of the key to use for the
 *                              operation.
 * \param[in]  key_buffer       The buffer containing the key context.
 * \param[in]  key_buffer_size  Size of the \p key_buffer buffer in bytes.
 * \param[in]  alg              A signature algorithm that is compatible with
 *                              the type of the key.
 * \param[in]  input            The input message to sign.
 * \param[in]  input_length     Size of the \p input buffer in bytes.
 * \param[out] signature        Buffer where the signature is to be written.
 * \param[in]  signature_size   Size of the \p signature buffer in bytes.
 * \param[out] signature_length On success, the number of bytes
 *                              that make up the returned signature value.
 *
 * \retval #PSA_SUCCESS \emptydescription
 * \retval #PSA_ERROR_BUFFER_TOO_SMALL
 *         The size of the \p signature buffer is too small. You can
 *         determine a sufficient buffer size by calling
 *         #PSA_SIGN_OUTPUT_SIZE(\c key_type, \c key_bits, \p alg)
 *         where \c key_type and \c key_bits are the type and bit-size
 *         respectively of the key.
 * \retval #PSA_ERROR_NOT_SUPPORTED \emptydescription
 * \retval #PSA_ERROR_INVALID_ARGUMENT \emptydescription
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED \emptydescription
 * \retval #PSA_ERROR_INSUFFICIENT_ENTROPY \emptydescription
 */
psa_status_t psa_sign_message_builtin(
    const psa_key_attributes_t *attributes,
    const uint8_t *key_buffer, size_t key_buffer_size,
    psa_algorithm_t alg, const uint8_t *input, size_t input_length,
    uint8_t *signature, size_t signature_size, size_t *signature_length);

/** Verify the signature of a message with a public key, using
 *  a hash-and-sign verification algorithm.
 *
 * \note The signature of this function is that of a PSA driver
 *       verify_message entry point. This function behaves as a verify_message
 *       entry point as defined in the PSA driver interface specification for
 *       transparent drivers.
 *
 * \note This function will call the driver for psa_verify_hash
 *       and go through driver dispatch again.
 *
 * \param[in]  attributes       The attributes of the key to use for the
 *                              operation.
 * \param[in]  key_buffer       The buffer containing the key context.
 * \param[in]  key_buffer_size  Size of the \p key_buffer buffer in bytes.
 * \param[in]  alg              A signature algorithm that is compatible with
 *                              the type of the key.
 * \param[in]  input            The message whose signature is to be verified.
 * \param[in]  input_length     Size of the \p input buffer in bytes.
 * \param[in]  signature        Buffer containing the signature to verify.
 * \param[in]  signature_length Size of the \p signature buffer in bytes.
 *
 * \retval #PSA_SUCCESS
 *         The signature is valid.
 * \retval #PSA_ERROR_INVALID_SIGNATURE
 *         The calculation was performed successfully, but the passed
 *         signature is not a valid signature.
 * \retval #PSA_ERROR_NOT_SUPPORTED \emptydescription
 * \retval #PSA_ERROR_INVALID_ARGUMENT \emptydescription
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY \emptydescription
 */
psa_status_t psa_verify_message_builtin(
    const psa_key_attributes_t *attributes,
    const uint8_t *key_buffer, size_t key_buffer_size,
    psa_algorithm_t alg, const uint8_t *input, size_t input_length,
    const uint8_t *signature, size_t signature_length);

/** Sign an already-calculated hash with a private key.
 *
 * \note The signature of this function is that of a PSA driver
 *       sign_hash entry point. This function behaves as a sign_hash
 *       entry point as defined in the PSA driver interface specification for
 *       transparent drivers.
 *
 * \param[in]  attributes       The attributes of the key to use for the
 *                              operation.
 * \param[in]  key_buffer       The buffer containing the key context.
 * \param[in]  key_buffer_size  Size of the \p key_buffer buffer in bytes.
 * \param[in]  alg              A signature algorithm that is compatible with
 *                              the type of the key.
 * \param[in]  hash             The hash or message to sign.
 * \param[in]  hash_length      Size of the \p hash buffer in bytes.
 * \param[out] signature        Buffer where the signature is to be written.
 * \param[in]  signature_size   Size of the \p signature buffer in bytes.
 * \param[out] signature_length On success, the number of bytes
 *                              that make up the returned signature value.
 *
 * \retval #PSA_SUCCESS \emptydescription
 * \retval #PSA_ERROR_BUFFER_TOO_SMALL
 *         The size of the \p signature buffer is too small. You can
 *         determine a sufficient buffer size by calling
 *         #PSA_SIGN_OUTPUT_SIZE(\c key_type, \c key_bits, \p alg)
 *         where \c key_type and \c key_bits are the type and bit-size
 *         respectively of the key.
 * \retval #PSA_ERROR_NOT_SUPPORTED \emptydescription
 * \retval #PSA_ERROR_INVALID_ARGUMENT \emptydescription
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY \emptydescription
 * \retval #PSA_ERROR_CORRUPTION_DETECTED \emptydescription
 * \retval #PSA_ERROR_INSUFFICIENT_ENTROPY \emptydescription
 */
psa_status_t psa_sign_hash_builtin(
    const psa_key_attributes_t *attributes,
    const uint8_t *key_buffer, size_t key_buffer_size,
    psa_algorithm_t alg, const uint8_t *hash, size_t hash_length,
    uint8_t *signature, size_t signature_size, size_t *signature_length);

/**
 * \brief Verify the signature a hash or short message using a public key.
 *
 * \note The signature of this function is that of a PSA driver
 *       verify_hash entry point. This function behaves as a verify_hash
 *       entry point as defined in the PSA driver interface specification for
 *       transparent drivers.
 *
 * \param[in]  attributes       The attributes of the key to use for the
 *                              operation.
 * \param[in]  key_buffer       The buffer containing the key context.
 * \param[in]  key_buffer_size  Size of the \p key_buffer buffer in bytes.
 * \param[in]  alg              A signature algorithm that is compatible with
 *                              the type of the key.
 * \param[in]  hash             The hash or message whose signature is to be
 *                              verified.
 * \param[in]  hash_length      Size of the \p hash buffer in bytes.
 * \param[in]  signature        Buffer containing the signature to verify.
 * \param[in]  signature_length Size of the \p signature buffer in bytes.
 *
 * \retval #PSA_SUCCESS
 *         The signature is valid.
 * \retval #PSA_ERROR_INVALID_SIGNATURE
 *         The calculation was performed successfully, but the passed
 *         signature is not a valid signature.
 * \retval #PSA_ERROR_NOT_SUPPORTED \emptydescription
 * \retval #PSA_ERROR_INVALID_ARGUMENT \emptydescription
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY \emptydescription
 */
psa_status_t psa_verify_hash_builtin(
    const psa_key_attributes_t *attributes,
    const uint8_t *key_buffer, size_t key_buffer_size,
    psa_algorithm_t alg, const uint8_t *hash, size_t hash_length,
    const uint8_t *signature, size_t signature_length);

typedef struct psa_crypto_local_input_s {
    uint8_t *buffer;
    size_t length;
} psa_crypto_local_input_t;

#define PSA_CRYPTO_LOCAL_INPUT_INIT ((psa_crypto_local_input_t) { NULL, 0 })

/** Allocate a local copy of an input buffer and copy the contents into it.
 *
 * \param[in] input             Pointer to input buffer.
 * \param[in] input_len         Length of the input buffer.
 * \param[out] local_input      Pointer to a psa_crypto_local_input_t struct
 *                              containing a local input copy.
 * \return                      #PSA_SUCCESS, if the buffer was successfully
 *                              copied.
 * \return                      #PSA_ERROR_INSUFFICIENT_MEMORY, if a copy of
 *                              the buffer cannot be allocated.
 */
psa_status_t psa_crypto_local_input_alloc(const uint8_t *input, size_t input_len,
                                          psa_crypto_local_input_t *local_input);

/** Free a local copy of an input buffer.
 *
 * \param[in] local_input       Pointer to a psa_crypto_local_input_t struct
 *                              populated by a previous call to
 *                              psa_crypto_local_input_alloc().
 */
void psa_crypto_local_input_free(psa_crypto_local_input_t *local_input);

typedef struct psa_crypto_local_output_s {
    uint8_t *original;
    uint8_t *buffer;
    size_t length;
} psa_crypto_local_output_t;

#define PSA_CRYPTO_LOCAL_OUTPUT_INIT ((psa_crypto_local_output_t) { NULL, NULL, 0 })

/** Allocate a local copy of an output buffer.
 *
 * \note                        This does not copy any data from the original
 *                              output buffer but only allocates a buffer
 *                              whose contents will be copied back to the
 *                              original in a future call to
 *                              psa_crypto_local_output_free().
 *
 * \param[in] output            Pointer to output buffer.
 * \param[in] output_len        Length of the output buffer.
 * \param[out] local_output     Pointer to a psa_crypto_local_output_t struct to
 *                              populate with the local output copy.
 * \return                      #PSA_SUCCESS, if the buffer was successfully
 *                              copied.
 * \return                      #PSA_ERROR_INSUFFICIENT_MEMORY, if a copy of
 *                              the buffer cannot be allocated.
 */
psa_status_t psa_crypto_local_output_alloc(uint8_t *output, size_t output_len,
                                           psa_crypto_local_output_t *local_output);

/** Copy from a local copy of an output buffer back to the original, then
 *  free the local copy.
 *
 * \param[in] local_output      Pointer to a psa_crypto_local_output_t struct
 *                              populated by a previous call to
 *                              psa_crypto_local_output_alloc().
 * \return                      #PSA_SUCCESS, if the local output was
 *                              successfully copied back to the original.
 * \return                      #PSA_ERROR_CORRUPTION_DETECTED, if the output
 *                              could not be copied back to the original.
 */
psa_status_t psa_crypto_local_output_free(psa_crypto_local_output_t *local_output);

#endif /* PSA_CRYPTO_CORE_H */
