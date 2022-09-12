#ifndef FEATURE_INDEXER__HPP
#define FEATURE_INDEXER__HPP

/*
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
* Author:  Jonathan Kans
*
*/

#include <corelib/ncbicntr.hpp>

#include <objects/general/Object_id.hpp>
#include <objects/seq/MolInfo.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seq_gap.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/submit/Seq_submit.hpp>
#include <objects/submit/Submit_block.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/seq_entry_handle.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/util/feature.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


// look-ahead class names
class CSeqEntryIndex;
class CSeqMasterIndex;
class CSeqsetIndex;
class CBioseqIndex;
class CGapIndex;
class CDescriptorIndex;
class CFeatureIndex;

typedef void (*FAddSnpFunc)(CBioseq_Handle bsh, string& na_acc);

// CSeqEntryIndex
//
// CSeqEntryIndex is the public, top-level Seq-entry exploration organizer.  A variable
// is created using the top-level sequence object, with the constructors taking optional
// fetch policy and feature collection flags, as well as an optional feature exploration
// depth parameter (for the default adaptive fetch policy):
//
//   CSeqEntryIndex idx(*m_entry, CSeqEntryIndex::eAdaptive);
//
// A Seq-entry wrapper is created if the top-level object is a Bioseq or Bioseq-set.
// Bioseqs within the Seq-entry are then indexed and added to a vector of CBioseqIndex.
//
// Bioseqs are explored with IterateBioseqs, or selected individually by GetBioseqIndex
// (given an accession, index number, or subregion):
//
//   idx.IterateBioseqs("U54469", [this](CBioseqIndex& bsx) {
//       ...
//   });
//
// The embedded lambda function statements are executed for each selected Bioseq.
//
// Internal indexing objects (i.e., CSeqMasterIndex, CSeqsetIndex, CBioseqIndex,
// CDescriptorIndex, and CFeatureIndex) are generated by the indexing process, and
// should not be created by the application.
class NCBI_XOBJUTIL_EXPORT CSeqEntryIndex : public CObjectEx
{
public:

    enum EPolicy {
        // far feature fetch policy
        eAdaptive = 0,
        eInternal = 1,
        eExternal = 2,
        eExhaustive = 3,
        eFtp = 4,
        eWeb = 5,
        eGenomes = 6
    };

    enum EFlags {
        fDefault =           0,
        fHideImpFeats =      1,
        fHideSNPFeats =      2,
        fHideCDDFeats =      4,
        fHideSTSFeats =      8,
        fHideExonFeats =    16,
        fHideIntronFeats =  32,
        fHideMiscFeats =    64,
        fShowSNPFeats =    128,
        fShowCDDFeats =    256,
        fGeneRNACDSOnly =  512,
        fHideGapFeats =   1024
    };
    typedef int TFlags; // Binary "OR" of EFlags

public:
    // Constructors take the top-level sequence object

    // The primary constructor uses an existing CScope created by the application
    CSeqEntryIndex (CSeq_entry_Handle& topseh, EPolicy policy = eAdaptive, TFlags flags = fDefault);
    CSeqEntryIndex (CBioseq_Handle& bsh, EPolicy policy = eAdaptive, TFlags flags = fDefault);

    // Alternative constructors take an object and create a new local default CScope
    CSeqEntryIndex (CSeq_entry& topsep, EPolicy policy = eAdaptive, TFlags flags = fDefault);
    CSeqEntryIndex (CBioseq_set& seqset, EPolicy policy = eAdaptive, TFlags flags = fDefault);
    CSeqEntryIndex (CBioseq& bioseq, EPolicy policy = eAdaptive, TFlags flags = fDefault);
    CSeqEntryIndex (CSeq_submit& submit, EPolicy policy = eAdaptive, TFlags flags = fDefault);

    // Specialized constructors are for streaming through release files, one component at a time

    // Submit-block obtained from top of Seq-submit release file
    CSeqEntryIndex (CSeq_entry& topsep, CSubmit_block &sblock, EPolicy policy = eAdaptive, TFlags flags = fDefault);
    // Seq-descr chain obtained from top of Bioseq-set release file
    CSeqEntryIndex (CSeq_entry& topsep, CSeq_descr &descr, EPolicy policy = eAdaptive, TFlags flags = fDefault);

private:
    // Prohibit copy constructor & assignment operator
    CSeqEntryIndex (const CSeqEntryIndex&) = delete;
    CSeqEntryIndex& operator= (const CSeqEntryIndex&) = delete;

public:
    // Bioseq exploration iterator
    template<typename Fnc> size_t IterateBioseqs (Fnc m);

    // GetBioseqIndex methods are provided for a variety of argument types

