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
 *   'seqfeat.asn'.
 *
 * NOTE TO DEVELOPERS:
 *   The Seq-feat structure in 'seqfeat.asn' is subtyped into various 
 *   more specialized structures by the content of the Seqfeat::data 
 *   item. Depending on what is in the data item, other fields in 
 *   Seq-feat (the ones marked optional in the ASN) are allowed or 
 *   prohibited from being present. This file lists the possible 
 *   subtypes of Seq-feat, along with the qualifiers that can occur 
 *   within the Seq-feat structure.
 *
 * WHEN EDITING THE LIST OF QUALIFIERS (i.e. the EQualifier enum):
 * - add or remove the qualifier to the various lists that define which
 *   qualifiers are legal for a given subtype 
 *   (in src/objects/seqfeat/SeqFeatData.cpp),
 * - add or remove the qualifier to the list of qualifiers known by the
 *   flatfile generator 
 *   (in include/objtools/seqfeat/items/flat_file_slots.hpp),
 * - add or remove processing code in the flat-file generator 
 *   (src/objtools/feature_item.cpp). This will probably necessitate 
 *   the addition or removal of a class that knows how to format the 
 *   new qualifier for display; look at the code for a similar 
 *   established qualifier for an idea of what needs to be done
 *   (check WHEN EDITING THE LIST OF QUALIFIERS comment in that file
 *   for additonal hints),
 * - add or remove the qualifier to the list of qualifiers known to 
 *   the feature table reader (in src/objtools/reader/readfeat.cpp); 
 *   it's an independent project with its own book-keeping concerning 
 *   qualifiers, but needs to be kept in sync,
 * - make sure corresponding code gets added to the validator 
 *   (in src/objtools/validator/...) which is another project with an 
 *   independent qualifier list that nonetheless needs to stay in sync,
 * - (additional subitems to be added as I become aware of them).
 *
 */

#ifndef OBJECTS_SEQFEAT_SEQFEATDATA_HPP
#define OBJECTS_SEQFEAT_SEQFEATDATA_HPP


// generated includes
#include <objects/seqfeat/SeqFeatData_.hpp>
#include <set>

#include <corelib/ncbistr.hpp>
#include <util/static_map.hpp>

// generated classes

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

class CFeatList;
class CBondList;
class CSiteList;

class NCBI_SEQFEAT_EXPORT CSeqFeatData : public CSeqFeatData_Base
{
    typedef CSeqFeatData_Base Tparent;
public:
    // constructor
    CSeqFeatData(void);
    // destructor
    ~CSeqFeatData(void);

    // ASCII representation of subtype (GenBank feature key, e.g.)
    enum EVocabulary {
        eVocabulary_full,
        eVocabulary_genbank
    };
    string GetKey(EVocabulary vocab = eVocabulary_full) const;

