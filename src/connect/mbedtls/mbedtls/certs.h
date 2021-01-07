/**
 * \file certs.h
 *
 * \brief Sample certificates and DHM parameters for testing
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 *
 *  This file is provided under the Apache License 2.0, or the
 *  GNU General Public License v2.0 or later.
 *
 *  **********
 *  Apache License 2.0:
 *
 *  Licensed under the Apache License, Version 2.0 (the "License"); you may
 *  not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 *  WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *  **********
 *
 *  **********
 *  GNU General Public License v2.0 or later:
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 *  **********
 */
#ifndef MBEDTLS_CERTS_H
#define MBEDTLS_CERTS_H

#if !defined(MBEDTLS_CONFIG_FILE)
#include "config.h"
#else
#include MBEDTLS_CONFIG_FILE
#endif

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(MBEDTLS_PEM_PARSE_C)
/* Concatenation of all CA certificates in PEM format if available */
extern const char   mbedtls_test_cas_pem[];
extern const size_t mbedtls_test_cas_pem_len;
#endif

/* List of all CA certificates, terminated by NULL */
extern const char * mbedtls_test_cas[];
extern const size_t mbedtls_test_cas_len[];

/*
 * Convenience for users who just want a certificate:
 * RSA by default, or ECDSA if RSA is not available
 */
extern const char * mbedtls_test_ca_crt;
extern const size_t mbedtls_test_ca_crt_len;
extern const char * mbedtls_test_ca_key;
extern const size_t mbedtls_test_ca_key_len;
extern const char * mbedtls_test_ca_pwd;
extern const size_t mbedtls_test_ca_pwd_len;
extern const char * mbedtls_test_srv_crt;
extern const size_t mbedtls_test_srv_crt_len;
extern const char * mbedtls_test_srv_key;
extern const size_t mbedtls_test_srv_key_len;
extern const char * mbedtls_test_cli_crt;
extern const size_t mbedtls_test_cli_crt_len;
extern const char * mbedtls_test_cli_key;
extern const size_t mbedtls_test_cli_key_len;

#if defined(MBEDTLS_ECDSA_C)
extern const char   mbedtls_test_ca_crt_ec[];
extern const size_t mbedtls_test_ca_crt_ec_len;
extern const char   mbedtls_test_ca_key_ec[];
extern const size_t mbedtls_test_ca_key_ec_len;
extern const char   mbedtls_test_ca_pwd_ec[];
extern const size_t mbedtls_test_ca_pwd_ec_len;
extern const char   mbedtls_test_srv_crt_ec[];
extern const size_t mbedtls_test_srv_crt_ec_len;
extern const char   mbedtls_test_srv_key_ec[];
extern const size_t mbedtls_test_srv_key_ec_len;
extern const char   mbedtls_test_cli_crt_ec[];
extern const size_t mbedtls_test_cli_crt_ec_len;
extern const char   mbedtls_test_cli_key_ec[];
extern const size_t mbedtls_test_cli_key_ec_len;
#endif

#if defined(MBEDTLS_RSA_C)
extern const char   mbedtls_test_ca_crt_rsa[];
extern const size_t mbedtls_test_ca_crt_rsa_len;
extern const char   mbedtls_test_ca_key_rsa[];
extern const size_t mbedtls_test_ca_key_rsa_len;
extern const char   mbedtls_test_ca_pwd_rsa[];
extern const size_t mbedtls_test_ca_pwd_rsa_len;
extern const char   mbedtls_test_srv_crt_rsa[];
extern const size_t mbedtls_test_srv_crt_rsa_len;
extern const char   mbedtls_test_srv_key_rsa[];
extern const size_t mbedtls_test_srv_key_rsa_len;
extern const char   mbedtls_test_cli_crt_rsa[];
extern const size_t mbedtls_test_cli_crt_rsa_len;
extern const char   mbedtls_test_cli_key_rsa[];
extern const size_t mbedtls_test_cli_key_rsa_len;
#endif

#ifdef __cplusplus
}
#endif

#endif /* certs.h */