    // Get first Bioseq index
    CRef<CBioseqIndex> GetBioseqIndex (void);
    // Get Nth Bioseq index
    CRef<CBioseqIndex> GetBioseqIndex (int n);
    // Get Bioseq index by accession
    CRef<CBioseqIndex> GetBioseqIndex (const string& accn);
    // Get Bioseq index by handle
    CRef<CBioseqIndex> GetBioseqIndex (CBioseq_Handle bsh);
    // Get Bioseq index by mapped feature
    CRef<CBioseqIndex> GetBioseqIndex (const CMappedFeat& mf);
    // Get Bioseq index by sublocation
    CRef<CBioseqIndex> GetBioseqIndex (const CSeq_loc& loc);

    // Seqset exploration iterator
    template<typename Fnc> size_t IterateSeqsets (Fnc m);

    const vector<CRef<CBioseqIndex>>& GetBioseqIndices(void);

    const vector<CRef<CSeqsetIndex>>& GetSeqsetIndices(void);

    bool DistributedReferences(void);

    void SetSnpFunc(FAddSnpFunc* snp);

    FAddSnpFunc* GetSnpFunc(void);

    void SetFeatDepth(int featDepth);

    int GetFeatDepth(void);

    void SetGapDepth(int gapDepth);

    int GetGapDepth(void);

    // Check all Bioseqs for failure to fetch remote sequence components or feature annotation
    bool IsFetchFailure(void);

    // Check for failure to create scope
    bool IsIndexFailure (void);

    CRef<CSeqMasterIndex> GetMasterIndex(void) const { return m_Idx; }

private:
    // Implementation details are in a separate CSeqMasterIndex object wrapped in a CRef
    CRef<CSeqMasterIndex> m_Idx;
};


// CSeqMasterIndex
//
// CSeqMasterIndex holds the implementation methods and variables for the CSeqEntryIndex
class NCBI_XOBJUTIL_EXPORT CSeqMasterIndex : public CObjectEx
{
public:
    // Constructor is separate from Initializers so that CSeqEntryIndex can capture a CRef to
    // its CSeqMasterIndex, making CWeakRef<CSeqMasterIndex> available to GetFeatureForProduct
    CSeqMasterIndex (void) { }

public:
    // Initializers take the top-level sequence object
    void x_Initialize (CSeq_entry_Handle& topseh, CSeqEntryIndex::EPolicy policy, CSeqEntryIndex::TFlags flags);
    void x_Initialize (CBioseq_Handle& bsh, CSeqEntryIndex::EPolicy policy, CSeqEntryIndex::TFlags flags);

    void x_Initialize (CSeq_entry& topsep, CSeqEntryIndex::EPolicy policy, CSeqEntryIndex::TFlags flags);
    void x_Initialize (CBioseq_set& seqset, CSeqEntryIndex::EPolicy policy, CSeqEntryIndex::TFlags flags);
    void x_Initialize (CBioseq& bioseq, CSeqEntryIndex::EPolicy policy, CSeqEntryIndex::TFlags flags);
    void x_Initialize (CSeq_submit& submit, CSeqEntryIndex::EPolicy policy, CSeqEntryIndex::TFlags flags);

    void x_Initialize (CSeq_entry& topsep, CSubmit_block &sblock, CSeqEntryIndex::EPolicy policy, CSeqEntryIndex::TFlags flags);
    void x_Initialize (CSeq_entry& topsep, CSeq_descr &descr, CSeqEntryIndex::EPolicy policy, CSeqEntryIndex::TFlags flags);

private:
    // Prohibit copy constructor & assignment operator
    CSeqMasterIndex (const CSeqMasterIndex&) = delete;
    CSeqMasterIndex& operator= (const CSeqMasterIndex&) = delete;

public:
    // Bioseq exploration iterator
    template<typename Fnc> size_t IterateBioseqs (Fnc m);

    // Get first Bioseq index
    CRef<CBioseqIndex> GetBioseqIndex (void);
    // Get Nth Bioseq index
    CRef<CBioseqIndex> GetBioseqIndex (int n);
    // Get Bioseq index by accession
    CRef<CBioseqIndex> GetBioseqIndex (const string& accn);
    // Get Bioseq index by handle
    CRef<CBioseqIndex> GetBioseqIndex (CBioseq_Handle bsh);
    // Get Bioseq index by string
    CRef<CBioseqIndex> GetBioseqIndex (string& str);
    // Get Bioseq index by feature
    CRef<CBioseqIndex> GetBioseqIndex (const CMappedFeat& mf);
    // Get Bioseq index by sublocation
    CRef<CBioseqIndex> GetBioseqIndex (const CSeq_loc& loc);

    // Seqset exploration iterator
    template<typename Fnc> size_t IterateSeqsets (Fnc m);