    enum ESubtype {
        // These no longer need to match the FEATDEF values in the
        // C toolkit's objfdef.h
        eSubtype_bad = 0,
        eSubtype_gene,
        eSubtype_org,
        eSubtype_cdregion,
        eSubtype_prot,
        eSubtype_preprotein,
        eSubtype_mat_peptide_aa,
        eSubtype_sig_peptide_aa,
        eSubtype_transit_peptide_aa,
        eSubtype_preRNA,
        eSubtype_mRNA,
        eSubtype_tRNA,
        eSubtype_rRNA,
        eSubtype_snRNA,
        eSubtype_scRNA,
        eSubtype_snoRNA,
        eSubtype_otherRNA,
        eSubtype_pub,
        eSubtype_seq,
        eSubtype_imp,
        eSubtype_allele,
        eSubtype_attenuator,
        eSubtype_C_region,
        eSubtype_CAAT_signal,
        eSubtype_Imp_CDS,
        eSubtype_conflict,
        eSubtype_D_loop,
        eSubtype_D_segment,
        eSubtype_enhancer,
        eSubtype_exon,
        eSubtype_EC_number,
        eSubtype_GC_signal,
        eSubtype_iDNA,
        eSubtype_intron,
        eSubtype_J_segment,
        eSubtype_LTR,
        eSubtype_mat_peptide,
        eSubtype_misc_binding,
        eSubtype_misc_difference,
        eSubtype_misc_feature,
        eSubtype_misc_recomb,
        eSubtype_misc_RNA,
        eSubtype_misc_signal,
        eSubtype_misc_structure,
        eSubtype_modified_base,
        eSubtype_mutation,
        eSubtype_N_region,
        eSubtype_old_sequence,
        eSubtype_polyA_signal,
        eSubtype_polyA_site,
        eSubtype_precursor_RNA,
        eSubtype_prim_transcript,
        eSubtype_primer_bind,
        eSubtype_promoter,
        eSubtype_protein_bind,
        eSubtype_RBS,
        eSubtype_repeat_region,
        eSubtype_repeat_unit,
        eSubtype_rep_origin,
        eSubtype_S_region,
        eSubtype_satellite,
        eSubtype_sig_peptide,
        eSubtype_source,
        eSubtype_stem_loop,
        eSubtype_STS,
        eSubtype_TATA_signal,
        eSubtype_terminator,
        eSubtype_transit_peptide,
        eSubtype_unsure,
        eSubtype_V_region,
        eSubtype_V_segment,
        eSubtype_variation,
        eSubtype_virion,
        eSubtype_3clip,
        eSubtype_3UTR,
        eSubtype_5clip,
        eSubtype_5UTR,
        eSubtype_10_signal,
        eSubtype_35_signal,
        eSubtype_gap,
        eSubtype_operon,
        eSubtype_oriT,
        eSubtype_site_ref,
        eSubtype_region,
        eSubtype_comment,
        eSubtype_bond,
        eSubtype_site,
        eSubtype_rsite,
        eSubtype_user,
        eSubtype_txinit,
        eSubtype_num,
        eSubtype_psec_str,
        eSubtype_non_std_residue,
        eSubtype_het,
        eSubtype_biosrc,
        eSubtype_max,
        eSubtype_any = 255
    };
    ESubtype GetSubtype(void) const;
    
    // Call after changes that could affect the subtype
    void InvalidateSubtype(void);

    static E_Choice GetTypeFromSubtype (ESubtype subtype);

    // List of available qualifiers for feature keys.
    // For more information see: 
    //   The DDBJ/EMBL/GenBank Feature Table: Definition
    //   (http://www.ncbi.nlm.nih.gov/projects/collab/FT/index.html)
    enum EQualifier
    {
        eQual_bad = 0,
        eQual_allele,
        eQual_anticodon,
        eQual_bound_moiety,
        eQual_cell_line,
        eQual_cell_type,
        eQual_chromosome,
        eQual_citation,
        eQual_clone,
        eQual_clone_lib,
        eQual_codon,
        eQual_codon_start,
        eQual_compare,
        eQual_cons_splice,
        eQual_country,
        eQual_cultivar,
        eQual_db_xref,
        eQual_dev_stage,
        eQual_direction,
        eQual_EC_number,
        eQual_ecotype,
        eQual_environmental_sample,
        eQual_estimated_length,
        eQual_evidence,
        eQual_exception,
        eQual_experiment,
        eQual_focus,
        eQual_frequency,
        eQual_function,
        eQual_gene,
        eQual_germline,
        eQual_haplotype,
        eQual_inference,
        eQual_insertion_seq,
        eQual_isolate,
        eQual_isolation_source,
        eQual_label,
        eQual_lab_host,
        eQual_locus_tag,
        eQual_map,
        eQual_macronuclear,
        eQual_mod_base,
        eQual_mol_type,
        eQual_note,
        eQual_number,
        eQual_old_locus_tag,
        eQual_operon,
        eQual_organelle,
        eQual_organism,
        eQual_partial,
        eQual_PCR_conditions,
        eQual_phenotype,
        eQual_pop_variant,
        eQual_plasmid,
        eQual_product,
        eQual_protein_id,
        eQual_proviral,
        eQual_pseudo,
        eQual_rearranged,
        eQual_replace,
        eQual_ribosomal_slippage,
        eQual_rpt_family,
        eQual_rpt_type,
        eQual_rpt_unit,
        eQual_rpt_unit_range,
        eQual_rpt_unit_seq,
        eQual_segment,
        eQual_sequence_mol,
        eQual_serotype,
        eQual_serovar,
        eQual_sex,
        eQual_specific_host,
        eQual_specimen_voucher,
        eQual_standard_name,
        eQual_strain,
        eQual_sub_clone,
        eQual_sub_species,
        eQual_sub_strain,
        eQual_tissue_lib,
        eQual_tissue_type,
        eQual_trans_splicing,
        eQual_transgenic,
        eQual_translation,
        eQual_transl_except,
        eQual_transl_table,
        eQual_transposon,
        eQual_usedin,
        eQual_variety,
        eQual_virion
    };
    typedef vector<EQualifier> TQualifiers;

