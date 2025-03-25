/* Automatically generated by generate_ssl_debug_helpers.py. DO NOT EDIT. */

/**
 * \file ssl_debug_helpers_generated.c
 *
 * \brief Automatically generated helper functions for debugging
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 *
 */

#include "ssl_misc.h"

#if defined(MBEDTLS_DEBUG_C)

#include "ssl_debug_helpers.h"


const char *mbedtls_ssl_named_group_to_str( uint16_t in )
{
    switch( in )
    {
    case MBEDTLS_SSL_IANA_TLS_GROUP_SECP192K1:
        return "secp192k1";
    case MBEDTLS_SSL_IANA_TLS_GROUP_SECP192R1:
        return "secp192r1";
    case MBEDTLS_SSL_IANA_TLS_GROUP_SECP224K1:
        return "secp224k1";
    case MBEDTLS_SSL_IANA_TLS_GROUP_SECP224R1:
        return "secp224r1";
    case MBEDTLS_SSL_IANA_TLS_GROUP_SECP256K1:
        return "secp256k1";
    case MBEDTLS_SSL_IANA_TLS_GROUP_SECP256R1:
        return "secp256r1";
    case MBEDTLS_SSL_IANA_TLS_GROUP_SECP384R1:
        return "secp384r1";
    case MBEDTLS_SSL_IANA_TLS_GROUP_SECP521R1:
        return "secp521r1";
    case MBEDTLS_SSL_IANA_TLS_GROUP_BP256R1:
        return "bp256r1";
    case MBEDTLS_SSL_IANA_TLS_GROUP_BP384R1:
        return "bp384r1";
    case MBEDTLS_SSL_IANA_TLS_GROUP_BP512R1:
        return "bp512r1";
    case MBEDTLS_SSL_IANA_TLS_GROUP_X25519:
        return "x25519";
    case MBEDTLS_SSL_IANA_TLS_GROUP_X448:
        return "x448";
    case MBEDTLS_SSL_IANA_TLS_GROUP_FFDHE2048:
        return "ffdhe2048";
    case MBEDTLS_SSL_IANA_TLS_GROUP_FFDHE3072:
        return "ffdhe3072";
    case MBEDTLS_SSL_IANA_TLS_GROUP_FFDHE4096:
        return "ffdhe4096";
    case MBEDTLS_SSL_IANA_TLS_GROUP_FFDHE6144:
        return "ffdhe6144";
    case MBEDTLS_SSL_IANA_TLS_GROUP_FFDHE8192:
        return "ffdhe8192";
    };

    return "UNKNOWN";
}
const char *mbedtls_ssl_sig_alg_to_str( uint16_t in )
{
    switch( in )
    {
    case MBEDTLS_TLS1_3_SIG_RSA_PKCS1_SHA256:
        return "rsa_pkcs1_sha256";
    case MBEDTLS_TLS1_3_SIG_RSA_PKCS1_SHA384:
        return "rsa_pkcs1_sha384";
    case MBEDTLS_TLS1_3_SIG_RSA_PKCS1_SHA512:
        return "rsa_pkcs1_sha512";
    case MBEDTLS_TLS1_3_SIG_ECDSA_SECP256R1_SHA256:
        return "ecdsa_secp256r1_sha256";
    case MBEDTLS_TLS1_3_SIG_ECDSA_SECP384R1_SHA384:
        return "ecdsa_secp384r1_sha384";
    case MBEDTLS_TLS1_3_SIG_ECDSA_SECP521R1_SHA512:
        return "ecdsa_secp521r1_sha512";
    case MBEDTLS_TLS1_3_SIG_RSA_PSS_RSAE_SHA256:
        return "rsa_pss_rsae_sha256";
    case MBEDTLS_TLS1_3_SIG_RSA_PSS_RSAE_SHA384:
        return "rsa_pss_rsae_sha384";
    case MBEDTLS_TLS1_3_SIG_RSA_PSS_RSAE_SHA512:
        return "rsa_pss_rsae_sha512";
    case MBEDTLS_TLS1_3_SIG_ED25519:
        return "ed25519";
    case MBEDTLS_TLS1_3_SIG_ED448:
        return "ed448";
    case MBEDTLS_TLS1_3_SIG_RSA_PSS_PSS_SHA256:
        return "rsa_pss_pss_sha256";
    case MBEDTLS_TLS1_3_SIG_RSA_PSS_PSS_SHA384:
        return "rsa_pss_pss_sha384";
    case MBEDTLS_TLS1_3_SIG_RSA_PSS_PSS_SHA512:
        return "rsa_pss_pss_sha512";
    case MBEDTLS_TLS1_3_SIG_RSA_PKCS1_SHA1:
        return "rsa_pkcs1_sha1";
    case MBEDTLS_TLS1_3_SIG_ECDSA_SHA1:
        return "ecdsa_sha1";
    case MBEDTLS_TLS1_3_SIG_NONE:
        return "none";
    };

    return "UNKNOWN";
}
const char *mbedtls_ssl_states_str( mbedtls_ssl_states in )
{
    switch (in) {
        case MBEDTLS_SSL_HELLO_REQUEST:
            return "MBEDTLS_SSL_HELLO_REQUEST";
        case MBEDTLS_SSL_CLIENT_HELLO:
            return "MBEDTLS_SSL_CLIENT_HELLO";
        case MBEDTLS_SSL_SERVER_HELLO:
            return "MBEDTLS_SSL_SERVER_HELLO";
        case MBEDTLS_SSL_SERVER_CERTIFICATE:
            return "MBEDTLS_SSL_SERVER_CERTIFICATE";
        case MBEDTLS_SSL_SERVER_KEY_EXCHANGE:
            return "MBEDTLS_SSL_SERVER_KEY_EXCHANGE";
        case MBEDTLS_SSL_CERTIFICATE_REQUEST:
            return "MBEDTLS_SSL_CERTIFICATE_REQUEST";
        case MBEDTLS_SSL_SERVER_HELLO_DONE:
            return "MBEDTLS_SSL_SERVER_HELLO_DONE";
        case MBEDTLS_SSL_CLIENT_CERTIFICATE:
            return "MBEDTLS_SSL_CLIENT_CERTIFICATE";
        case MBEDTLS_SSL_CLIENT_KEY_EXCHANGE:
            return "MBEDTLS_SSL_CLIENT_KEY_EXCHANGE";
        case MBEDTLS_SSL_CERTIFICATE_VERIFY:
            return "MBEDTLS_SSL_CERTIFICATE_VERIFY";
        case MBEDTLS_SSL_CLIENT_CHANGE_CIPHER_SPEC:
            return "MBEDTLS_SSL_CLIENT_CHANGE_CIPHER_SPEC";
        case MBEDTLS_SSL_CLIENT_FINISHED:
            return "MBEDTLS_SSL_CLIENT_FINISHED";
        case MBEDTLS_SSL_SERVER_CHANGE_CIPHER_SPEC:
            return "MBEDTLS_SSL_SERVER_CHANGE_CIPHER_SPEC";
        case MBEDTLS_SSL_SERVER_FINISHED:
            return "MBEDTLS_SSL_SERVER_FINISHED";
        case MBEDTLS_SSL_FLUSH_BUFFERS:
            return "MBEDTLS_SSL_FLUSH_BUFFERS";
        case MBEDTLS_SSL_HANDSHAKE_WRAPUP:
            return "MBEDTLS_SSL_HANDSHAKE_WRAPUP";
        case MBEDTLS_SSL_NEW_SESSION_TICKET:
            return "MBEDTLS_SSL_NEW_SESSION_TICKET";
        case MBEDTLS_SSL_SERVER_HELLO_VERIFY_REQUEST_SENT:
            return "MBEDTLS_SSL_SERVER_HELLO_VERIFY_REQUEST_SENT";
        case MBEDTLS_SSL_HELLO_RETRY_REQUEST:
            return "MBEDTLS_SSL_HELLO_RETRY_REQUEST";
        case MBEDTLS_SSL_ENCRYPTED_EXTENSIONS:
            return "MBEDTLS_SSL_ENCRYPTED_EXTENSIONS";
        case MBEDTLS_SSL_END_OF_EARLY_DATA:
            return "MBEDTLS_SSL_END_OF_EARLY_DATA";
        case MBEDTLS_SSL_CLIENT_CERTIFICATE_VERIFY:
            return "MBEDTLS_SSL_CLIENT_CERTIFICATE_VERIFY";
        case MBEDTLS_SSL_CLIENT_CCS_AFTER_SERVER_FINISHED:
            return "MBEDTLS_SSL_CLIENT_CCS_AFTER_SERVER_FINISHED";
        case MBEDTLS_SSL_CLIENT_CCS_BEFORE_2ND_CLIENT_HELLO:
            return "MBEDTLS_SSL_CLIENT_CCS_BEFORE_2ND_CLIENT_HELLO";
        case MBEDTLS_SSL_SERVER_CCS_AFTER_SERVER_HELLO:
            return "MBEDTLS_SSL_SERVER_CCS_AFTER_SERVER_HELLO";
        case MBEDTLS_SSL_CLIENT_CCS_AFTER_CLIENT_HELLO:
            return "MBEDTLS_SSL_CLIENT_CCS_AFTER_CLIENT_HELLO";
        case MBEDTLS_SSL_SERVER_CCS_AFTER_HELLO_RETRY_REQUEST:
            return "MBEDTLS_SSL_SERVER_CCS_AFTER_HELLO_RETRY_REQUEST";
        case MBEDTLS_SSL_HANDSHAKE_OVER:
            return "MBEDTLS_SSL_HANDSHAKE_OVER";
        case MBEDTLS_SSL_TLS1_3_NEW_SESSION_TICKET:
            return "MBEDTLS_SSL_TLS1_3_NEW_SESSION_TICKET";
        case MBEDTLS_SSL_TLS1_3_NEW_SESSION_TICKET_FLUSH:
            return "MBEDTLS_SSL_TLS1_3_NEW_SESSION_TICKET_FLUSH";
        default:
            return "UNKNOWN_VALUE";
    }
}