    // Getters
    CRef<CObjectManager> GetObjectManager (void) const { return m_Objmgr; }
    CRef<CScope> GetScope (void) const { return m_Scope; }
    CSeq_entry_Handle GetTopSEH (void) const { return m_Tseh; }
    CConstRef<CSeq_entry> GetTopSEP (void) const { return m_Tsep; }
    CConstRef<CSubmit_block> GetSbtBlk (void) const { return m_SbtBlk; }
    CConstRef<CSeq_descr> GetTopDescr (void) const { return m_TopDescr; }
    CRef<feature::CFeatTree> GetFeatTree (void) const { return m_FeatTree; }

    const vector<CRef<CBioseqIndex>>& GetBioseqIndices(void);

    const vector<CRef<CSeqsetIndex>>& GetSeqsetIndices(void);

    void SetHasOperon (bool hasOp) { m_HasOperon = hasOp; }
    bool HasOperon (void) const { return m_HasOperon; }

    bool IsSmallGenomeSet (void) const { return m_IsSmallGenomeSet; }

    bool DistributedReferences (void) const { return m_DistributedReferences; }

    void SetSnpFunc(FAddSnpFunc* snp);

    FAddSnpFunc* GetSnpFunc(void);

    void SetFeatDepth(int featDepth);

    int GetFeatDepth(void);

    void SetGapDepth(int gapDepth);

    int GetGapDepth(void);

    // Check all Bioseqs for failure to fetch remote sequence components or remote feature annotation
    bool IsFetchFailure(void);

    // Check for failure to create scope
    bool IsIndexFailure (void) const { return m_IndexFailure; }
    void SetIndexFailure (bool fails) { m_IndexFailure = fails; }

private:
    // Common initialization function called by each Initialize variant
    void x_Init (void);

    // Recursive exploration to populate vector of index objects for Bioseqs in Seq-entry
    void x_InitSeqs (const CSeq_entry& sep, CRef<CSeqsetIndex> prnt, int level = 0);

private:
    CRef<CObjectManager> m_Objmgr;
    CRef<CScope> m_Scope;
    CSeq_entry_Handle m_Tseh;

    CConstRef<CSeq_entry> m_Tsep;
    CConstRef<CSubmit_block> m_SbtBlk;
    CConstRef<CSeq_descr> m_TopDescr;
    CRef<feature::CFeatTree> m_FeatTree;

    CSeqEntryIndex::EPolicy m_Policy;
    CSeqEntryIndex::TFlags m_Flags;

    vector<CRef<CBioseqIndex>> m_BsxList;

    // map from accession string to CBioseqIndex object
    typedef map<string, CRef<CBioseqIndex> > TAccnIndexMap;
    TAccnIndexMap m_AccnIndexMap;

    // map from CBioseq_Handle to CBioseqIndex object via best Seq-id string
    typedef map<string, CRef<CBioseqIndex> > TBestIdIndexMap;
    TBestIdIndexMap m_BestIdIndexMap;

    vector<CRef<CSeqsetIndex>> m_SsxList;

    bool m_HasOperon;
    bool m_IsSmallGenomeSet;

    bool m_DistributedReferences;

    FAddSnpFunc* m_SnpFunc;

    int m_FeatDepth;
    int m_GapDepth;

    mutable CAtomicCounter m_Counter;

    bool m_IndexFailure;
};


// CSeqsetIndex
//
// CSeqsetIndex stores information about an element in the Bioseq-set hierarchy
class NCBI_XOBJUTIL_EXPORT CSeqsetIndex : public CObjectEx
{
public:
    // Constructor
    CSeqsetIndex (CBioseq_set_Handle ssh,
                  const CBioseq_set& bssp,
                  CRef<CSeqsetIndex> prnt);

private:
    // Prohibit copy constructor & assignment operator
    CSeqsetIndex (const CSeqsetIndex&) = delete;
    CSeqsetIndex& operator= (const CSeqsetIndex&) = delete;

public:
    // Getters
    CBioseq_set_Handle GetSeqsetHandle (void) const { return m_Ssh; }
    const CBioseq_set& GetSeqset (void) const { return m_Bssp; }
    CRef<CSeqsetIndex> GetParent (void) const { return m_Prnt; }

    CBioseq_set::TClass GetClass (void) const { return m_Class; }

private:
    CBioseq_set_Handle m_Ssh;
    const CBioseq_set& m_Bssp;
    CRef<CSeqsetIndex> m_Prnt;

    CBioseq_set::TClass m_Class;
};