    // Test wheather a certain qualifier is legal for the feature
    bool IsLegalQualifier(EQualifier qual) const;
    static bool IsLegalQualifier(ESubtype subtype, EQualifier qual);

    // Get a list of all the legal qualifiers for the feature.
    const TQualifiers& GetLegalQualifiers(void) const;
    static const TQualifiers& GetLegalQualifiers(ESubtype subtype);

    // Get the list of all mandatory qualifiers for the feature.
    const TQualifiers& GetMandatoryQualifiers(void) const;
    static const TQualifiers& GetMandatoryQualifiers(ESubtype subtype);
    
    // Convert a qualifier from an enumerated value to a string representation.
    static const string& GetQulifierAsString(EQualifier qual);

    static const CFeatList* GetFeatList();
    static const CBondList* GetBondList();
    static const CSiteList* GetSiteList();

private:
    mutable ESubtype m_Subtype; // cached

    static void s_InitSubtypesTable(void);
    static void s_InitLegalQuals(void);
    static void s_InitMandatoryQuals(void);

    // Prohibit copy constructor and assignment operator
    CSeqFeatData(const CSeqFeatData& value);
    CSeqFeatData& operator=(const CSeqFeatData& value);
    
};


/////////////////// CSeqFeatData inline methods

// constructor
inline
CSeqFeatData::CSeqFeatData(void) : m_Subtype(eSubtype_any)
{
}


inline
void CSeqFeatData::InvalidateSubtype(void)
{
    m_Subtype = eSubtype_any;
}


inline
bool CSeqFeatData::IsLegalQualifier(EQualifier qual) const
{
    return IsLegalQualifier(GetSubtype(), qual);
}


inline
const CSeqFeatData::TQualifiers& CSeqFeatData::GetLegalQualifiers(void) const
{
    return GetLegalQualifiers(GetSubtype());
}


inline
const CSeqFeatData::TQualifiers& CSeqFeatData::GetMandatoryQualifiers(void) const
{
    return GetMandatoryQualifiers(GetSubtype());
}

/////////////////// end of CSeqFeatData inline methods




/// CFeatListItem - basic configuration data for one "feature" type.
/// "feature" expanded to include things that are not SeqFeats.
class NCBI_SEQFEAT_EXPORT CFeatListItem
{
public:
    CFeatListItem() {}
    CFeatListItem(int type, int subtype, const char* desc, const char* key)
        : m_Type(type)
        , m_Subtype(subtype)
        , m_Description(desc)
        , m_StorageKey(key) {}
        
    bool operator<(const CFeatListItem& rhs) const;
    
    int         GetType() const;
    int         GetSubtype() const;
    string      GetDescription() const;
    string      GetStoragekey() const;
    
private:
    int         m_Type;         ///< Feature type, or e_not_set for default values.
    int         m_Subtype;      ///< Feature subtype or eSubtype_any for default values.
    string      m_Description;  ///< a string for display purposes.
    string      m_StorageKey;   ///< a short string to use as a key or part of a key
                                ///< when storing a value by key/value string pairs.
};


// Inline methods.

inline
int CFeatListItem::GetType() const
{ return m_Type; }

inline
int CFeatListItem::GetSubtype() const
{ return m_Subtype; }

inline
string CFeatListItem::GetDescription() const
{ return m_Description; }

inline
string CFeatListItem::GetStoragekey() const
{ return m_StorageKey; }


