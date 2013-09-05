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
 * Authors:  Frank Ludwig
 *
 * File Description:  Common file reader utility functions.
 *
 */

#ifndef OBJTOOLS_READERS___READ_UTIL__HPP
#define OBJTOOLS_READERS___READ_UTIL__HPP

#include <corelib/ncbistd.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects) // namespace ncbi::objects::

//  ============================================================================
/// Common file reader utility functions.
///
class NCBI_XOBJREAD_EXPORT CReadUtil
//  ============================================================================
{
public:
    /// Tokenize a given string, respecting quoted substrings an atomic units.
    ///
    static void Tokenize(
        const string& instr,
        const string& delim,
        vector< string >& tokens);

    /// Convert a raw ID string to a Seq-id, based in given customization flags.
    /// Recognized flags are:
    ///   CReaderBase::fAllIdsAsLocal, CReaderBase::fNumericalIdsAsLocal
    /// By default, numerical IDs below 500 are recognized as local IDs, and 500
    /// and above are considered GI numbers.
    ///
    static CRef<CSeq_id> AsSeqId(
        const string& rawId,
        unsigned int flags =0,
        bool localInts = true);

    static const TGi kMinNumericGi;

};

END_SCOPE(objects)
END_NCBI_SCOPE

#endif  // OBJTOOLS_READERS___READ_UTIL__HPP