#if defined(MBEDTLS_SSL_EARLY_DATA) && defined(MBEDTLS_SSL_CLI_C)
const char *mbedtls_ssl_early_data_status_str( mbedtls_ssl_early_data_status in )
{
    switch (in) {
        case MBEDTLS_SSL_EARLY_DATA_STATUS_NOT_INDICATED:
            return "MBEDTLS_SSL_EARLY_DATA_STATUS_NOT_INDICATED";
        case MBEDTLS_SSL_EARLY_DATA_STATUS_ACCEPTED:
            return "MBEDTLS_SSL_EARLY_DATA_STATUS_ACCEPTED";
        case MBEDTLS_SSL_EARLY_DATA_STATUS_REJECTED:
            return "MBEDTLS_SSL_EARLY_DATA_STATUS_REJECTED";
        default:
            return "UNKNOWN_VALUE";
    }
}

#endif /* defined(MBEDTLS_SSL_EARLY_DATA) && defined(MBEDTLS_SSL_CLI_C) */
const char *mbedtls_ssl_protocol_version_str( mbedtls_ssl_protocol_version in )
{
    switch (in) {
        case MBEDTLS_SSL_VERSION_UNKNOWN:
            return "MBEDTLS_SSL_VERSION_UNKNOWN";
        case MBEDTLS_SSL_VERSION_TLS1_2:
            return "MBEDTLS_SSL_VERSION_TLS1_2";
        case MBEDTLS_SSL_VERSION_TLS1_3:
            return "MBEDTLS_SSL_VERSION_TLS1_3";
        default:
            return "UNKNOWN_VALUE";
    }
}

