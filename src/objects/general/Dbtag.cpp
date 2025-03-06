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

// standard includes

// generated includes
#include <ncbi_pch.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/general/Object_id.hpp>
#include <corelib/ncbistd.hpp>
#include <util/compile_time.hpp>

// generated classes

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

// All these maps are sorted at compile time case insensitive
// No need to presort them

struct TApprovedDbTags
{
    CDbtag::TDbtagGroup m_groups = CDbtag::fNone;
    CDbtag::EDbtagType  m_tag    = CDbtag::eDbtagType_bad;
    std::string_view    m_alias;

    constexpr TApprovedDbTags() = default;
    constexpr TApprovedDbTags(const CDbtag::TDbtagGroup& _group, CDbtag::EDbtagType _tag) :
        m_groups{_group}, m_tag{_tag} {}
    constexpr TApprovedDbTags(const CDbtag::TDbtagGroup& _group, CDbtag::EDbtagType _tag, string_view _alias) :
        m_groups{_group}, m_tag{_tag}, m_alias{_alias} {}
};

static constexpr auto sc_ApprovedTags = ct::const_map<ct::tagStrNocase, TApprovedDbTags>::construct(
{
#include "Dbtag.inc"
});

MAKE_CONST_SET(sc_SkippableDbXrefs, ct::tagStrNocase,
{
    "BankIt",
    "NCBIFILE",
    "TMSMART"
})

struct STaxidTaxname {
    const char* m_genus;
    const char* m_species;
    const char* m_subspecies;
};

MAKE_CONST_MAP(sc_TaxIdTaxnameMap, TTaxId, STaxidTaxname,
{
    { 7955, { "Danio", "rerio", "" }  },
    { 8022, { "Oncorhynchus", "mykiss", "" }  },
    { 9606, { "Homo", "sapiens", "" }  },
    { 9615, { "Canis", "lupus", "familiaris" }  },
    { 9838, { "Camelus", "dromedarius", "" }  },
    { 9913, { "Bos", "taurus", "" }  },
    { 9986, { "Oryctolagus", "cuniculus", "" }  },
    { 10090, { "Mus", "musculus", "" }  },
    { 10093, { "Mus", "pahari", "" }  },
    { 10094, { "Mus", "saxicola", "" }  },
    { 10096, { "Mus", "spretus", "" }  },
    { 10098, { "Mus", "cookii", "" }  },
    { 10105, { "Mus", "minutoides", "" }  },
    { 10116, { "Rattus", "norvegicus", "" }  },
    { 10117, { "Rattus", "rattus", "" }  }
})

namespace {

CDbtag::TDbtagGroup xFindStrict(string_view _key)
{
    const auto& _cont = sc_ApprovedTags;
    auto it = _cont.find(_key);
    if (it == _cont.end())
        return 0;

    if (_key != it->first && _key != it->second.m_alias)
        return 0;

    return it->second.m_groups;
}

bool xGetStrict(string_view _key, CDbtag::EDbtagType& _retval)
{
    const auto& _cont = sc_ApprovedTags;
    auto it = _cont.find(_key);
    if (it == _cont.end())
        return false;

    if (_key != it->first && _key != it->second.m_alias)
        return false;

    _retval = it->second.m_tag;
    return true;
}

CDbtag::TDbtagGroup xFindCorrectCaps(const string& v, string_view& correct_caps)
{
    const auto& _cont = sc_ApprovedTags;

    if (auto it = _cont.find(v); it != _cont.end()) {
        if (it->second.m_alias == string_view(v))
            correct_caps = it->second.m_alias;
        else
            correct_caps = it->first;
        return it->second.m_groups;
    }

    return CDbtag::fNone;
}

}

// destructor
CDbtag::~CDbtag(void)
{
}

bool CDbtag::Match(const CDbtag& dbt2) const
{
    if (! PNocase().Equals(GetDb(), dbt2.GetDb()))
        return false;
    return ((GetTag()).Match((dbt2.GetTag())));
}


bool CDbtag::SetAsMatchingTo(const CDbtag& dbt2)
{
    if ( !SetTag().SetAsMatchingTo(dbt2.GetTag()) ) {
        return false;
    }
    SetDb(dbt2.GetDb());
    return true;
}


int CDbtag::Compare(const CDbtag& dbt2) const
{
    int ret = PNocase().Compare(GetDb(), dbt2.GetDb());
    if (ret == 0) {
        ret = GetTag().Compare(dbt2.GetTag());
    }
    return ret;
}


// Appends a label to "label" based on content of CDbtag
void CDbtag::GetLabel(string* label) const
{
    const CObject_id& id = GetTag();
    switch (id.Which()) {
    case CObject_id::e_Str:
    {
        const string& db  = GetDb();
        const string& str = id.GetStr();
        if (str.size() > db.size()  &&  str[db.size()] == ':'
            &&  NStr::StartsWith(str, db, NStr::eNocase)) {
            *label += str; // already prefixed; no need to re-tag
        } else {
            *label += db + ": " + str;
        }
        break;
    }
    case CObject_id::e_Id:
        *label += GetDb() + ": " + NStr::IntToString(id.GetId());
        break;
    default:
        *label += GetDb();
    }
}

// Test if CDbtag.db is in the approved databases list.
// NOTE: 'GenBank', 'EMBL', 'DDBJ' and 'REBASE' are approved only in
//        the context of a RefSeq record.
// NOTE: 'GenBank' is approved in the context of a ProbeDb record.
bool CDbtag::IsApproved( EIsRefseq refseq, EIsSource is_source, EIsEstOrGss is_est_or_gss ) const
{
    if ( !CanGetDb() ) {
        return false;
    }
    const string& db = GetDb();

    CDbtag::TDbtagGroup group = xFindStrict(db);
    if (group == 0)
        return false;


    if( (refseq == eIsRefseq_Yes) && (group & fRefSeq) ) {
        return true;
    }

    if( is_source == eIsSource_Yes ) {
        bool found = (group & fSrc);
        if ( ! found && (is_est_or_gss == eIsEstOrGss_Yes) ) {
            // special case: for EST or GSS, source features are allowed non-src dbxrefs
            found = ( (group & fGenBank) ||
                      (group & fRefSeq) );
        }
        return found;
    } else {
        return (group & fGenBank);
    }
}


