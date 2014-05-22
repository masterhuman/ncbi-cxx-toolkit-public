/* $Id$
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
 * Author:  .......
 *
 * File Description:
 *   .......
 *
 * Remark:
 *   This code was originally generated by application DATATOOL
 *   using the following specifications:
 *   'macro.asn'.
 */

// standard includes
#include <ncbi_pch.hpp>

// generated includes
#include <objects/macro/Replace_func.hpp>
#include <objects/macro/Simple_replace.hpp>

// generated classes

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

// destructor
CReplace_func::~CReplace_func(void)
{
}


bool s_WholeWordReplaceNocase(string& val, const string& find, const string& repl)
{
    if (find.empty()) {
        return false;
    }
    size_t pos = NStr::Find(val, find, 0, string::npos, NStr::eFirst, NStr::eNocase);
    size_t len = find.length();
    bool rval = false;

    while (pos != string::npos) {
        if ((pos == 0 || !isalpha(val.c_str()[pos - 1]))
            && !isalpha(val.c_str()[pos + len])) {
            if (pos == 0) {
                val = repl + val.substr(len);                
            } else {
                val = val.substr(0, pos) + repl + val.substr(pos + len);
            }
            rval = true;
            pos = NStr::Find(val, find, pos + repl.length(), string::npos, NStr::eFirst, NStr::eNocase);
        } else {
            pos = NStr::Find(val, find, pos + 1, string::npos, NStr::eFirst, NStr::eNocase);
        }
    }
    return rval;
}


bool s_ReplaceNocase(string& val, const string& find, const string& repl)
{
    if (find.empty()) {
        return false;
    }
    size_t pos = NStr::Find(val, find, 0, string::npos, NStr::eFirst, NStr::eNocase);
    size_t len = find.length();
    bool rval = false;

    while (pos != string::npos) {
        if (pos == 0) {
            val = repl + val.substr(len);                
        } else {
            val = val.substr(0, pos) + repl + val.substr(pos + len);
        }
        rval = true;
        pos = NStr::Find(val, find, pos + repl.length(), string::npos, NStr::eFirst, NStr::eNocase);
    }
    return rval;
}


bool CReplace_func::ApplyToString(string& val, CRef<CString_constraint> find) const
{
    bool rval = false;
    if (IsSimple_replace()) {
        rval = GetSimple_replace().ApplyToString(val, find);
    } else if (IsHaem_replace()) {
        string repl = GetHaem_replace();
        rval = s_WholeWordReplaceNocase(val, repl, "heme");
        rval |= s_ReplaceNocase(val, repl, "hem");
    } 
    return rval;
}


END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE

/* Original file checksum: lines: 57, chars: 1729, CRC32: 1d680fe4 */