// CBioseqIndex
//
// CBioseqIndex is the exploration organizer for a given Bioseq.  It provides methods to
// obtain descriptors and iterate through features that apply to the Bioseq.  (These are
// stored in vectors, which are initialized upon first request.)
//
// CBioseqIndex also maintains a CFeatTree for its Bioseq, used to find the best gene for
// each feature.
//
// Descriptors are explored with:
//
//   bsx.IterateDescriptors([this](CDescriptorIndex& sdx) {
//       ...
//   });
//
// and are presented based on the order of the descriptor chain hierarchy, starting with
// descriptors packaged on the Bioseq, then on its parent Bioseq-set, etc.
//
// Features are explored with:
//
//   bsx.IterateFeatures([this](CFeatureIndex& sfx) {
//       ...
//   });
//
// and are presented in order of biological position along the parent sequence.
//
// Fetching external features uses SAnnotSelector adaptive depth unless explicitly overridden.
class NCBI_XOBJUTIL_EXPORT CBioseqIndex : public CObjectEx
{
public:
    // Constructor
    CBioseqIndex (CBioseq_Handle bsh,
                  const CBioseq& bsp,
                  CBioseq_Handle obsh,
                  CRef<CSeqsetIndex> prnt,
                  CSeq_entry_Handle tseh,
                  CRef<CScope> scope,
                  CSeqMasterIndex& idx,
                  CSeqEntryIndex::EPolicy policy,
                  CSeqEntryIndex::TFlags flags);

    // Destructor
    ~CBioseqIndex (void);

private:
    // Prohibit copy constructor & assignment operator
    CBioseqIndex (const CBioseqIndex&) = delete;
    CBioseqIndex& operator= (const CBioseqIndex&) = delete;

public:
    // Gap exploration iterator
    template<typename Fnc> size_t IterateGaps (Fnc m);

    // Descriptor exploration iterator
    template<typename Fnc> size_t IterateDescriptors (Fnc m);

    // Feature exploration iterator
    template<typename Fnc> size_t IterateFeatures (Fnc m);
    template<typename Fnc> size_t IterateFeatures (CSeq_loc& slp, Fnc m);

    // Getters
    CBioseq_Handle GetBioseqHandle (void) const { return m_Bsh; }
    const CBioseq& GetBioseq (void) const { return m_Bsp; }
    CBioseq_Handle GetOrigBioseqHandle (void) const { return m_OrigBsh; }
    CRef<CSeqsetIndex> GetParent (void) const { return m_Prnt; }
    CRef<CScope> GetScope (void) const { return m_Scope; }
    CRef<CSeqVector> GetSeqVector (void) const { return m_SeqVec; }

    // Get master index
    CWeakRef<CSeqMasterIndex> GetSeqMasterIndex (void) const { return m_Idx; }

    // Get sequence letters from Bioseq
    string GetSequence (void);
    void GetSequence (string& buffer);
    // Get sequence letters from Bioseq subrange
    string GetSequence (int from, int to);
    void GetSequence (int from, int to, string& buffer);

    // Map from GetBestGene result to CFeatureIndex object
    CRef<CFeatureIndex> GetFeatIndex (const CMappedFeat& mf);

    const vector<CRef<CGapIndex>>& GetGapIndices(void);

    const vector<CRef<CDescriptorIndex>>& GetDescriptorIndices(void);

    const vector<CRef<CFeatureIndex>>& GetFeatureIndices(void);

    // Get feature (CDS, mRNA, Prot) with product pointing to this Bioseq (protein, cDNA, peptide)
    CRef<CFeatureIndex> GetFeatureForProduct(void);

    // Get Bioseq index containing feature with product pointing to this Bioseq
    CWeakRef<CBioseqIndex> GetBioseqForProduct (void);

    // Get best (longest) protein feature on this protein Bioseq
    CRef<CFeatureIndex> GetBestProteinFeature(void);

    // Flag to indicate failure to fetch remote sequence components or feature annotation
    bool IsFetchFailure (void) const { return m_FetchFailure; }

    void SetFetchFailure (bool fails) { m_FetchFailure = fails; }

public:
    // Seq-inst fields
    bool IsNA (void) const {  return m_IsNA; }
    bool IsAA (void) const { return m_IsAA; }
    CSeq_inst::TTopology GetTopology (void) const { return m_Topology; }
    CSeq_inst::TLength GetLength (void) const { return m_Length; }

    bool IsDelta (void) const { return m_IsDelta; }
    bool IsDeltaLitOnly (void) const { return m_IsDeltaLitOnly; }
    bool IsVirtual (void) const { return m_IsVirtual; }
    bool IsMap (void) const { return m_IsMap; }

    // Seq-id fields
    const string& GetAccession (void) const { return m_Accession; }

    bool IsRefSeq (void) const { return m_IsRefSeq; }
    bool IsNC (void) const { return m_IsNC; }
    bool IsNM (void) const { return m_IsNM; }
    bool IsNR (void) const { return m_IsNR; }
    bool IsNZ (void) const { return m_IsNZ; }
    bool IsPatent (void) const { return m_IsPatent; }
    bool IsPDB (void) const { return m_IsPDB; }
    bool IsWP (void) const { return m_IsWP; }
    bool IsThirdParty (void) const { return m_ThirdParty; }
    bool IsWGSMaster (void) const { return m_WGSMaster; }
    bool IsTSAMaster (void) const { return m_TSAMaster; }
    bool IsTLSMaster (void) const { return m_TLSMaster; }