const char* CDbtag::IsApprovedNoCase(EIsRefseq refseq, EIsSource is_source ) const
{
    if ( !CanGetDb() ) {
        return NULL;
    }
    const string& db = GetDb();

    string_view caps;

    TDbtagGroup group = xFindCorrectCaps(db, caps);

    if ( (refseq == eIsRefseq_Yes) && (group & fRefSeq)) {
        return caps.data();
    }
    if ( (is_source == eIsSource_Yes ) && (group & fSrc)) {
        return caps.data();
    }
    if (!caps.empty())
        return caps.data();

    return nullptr;
}


bool CDbtag::IsApproved(TDbtagGroup group) const
{
    if ( !CanGetDb() ) {
        return false;
    }
    const string& db = GetDb();

    auto allowed = xFindStrict(db);
    return (allowed & group);
}


bool CDbtag::IsSkippable(void) const
{
    return sc_SkippableDbXrefs.find(GetDb())
        != sc_SkippableDbXrefs.end();
}


// Retrieve the enumerated type for the dbtag
CDbtag::EDbtagType CDbtag::GetType(void) const
{
    if (m_Type == eDbtagType_bad) {
        if ( !CanGetDb() ) {
            return m_Type;
        }

        const string& db = GetDb();

        if (xGetStrict(db, m_Type))
            return m_Type;
    }

    return m_Type;
}

CDbtag::TDbtagGroup CDbtag::GetDBFlags (string& correct_caps) const
{
    correct_caps.clear();
    CDbtag::TDbtagGroup rsult = fNone;

    if ( !CanGetDb() ) {
        return fNone;
    }
    const string& db = GetDb();

    string_view caps;

    auto groups = xFindCorrectCaps(db, caps);
    if (groups) {
        correct_caps = caps;
        return groups;
    }

    return rsult;
}


bool CDbtag::GetDBFlags (bool& is_refseq, bool& is_src, string& correct_caps) const
{
    CDbtag::TDbtagGroup group = CDbtag::GetDBFlags(correct_caps);

    is_refseq = ((group & fRefSeq) != 0);
    is_src    = ((group & fSrc)    != 0);

    return group != fNone;
}


// Force a refresh of the internal type
void CDbtag::InvalidateType(void)
{
    m_Type = eDbtagType_bad;
}


//=========================================================================//
//                              URLs                                       //
//=========================================================================//

// special case URLs
static constexpr string_view kFBan = "http://www.fruitfly.org/cgi-bin/annot/fban?";  // url not found "Internal Server Error" tested 7/13/2016
static constexpr string_view kHInvDbHIT = "http://www.jbirc.aist.go.jp/hinv/hinvsys/servlet/ExecServlet?KEN_INDEX=0&KEN_TYPE=30&KEN_STR="; // access forbidden 7/13/2016
static constexpr string_view kHInvDbHIX = "http://www.jbirc.aist.go.jp/hinv/hinvsys/servlet/ExecServlet?KEN_INDEX=0&KEN_TYPE=31&KEN_STR="; // "Internal Server Error" tested 7/13/2016
static constexpr string_view kDictyPrim = "http://dictybase.org/db/cgi-bin/gene_page.pl?primary_id=";  // url not found tested 7/13/2016
static constexpr string_view kMiRBaseMat = "http://www.mirbase.org/cgi-bin/mature.pl?mature_acc="; // https not available tested 7/13/2016
static constexpr string_view kMaizeGDBInt = "https://www.maizegdb.org/cgi-bin/displaylocusrecord.cgi?id=";
static constexpr string_view kMaizeGDBStr = "https://www.maizegdb.org/cgi-bin/displaylocusrecord.cgi?term=";
static constexpr string_view kHomdTax = "http://www.homd.org/taxon="; // https not available tested 7/13/2016
static constexpr string_view kHomdSeq = "http://www.homd.org/seq="; // https not available tested 7/13/2016


// mapping of DB to its URL; sorting is not needed

