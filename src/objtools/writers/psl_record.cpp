
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
 * File Description:  Write alignment
 *
 */

#include <ncbi_pch.hpp>

#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seqalign/Spliced_seg.hpp>
#include <objects/seqalign/Spliced_exon.hpp>
#include <objects/seqalign/Sparse_seg.hpp>
#include <objects/seqalign/Sparse_align.hpp>
#include <objects/seqalign/Product_pos.hpp>
#include <objects/seqalign/Prot_pos.hpp>
#include <objects/seqalign/Spliced_exon_chunk.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seq/seq_id_handle.hpp>

#include <objmgr/scope.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/util/sequence.hpp>

#include <objtools/writers/writer_exception.hpp>
#include <objtools/writers/write_util.hpp>
#include <objtools/writers/psl_writer.hpp>
#include "psl_record.hpp"

#include <util/sequtil/sequtil_manip.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

//  ----------------------------------------------------------------------------
void
sDebugChunkArray(
    const vector<int>& origArray,
    int chunkSize,
    vector<vector<int>>& chunks)
//  ----------------------------------------------------------------------------
{
    if (origArray.empty()) {
        return;
    }
    auto numElements = origArray.size();
    for (auto index=0; index < numElements; ++index) {
        if (0 == index % chunkSize) {
            chunks.push_back(vector<int>());
        }
        chunks.back().push_back(origArray[index]);
    }
}

//  ----------------------------------------------------------------------------
string
sDebugFormatValue(
    const string& label,
    const string& data)
//  ----------------------------------------------------------------------------
{
    string formattedValue = (label + string(12, ' ')).substr(0, 12) + ": ";
    formattedValue += data;
    formattedValue += "\n";
    return formattedValue;
}

//  ----------------------------------------------------------------------------
void
CPslRecord::xInitializeStrands(
    CScope& scope,
    const CSpliced_seg& splicedSeg)
//  ----------------------------------------------------------------------------
{
    if (splicedSeg.CanGetProduct_strand()) {
        mStrandQ = splicedSeg.GetProduct_strand();
    }
    if (splicedSeg.CanGetGenomic_strand()) {
        mStrandT = splicedSeg.GetGenomic_strand();
    }
}


//  ----------------------------------------------------------------------------
void
CPslRecord::xInitializeInsertsQ(
    CScope& scope,
    const CSpliced_seg& splicedSeg)
//  ----------------------------------------------------------------------------
{
    if (mNumInsertQ != -1  &&  mBaseInsertQ != -1) {
        return;
    }
    if (mStrandQ == eNa_strand_unknown) {
        xInitializeStrands(scope, splicedSeg);
    }
    if (mEndQ == -1) {
        xInitializeSequenceQ(scope, splicedSeg);
    }

    mNumInsertQ = mBaseInsertQ = 0;
    const auto& exonList = splicedSeg.GetExons();

    int startQQ(-1), endQQ(-1);
    for (auto pExon: exonList) {
        int exonStartQ = static_cast<int>(pExon->GetProduct_start().AsSeqPos());
        if (startQQ == -1  ||  exonStartQ < startQQ) {
            startQQ = exonStartQ;
        }
        int exonEndQ = static_cast<int>(pExon->GetProduct_end().AsSeqPos());
        if (endQQ == -1  ||  exonEndQ >= endQQ) {
            endQQ = exonEndQ + 1;
        }
    }
    mBaseInsertQ = mEndQ - endQQ;
}

//  ----------------------------------------------------------------------------
void
CPslRecord::xInitializeInsertsT(
    CScope& scope,
    const CSpliced_seg& splicedSeg)
