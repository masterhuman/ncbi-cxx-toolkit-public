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
 *   using specifications from the ASN data definition file
 *   'general.asn'.
 *
 * ---------------------------------------------------------------------------
 */

#ifndef OBJECTS_GENERAL_DBTAG_HPP
#define OBJECTS_GENERAL_DBTAG_HPP


// generated includes
#include <objects/general/Dbtag_.hpp>

// generated classes

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

class NCBI_GENERAL_EXPORT CDbtag : public CDbtag_Base
{
    typedef CDbtag_Base Tparent;
public:

    // this must be kept in sync with the (private) list in Dbtag.cpp!
    enum EDbtagType {
        eDbtagType_bad,
        
        eDbtagType_AFTOL,
        eDbtagType_ASAP,
        eDbtagType_ATCC,
        eDbtagType_ATCC_dna,
        eDbtagType_ATCC_in_host,
        eDbtagType_AceView_WormGenes,
        eDbtagType_ApiDB,
        eDbtagType_ApiDB_CryptoDB,
        eDbtagType_ApiDB_PlasmoDB,
        eDbtagType_ApiDB_ToxoDB,
        eDbtagType_BDGP_EST,
        eDbtagType_BDGP_INS,
        eDbtagType_BEETLEBASE,
        eDbtagType_BoLD,
        eDbtagType_CDD,
        eDbtagType_CK,
        eDbtagType_COG,
        eDbtagType_ENSEMBL,
        eDbtagType_ERIC,
        eDbtagType_ESTLIB,
        eDbtagType_EcoGene,
        eDbtagType_FANTOM_DB,
        eDbtagType_FLYBASE,
        eDbtagType_GABI,
        eDbtagType_GDB,
        eDbtagType_GI,
        eDbtagType_GO,
        eDbtagType_GOA,
        eDbtagType_GRIN,
        eDbtagType_GeneDB,
        eDbtagType_GeneID,
//        eDbtagType_Genew,
        eDbtagType_H_InvDB,
        eDbtagType_HGNC,
        eDbtagType_HSSP,
        eDbtagType_IFO,
        eDbtagType_IMGT_GENEDB,
        eDbtagType_IMGT_HLA,
        eDbtagType_IMGT_LIGM,
        eDbtagType_ISD,
        eDbtagType_ISFinder,
        eDbtagType_InterimID,
        eDbtagType_Interpro,
        eDbtagType_JCM,
        eDbtagType_JGIDB,
        eDbtagType_LocusID,
        eDbtagType_MGD,
        eDbtagType_MGI,
        eDbtagType_MIM,
        eDbtagType_MaizeGDB,
        eDbtagType_NMPDR,
        eDbtagType_NRESTdb,
        eDbtagType_NextDB,
        eDbtagType_PDB,
        eDbtagType_PFAM,
        eDbtagType_PGN,
        eDbtagType_PID,
        eDbtagType_PIDd,
        eDbtagType_PIDe,
        eDbtagType_PIDg,
        eDbtagType_PIR,
        eDbtagType_PSEUDO,
        eDbtagType_Pathema,
        eDbtagType_RATMAP,
        eDbtagType_RFAM,
        eDbtagType_RGD,
        eDbtagType_RZPD,
        eDbtagType_RiceGenes,
        eDbtagType_SEED,
        eDbtagType_SGD,
        eDbtagType_SoyBase,
        eDbtagType_SubtiList,
        eDbtagType_TIGRFAM,
        eDbtagType_UNITE,
        eDbtagType_UniGene,
        eDbtagType_UniProt_SwissProt,
        eDbtagType_UniProt_TrEMBL,
        eDbtagType_UniSTS,
        eDbtagType_VBASE2,
        eDbtagType_VectorBase,
        eDbtagType_WorfDB,
        eDbtagType_WormBase,
        eDbtagType_Xenbase,
        eDbtagType_ZFIN,
        eDbtagType_axeldb,
        eDbtagType_dbClone,
        eDbtagType_dbCloneLib,
        eDbtagType_dbEST,
        eDbtagType_dbProbe,
        eDbtagType_dbSNP,
        eDbtagType_dbSTS,
        eDbtagType_dictyBase,
        eDbtagType_niaEST,
        eDbtagType_taxon,

        // only approved for RefSeq
        eDbtagType_GenBank,
        eDbtagType_EMBL,
        eDbtagType_DDBJ,
        eDbtagType_REBASE,
        eDbtagType_CloneID,
        eDbtagType_CCDS,
        eDbtagType_ECOCYC,
        eDbtagType_HPRD,
        eDbtagType_miRBase,
        eDbtagType_PBR,
        eDbtagType_TAIR,
        eDbtagType_VBRC
    };

    // constructor
    CDbtag(void);
    // destructor
    ~CDbtag(void);

    // Comparison functions
    bool Match(const CDbtag& dbt2) const;
    int Compare(const CDbtag& dbt2) const;
    
    // Appends a label to "label" based on content of CDbtag
    void GetLabel(string* label) const;

    // Test if DB is approved by the consortium.
    // 'GenBank', 'EMBL' and 'DDBJ' are approved only in the
    // context of a RefSeq record.
    bool IsApproved(bool refseq = false) const;
    // Test if DB is approved (case insensitive).
    // Returns the case sensetive DB name if approved, NULL otherwise.
    const char* IsApprovedNoCase(bool refseq = false) const;

    // Test if appropriate to omit from displays, formatting, etc.
    bool IsSkippable(void) const;

    // Retrieve the enumerated type for the dbtag
    EDbtagType GetType(void) const;

    // Force a refresh of the internal type
    void InvalidateType(void);
    
    // Get a URL to the resource (if available)
    // @return
    //   the URL or an empty string if non is available
    string GetUrl(void) const;

private:

    // our enumerated (parsed) type
    mutable EDbtagType m_Type;

    // Prohibit copy constructor & assignment operator
    CDbtag(const CDbtag&);
    CDbtag& operator= (const CDbtag&);
};



/////////////////// CDbtag inline methods

// constructor
inline
CDbtag::CDbtag(void)
    : m_Type(eDbtagType_bad)
{
}


/////////////////// end of CDbtag inline methods


END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE

#endif // OBJECTS_GENERAL_DBTAG_HPP