MAKE_CONST_MAP(sc_UrlMap, CDbtag::EDbtagType, string,
{
    { CDbtag::eDbtagType_AFTOL, "https://wasabi.lutzonilab.net/pub/displayTaxonInfo?aftol_id=" },
    { CDbtag::eDbtagType_APHIDBASE, "http://bipaa.genouest.org/apps/grs-2.3/grs?reportID=aphidbase_transcript_report&objectID=" }, // "Service Unavailable" tested 7/13/2016
    { CDbtag::eDbtagType_ASAP, "https://asap.genetics.wisc.edu/asap/feature_info.php?FeatureID=" },
    { CDbtag::eDbtagType_ATCC, "https://www.atcc.org/Products/All/" },
    { CDbtag::eDbtagType_AceView_WormGenes, "https://www.ncbi.nlm.nih.gov/IEB/Research/Acembly/av.cgi?db=worm&c=gene&q=" },
    { CDbtag::eDbtagType_AntWeb, "https://www.antweb.org/specimen.do?name=" },
    { CDbtag::eDbtagType_ApiDB, "http://www.apidb.org/apidb/showRecord.do?name=GeneRecordClasses.ApiDBGeneRecordClass&primary_key=" }, // https not available tested 7/13/2016
    { CDbtag::eDbtagType_ApiDB_CryptoDB, "http://cryptodb.org/cryptodb/showRecord.do?name=GeneRecordClasses.GeneRecordClass&project_id=CryptoDB&source_id=" },  // https not available tested 7/13/2016
    { CDbtag::eDbtagType_ApiDB_PlasmoDB, "http://plasmodb.org/plasmo/showRecord.do?name=GeneRecordClasses.GeneRecordClass&project_id=PlasmoDB&source_id=" }, // https not available tested 7/13/2016
    { CDbtag::eDbtagType_ApiDB_ToxoDB, "http://toxodb.org/toxo/showRecord.do?name=GeneRecordClasses.GeneRecordClass&project_id=ToxoDB&source_id=" }, // https not available tested 7/13/2016
    { CDbtag::eDbtagType_BB, "https://beetlebase.org/cgi-bin/cmap/feature_search?features=" },
    { CDbtag::eDbtagType_BEETLEBASE, "https://www.beetlebase.org/cgi-bin/report.cgi?name=" },
    { CDbtag::eDbtagType_BGD, "http://bovinegenome.org/genepages/btau40/genes/" }, // https not available tested 7/13/2016
    { CDbtag::eDbtagType_BoLD, "https://portal.boldsystems.org/record/" },
    { CDbtag::eDbtagType_CCDS, "https://www.ncbi.nlm.nih.gov/CCDS/CcdsBrowse.cgi?REQUEST=CCDS&DATA=" },
    { CDbtag::eDbtagType_CDD, "https://www.ncbi.nlm.nih.gov/Structure/cdd/cddsrv.cgi?uid=" },
    { CDbtag::eDbtagType_CGNC, "http://birdgenenames.org/cgnc/GeneReport?id=" }, // https not available tested 7/13/2016
    { CDbtag::eDbtagType_CK, "http://flybane.berkeley.edu/cgi-bin/cDNA/CK_clone.pl?db=CK&dbid=" }, // url not found tested 7/13/2016
    { CDbtag::eDbtagType_COG, "https://www.ncbi.nlm.nih.gov/research/cog/cog/" },
    { CDbtag::eDbtagType_CollecTF, "https://collectf.umbc.edu/" },
    { CDbtag::eDbtagType_ECOCYC, "http://biocyc.org/ECOLI/new-image?type=GENE&object=" }, // https does not result in security cert warning, but "page can't be displayed", tested 7/13/2016
    { CDbtag::eDbtagType_FANTOM_DB, "https://fantom.gsc.riken.jp/db/annotate/main.cgi?masterid=" },
    { CDbtag::eDbtagType_FBOL, "http://www.fungalbarcoding.org/BioloMICS.aspx?Table=Fungal%20barcodes&Fields=All&Rec=" }, // https not available tested 7/13/2016
    { CDbtag::eDbtagType_FLYBASE, "http://flybase.org/reports/" }, // https not available, http site "experiencing problems" tested 7/13/2016
    { CDbtag::eDbtagType_Fungorum, "http://www.indexfungorum.org/Names/NamesRecord.asp?RecordID=" }, // https not available tested 7/13/2016
    { CDbtag::eDbtagType_GABI, "https://www.gabipd.org/database/cgi-bin/GreenCards.pl.cgi?Mode=ShowSequence&App=ncbi&SequenceId=" },
    { CDbtag::eDbtagType_GEO, "https://www.ncbi.nlm.nih.gov/geo/query/acc.cgi?acc=" },
    { CDbtag::eDbtagType_GO, "http://amigo.geneontology.org/amigo/term/GO:" },
    { CDbtag::eDbtagType_GOA, "https://www.ebi.ac.uk/ego/GProtein?ac=" },
    { CDbtag::eDbtagType_GRIN, "https://www.ars-grin.gov/cgi-bin/npgs/acc/display.pl?" },
    { CDbtag::eDbtagType_GeneDB, "http://old.genedb.org/genedb/Search?organism=All%3A*&name=" }, // https not available tested 7/13/2016
    { CDbtag::eDbtagType_GeneID, "https://www.ncbi.nlm.nih.gov/gene/" },
    { CDbtag::eDbtagType_GrainGenes, "http://wheat.pw.usda.gov/cgi-bin/graingenes/report.cgi?class=marker&name=" }, // https not available tested 7/13/2016
    { CDbtag::eDbtagType_Greengenes, "http://greengenes.lbl.gov/cgi-bin/show_one_record_v2.pl?prokMSA_id=" }, // https not available tested 7/13/2016
    { CDbtag::eDbtagType_HGNC, "https://www.genenames.org/data/gene-symbol-report/#!/hgnc_id/HGNC:" }, // https not available tested 7/13/2016
    { CDbtag::eDbtagType_HMP, "https://www.hmpdacc.org/catalog/grid.php?dataset=genomic&hmp_id=" },
    { CDbtag::eDbtagType_HOMD, "http://www.homd.org/" }, // https not available tested 7/13/2016
    { CDbtag::eDbtagType_HPM, "http://www.humanproteomemap.org/protein.php?hpm_id=" }, // https not available tested 7/13/2016
    { CDbtag::eDbtagType_HPRD, "http://www.hprd.org/protein/" }, // https not available, http returns "Service Temporarily Unavailable" tested 7/13/2016
    { CDbtag::eDbtagType_HSSP, "http://mrs.cmbi.ru.nl/m6/search?db=all&q=" }, // not sure this points to a useful URL tested 7/13/2016
    { CDbtag::eDbtagType_H_InvDB, "https://www.h-invitational.jp" },
    { CDbtag::eDbtagType_IFO, "https://www.nbrc.nite.go.jp/NBRC2/NBRCCatalogueDetailServlet?ID=NBRC&CAT=" },
    { CDbtag::eDbtagType_IMGT_GENEDB, "http://www.imgt.org/IMGT_GENE-DB/GENElect?species=Homo+sapiens&query=2+" }, // https not available, http "detected an unhandled exception" tested 7/13/2016
    { CDbtag::eDbtagType_IMGT_HLA, "https://www.ebi.ac.uk/cgi-bin/ipd/imgt/hla/get_allele.cgi?" },
    { CDbtag::eDbtagType_IMGT_LIGM, "http://www.imgt.org/cgi-bin/IMGTlect.jv?query=201+" }, // https not available, http "detected an unhandled exception" tested 7/13/2016
    { CDbtag::eDbtagType_IRD, "https://www.fludb.org/brc/fluSegmentDetails.do?irdSubmissionId=" },
    { CDbtag::eDbtagType_ISD, "http://www.flu.lanl.gov/search/view_record.html?accession=" }, // http "page can't be displayed" tested 7/13/2016
    { CDbtag::eDbtagType_ISFinder, "http://www-is.biotoul.fr/scripts/is/is_spec.idc?name=" }, // url not found tested 7/13/2016
    { CDbtag::eDbtagType_InterimID, "https://www.ncbi.nlm.nih.gov/gene/" },
    { CDbtag::eDbtagType_Interpro, "https://www.ebi.ac.uk/interpro/entry/InterPro/" },
    { CDbtag::eDbtagType_IntrepidBio, "http://server1.intrepidbio.com/FeatureBrowser/gene/browse/" }, // http request shows "Database is down for maint" tested 7/13/2016
    { CDbtag::eDbtagType_JCM, "https://www.jcm.riken.go.jp/cgi-bin/jcm/jcm_number?JCM=" },
    { CDbtag::eDbtagType_JGIDB, "http://genome.jgi-psf.org/cgi-bin/jgrs?id=" }, // https page "can't be displayed" tested 7/13/2016
    { CDbtag::eDbtagType_LocusID, "https://www.ncbi.nlm.nih.gov/gene/" },
    { CDbtag::eDbtagType_MGI, "http://www.informatics.jax.org/marker/MGI:" }, // https page "can't be displayed" tested 7/13/2016
    { CDbtag::eDbtagType_MIM, "https://www.ncbi.nlm.nih.gov/omim/" },
    { CDbtag::eDbtagType_MaizeGDB, "https://www.maizegdb.org/cgi-bin/displaylocusrecord.cgi?" },
    { CDbtag::eDbtagType_MycoBank, "http://www.mycobank.org/MycoTaxo.aspx?Link=T&Rec=" }, // https not available tested 7/13/2016
    { CDbtag::eDbtagType_NMPDR, "http://www.nmpdr.org/linkin.cgi?id=" }, // https not available, http "Internal Server Error" tested 7/13/2016
    { CDbtag::eDbtagType_NRESTdb, "http://genome.ukm.my/nrestdb/db/single_view_est.php?id=" }, //  http "page can't be displayed" tested 7/13/2016
    { CDbtag::eDbtagType_NextDB, "http://nematode.lab.nig.ac.jp/cgi-bin/db/ShowGeneInfo.sh?celk=" }, // url not found tested 7/13/2016
    { CDbtag::eDbtagType_OrthoMCL, "http://orthomcl.org/orthomcl/showRecord.do?name=GroupRecordClasses.GroupRecordClass&group_name=" }, // https not available
    { CDbtag::eDbtagType_Osa1, "http://rice.plantbiology.msu.edu/cgi-bin/gbrowse/rice/?name=" }, // https "page can't be displayed" tested 7/13/2016
    { CDbtag::eDbtagType_PBR, "https://www.poxvirus.org/query.asp?web_id=" },
    { CDbtag::eDbtagType_PBmice, "http://www.idmshanghai.cn/PBmice/DetailedSearch.do?type=insert&id=" }, // https not available tested 7/13/2016
    { CDbtag::eDbtagType_PDB, "http://www.rcsb.org/pdb/cgi/explore.cgi?pdbId=" }, // https "page can't be displayed" tested 7/13/2016
    { CDbtag::eDbtagType_PFAM, "https://pfam.xfam.org/family/" },
    { CDbtag::eDbtagType_PGN, "http://pgn.cornell.edu/cgi-bin/search/seq_search_result.pl?identifier=" }, // http page states info no longer avail at this website, includes links to look for a new location tested 7/13/2016
    { CDbtag::eDbtagType_Phytozome, "https://phytozome.jgi.doe.gov/pz/portal.html#!results?search=0&crown=1&star=1&method=0&searchText=" },
    { CDbtag::eDbtagType_PomBase, "http://www.pombase.org/spombe/result/" }, // https not available tested 7/13/2016
    { CDbtag::eDbtagType_RAP_DB, "http://rapdb.dna.affrc.go.jp/cgi-bin/gbrowse_details/latest?name=" }, // https appears available, domain appears to exist but http "page not found" with note about release of a major update tested 7/13/2016
    { CDbtag::eDbtagType_RATMAP, "https://ratmap.gen.gu.se/ShowSingleLocus.htm?accno=" },
    { CDbtag::eDbtagType_RBGE_garden, "https://data.rbge.org.uk/living/" },
    { CDbtag::eDbtagType_RBGE_herbarium, "https://data.rbge.org.uk/herb/" },
    { CDbtag::eDbtagType_REBASE, "http://rebase.neb.com/rebase/enz/" }, // ID-4590 : https not available 02/14/2018
    { CDbtag::eDbtagType_RFAM, "http://rfam.xfam.org/family/" }, // https not available tested 7/13/2016
    { CDbtag::eDbtagType_RGD, "https://rgd.mcw.edu/rgdweb/search/search.html?term=" },
    { CDbtag::eDbtagType_RiceGenes, "http://ars-genome.cornell.edu/cgi-bin/WebAce/webace?db=ricegenes&class=Marker&object=" }, // http "page can't be displayed" tested 7/13/2016
    { CDbtag::eDbtagType_SGD, "https://www.yeastgenome.org/locus/" }, // url not found tested 7/13/2016
    { CDbtag::eDbtagType_SGN, "http://www.sgn.cornell.edu/search/est.pl?request_type=7&request_id=" }, // https not available, http automatically redirects to https, then shows security cert issue, tested 7/13/2016
    { CDbtag::eDbtagType_SK_FST, "http://aafc-aac.usask.ca/fst/" }, // https not available tested 7/13/2016
    { CDbtag::eDbtagType_SRPDB, "http://rnp.uthscsa.edu/rnp/SRPDB/rna/sequences/fasta/" }, // https not available tested 7/13/2016
    { CDbtag::eDbtagType_SubtiList, "http://genolist.pasteur.fr/SubtiList/genome.cgi?external_query+" }, // https not available tested 7/13/2016
    { CDbtag::eDbtagType_TAIR, "https://www.arabidopsis.org/servlets/TairObject?type=locus&name=" },
    { CDbtag::eDbtagType_TIGRFAM, "http://www.jcvi.org/cgi-bin/tigrfams/HmmReportPage.cgi?acc=" }, // https not available tested 7/13/2016
    { CDbtag::eDbtagType_UNITE, "https://unite.ut.ee/bl_forw.php?nimi=" },
    { CDbtag::eDbtagType_UniGene, "https://www.ncbi.nlm.nih.gov/unigene?term=" },
    { CDbtag::eDbtagType_UniProt_SwissProt, "https://www.uniprot.org/uniprot/" },
    { CDbtag::eDbtagType_UniProt_TrEMBL, "https://www.uniprot.org/uniprot/" },
    { CDbtag::eDbtagType_UniSTS, "https://www.ncbi.nlm.nih.gov/probe?term=" },
    { CDbtag::eDbtagType_VBASE2, "http://www.vbase2.org/vgene.php?id=" }, // https not available tested 7/13/2016
    { CDbtag::eDbtagType_VBRC, "http://vbrc.org/query.asp?web_view=curation&web_id=" }, // https not available tested 7/13/2016
    { CDbtag::eDbtagType_VectorBase, "https://vectorbase.org/gene/" },
    { CDbtag::eDbtagType_Vega, "http://vega.archive.ensembl.org/id/"  },
    { CDbtag::eDbtagType_WorfDB, "http://worfdb.dfci.harvard.edu/search.pl?form=1&search=" },
    { CDbtag::eDbtagType_WormBase, "https://www.wormbase.org/search/gene/" },
    { CDbtag::eDbtagType_Xenbase, "https://www.xenbase.org/entry/gene/showgene.do?method=display&geneId=" },
    { CDbtag::eDbtagType_ZFIN, "https://zfin.org/" },
    { CDbtag::eDbtagType_axeldb, "http://www.dkfz-heidelberg.de/tbi/services/axeldb/clone/xenopus?name=" }, // https not available tested 7/13/2016
    { CDbtag::eDbtagType_dbClone, "https://www.ncbi.nlm.nih.gov/sites/entrez?db=clone&cmd=Retrieve&list_uids=" },
    { CDbtag::eDbtagType_dbCloneLib, "https://www.ncbi.nlm.nih.gov/sites/entrez?db=clonelib&cmd=Retrieve&list_uids=" },
    { CDbtag::eDbtagType_dbEST, "https://www.ncbi.nlm.nih.gov/nucest/" },
    { CDbtag::eDbtagType_dbProbe, "https://www.ncbi.nlm.nih.gov/sites/entrez?db=probe&cmd=Retrieve&list_uids=" },
    { CDbtag::eDbtagType_dbSNP, "https://www.ncbi.nlm.nih.gov/snp/rs" },
    { CDbtag::eDbtagType_dbSTS, "https://www.ncbi.nlm.nih.gov/nuccore/" },
    { CDbtag::eDbtagType_dictyBase, "https://dictybase.org/db/cgi-bin/gene_page.pl?dictybaseid=" },
    { CDbtag::eDbtagType_miRBase, "http://www.mirbase.org/cgi-bin/mirna_entry.pl?acc=" }, // https not available tested 7/13/2016
    { CDbtag::eDbtagType_niaEST, "https://lgsun.grc.nia.nih.gov/cgi-bin/pro3?sname1=" }, // project appears to be abandoned, tested 7/16/2021
    { CDbtag::eDbtagType_taxon, "https://www.ncbi.nlm.nih.gov/Taxonomy/Browser/wwwtax.cgi?" },
    { CDbtag::eDbtagType_BEEBASE, "http://hymenopteragenome.org/cgi-bin/gb2/gbrowse/bee_genome45/?name=" }, // https not available tested 7/13/2016
    { CDbtag::eDbtagType_NASONIABASE, "http://hymenopteragenome.org/cgi-bin/gbrowse/nasonia10_scaffold/?name=" }, // https not available tested 7/13/2016
    { CDbtag::eDbtagType_BioProject, "https://www.ncbi.nlm.nih.gov/bioproject/" },
    { CDbtag::eDbtagType_IKMC, "https://www.mousephenotype.org/data/alleles/project_id?ikmc_project_id=" },
    { CDbtag::eDbtagType_ViPR, "https://www.viprbrc.org/brc/viprStrainDetails.do?viprSubmissionId=" },
    { CDbtag::eDbtagType_SRA, "https://www.ncbi.nlm.nih.gov/sra/" },
    { CDbtag::eDbtagType_RefSeq, "https://www.ncbi.nlm.nih.gov/nuccore/" },
    { CDbtag::eDbtagType_EnsemblGenomes, "http://ensemblgenomes.org/id/" }, // https not available tested 7/13/2016
    { CDbtag::eDbtagType_EnsemblGenomes_Gn, "http://ensemblgenomes.org/id/" }, // https not available tested 7/13/2016
    { CDbtag::eDbtagType_EnsemblGenomes_Tr, "http://ensemblgenomes.org/id/" }, // https not available tested 7/13/2016
    { CDbtag::eDbtagType_TubercuList, "http://tuberculist.epfl.ch/quicksearch.php?gene+name=" }, // https not available tested 7/13/2016
    { CDbtag::eDbtagType_MedGen, "https://www.ncbi.nlm.nih.gov/medgen/" },
    { CDbtag::eDbtagType_CGD, "http://www.candidagenome.org/cgi-bin/locus.pl?locus=" }, // https not available tested 7/13/2016
    { CDbtag::eDbtagType_Assembly, "https://www.ncbi.nlm.nih.gov/assembly/" },
    { CDbtag::eDbtagType_GenBank, "https://www.ncbi.nlm.nih.gov/nuccore/" },
    { CDbtag::eDbtagType_BioSample, "https://www.ncbi.nlm.nih.gov/biosample/" },
    { CDbtag::eDbtagType_ISHAM_ITS, "http://its.mycologylab.org/BioloMICS.aspx?Table=Sequences&ExactMatch=T&Name=MITS" }, // https not available tested 7/13/2016
    { CDbtag::eDbtagType_I5KNAL, "https://i5k.nal.usda.gov/" },
    { CDbtag::eDbtagType_VISTA, "https://enhancer.lbl.gov/cgi-bin/dbxref.pl?id=" }, // https not available tested 7/13/2016
    { CDbtag::eDbtagType_BEI, "https://www.beiresources.org/Catalog/animalViruses/" },
    { CDbtag::eDbtagType_Araport, "https://bar.utoronto.ca/thalemine/portal.do?externalids=" },
    { CDbtag::eDbtagType_VGNC, "http://vertebrate.genenames.org/data/gene-symbol-report/#!/vgnc_id/VGNC:" }, // https not available tested 7/13/2016
    { CDbtag::eDbtagType_RNAcentral, "http://rnacentral.org/rna/" },
    { CDbtag::eDbtagType_PeptideAtlas, "https://db.systemsbiology.net/sbeams/cgi/PeptideAtlas/Search?action=GO&search_key=" },
    { CDbtag::eDbtagType_EPDnew, "http://epd.vital-it.ch/cgi-bin/get_doc?format=genome&entry=" },
    { CDbtag::eDbtagType_dbVar, "https://www.ncbi.nlm.nih.gov/dbvar/variants/" },
    { CDbtag::eDbtagType_EnsemblRapid, "https://rapid.ensembl.org/id/" },
    { CDbtag::eDbtagType_AllianceGenome, "https://www.alliancegenome.org/gene/" },
    { CDbtag::eDbtagType_EchinoBase, "https://www.echinobase.org/entry/gene/showgene.do?method=displayGeneSummary&geneId=" },

    { CDbtag::eDbtagType_ENSEMBL, "https://www.ensembl.org/id/" }, // url seems incorrect, includes msg user has been redirected and  "Error 404 Page not found" tested 7/13/2016
    { CDbtag::eDbtagType_Ensembl, "https://www.ensembl.org/id/" }, // url seems incorrect, includes msg user has been redirected and  "Error 404 Page not found" tested 7/13/2016
    { CDbtag::eDbtagType_PseudoCAP, "http://www.pseudomonas.com/primarySequenceFeature/list?c1=name&e1=1&v1=" }, // url not found tested 7/13/2016
    { CDbtag::eDbtagType_PseudoCap, "http://www.pseudomonas.com/primarySequenceFeature/list?c1=name&e1=1&v1=" }, // url not found tested 7/13/2016

    { CDbtag::eDbtagType_AmoebaDB, "https://amoebadb.org/amoeba/app/record/gene/" },
    { CDbtag::eDbtagType_CryptoDB, "https://cryptodb.org/cryptodb/app/record/gene/" },
    { CDbtag::eDbtagType_FungiDB, "https://fungidb.org/fungidb/app/record/gene/" },
    { CDbtag::eDbtagType_GiardiaDB, "https://giardiadb.org/giardiadb/app/record/gene/" },
    { CDbtag::eDbtagType_MicrosporidiaDB, "https://microsporidiadb.org/micro/app/record/gene/" },
    { CDbtag::eDbtagType_PiroplasmaDB, "https://piroplasmadb.org/piro/app/record/gene/" },
    { CDbtag::eDbtagType_PlasmoDB, "https://plasmodb.org/plasmo/app/record/gene/" },
    { CDbtag::eDbtagType_ToxoDB, "https://toxodb.org/toxo/app/record/gene/" },
    { CDbtag::eDbtagType_TrichDB, "https://trichdb.org/trichdb/app/record/gene/" },
    { CDbtag::eDbtagType_TriTrypDB, "https://tritrypdb.org/tritrypdb/app/record/gene/" },
    { CDbtag::eDbtagType_VEuPathDB, "https://veupathdb.org/gene/" },

    { CDbtag::eDbtagType_NCBIOrtholog, "https://www.ncbi.nlm.nih.gov/gene/" }, // modified below
})