//  ----------------------------------------------------------------------------
{
    if (mNumInsertT != -1  &&  mBaseInsertT != -1) {
        return;
    }
    if (mStrandT == eNa_strand_unknown) {
        xInitializeStrands(scope, splicedSeg);
    }

    mNumInsertT = mBaseInsertT = 0;
    const auto& exonList = splicedSeg.GetExons();
    if (mStrandT == eNa_strand_plus) {
        int lastExonEndT = -1;
        for (auto pExon: exonList) {
            if (lastExonEndT == -1) {
                lastExonEndT = pExon->GetGenomic_end() + 1;
                continue;
            }
            auto exonStart = pExon->GetGenomic_start();
            if (exonStart > lastExonEndT) {
                mNumInsertT++;
                mBaseInsertT += (exonStart - lastExonEndT);
            }
            lastExonEndT = pExon->GetGenomic_end() + 1;
        }
        mNumInsertT = 0;
    }
    else { // eNa_strand_minus
        int lastExonStartT = -1;
        for (auto pExon: exonList) {
            if (lastExonStartT == -1) {
                lastExonStartT = pExon->GetGenomic_start();
                continue;
            }
            auto exonEnd = pExon->GetGenomic_end() + 1;
            if (exonEnd < lastExonStartT) {
                mNumInsertT++;
                mBaseInsertT += (lastExonStartT - exonEnd);
            }
            lastExonStartT = pExon->GetGenomic_start();
        }
        mNumInsertT = 0;
    }
}

//  ----------------------------------------------------------------------------
void
CPslRecord::xInitializeMatchesMismatches(
    CScope& scope,
    const CSpliced_seg& splicedSeg)
//  ----------------------------------------------------------------------------
{
    if (mMatches != -1  &&  mMisMatches != -1  &&  mRepMatches != -1) {
        return;
    }
    if (mBaseInsertT == -1) {
        xInitializeInsertsT(scope, splicedSeg);
    }

    mMatches = mMisMatches = mRepMatches = 0;
    const auto& exonList = splicedSeg.GetExons();
    for (auto pExon: exonList) {
        const auto& exonParts = pExon->GetParts();
        for (auto part: exonParts) {
            if (part->IsMatch()) {
                mMatches += part->GetMatch();
            }
            else if (part->IsMismatch()) {
                mMisMatches += part->GetMismatch();
            }
        } 
    }
    mMatches += mBaseInsertT;
}

//  ----------------------------------------------------------------------------
void
CPslRecord::xInitializeSequenceQ(
    CScope& scope,
    const CSpliced_seg& splicedSeg)
//  ----------------------------------------------------------------------------
{
    if (mSizeQ != -1  &&  mStartQ != -1  &&  mEndQ != -1) {
        return;
    }
    const auto& queryId = splicedSeg.GetProduct_id();
    auto querySeqHandle = scope.GetBioseqHandle(queryId);
    CWriteUtil::GetBestId(querySeqHandle.GetSeq_id_Handle(), scope, mNameQ);
    mSizeQ = querySeqHandle.GetInst_Length();
    mStartQ = 0;
    mEndQ = mStartQ + mSizeQ;
}

//  ----------------------------------------------------------------------------
void
CPslRecord::xInitializeSequenceT(
    CScope& scope,
    const CSpliced_seg& splicedSeg)
//  ----------------------------------------------------------------------------
{
    if (mSizeT != -1  &&  mStartT != -1  &&  mEndT != -1) {
        return;
    }
    const auto& targetId = splicedSeg.GetGenomic_id();
    auto targetSeqHandle = scope.GetBioseqHandle(targetId);
    CWriteUtil::GetBestId(targetSeqHandle.GetSeq_id_Handle(), scope, mNameT);
    mSizeT = targetSeqHandle.GetInst_Length();

    const auto& exonList = splicedSeg.GetExons();
    for (auto pExon: exonList) {
        int exonStartT = static_cast<int>(pExon->GetGenomic_start());
        if (mStartT == -1  ||  exonStartT < mStartT) {
            mStartT = exonStartT;
        }
        int exonEndT = static_cast<int>(pExon->GetGenomic_end());
        if (mEndT == -1  ||  exonEndT >= mEndT) {
            mEndT = exonEndT + 1;
        }
    }
}

//  ----------------------------------------------------------------------------
void
CPslRecord::xInitializeBlocks(
    CScope& scope,
    const CSpliced_seg& splicedSeg)
