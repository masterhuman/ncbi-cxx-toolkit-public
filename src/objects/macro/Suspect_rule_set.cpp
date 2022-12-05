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
#include <objects/macro/Suspect_rule_set.hpp>


#include <util/multipattern_search.hpp>

// generated classes

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

// constructor
CSuspect_rule_set::CSuspect_rule_set(void)
{
}
// destructor
CSuspect_rule_set::~CSuspect_rule_set(void)
{
}


void CSuspect_rule_set::Screen(const char* input, char* output) const
{
    const auto& callback = [&](size_t n) { output[n] = 1; };
    if (m_Precompiled_FSM) {
        CMultipatternSearch::Search(input, *m_Precompiled_FSM, callback);
        return;
    }

    if (!m_FSM) {
        m_FSM.reset(new CMultipatternSearch);
        vector<string> patterns;
        for (auto rule: Get()) {
            const CSearch_func& find = rule->GetFind();
            patterns.push_back(find.GetRegex());
        }
        m_FSM->AddPatterns(patterns);
    }
    m_FSM->Search(input, callback);
}


END_objects_SCOPE // namespace ncbi::objects::
END_NCBI_SCOPE

/* Original file checksum: lines: 57, chars: 1741, CRC32: b1125738 */
