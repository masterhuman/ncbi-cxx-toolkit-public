/* ftablock.h
 *
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
 * File Name:  ftablock.h
 *
 * Author: Karl Sirotkin, Hsiu-Chuan Chen
 *
 * File Description:
 * -----------------
 */

#ifndef  _BLOCK_
#define  _BLOCK_

#include <objects/seqloc/Patent_seq_id.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seq/Linkage_evidence.hpp>
#include <objects/general/Date_std.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/OrgMod.hpp>
#include <objects/seqfeat/Genetic_code.hpp>
#include <objects/pub/Pub.hpp>
#include <objects/seq/Delta_seq.hpp>

#include <objtools/flatfile/flatfile_parse_info.hpp>
#include "valnode.h" 

BEGIN_NCBI_SCOPE

typedef std::list<CRef<objects::CSeq_feat> > TSeqFeatList;
typedef std::list<std::string> TAccessionList;
typedef std::list<CRef<objects::CSeq_id> > TSeqIdList;
typedef std::list<CRef<objects::COrgMod> > TOrgModList;
typedef std::vector<CRef<objects::CGb_qual> > TGbQualVector;
typedef std::list<CRef<objects::CSeqdesc> > TSeqdescList;
typedef std::vector<CRef<objects::CUser_object> > TUserObjVector;
typedef std::list<CRef<objects::CPub> > TPubList;
typedef std::list<CRef<objects::CSeq_loc> > TSeqLocList;
typedef std::list<CRef<objects::CDelta_seq> > TDeltaList;


#define ParFlat_ENTRYNODE    500

string location_to_string_or_unknown(const objects::CSeq_loc&);

//  ============================================================================
struct InfoBioseq
//  ============================================================================
{
    InfoBioseq() {};

    ~InfoBioseq()
    {
        ids.clear();
    }

    TSeqIdList ids; // for this Bioseq
    string mLocus;
    string mAccNum;

};
using InfoBioseqPtr = InfoBioseq*;

typedef struct protein_block {
    objects::CSeq_entry* biosep; /* for the toppest level of the BioseqSet */

    bool           segset;      /* TRUE if a BioseqSet SeqEntry */

    TEntryList     entries;     /* a ProtRef SeqEntry list, link to above
                                   biosep */

    TSeqFeatList   feats;       /* a CodeRegionPtr list to link the BioseqSet
                                   with class = nuc-prot */
    objects::CGenetic_code::C_E gcode;         /* for this Bioseq */
    InfoBioseqPtr  ibp;
    Uint1          genome;
    Int4           orig_gcode;

    protein_block() :
        biosep(nullptr),
        segset(false),
        ibp(NULL),
        genome(0),
        orig_gcode(0)
    {}

} ProtBlk, *ProtBlkPtr;

typedef struct _locus_cont {
    Int4 bases;
    Int4 bp;
    Int4 strand;
    Int4 molecule;
    Int4 topology;
    Int4 div;
    Int4 date;
} LocusCont, *LocusContPtr;


typedef struct _gap_feats {
    Int4    from;
    Int4    to;
    Int4    estimated_length;
    bool    leftNs;
    bool    rightNs;
    bool    assembly_gap;
    char* gap_type;
    Int4    asn_gap_type;

    objects::CLinkage_evidence::TLinkage_evidence asn_linkage_evidence;

    struct _gap_feats *next;

    _gap_feats();

} GapFeats, *GapFeatsPtr;

typedef struct token_block {
    char*                 str;        /* the token string */
    struct token_block *next;       /* points to next token */
} TokenBlk, *TokenBlkPtr;

typedef struct token_statistics_block {
    TokenBlkPtr list;                   /* a pointer points to the first
                                           token */
    Int2        num;                    /* total number of token in the
                                           chain list */
} TokenStatBlk, *TokenStatBlkPtr;

typedef struct _XmlIndex {
    Int4                  tag;
    Int4                  order;
    size_t                start;        /* Offset from the beginning of the
                                           record, not file! */
    size_t                end;          /* Offset from the beginning of the
                                           record, not file! */
    Int4                  start_line;
    Int4                  end_line;
    Int2                  type;         /* Used for references */
    struct _XmlIndex *subtags;
    struct _XmlIndex *next;
} XmlIndex, *XmlIndexPtr;

typedef std::list<std::string> TKeywordList;