string CDbtag::GetUrl(void) const
{
    return GetUrl( kEmptyStr, kEmptyStr, kEmptyStr );
}

string CDbtag::GetUrl(TTaxId taxid) const
{
    auto find_iter = sc_TaxIdTaxnameMap.find(taxid);
    if( find_iter == sc_TaxIdTaxnameMap.end() ) {
        return GetUrl();
    } else {
        const STaxidTaxname & taxinfo = find_iter->second;
        return GetUrl( taxinfo.m_genus, taxinfo.m_species, taxinfo.m_subspecies );
    }
}

string CDbtag::GetUrl(const string & taxname_arg ) const
{
    // The exact number doesn't matter, as long as it's long enough
    // to cover all reasonable cases
    const static SIZE_TYPE kMaxLen = 500;

    if( taxname_arg.empty() || taxname_arg.length() > kMaxLen ) {
        return GetUrl();
    }

    // make a copy because we're changing it
    string taxname = taxname_arg;

    // convert all non-alpha chars to spaces
    NON_CONST_ITERATE( string, str_iter, taxname ) {
        const char ch = *str_iter;
        if( ! isalpha(ch) ) {
            *str_iter = ' ';
        }
    }

    // remove initial and final spaces
    NStr::TruncateSpacesInPlace( taxname );

    // extract genus, species, subspeces

    vector<string> taxname_parts;
    NStr::Split(taxname, " ", taxname_parts, NStr::fSplit_Tokenize);

    if( taxname_parts.size() == 2 || taxname_parts.size() == 3 ) {
        string genus;
        string species;
        string subspecies;

        genus = taxname_parts[0];
        species = taxname_parts[1];

        if( taxname_parts.size() == 3 ) {
            subspecies = taxname_parts[2];
        }

        return GetUrl( genus, species, subspecies );
    }

    // if we couldn't figure out the taxname, use the default behavior
    return GetUrl();
}