/// CConfigurableItems - a static list of items that can be configured.
///
/// One can iterate through all items. Iterators dereference to CFeatListItem.
/// One can also get an item by type and subtype.

class NCBI_SEQFEAT_EXPORT CFeatList
{
private:
    typedef set<CFeatListItem>    TFeatTypeContainer;
    
public:
    
    CFeatList();
    ~CFeatList();
    
    bool    TypeValid(int type, int subtype) const;
    
    /// can get all static information for one type/subtype.
    bool    GetItem(int type, int subtype, CFeatListItem& config_item) const;
    bool    GetItemBySubtype(int subtype,  CFeatListItem& config_item) const;
    
    bool    GetItemByDescription(const string& desc, CFeatListItem& config_item) const;
    bool    GetItemByKey(const string& key, CFeatListItem& config_item) const;
    
    /// Get the displayable description of this type of feature.
    string      GetDescription(int type, int subtype) const;
    
    /// Get the feature's type and subtype from its description.
    /// return true on success, false if type and subtype are not valid.
    bool        GetTypeSubType(const string& desc, int& type, int& subtype) const;
    
    /// Get the key used to store this type of feature.
    string      GetStoragekey(int type, int subtype) const;
    /// Get the key used to store this type of feature by only subtype.
    string      GetStoragekey(int subtype) const;
    
    /// Get hierarchy of keys above this subtype, starting with "Master"
    /// example, eSubtype_gene, will return {"Master", "Gene"}
    /// but eSubtype_allele will return {"Master", "Import", "allele"}
    vector<string>  GetStoragekeys(int subtype) const;
    
    /// return a list of all the feature descriptions for a menu or other control.
    /// if hierarchical is true use in an Fl_Menu_ descendant to make hierarchical menus.
    void    GetDescriptions(
                            vector<string> &descs,          ///> output, list of description strings.
                            bool hierarchical = false       ///> make descriptions hierachical, separated by '/'.
                            ) const;
    
    // Iteratable list of key values (type/subtype).
    // can iterate through all values including defaults or with only
    // real Feature types/subtypes.
    typedef TFeatTypeContainer::const_iterator const_iterator;
    
    size_t          size() const;
    const_iterator  begin() const;
    const_iterator  end() const;
private:
        /// initialize our container of feature types and descriptions.
        void    x_Init(void);
    
    TFeatTypeContainer    m_FeatTypes;    ///> all valid types and subtypes.
    
    typedef map<int, CFeatListItem>   TSubtypeMap;
    TSubtypeMap    m_FeatTypeMap; ///> indexed by subtype only.
    
};


inline
size_t CFeatList::size() const
{
    return m_FeatTypes.size();
}


inline
CFeatList::const_iterator CFeatList::begin() const
{
    return m_FeatTypes.begin();
}


inline
CFeatList::const_iterator CFeatList::end() const
{
    return m_FeatTypes.end();
}


class NCBI_SEQFEAT_EXPORT CBondList
{
public:
    typedef pair <const char *, const CSeqFeatData::EBond> TBondKey;

private:
    typedef CStaticArrayMap <const char*, const CSeqFeatData::EBond, PNocase_CStr> TBondMap;
    
public:
    
    CBondList();
    ~CBondList();
    
    bool    IsBondName (string str) const;
    bool    IsBondName (string str, CSeqFeatData::EBond& bond_type) const;
    CSeqFeatData::EBond GetBondType (string str) const;
            
    // Iteratable list of key values (type/subtype).
    // can iterate through all values including defaults or with only
    // real Feature types/subtypes.
    typedef TBondMap::const_iterator const_iterator;
    
    size_t          size() const;
    const_iterator  begin() const;
    const_iterator  end() const;
private:
        /// initialize our container of feature types and descriptions.
        void    x_Init(void);
    
    DECLARE_CLASS_STATIC_ARRAY_MAP(TBondMap, sm_BondKeys);
    
};


inline
size_t CBondList::size() const
{
    return sm_BondKeys.size();
}


inline
CBondList::const_iterator CBondList::begin() const
{
    return sm_BondKeys.begin();
}


inline
CBondList::const_iterator CBondList::end() const
{
    return sm_BondKeys.end();
}