//  ----------------------------------------------------------------------------
{
    if (mExonCount != -1) {
        return;
    }
    const auto& exonList = splicedSeg.GetExons();
    mExonCount = static_cast<int>(exonList.size());
    for (auto pExon: exonList) {
        int exonStartQ = static_cast<int>(pExon->GetProduct_start().AsSeqPos());
        mExonStartsQ.push_back(exonStartQ);
        int exonStartT = static_cast<int>(pExon->GetGenomic_start());
        mExonStartsT.push_back(exonStartT);
        auto blockSize = static_cast<int>(
            pExon->GetGenomic_end() - exonStartT + 1);
        mExonSizes.push_back(blockSize);
    }
    if (mStrandT == eNa_strand_minus) {
        std::reverse(mExonStartsT.begin(), mExonStartsT.end());
        std::reverse(mExonSizes.begin(), mExonSizes.end());
    }
}


//  ----------------------------------------------------------------------------
void
CPslRecord::xValidateSegment(
    CScope& scope,
    const CSpliced_seg& splicedSeg)
//  ----------------------------------------------------------------------------
{
    const auto& exonList = splicedSeg.GetExons();
    for (auto pExon: exonList) {
        if (!pExon->CanGetProduct_start()  || !pExon->CanGetProduct_end()) {
            //throw?
        }
        if (!pExon->CanGetGenomic_start()  || !pExon->CanGetGenomic_end()) {
            //throw?
        }
    }
}


//  ----------------------------------------------------------------------------
void
CPslRecord::Initialize(
    CScope& scope,
    const CSpliced_seg& splicedSeg)
//  ----------------------------------------------------------------------------
{
    xValidateSegment(scope, splicedSeg);

    xInitializeStrands(scope, splicedSeg);
    xInitializeMatchesMismatches(scope, splicedSeg);
    //nCount?
    xInitializeSequenceQ(scope, splicedSeg);
    xInitializeSequenceT(scope, splicedSeg);
    xInitializeInsertsQ(scope, splicedSeg);
    xInitializeInsertsT(scope, splicedSeg);
    xInitializeBlocks(scope, splicedSeg);
}

//  ----------------------------------------------------------------------------
string
CPslRecord::xFieldMatches(bool debug) const
//  ----------------------------------------------------------------------------
{
    auto rawString = NStr::IntToString(mMatches);
    if (debug) {
        return sDebugFormatValue("matches", rawString);
    }
    return rawString;
}

//  ----------------------------------------------------------------------------
string
CPslRecord::xFieldMisMatches(bool debug) const
//  ----------------------------------------------------------------------------
{
    auto rawString = NStr::IntToString(mMisMatches);
    if (debug) {
        return sDebugFormatValue("misMatches", rawString);
    }
    return "\t" + rawString;
}

//  ----------------------------------------------------------------------------
string
CPslRecord::xFieldRepMatches(bool debug) const
//  ----------------------------------------------------------------------------
{
    auto rawString = NStr::IntToString(mRepMatches);
    if (debug) {
        return sDebugFormatValue("repMatches", rawString);
    }
    return "\t" + rawString;
}

//  ----------------------------------------------------------------------------
string
CPslRecord::xFieldCountN(bool debug) const
//  ----------------------------------------------------------------------------
{
    auto rawString = NStr::IntToString(mCountN);
    if (debug) {
        return sDebugFormatValue("nCount", rawString);
    }
    return "\t" + rawString;
}

//  ----------------------------------------------------------------------------
string
CPslRecord::xFieldNumInsertQ(bool debug) const
//  ----------------------------------------------------------------------------
{
    auto rawString = NStr::IntToString(mNumInsertQ);
    if (debug) {
        return sDebugFormatValue("qNumInsert", rawString);
    }
    return "\t" + rawString;
}

//  ----------------------------------------------------------------------------
string
CPslRecord::xFieldBaseInsertQ(bool debug) const
//  ----------------------------------------------------------------------------
{
    auto rawString = NStr::IntToString(mBaseInsertQ);
    if (debug) {
        return sDebugFormatValue("qBaseInsert", rawString);
    }
    return "\t" + rawString;
}