string CDbtag::GetUrl(const string & genus,
                      const string & species,
                      const string & subspecies) const
{
    auto it = sc_UrlMap.find(GetType());
    if (it == sc_UrlMap.end()) {
        return kEmptyStr;
    }

    auto prefix = it->second;

    string tag;
    bool nonInteger = false;
    if (GetTag().IsStr()) {
        tag = GetTag().GetStr();
        // integer db_xrefs are supposed to be converted to IsId, mark as not an integer
        nonInteger = true;
    } else if (GetTag().IsId()) {
        tag = NStr::IntToString(GetTag().GetId());
    }
    if (NStr::IsBlank(tag)) {
        return kEmptyStr;
    }

    // URLs are constructed by catenating the URL prefix with the specific tag
    // except in a few cases handled below.
    switch (GetType()) {
    case CDbtag::eDbtagType_FLYBASE:
        if (NStr::Find(tag, "FBan") != NPOS) {
            prefix = kFBan;
        }
        break;

    case eDbtagType_GeneID:
        if (nonInteger) {
            // GeneID must be an integer
            return kEmptyStr;
        }
        break;

    case eDbtagType_BEI:
        tag += ".aspx";
        break;

    case CDbtag::eDbtagType_BoLD:
        tag = NStr::Replace(tag, ".", "#");
        break;

    case CDbtag::eDbtagType_Fungorum:
        {
            int num_skip = 0;
            string::const_iterator tag_iter = tag.begin();
            for ( ; tag_iter != tag.end() && ! isdigit(*tag_iter) ; ++tag_iter ) {
                num_skip++;
            }
            if (num_skip > 0) {
                tag = tag.substr(num_skip);
            }
        }
        break;

    case eDbtagType_MGI:
    case eDbtagType_MGD:
        if (NStr::StartsWith(tag, "MGI:", NStr::eNocase)  ||
            NStr::StartsWith(tag, "MGD:", NStr::eNocase)) {
            tag = tag.substr(4);
        }
        break;

    case eDbtagType_HGNC:
        if (NStr::StartsWith(tag, "HGNC:", NStr::eNocase)) {
            tag = tag.substr(5);
        }
        break;

    case eDbtagType_VGNC:
        if (NStr::StartsWith(tag, "VGNC:", NStr::eNocase)) {
            tag = tag.substr(5);
        }
        break;

    case eDbtagType_RGD:
        if (NStr::StartsWith(tag, "RGD:", NStr::eNocase)) {
            tag = tag.substr(4);
        }
        break;

    case eDbtagType_PID:
        if (tag[0] == 'g') {
            tag = tag.substr(1);
        }
        break;

    case eDbtagType_SRPDB:
        tag += ".fasta";
        break;

    case eDbtagType_UniSTS:
        tag += "%20%5BUniSTS%20ID%5D";
        break;

    case eDbtagType_dbSNP:
        if (NStr::StartsWith(tag, "rs", NStr::eNocase)) {
            tag = tag.substr(2);
        }
        break;

    case eDbtagType_dbSTS:
        break;

    case eDbtagType_niaEST:
        tag += "&val=1";
        break;

    case eDbtagType_MaizeGDB:
        if (GetTag().IsId()) {
            prefix = kMaizeGDBInt;
        } else if (GetTag().IsStr()) {
            prefix = kMaizeGDBStr;
        }
        break;

    case eDbtagType_GDB:
        {{
                SIZE_TYPE pos = NStr::Find(tag, "G00-");
                if (pos != NPOS) {
                    tag = tag.substr(pos + 4);
                    tag.erase(remove(tag.begin(), tag.end(), '-'), tag.end());
                } else if (!isdigit((unsigned char) tag[0])) {
                    return kEmptyStr;
                }
                break;
            }}

    case eDbtagType_REBASE:
        tag += ".html";
        break;

    case eDbtagType_H_InvDB:
        if (NStr::Find(tag, "HIT")) {
            prefix = kHInvDbHIT;
        } else if (NStr::Find(tag, "HIX")) {
            prefix = kHInvDbHIX;
        }
        break;

    case eDbtagType_SK_FST:
        return prefix;
        break;

    case CDbtag::eDbtagType_taxon:
        if (isdigit((unsigned char) tag[0])) {
            tag.insert(0, "id=");
        } else {
            tag.insert(0, "name=");
        }
        break;

    case CDbtag::eDbtagType_dictyBase:
        if (NStr::Find(tag, "_") != NPOS) {
            prefix = kDictyPrim;
        }
        break;


    case CDbtag::eDbtagType_miRBase:
        if (NStr::Find(tag, "MIMAT") != NPOS) {
            prefix = kMiRBaseMat;
        }
        break;

    case CDbtag::eDbtagType_WormBase:
        {
            int num_alpha = 0;
            int num_digit = 0;
            int num_unscr = 0;
            if( x_LooksLikeAccession (tag, num_alpha, num_digit, num_unscr) &&
                num_alpha == 3 && num_digit == 5 )
                {
                    prefix = "http://www.wormbase.org/search/protein/";
                }
        }
        break;

    case CDbtag::eDbtagType_HOMD:
        if( NStr::StartsWith(tag, "tax_") ) {
            prefix = kHomdTax;
            tag = tag.substr(4);
        } else if( NStr::StartsWith(tag, "seq_") ) {
            prefix = kHomdSeq;
            tag = tag.substr(4);
        }
        break;

    case eDbtagType_IRD:
        tag += "&decorator=influenza";
        break;

    case eDbtagType_ATCC:
        tag += ".aspx";
        break;

    case eDbtagType_ViPR:
        tag += "&decorator=vipr";
        break;

    case CDbtag::eDbtagType_IMGT_GENEDB:
        if( ! genus.empty() ) {
            string taxname_url_piece = genus + "+" + species;
            if( ! subspecies.empty() ) {
                taxname_url_piece += "+" + subspecies;
            }
            string ret = prefix;
            return NStr::Replace( ret,
                                  "species=Homo+sapiens&",
                                  "species=" + taxname_url_piece + "&" ) +
                tag;
        }
        break;

    case CDbtag::eDbtagType_IMGT_HLA:
        if( NStr::StartsWith(tag, "HLA") ) {
            prefix = "http://www.ebi.ac.uk/Tools/dbfetch/dbfetch?db=imgthla;id=";
        }
        break;

    case eDbtagType_RefSeq:
        {{
                string::const_iterator tag_iter = tag.begin();
                if (isalpha (*tag_iter)) {
                    ++tag_iter;
                    if (*tag_iter == 'P') {
                        ++tag_iter;
                        if (*tag_iter == '_') {
                            prefix = "https://www.ncbi.nlm.nih.gov/protein/";
                        }
                    }
                }
            }}
        break;

    case CDbtag::eDbtagType_GO:
        if (!tag.empty()){
            while (tag.size() < SIZE_TYPE(7)){
                tag = '0' + tag;
            }
        }
        break;


    case CDbtag::eDbtagType_IFO:
        if (!tag.empty()){
            while (tag.size() < SIZE_TYPE(8)){
                tag = '0' + tag;
            }
        }
        break;


    case eDbtagType_ISHAM_ITS:
        if (NStr::StartsWith(tag, "MITS", NStr::eNocase)) {
            tag = tag.substr(4);
        }
        break;

    case CDbtag::eDbtagType_EPDnew:
        if( ! genus.empty()  &&   ! species.empty() ) {
            string abbrev = "";
            if (NStr::Equal (genus, "Homo") && NStr::Equal (species, "sapiens")) {
                abbrev = "hg";
            } else {
                string gen = genus;
                string spc = species;
                gen = NStr::ToLower(gen);
                spc = NStr::ToLower(spc);
                abbrev = gen.substr(0, 1) + spc.substr(0, 1);
            }
            tag += "&db=" + abbrev;
        }
        break;

    case CDbtag::eDbtagType_NCBIOrtholog:
        tag += "/ortholog";
        break;

    default:
        break;
    }

    return string(prefix) + tag;
}

