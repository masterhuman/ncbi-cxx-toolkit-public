#ifndef UTIL___SMALLDNS__HPP
#define UTIL___SMALLDNS__HPP

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
 * Author:  Anton Golikov
 *
 */

/// @file smalldns.hpp
/// Resolve host name to ip address and back using preset ini-file.
 
#include <corelib/ncbistd.hpp>
#include <map>


/** @addtogroup RegistryDNS
 *
 * @{
 */


BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
///
/// CSmallDNS --
///
/// Resolve host name to ip address and back using preset ini-file.

class CSmallDNS
{
public:
    /// Creates small DNS service from the registry-like file
    /// named by "local_hosts_file" using section "[LOCAL_DNS]".
    CSmallDNS(const string& local_hosts_file = "./hosts.ini");
    
    ~CSmallDNS();
    
    /// Validate if "ip" contains a legal IP address in a dot notation
    /// (a la "255.255.255.255").
    static bool IsValidIP(const string& ip);

    /// Get local (current) host name (uname call).
    static string GetLocalHost(void);
    
    /// Get local (current) host ip address from registry.
    string GetLocalIP(void) const;
    
    /// Given host name "hostname", return its IP address in dot notation.
    /// If the "hostname" is not in the dot notation, then look it up in
    /// the registry. Return empty string if "hostname" cannot be resolved.
    string LocalResolveDNS(const string& hostname) const;

    /// Given ip address in dot notation, return host name by look up in
    /// the registry. Return empty string if "ip" cannot be resolved to hostname.
    string LocalBackResolveDNS(const string& ip) const;

protected:
    static string sm_localHostName;
    map<string, string> m_map;
};


/* @} */


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.4  2003/10/20 21:14:58  ivanov
 * Formal code rearrangement. Added standart footer.
 *
 * ===========================================================================
 */

#endif  /* UTIL___SMALLDNS__HPP */