const char *mbedtls_tls_prf_types_str( mbedtls_tls_prf_types in )
{
    switch (in) {
        case MBEDTLS_SSL_TLS_PRF_NONE:
            return "MBEDTLS_SSL_TLS_PRF_NONE";
        case MBEDTLS_SSL_TLS_PRF_SHA384:
            return "MBEDTLS_SSL_TLS_PRF_SHA384";
        case MBEDTLS_SSL_TLS_PRF_SHA256:
            return "MBEDTLS_SSL_TLS_PRF_SHA256";
        case MBEDTLS_SSL_HKDF_EXPAND_SHA384:
            return "MBEDTLS_SSL_HKDF_EXPAND_SHA384";
        case MBEDTLS_SSL_HKDF_EXPAND_SHA256:
            return "MBEDTLS_SSL_HKDF_EXPAND_SHA256";
        default:
            return "UNKNOWN_VALUE";
    }
}

const char *mbedtls_ssl_key_export_type_str( mbedtls_ssl_key_export_type in )
{
    switch (in) {
        case MBEDTLS_SSL_KEY_EXPORT_TLS12_MASTER_SECRET:
            return "MBEDTLS_SSL_KEY_EXPORT_TLS12_MASTER_SECRET";
#if defined(MBEDTLS_SSL_PROTO_TLS1_3)
        case MBEDTLS_SSL_KEY_EXPORT_TLS1_3_CLIENT_EARLY_SECRET:
            return "MBEDTLS_SSL_KEY_EXPORT_TLS1_3_CLIENT_EARLY_SECRET";
        case MBEDTLS_SSL_KEY_EXPORT_TLS1_3_EARLY_EXPORTER_SECRET:
            return "MBEDTLS_SSL_KEY_EXPORT_TLS1_3_EARLY_EXPORTER_SECRET";
        case MBEDTLS_SSL_KEY_EXPORT_TLS1_3_CLIENT_HANDSHAKE_TRAFFIC_SECRET:
            return "MBEDTLS_SSL_KEY_EXPORT_TLS1_3_CLIENT_HANDSHAKE_TRAFFIC_SECRET";
        case MBEDTLS_SSL_KEY_EXPORT_TLS1_3_SERVER_HANDSHAKE_TRAFFIC_SECRET:
            return "MBEDTLS_SSL_KEY_EXPORT_TLS1_3_SERVER_HANDSHAKE_TRAFFIC_SECRET";
        case MBEDTLS_SSL_KEY_EXPORT_TLS1_3_CLIENT_APPLICATION_TRAFFIC_SECRET:
            return "MBEDTLS_SSL_KEY_EXPORT_TLS1_3_CLIENT_APPLICATION_TRAFFIC_SECRET";
        case MBEDTLS_SSL_KEY_EXPORT_TLS1_3_SERVER_APPLICATION_TRAFFIC_SECRET:
            return "MBEDTLS_SSL_KEY_EXPORT_TLS1_3_SERVER_APPLICATION_TRAFFIC_SECRET";
#endif
        default:
            return "UNKNOWN_VALUE";
    }
}



#endif /* MBEDTLS_DEBUG_C */
/* End of automatically generated file. */

