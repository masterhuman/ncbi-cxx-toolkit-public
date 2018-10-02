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
* Author:  Frank Ludwig, NCBI
*
* File Description:
*   Test application for the CFormatGuess component
*
* ===========================================================================
*/

#ifndef BED_IMPORT_DATA__HPP
#define BED_IMPORT_DATA__HPP

#include <corelib/ncbifile.hpp>
#include <objects/seqloc/Na_strand.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>

//#include <util/line_reader.hpp>

#include "feat_import_data.hpp"

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE

//  ============================================================================
class CBedImportData:
    public CFeatImportData
//  ============================================================================
{
public:
    struct RgbValue {
        int R; int G; int B;
        RgbValue(int r=0, int g=0, int b=0): R(r), G(g), B(b) {};
    };

    CBedImportData(
        const CIdResolver&,
        CFeatMessageHandler&);

    CBedImportData(
        const CBedImportData& rhs);

    virtual ~CBedImportData() {};

    virtual void InitializeFrom(
        const std::vector<std::string>&,
        unsigned int) override;

    virtual void Serialize(
        CNcbiOstream&) override;

    const CSeq_loc& ChromLocation() const { return mChromLocation; };
    const std::string& Name() const { return mName; };
    int Score() const { return mScore; };
    const CSeq_loc& ThickLocation() const { return mThickLocation; };
    const RgbValue& Rgb() const { return mRgb; };
    const CSeq_loc& BlocksLocation() const { return mBlocksLocation; };
    

protected:
    void
    xSetChromLocation(
        const std::string&,
        const std::string&,
        const std::string&);
    void
    xSetName(
        const std::string&);
    void
    xSetScore(
        const std::string&);
    void
    xSetStrand(
        const std::string&);
    void
    xSetThickLocation(
        const std::string&,
        const std::string&);
    void
    xSetRgb(
        const std::string&,
        const std::string&);

    void
    xSetBlocks(
        const std::string&,
        const std::string&,
        const std::string&);

    CSeq_loc mChromLocation;
    std::string mName;
    int mScore;
    CSeq_loc mThickLocation;
    RgbValue mRgb;
    CSeq_loc mBlocksLocation;
};

END_objects_SCOPE
END_NCBI_SCOPE

#endif
