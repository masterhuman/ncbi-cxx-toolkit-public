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
 * Author: Frank Ludwig
 *
 * File Description:  
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbifile.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objtools/import/gff_util.hpp>

#include <objtools/import/feat_import_error.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

//  ============================================================================
bool
GffUtil::InitializeFrame(
    const vector<string>& gffColumns,
    std::string& phase)
//  ============================================================================
{
    phase = gffColumns[7];
    vector<string> valid = {".", "0", "1", "2"};  
    auto validIt = find(valid.begin(), valid.end(), phase);
    return (validIt != valid.end());
}

//  ============================================================================
CCdregion::TFrame
GffUtil::PhaseToFrame(
    const std::string& phase)
//  ============================================================================
{
    vector<CCdregion::TFrame> frameValues = {
        CCdregion::eFrame_one, CCdregion::eFrame_two, CCdregion::eFrame_three};

    if (phase == ".") {
        return CCdregion::eFrame_not_set;
    }
    else {
        return frameValues[phase[0] - '0'];
    }
}

END_SCOPE(objects)
END_NCBI_SCOPE
