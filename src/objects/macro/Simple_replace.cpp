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
#include <objects/macro/Simple_replace.hpp>

// generated classes

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

// destructor
CSimple_replace::~CSimple_replace(void)
{
}


const string sm_WeaselWords[] = {
  "candidate",
  "hypothetical",
  "novel",
  "possible",
  "potential",
  "predicted", 
  "probable", 
  "putative",
  "candidate",  
  "uncharacterized",  
  "unique",
};


bool s_IsWeaselWord(const string& value)
{
    size_t max = sizeof(sm_WeaselWords) / sizeof(const string);

    const string *begin = sm_WeaselWords;
    const string *end = &(sm_WeaselWords[max]);

    if (find(begin, end, value) != end) {
        return true;
    } else {
        return false;
    }
}


bool SkipWeasel(string& str)
{
    if (str.empty()) {
        return false;
    }
  
    vector<string> tokens;
    NStr::Split(str, " ", tokens, 0);
    bool rval = false;
    
    while (tokens.size() > 0 && s_IsWeaselWord(tokens[0])) {
        tokens.erase(tokens.begin());
        rval = true;
    }

    if (rval) {
        str = NStr::Join(tokens, " ");
    }
    return rval;
}


bool CSimple_replace::ApplyToString(string& result, const CMatchString& str,  CRef<CString_constraint> find) const
{
    bool use_putative = false;
    if (IsSetWeasel_to_putative() && GetWeasel_to_putative()) {
        use_putative = str.HasWeasel();
    }

    bool rval = false;
    if (!find) {
        result = GetReplace();
        rval = true;
    } else if (IsSetWhole_string() && GetWhole_string()) {
        if (find->Match(str)) {
            result = GetReplace();
            rval = true;
        }
    } else {
       const string& replace = IsSetReplace() ? GetReplace() : kEmptyStr;
       rval = find->ReplaceStringConstraintPortionInString(result, str, replace);
    }

    if (use_putative) {
        result = "putative " + str;
        rval = true;
    }
    return rval;
}


END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE

/* Original file checksum: lines: 57, chars: 1735, CRC32: 2e3dc355 */