    string GetGeneralStr (void) const { return m_GeneralStr; }
    int GetGeneralId (void) const { return m_GeneralId; }

    string GetPatentCountry (void) const { return m_PatentCountry; }
    string GetPatentNumber (void) const { return m_PatentNumber; }
    int GetPatentSequence (void) const { return m_PatentSequence; }

    int GetPDBChain (void) const { return m_PDBChain; }
    string GetPDBChainID (void) const { return m_PDBChainID; }

    // Most important descriptor fields

    const string& GetTitle (void);

    CConstRef<CMolInfo> GetMolInfo (void);
    CMolInfo::TBiomol GetBiomol (void);
    CMolInfo::TTech GetTech (void);
    CMolInfo::TCompleteness GetCompleteness (void);

    CConstRef<CBioSource> GetBioSource (void);
    const string& GetTaxname (void);

    const string& GetDescTaxname (void);

    bool IsHTGTech (void);
    bool IsHTGSUnfinished (void);
    bool IsTLS (void);
    bool IsTSA (void);
    bool IsWGS (void);
    bool IsEST_STS_GSS (void);

    bool IsUseBiosrc (void);

    const string& GetCommon (void);
    const string& GetLineage (void);
    TTaxId GetTaxid (void);
    bool IsUsingAnamorph (void);

    CTempString GetGenus (void);
    CTempString GetSpecies (void);
    bool IsMultispecies (void);
    CBioSource::TGenome GetGenome (void);
    bool IsPlasmid (void);
    bool IsChromosome (void);

    const string& GetOrganelle (void);

    string GetFirstSuperKingdom (void);
    string GetSecondSuperKingdom (void);
    bool IsCrossKingdom (void);

    CTempString GetChromosome (void);
    CTempString GetLinkageGroup (void);
    CTempString GetClone (void);
    bool HasClone (void);
    CTempString GetMap (void);
    CTempString GetPlasmid (void);
    CTempString GetSegment (void);

    CTempString GetBreed (void);
    CTempString GetCultivar (void);
    CTempString GetSpecimenVoucher (void);
    CTempString GetIsolate (void);
    CTempString GetStrain (void);
    CTempString GetSubstrain (void);
    CTempString GetMetaGenomeSource (void);

    bool IsHTGSCancelled (void);
    bool IsHTGSDraft (void);
    bool IsHTGSPooled (void);
    bool IsTPAExp (void);
    bool IsTPAInf (void);
    bool IsTPAReasm (void);
    bool IsUnordered (void);

    CTempString GetPDBCompound (void);

    bool IsForceOnlyNearFeats (void);

    bool IsUnverified (void);
    bool IsUnverifiedFeature (void);
    bool IsUnverifiedOrganism (void);
    bool IsUnverifiedMisassembled (void);
    bool IsUnverifiedContaminant (void);

    bool IsUnreviewed (void);
    bool IsUnreviewedUnannotated (void);

    CTempString GetTargetedLocus (void);

    const string& GetComment (void);
    bool IsPseudogene (void);

    bool HasOperon (void);
    bool HasGene (void);
    bool HasMultiIntervalGenes (void);
    bool HasSource (void);

    string GetrEnzyme (void);

private:
    // Common gap collection, delayed until actually needed
    void x_InitGaps (void);

    // Common descriptor collection, delayed until actually needed
    void x_InitDescs (void);

    // Common feature collection, delayed until actually needed
    void x_InitFeats (void);
    void x_InitFeats (CSeq_loc& slp);

    void x_DefaultSelector(SAnnotSelector& sel, CSeqEntryIndex::EPolicy policy, CSeqEntryIndex::TFlags flags, bool onlyNear, CScope& scope);

    // common implementation method
    void x_InitFeats (CSeq_loc* slpp);

    // Set BioSource flags
    void x_InitSource (void);

private:
    CBioseq_Handle m_Bsh;
    const CBioseq& m_Bsp;
    CBioseq_Handle m_OrigBsh;
    CRef<CSeqsetIndex> m_Prnt;
    CSeq_entry_Handle m_Tseh;
    CRef<CScope> m_Scope;

    CWeakRef<CSeqMasterIndex> m_Idx;

    bool m_GapsInitialized;
    vector<CRef<CGapIndex>> m_GapList;

    bool m_DescsInitialized;
    vector<CRef<CDescriptorIndex>> m_SdxList;

    bool m_FeatsInitialized;
    vector<CRef<CFeatureIndex>> m_SfxList;

    bool m_SourcesInitialized;