// static
bool CDbtag::x_LooksLikeAccession(const string &tag,
        int &out_num_alpha,
        int &out_num_digit,
        int &out_num_unscr)
{
    if ( tag.empty() ) return false;

    if ( tag.length() >= 16) return false;

    if ( ! isupper(tag[0]) ) return false;

    int     numAlpha = 0;
    int     numDigits = 0;
    int     numUndersc = 0;

    string::const_iterator tag_iter = tag.begin();
    if ( NStr::StartsWith(tag, "NZ_") ) {
        tag_iter += 3;
    }
    for ( ; tag_iter != tag.end() && isalpha(*tag_iter); ++tag_iter ) {
        numAlpha++;
    }
    for ( ; tag_iter != tag.end() && *tag_iter == '_'; ++tag_iter ) {
        numUndersc++;
    }
    for ( ; tag_iter != tag.end() && isdigit(*tag_iter) ; ++tag_iter ) {
        numDigits++;
    }
    if ( tag_iter != tag.end() && *tag_iter != ' ' && *tag_iter != '.') {
        return false;
    }

    if (numUndersc > 1) return false;

    out_num_alpha = numAlpha;
    out_num_digit = numDigits;
    out_num_unscr = numUndersc;

    if (numUndersc == 0) {
        if (numAlpha == 1 && numDigits == 5) return true;
        if (numAlpha == 2 && numDigits == 6) return true;
        if (numAlpha == 3 && numDigits == 5) return true;
        if (numAlpha == 4 && numDigits == 8) return true;
        if (numAlpha == 4 && numDigits == 9) return true;
        if (numAlpha == 5 && numDigits == 7) return true;
    } else if (numUndersc == 1) {
        if (numAlpha != 2 || (numDigits != 6 && numDigits != 8 && numDigits != 9)) return false;
        if (tag[0] == 'N' || tag[0] == 'X' || tag[0] == 'Z') {
            if (tag[1] == 'M' ||
                tag[1] == 'C' ||
                tag[1] == 'T' ||
                tag[1] == 'P' ||
                tag[1] == 'G' ||
                tag[1] == 'R' ||
                tag[1] == 'S' ||
                tag[1] == 'W' ||
                tag[1] == 'Z') {
                    return true;
            }
        }
        if (tag[0] == 'A' || tag[0] == 'Y') {
            if (tag[1] == 'P') return true;
        }
    }

    return false;
}

END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE
