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
 * Author:  Craig Wallin, NCBI
 *
 * File Description:
 *
 *   Unit test for CEntrezgene
 *
 * ===========================================================================
 */

#include <ncbi_pch.hpp>

#include <corelib/ncbiapp.hpp>
#include <corelib/test_boost.hpp>

#include <objects/entrezgene/Entrezgene.hpp>
#include <objects/entrezgene/Gene_commentary.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seqfeat/Gene_nomenclature.hpp>
#include <objects/seqfeat/Gene_ref.hpp>

#include <objtools/eutils/api/efetch.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);

// Read a public Entrezgene record.

static void s_GetObject(const string& gene_id, CEntrezgene& eg_obj)
{
    LOG_POST(Note << "Looking up GeneID " << gene_id);
    CRef<CEUtils_ConnContext> ctx(new CEUtils_ConnContext);
    CEFetch_Request req(ctx);
    req.SetDatabase("gene");
    req.GetId().AddId(gene_id);
    string eg_str;
    // A very limited and simple retry.
    try {
        SleepMilliSec(333); // per e-utils guidelines
        req.Read(&eg_str);
        CNcbiIstrstream istr(eg_str);
        istr >> MSerial_AsnText >> eg_obj;
    } catch(...) {
        // simple retry, only retry once
        SleepMilliSec(30000); // maybe enough time, maybe not
        req.Read(&eg_str);
        CNcbiIstrstream istr(eg_str);
        istr >> MSerial_AsnText >> eg_obj;
    }
}

BOOST_AUTO_TEST_CASE(s_TestDescription)
{
    CEntrezgene eg_obj;

    // Data comes from gene desc
    s_GetObject("2778", eg_obj);
    BOOST_CHECK_EQUAL(eg_obj.GetDescription(), "GNAS complex locus");

    // Data comes from prot desc
    s_GetObject("4514", eg_obj);
    BOOST_CHECK_EQUAL(eg_obj.GetDescription(), "cytochrome c oxidase subunit III");

    // Data comes from prot name
    s_GetObject("4508", eg_obj);
    BOOST_CHECK_EQUAL(eg_obj.GetDescription(), "ATP synthase F0 subunit 6");

    // Data comes from rna ext name
    s_GetObject("4549", eg_obj);
    BOOST_CHECK_EQUAL(eg_obj.GetDescription(), "s-rRNA");

    // Data comes from gene type
    s_GetObject("4511", eg_obj);
    BOOST_CHECK_EQUAL(eg_obj.GetDescription(), "tRNA-Cys");
}

BOOST_AUTO_TEST_CASE(s_TestNomenclature)
{
    CEntrezgene eg_obj;
    CRef<CGene_nomenclature> nomen;

    // Data comes from formal-name element.
    s_GetObject("4535", eg_obj);
    BOOST_CHECK_EQUAL(eg_obj.GetGene().IsSetFormal_name(), true);
    nomen = eg_obj.GetNomenclature();
    BOOST_CHECK_EQUAL(nomen->GetStatus(), CGene_nomenclature::eStatus_official);
    BOOST_CHECK_EQUAL(nomen->GetSymbol(), "MT-ND1");
    BOOST_CHECK_EQUAL(nomen->GetName(), "mitochondrially encoded NADH dehydrogenase 1");
    BOOST_CHECK_EQUAL(nomen->GetSource().GetDb(), "HGNC");
    BOOST_CHECK_EQUAL(nomen->GetSource().GetTag().GetStr(), "HGNC:7455");

    // Interim symbol and name, from general comment elements.
    s_GetObject("121110513", eg_obj);
    BOOST_CHECK_EQUAL(eg_obj.GetGene().IsSetFormal_name(), false);
    nomen = eg_obj.GetNomenclature();
    BOOST_CHECK_EQUAL(nomen->GetStatus(), CGene_nomenclature::eStatus_interim);
    BOOST_CHECK_EQUAL(nomen->GetSymbol(), "TRNASTOP-UCA");
    BOOST_CHECK_EQUAL(nomen->GetName(), "transfer RNA opal suppressor (anticodon UCA)");
    // TODO: check source field when it becomes available

    // Official symbol and name, from general comment elements.
    s_GetObject("1", eg_obj);
    BOOST_CHECK_EQUAL(eg_obj.GetGene().IsSetFormal_name(), false);
    nomen = eg_obj.GetNomenclature();
    BOOST_CHECK_EQUAL(nomen->GetStatus(), CGene_nomenclature::eStatus_official);
    BOOST_CHECK_EQUAL(nomen->GetSymbol(), "A1BG");
    BOOST_CHECK_EQUAL(nomen->GetName(), "alpha-1-B glycoprotein");
    // TODO: check source field when it becomes available

    // None available in the data.
    s_GetObject("107547967", eg_obj);
    BOOST_CHECK_EQUAL(eg_obj.GetGene().IsSetFormal_name(), false);
    nomen = eg_obj.GetNomenclature();
    BOOST_CHECK_EQUAL(nomen->GetStatus(), CGene_nomenclature::eStatus_unknown);
}

BOOST_AUTO_TEST_CASE(s_TestFindComment)
{
    CEntrezgene eg_obj;
    CRef<CGene_commentary> comment;

    s_GetObject("1", eg_obj);
    // Comment not found.
    comment = eg_obj.FindComment("Nonesuch");
    BOOST_CHECK_EQUAL(true, comment.Empty());
    // Comment found.
    comment = eg_obj.FindComment("RefSeq Status");
    BOOST_CHECK_EQUAL(true, comment.NotEmpty());
    if (comment) {
        BOOST_CHECK_EQUAL(true, comment->IsSetLabel());
        if (comment->IsSetLabel()) {
            BOOST_CHECK_EQUAL("REVIEWED", comment->GetLabel());
        }
    }
}