class NCBI_SEQFEAT_EXPORT CSiteList
{
public:
    typedef pair <const char *, const CSeqFeatData::ESite> TSiteKey;

private:
    typedef CStaticArrayMap <const char*, const CSeqFeatData::ESite, PNocase_CStr> TSiteMap;
    
public:
    
    CSiteList();
    ~CSiteList();
    
    bool    IsSiteName (string str) const;
    bool    IsSiteName (string str, CSeqFeatData::ESite& site_type) const;
    CSeqFeatData::ESite GetSiteType (string str) const;
            
    // Iteratable list of key values (type/subtype).
    // can iterate through all values including defaults or with only
    // real Feature types/subtypes.
    typedef TSiteMap::const_iterator const_iterator;
    
    size_t          size() const;
    const_iterator  begin() const;
    const_iterator  end() const;
private:
        /// initialize our container of feature types and descriptions.
        void    x_Init(void);
    
    DECLARE_CLASS_STATIC_ARRAY_MAP(TSiteMap, sm_SiteKeys);
    
};


inline
size_t CSiteList::size() const
{
    return sm_SiteKeys.size();
}


inline
CSiteList::const_iterator CSiteList::begin() const
{
    return sm_SiteKeys.begin();
}


inline
CSiteList::const_iterator CSiteList::end() const
{
    return sm_SiteKeys.end();
}



END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE


#endif // OBJECTS_SEQFEAT_SEQFEATDATA_HPP

/*
* ===========================================================================
*
* $Log$
* Revision 1.17  2006/08/08 17:14:09  dicuccio
* Make static array map data truly private
*
* Revision 1.16  2006/08/03 12:00:21  bollin
* added CBondList and CSiteList for listing the available site and bond types
* and their descriptive names
*
* Revision 1.15  2006/04/04 18:57:55  rsmith
* Remove obsolete declaration of GetFeatConfigList()
*
* Revision 1.14  2006/04/03 20:23:23  dicuccio
* Cosmetic clean-ups; promoted some functions to inline
*
* Revision 1.13  2006/04/03 17:06:19  rsmith
* move Feat Config LIst from gui/config to SeqFeatData
*
* Revision 1.12  2005/11/16 15:09:41  ludwigf
* ADDED: New GB qualifiers "/ribosomal_slippage" and "/trans_plicing".
*
* CHANGED: Turned any occurrences of qualifiers "/exception='ribosomal
* slippage'" and "/exception='trans-splicing'" into qualifiers
* "/ribosomal_slippage" or "/trans_splicing".
*
* Revision 1.11  2005/11/01 14:03:33  ludwigf
* ADDED: Additional qualifiers "rpt_unit_range" and "rpt_unit_seq". Both are
* implemented as GB_qualifiers with string values.
*
* Revision 1.10  2005/10/26 13:30:17  ludwigf
* Removed qualifier "evidence".
* Added qualifiers "experiment" and "inference".
*
* Revision 1.9  2005/09/30 19:12:37  vasilche
* Fixed MT-safety of tables initialization.
*
* Revision 1.8  2005/05/10 17:02:30  grichenk
* Optimized GetTypeFromSubtype().
*
* Revision 1.7  2004/08/19 14:56:02  shomrat
* Added qualifiers old_locus_tag and compare
*
* Revision 1.6  2004/05/19 14:39:42  shomrat
* Added list of qualifiers
*
* Revision 1.5  2003/10/24 17:13:49  shomrat
* added gap, operaon and oriT subtypes
*
* Revision 1.4  2003/04/19 16:38:06  dicuccio
* Remove compiler warning about nested c-style comments
*
* Revision 1.3  2003/04/18 21:20:44  kans
* regrouped ESubtype, removing most explicit numbers, added GetTypeFromSubtype, moved log to end of file
*
* Revision 1.2  2002/12/26 12:43:27  dicuccio
* Added Win32 export specifiers
*
* Revision 1.1  2001/10/30 20:25:56  ucko
* Implement feature labels/keys, subtypes, and sorting
*
*
* ===========================================================================
*/
/* Original file checksum: lines: 90, chars: 2439, CRC32: 742431cc */