    bool m_FeatForProdInitialized;
    CRef<CFeatureIndex> m_FeatureForProduct;

    bool m_BestProtFeatInitialized;
    CRef<CFeatureIndex> m_BestProteinFeature;

    // CFeatureIndex from CMappedFeat for use with GetBestGene
    typedef map<CMappedFeat, CRef<CFeatureIndex> > TFeatIndexMap;
    TFeatIndexMap m_FeatIndexMap;

    CRef<CSeqVector> m_SeqVec;

    CSeqEntryIndex::EPolicy m_Policy;
    CSeqEntryIndex::TFlags m_Flags;

    bool m_FetchFailure;

private:
    // Seq-inst fields
    bool m_IsNA;
    bool m_IsAA;
    CSeq_inst::TTopology m_Topology;
    CSeq_inst::TLength m_Length;

    bool m_IsDelta;
    bool m_IsDeltaLitOnly;
    bool m_IsVirtual;
    bool m_IsMap;

    // Seq-id fields
    string m_Accession;

    bool m_IsRefSeq;
    bool m_IsNC;
    bool m_IsNM;
    bool m_IsNR;
    bool m_IsNZ;
    bool m_IsPatent;
    bool m_IsPDB;
    bool m_IsWP;
    bool m_ThirdParty;
    bool m_WGSMaster;
    bool m_TSAMaster;
    bool m_TLSMaster;

    string m_GeneralStr;
    int m_GeneralId;

    string m_PatentCountry;
    string m_PatentNumber;
    int m_PatentSequence;

    int m_PDBChain;
    string m_PDBChainID;

    // Instantiated title
    string m_Title;

    // MolInfo fields
    CConstRef<CMolInfo> m_MolInfo;
    CMolInfo::TBiomol m_Biomol;
    CMolInfo::TTech m_Tech;
    CMolInfo::TCompleteness m_Completeness;

    bool m_HTGTech;
    bool m_HTGSUnfinished;
    bool m_IsTLS;
    bool m_IsTSA;
    bool m_IsWGS;
    bool m_IsEST_STS_GSS;

    bool m_UseBiosrc;

    // BioSource fields
    CConstRef<CBioSource> m_DescBioSource;
    string m_DescTaxname;

    CConstRef<CBioSource> m_BioSource;
    string m_Taxname;

    string m_Common;
    string m_Lineage;
    TTaxId m_Taxid;
    bool m_UsingAnamorph;

    CTempString m_Genus;
    CTempString m_Species;
    bool m_Multispecies;
    CBioSource::TGenome m_Genome;
    bool m_IsPlasmid;
    bool m_IsChromosome;

    string m_Organelle;

    string m_FirstSuperKingdom;
    string m_SecondSuperKingdom;
    bool m_IsCrossKingdom;

    // Subsource fields
    CTempString m_Chromosome;
    CTempString m_LinkageGroup;
    CTempString m_Clone;
    bool m_has_clone;
    CTempString m_Map;
    CTempString m_Plasmid;
    CTempString m_Segment;

    // Orgmod fields
    CTempString m_Breed;
    CTempString m_Cultivar;
    CTempString m_SpecimenVoucher;
    CTempString m_Isolate;
    CTempString m_Strain;
    CTempString m_Substrain;
    CTempString m_MetaGenomeSource;

    // Keyword fields (genbank or embl blocks)
    bool m_HTGSCancelled;
    bool m_HTGSDraft;
    bool m_HTGSPooled;
    bool m_TPAExp;
    bool m_TPAInf;
    bool m_TPAReasm;
    bool m_Unordered;

    // PDB block fields
    CTempString m_PDBCompound;

    // User object fields
    bool m_ForceOnlyNearFeats;

    bool m_IsUnverified;
    bool m_IsUnverifiedFeature;
    bool m_IsUnverifiedOrganism;
    bool m_IsUnverifiedMisassembled;
    bool m_IsUnverifiedContaminant;
    CTempString m_UnverifiedPrefix;

    bool m_IsUnreviewed;
    bool m_IsUnreviewedUnannotated;
    CTempString m_UnreviewedPrefix;
    CTempString m_TargetedLocus;

    // Comment fields
    string m_Comment;
    bool m_IsPseudogene;

    // Feature fields
    bool m_HasGene;
    bool m_HasMultiIntervalGenes;
    bool m_HasSource;

    // Map fields
    string m_rEnzyme;
};


// CGapIndex
//
// CGapIndex stores information about an indexed descriptor
class NCBI_XOBJUTIL_EXPORT CGapIndex : public CObject
{
public:
    // Constructor
    CGapIndex (TSeqPos start,
               TSeqPos end,
               TSeqPos length,
               const string& type,
               const vector<string>& evidence,
               bool isUnknownLength,
               bool isAssemblyGap,
               CBioseqIndex& bsx);

private:
    // Prohibit copy constructor & assignment operator
    CGapIndex (const CGapIndex&) = delete;
    CGapIndex& operator= (const CGapIndex&) = delete;

public:
    // Getters