typedef struct indexblk_struct {
    Char               acnum[200];      /* accession num */
    Int2               vernum;          /* version num */
    size_t             offset;          /* byte-offset of in the flatfile at
                                           which the entry starts */
    Char               locusname[200];  /* locus name */
    Char               division[4];     /* division code */
    size_t             bases;           /* basepair length of the entry */
    Uint2              segnum;          /* the number of the entry w/i a
                                           segment set */
    Uint2              segtotal;        /* total number of members in
                                           segmented set to which this
                                           entry belongs */
    Char               blocusname[200]; /* base locus name s.t. w/o tailing
                                           number */
    size_t             linenum;         /* line number at which the entry
                                           starts */
    Uint1              drop;            /* 1 if the accession should be
                                           dropped, otherwise 0 */
    size_t             len;             /* total length (or sizes in bytes)
                                           of the entry */

    CRef<objects::CDate_std> date; /* the record's entry-date or last
                                                  update's date */

    CRef<objects::CPatent_seq_id> psip; /* patent reference */

    bool               EST;             /* special EST entries */
    bool               STS;             /* special STS entries */
    bool               GSS;             /* special Genome servey entries */
    bool               HTC;             /* high throughput cDNA */
    Int2               htg;             /* special HTG [0,1,2,3] entries */
    bool               is_contig;       /* TRUE if entry has CONTIG line,
                                           otherwise FALSE */
    bool               is_mga;          /* TRUE if entry has MGA line,
                                           otherwise FALSE */
    bool               origin;          /* TRUE if sequence is present */
    bool               is_pat;          /* TRUE if accession prefix is
                                           patented and matches source.
                                           FALSE - otherwise. */
    bool               is_wgs;
    bool               is_tpa;
    bool               is_tsa;
    bool               is_tls;
    bool               is_tpa_wgs_con;  /* TRUE if "is_contig", "is_wgs" and
                                           "is_tpa" are TRUE */
    bool               tsa_allowed;
    LocusCont          lc;
    string            moltype;         /* the value of /mol_type qual */
    GapFeatsPtr        gaps;

//    list<string>       secondary_accessions;
    TokenBlkPtr        secaccs;
    XmlIndexPtr        xip;
    bool               embl_new_ID;
    bool               env_sample_qual; /* TRUE if at least one source
                                           feature has /environmental_sample
                                           qualifier */
    bool               is_prot;
    string            organism;        /* The value of /organism qualifier */
    Int4               taxid;           /* The value gotten from source feature
                                           /db_xref qualifier if any */
    bool               no_gc_warning;   /* If TRUE then suppress
                                           ERR_SERVER_GcFromSuppliedLineage
                                           WARNING message */
    size_t             qsoffset;
    size_t             qslength;
    Int4               wgs_and_gi;      /* 01 - has GI, 02 - WGS contig,
                                           03 - both above */
    bool               got_plastid;     /* Set to TRUE if there is at least
                                           one /organelle qual beginning
                                           with "plastid" */
    string               wgssec;     /* Reserved buffer for WGS master or
                                           project accession as secondary */
    int               gc_genomic;      /* Genomic Genetic code from OrgRef */
    int               gc_mito;         /* Mitochondrial Genetic code */
    TKeywordList       keywords;        /* All keywords from a flat record */
    bool               assembly;        /* TRUE for TPA:assembly in
                                           KEYWORDS line */
    bool               specialist_db;   /* TRUE for TPA:specialist_db in
                                           KEYWORDS line */
    bool               inferential;     /* TRUE for TPA:inferential in
                                           KEYWORDS line */
    bool               experimental;    /* TRUE for TPA:experimental in
                                           KEYWORDS line */
    string            submitter_seqid;
    Parser *ppp;

    indexblk_struct();

} Indexblk, *IndexblkPtr;

//  ============================================================================
struct FTAOperon
//  ============================================================================
{
    FTAOperon(
        const string& featName,
        const string& operon,
        const objects::CSeq_loc& location) :
        mFeatname(featName),
        mOperon(operon),
        mLocation(&location)
    {};

    ~FTAOperon() 
    {};

    string LocationStr() const {
        if (mLocStr.empty()) {
            mLocStr = location_to_string_or_unknown(*mLocation);
        }
        return mLocStr;
    }

    bool IsOperon() const {
        return (mFeatname == "operon");
    }

    const string mFeatname;
    const string mOperon;
    CConstRef<objects::CSeq_loc> mLocation;
    mutable string mLocStr;
};


//  ============================================================================
class DataBlk
//  ============================================================================
{
public:
    DataBlk(
        DataBlk* parent = nullptr,
        int type_ = 0,
        char* offset = nullptr,
        size_t len_ = 0) :
        mType(type_),
        mpData(nullptr),
        mOffset(offset),
        len(len_),
        mpQscore(nullptr),
        mDrop(0),
        mpNext(nullptr)
    {
        if (parent) {
            while (parent->mpNext) {
                parent = parent->mpNext;
            }
            parent->mpNext = this;
        }
    };

    static void operator delete(void* p);
/*
    ~DataBlk()
    {
        delete[] mpQscore;
        //delete (char*) mpData;
        delete reinterpret_cast<char*>(mpData);
        if (mType == ParFlat_ENTRYNODE) {
            delete mOffset;
        }
        delete mpNext;
    }
    */
public:

    int mType;  // which keyword block or node type
    void* mpData;  // any pointer type points to information block
    char* mOffset;  // points to beginning of the entry in the memory
    size_t len;  // lenght of data in bytes 
    char* mpQscore;  // points to quality score buffer
    bool mDrop;
    DataBlk* mpNext;  // next in line
};
using DataBlkPtr = DataBlk*;


//  ============================================================================
struct EntryBlk {
//  ============================================================================
    DataBlkPtr              chain;      /* a header points to key-word
                                           block information */
    CRef<objects::CSeq_entry> seq_entry; /* points to sequence entry */

    EntryBlk() :
        chain(nullptr)
    {}
};
using EntryBlkPtr = EntryBlk*;


typedef struct keyword_block {
    const char *str;
    Int2       len;
} KwordBlk, *KwordBlkPtr;

/**************************************************************************/

void xFreeEntry(DataBlkPtr entry);
void FreeIndexblk(IndexblkPtr ibp);
void GapFeatsFree(GapFeatsPtr gfp);
void XMLIndexFree(XmlIndexPtr xip);
void FreeEntryBlk(EntryBlkPtr entry);
EntryBlkPtr CreateEntryBlk();
DataBlkPtr CreateDataBlk(DataBlk* parent=nullptr, int type=0, char* offset=nullptr, size_t len=0);


END_NCBI_SCOPE

#endif