//  ----------------------------------------------------------------------------
string
CPslRecord::xFieldNumInsertT(bool debug) const
//  ----------------------------------------------------------------------------
{
    auto rawString = NStr::IntToString(mNumInsertT);
    if (debug) {
        return sDebugFormatValue("tNumInsert", rawString);
    }
    return "\t" + rawString;
}

//  ----------------------------------------------------------------------------
string
CPslRecord::xFieldBaseInsertT(bool debug) const
//  ----------------------------------------------------------------------------
{
    auto rawString = NStr::IntToString(mBaseInsertT);
    if (debug) {
        return sDebugFormatValue("tBaseInsert", rawString);
    }
    return "\t" + rawString;
}

//  ----------------------------------------------------------------------------
string
CPslRecord::xFieldStrand(bool debug) const
//  ----------------------------------------------------------------------------
{
    string rawString = (mStrandT == eNa_strand_minus ? "-" : "+");
    //if (mStrandT == eNa_strand_plus) {
    //    rawString += "+";
    //}
    //else if (mStrandT == eNa_strand_minus) {
    //    rawString += "-";
    //}
    if (debug) {
        return sDebugFormatValue("strand", rawString);
    }
    return "\t" + rawString;
}

//  ----------------------------------------------------------------------------
string
CPslRecord::xFieldNameQ(bool debug) const
//  ----------------------------------------------------------------------------
{
    auto rawString = mNameQ;
    if (debug) {
        if (rawString.empty()) {
            rawString = ".";
        }
        return sDebugFormatValue("qName", rawString);
    }
    return "\t" + rawString;
}

//  ----------------------------------------------------------------------------
string
CPslRecord::xFieldSizeQ(bool debug) const
//  ----------------------------------------------------------------------------
{
    auto rawString = NStr::IntToString(mSizeQ);
    if (debug) {
        return sDebugFormatValue("qSize", rawString);
    }
    return "\t" + rawString;
}

//  ----------------------------------------------------------------------------
string
CPslRecord::xFieldStartQ(bool debug) const
//  ----------------------------------------------------------------------------
{
    auto rawString = NStr::IntToString(mStartQ);
    if (debug) {
        return sDebugFormatValue("qStart", rawString);
    }
    return "\t" + rawString;
}

//  ----------------------------------------------------------------------------
string
CPslRecord::xFieldEndQ(bool debug) const
//  ----------------------------------------------------------------------------
{
    auto rawString = NStr::IntToString(mEndQ);
    if (debug) {
        return sDebugFormatValue("qEnd", rawString);
    }
    return "\t" + rawString;
}

//  ----------------------------------------------------------------------------
string
CPslRecord::xFieldNameT(bool debug) const
//  ----------------------------------------------------------------------------
{
    auto rawString = mNameT;
    if (debug) {
        if (rawString.empty()) {
            rawString = ".";
        }
        return sDebugFormatValue("tName", rawString);
    }
    return "\t" + rawString;
}

//  ----------------------------------------------------------------------------
string
CPslRecord::xFieldSizeT(bool debug) const
//  ----------------------------------------------------------------------------
{
    auto rawString = NStr::IntToString(mSizeT);
    if (debug) {
        return sDebugFormatValue("tSize", rawString);
    }
    return "\t" + rawString;
}

//  ----------------------------------------------------------------------------
string
CPslRecord::xFieldStartT(bool debug) const
//  ----------------------------------------------------------------------------
{
    auto rawString = NStr::IntToString(mStartT);
    if (debug) {
       return sDebugFormatValue("tStart", rawString);
    }
    return "\t" + rawString;
}

//  ----------------------------------------------------------------------------
string
CPslRecord::xFieldEndT(bool debug) const
//  ----------------------------------------------------------------------------
{
    auto rawString = NStr::IntToString(mEndT);
    if (debug) {
        return sDebugFormatValue("tEnd", rawString);
    }
    return "\t" + rawString;
}