    TSeqPos GetStart (void) const { return m_Start; }
    TSeqPos GetEnd (void) const { return m_End; }
    TSeqPos GetLength (void) const { return m_Length; }
    const string GetGapType (void) const { return m_GapType; }
    const vector<string>& GetGapEvidence (void) const { return m_GapEvidence; }
    bool IsUnknownLength (void) const { return m_IsUnknownLength; }
    bool IsAssemblyGap (void) const { return m_IsAssemblyGap; }

    // Get parent Bioseq index
    CWeakRef<CBioseqIndex> GetBioseqIndex (void) const { return m_Bsx; }

private:
    CWeakRef<CBioseqIndex> m_Bsx;

    TSeqPos m_Start;
    TSeqPos m_End;
    TSeqPos m_Length;

    string m_GapType;
    vector<string> m_GapEvidence;

    bool m_IsUnknownLength;
    bool m_IsAssemblyGap;
};


// CDescriptorIndex
//
// CDescriptorIndex stores information about an indexed descriptor
class NCBI_XOBJUTIL_EXPORT CDescriptorIndex : public CObject
{
public:
    // Constructor
    CDescriptorIndex (const CSeqdesc& sd,
                      CBioseqIndex& bsx);

private:
    // Prohibit copy constructor & assignment operator
    CDescriptorIndex (const CDescriptorIndex&) = delete;
    CDescriptorIndex& operator= (const CDescriptorIndex&) = delete;

public:
    // Getters
    const CSeqdesc& GetSeqDesc (void) const { return m_Sd; }

    // Get parent Bioseq index
    CWeakRef<CBioseqIndex> GetBioseqIndex (void) const { return m_Bsx; }

    // Get descriptor type (e.g., CSeqdesc::e_Molinfo)
    CSeqdesc::E_Choice GetType (void) const { return m_Type; }

private:
    const CSeqdesc& m_Sd;
    CWeakRef<CBioseqIndex> m_Bsx;

    CSeqdesc::E_Choice m_Type;
};


// CFeatureIndex
//
// CFeatureIndex stores information about an indexed feature
class NCBI_XOBJUTIL_EXPORT CFeatureIndex : public CObject
{
public:
    // Constructor
    CFeatureIndex (CSeq_feat_Handle sfh,
                   const CMappedFeat mf,
                   CConstRef<CSeq_loc> feat_loc,
                   CBioseqIndex& bsx);

private:
    // Prohibit copy constructor & assignment operator
    CFeatureIndex (const CFeatureIndex&) = delete;
    CFeatureIndex& operator= (const CFeatureIndex&) = delete;

public:
    // Getters
    CSeq_feat_Handle GetSeqFeatHandle (void) const { return m_Sfh; }
    const CMappedFeat GetMappedFeat (void) const { return m_Mf; }
    CRef<CSeqVector> GetSeqVector (void) const { return m_SeqVec; }

    CConstRef<CSeq_loc> GetMappedLocation(void) const { return m_Fl; }

    // Get parent Bioseq index
    CWeakRef<CBioseqIndex> GetBioseqIndex (void) const { return m_Bsx; }

    // Get feature type (e.g. CSeqFeatData::e_Rna)
    CSeqFeatData::E_Choice GetType (void) const { return m_Type; }

    // Get feature subtype (e.g. CSeqFeatData::eSubtype_mRNA)
    CSeqFeatData::ESubtype GetSubtype (void) const { return m_Subtype; }

    TSeqPos GetStart (void) const { return m_Start; }
    TSeqPos GetEnd (void) const { return m_End; }

    // Get sequence letters under feature intervals
    string GetSequence (void);
    void GetSequence (string& buffer);
    // Get sequence letters under feature subrange
    string GetSequence (int from, int to);
    void GetSequence (int from, int to, string& buffer);

    // Map from feature to CFeatureIndex for best gene using CFeatTree in parent CBioseqIndex
    CRef<CFeatureIndex> GetBestGene (void);

    // Map from feature to CFeatureIndex for best VDJC parent using CFeatTree in parent CBioseqIndex
    CRef<CFeatureIndex> GetBestParent (void);

    // Find CFeatureIndex object for overlapping source feature using internal CFeatTree
    CRef<CFeatureIndex> GetOverlappingSource (void);

private:
    void SetFetchFailure (bool fails);

private:
    CSeq_feat_Handle m_Sfh;
    const CMappedFeat m_Mf;
    CConstRef<CSeq_loc> m_Fl;
    CRef<CSeqVector> m_SeqVec;
    CWeakRef<CBioseqIndex> m_Bsx;

    CSeqFeatData::E_Choice m_Type;
    CSeqFeatData::ESubtype m_Subtype;

    TSeqPos m_Start;
    TSeqPos m_End;
};


// CWordPairIndexer
//
// CWordPairIndexer generates normalized terms and adjacent word pairs for Entrez indexing
class NCBI_XOBJUTIL_EXPORT CWordPairIndexer
{
public:
    // Constructor
    CWordPairIndexer (void) { }

private:
    // Prohibit copy constructor & assignment operator
    CWordPairIndexer (const CWordPairIndexer&) = delete;
    CWordPairIndexer& operator= (const CWordPairIndexer&) = delete;

public:
    void PopulateWordPairIndex (string str);

    template<typename Fnc> void IterateNorm (Fnc m);
    template<typename Fnc> void IteratePair (Fnc m);

public:
    static string ConvertUTF8ToAscii(const string& str);
    static string TrimPunctuation (const string& str);
    static string TrimMixedContent (const string& str);
    static bool IsStopWord(const string& str);

    const vector<string>& GetNorm (void) const { return m_Norm; }
    const vector<string>& GetPair (void) const { return m_Pair; }

private:
    string x_AddToWordPairIndex (string item, string prev);

    vector<string> m_Norm;
    vector<string> m_Pair;
};


// Inline lambda function implementations

// Visit CBioseqIndex objects for all Bioseqs
template<typename Fnc>
inline
size_t CSeqEntryIndex::IterateBioseqs (Fnc m)

{
    return m_Idx->IterateBioseqs(m);
}

template<typename Fnc>
inline
size_t CSeqMasterIndex::IterateBioseqs (Fnc m)

{
    int count = 0;
    for (auto& bsx : m_BsxList) {
        m(*bsx);
        count++;
    }
    return count;
}

// Visit CSeqsetIndex objects for all Seqsets
template<typename Fnc>
inline
size_t CSeqEntryIndex::IterateSeqsets (Fnc m)

{
    return m_Idx->IterateSeqsets(m);
}

template<typename Fnc>
inline
size_t CSeqMasterIndex::IterateSeqsets (Fnc m)

{
    int count = 0;
    for (auto& ssx : m_SsxList) {
        m(*ssx);
        count++;
    }
    return count;
}

// Visit CGapIndex objects for all gaps
template<typename Fnc>
inline
size_t CBioseqIndex::IterateGaps (Fnc m)

{
    int count = 0;
    try {
        // Delay gap collection until first request
        if (! m_GapsInitialized) {
            x_InitGaps();
        }

        for (auto& sgx : m_GapList) {
            count++;
            m(*sgx);
        }
    }
    catch (CException& e) {
        ERR_POST(Error << "Error in CBioseqIndex::IterateGaps: " << e.what());
    }
    return count;
}

// Visit CDescriptorIndex objects for all descriptors
template<typename Fnc>
inline
size_t CBioseqIndex::IterateDescriptors (Fnc m)

{
    int count = 0;
    try {
        // Delay descriptor collection until first request
        if (! m_DescsInitialized) {
            x_InitDescs();
        }

        for (auto& sdx : m_SdxList) {
            count++;
            m(*sdx);
        }
    }
    catch (CException& e) {
        ERR_POST(Error << "Error in CBioseqIndex::IterateDescriptors: " << e.what());
    }
    return count;
}

// Visit CFeatureIndex objects for all features
template<typename Fnc>
inline
size_t CBioseqIndex::IterateFeatures (Fnc m)

{
    int count = 0;
    try {
        // Delay feature collection until first request
        if (! m_FeatsInitialized) {
            x_InitFeats();
        }

        for (auto& sfx : m_SfxList) {
            count++;
            m(*sfx);
        }
    }
    catch (CException& e) {
        ERR_POST(Error << "Error in CBioseqIndex::IterateFeatures: " << e.what());
    }
    return count;
}

template<typename Fnc>
inline
size_t CBioseqIndex::IterateFeatures (CSeq_loc& slp, Fnc m)

{
    int count = 0;
    try {
        // Delay feature collection until first request, but do not bail on m_FeatsInitialized flag
        x_InitFeats(slp);

        for (auto& sfx : m_SfxList) {
            count++;
            m(*sfx);
        }
    }
    catch (CException& e) {
        ERR_POST(Error << "Error in CBioseqIndex::IterateFeatures: " << e.what());
    }
    return count;
}

template<typename Fnc>
inline
void CWordPairIndexer::IterateNorm (Fnc m)

{
    for (auto& str : m_Norm) {
        m(str);
    }
}

template<typename Fnc>
inline
void CWordPairIndexer::IteratePair (Fnc m)

{
    for (auto& str : m_Pair) {
        m(str);
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE

#endif  /* FEATURE_INDEXER__HPP */
