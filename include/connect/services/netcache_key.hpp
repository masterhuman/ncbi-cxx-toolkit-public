#ifndef CONN___NETCACHE_KEY__HPP
#define CONN___NETCACHE_KEY__HPP

/*  $Id$
 * ===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnology Information
 *
 *  This software/database is a "United States Government Work" under the
 *  terms of the United States Copyright Act.  It was written as part of
 *  the author's official duties as a United States Government employee and
 *  thus cannot be copyrighted.  This software/database is freely available
 *  to the public for use. The National Library of Medicine and the U.S.
 *  Government have not placed any restriction on its use or reproduction.
 *
 *  Although all reasonable efforts have been taken to ensure the accuracy
 *  and reliability of the software and data, the NLM and the U.S.
 *  Government do not and cannot warrant the performance or results that
 *  may be obtained by using this software or data. The NLM and the U.S.
 *  Government disclaim all warranties, express or implied, including
 *  warranties of performance, merchantability or fitness for any particular
 *  purpose.
 *
 *  Please cite the author in any work or product based on this material.
 *
 * ===========================================================================
 *
 * Authors:  Anatoliy Kuznetsov, Maxim Didenko
 *
 * File Description:
 *   Net cache client API.
 *
 */

/// @file netcache_client.hpp
/// NetCache client specs.
///

#include <connect/connect_export.h>
//#include <connect/ncbi_connutil.h>
#include <corelib/ncbistl.hpp>

BEGIN_NCBI_SCOPE


/** @addtogroup NetCacheClient
 *
 * @{
 */


/// Meaningful information encoded in the NetCache key
///
///
struct NCBI_XCONNECT_EXPORT CNetCacheKey
{
public:
    CNetCacheKey(unsigned _id, const string& _host, unsigned _port, unsigned ver = 1)
        : id(_id), host(_host), port(_port), version(ver) {}
    /// Create the key out of string
    /// @sa ParseBlobKey()
    explicit CNetCacheKey(const string& key_str);


    operator string() const;

    unsigned     id;        ///< BLOB id
    string       host;      ///< server name
    unsigned     port;      ///< TCP/IP port number
    unsigned     version;   ///< Key version
};



/* @} */


END_NCBI_SCOPE

#endif  /* CONN___NETCACHE_KEY__HPP */