//  ----------------------------------------------------------------------------
string
CPslRecord::xFieldBlockCount(bool debug) const
//  ----------------------------------------------------------------------------
{
    auto rawString = NStr::IntToString(mExonCount);
    if (debug) {
        return sDebugFormatValue("blockCount", rawString);
    }
    return "\t" + rawString;
}

//  ----------------------------------------------------------------------------
string
CPslRecord::xFieldBlockSizes(bool debug) const
//  ----------------------------------------------------------------------------
{
    if (debug) {
        if (mExonSizes.empty()) {
            return sDebugFormatValue("blockSizes", ".");
        }
        vector<vector<int>> chunks;
        sDebugChunkArray(mExonSizes, 5, chunks);
        bool labelWritten = false;
        string field;
        for (auto& chunk: chunks) {
            auto value = NStr::JoinNumeric(chunk.begin(), chunk.end(), ", ");
            if (!labelWritten) {
                field += sDebugFormatValue("blockSizes", value);
                labelWritten = true;
            }
            else {
                field += "              ";
                field += value;
                field += "\n";
            }
        }
        return field;
    }
    return "\t" + NStr::JoinNumeric(mExonSizes.begin(), mExonSizes.end(), ",");
}

//  ----------------------------------------------------------------------------
string
CPslRecord::xFieldStartsQ(bool debug) const
//  ----------------------------------------------------------------------------
{
    if (debug) {
        if (mExonStartsQ.empty()) {
            return sDebugFormatValue("qStarts", ".");
        }
        vector<vector<int>> chunks;
        sDebugChunkArray(mExonStartsQ, 5, chunks);
        bool labelWritten = false;
        string field;
        for (auto& chunk: chunks) {
            auto value = NStr::JoinNumeric(chunk.begin(), chunk.end(), ", ");
            if (!labelWritten) {
                field += sDebugFormatValue("qStarts", value);
                labelWritten = true;
            }
            else {
                field += "              ";
                field += value;
                field += "\n";
            }
        }
        return field;
    }
    return "\t" + NStr::JoinNumeric(mExonStartsQ.begin(), mExonStartsQ.end(), ",");
}

//  ----------------------------------------------------------------------------
string
CPslRecord::xFieldStartsT(bool debug) const
//  ----------------------------------------------------------------------------
{
    if (debug) {
        if (mExonStartsT.empty()) {
            return sDebugFormatValue("tStarts", ".");
        }
        vector<vector<int>> chunks;
        sDebugChunkArray(mExonStartsT, 5, chunks);
        bool labelWritten = false;
        string field;
        for (auto& chunk: chunks) {
            auto value = NStr::JoinNumeric(chunk.begin(), chunk.end(), ", ");
            if (!labelWritten) {
                field += sDebugFormatValue("tStarts", value);
                labelWritten = true;
            }
            else {
                field += "              ";
                field += value;
                field += "\n";
            }
        }
        return field;
    }
    return "\t" + NStr::JoinNumeric(mExonStartsT.begin(), mExonStartsT.end(), ",");
}

//  ----------------------------------------------------------------------------
void
CPslRecord::Write(
    ostream& ostr,
    bool debug) const
//  ----------------------------------------------------------------------------
{
    ostr << xFieldMatches(debug) 
         << xFieldMisMatches(debug) 
         << xFieldRepMatches(debug)
         << xFieldCountN(debug)
         << xFieldNumInsertQ(debug)
         << xFieldBaseInsertQ(debug)
         << xFieldNumInsertT(debug)
         << xFieldBaseInsertT(debug)
         << xFieldStrand(debug)
         << xFieldNameQ(debug)
         << xFieldSizeQ(debug)
         << xFieldStartQ(debug)
         << xFieldEndQ(debug)
         << xFieldNameT(debug)
         << xFieldSizeT(debug)
         << xFieldStartT(debug)
         << xFieldEndT(debug)
         << xFieldBlockCount(debug)
         << xFieldBlockSizes(debug)
         << xFieldStartsQ(debug)
         << xFieldStartsT(debug) 
         << endl;
}

END_NCBI_SCOPE

