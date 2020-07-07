/* ftanet.cpp
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
 * File Name:  ftanet.cpp
 *
 * Author: Sergey Bazhin
 *
 * File Description:
 * -----------------
 *      Functions for real working with the servers and network.
 *
 */
#include <ncbi_pch.hpp>

#include <objtools/flatfile/ftacpp.hpp>

#include <objtools/data_loaders/genbank/gbloader.hpp>

#include <objects/seqfeat/Org_ref.hpp>
#include <objects/taxon1/taxon1.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Pubdesc.hpp>
#include <objects/pub/Pub_equiv.hpp>
#include <objects/pub/Pub.hpp>
#include <objects/biblio/Cit_gen.hpp>
#include <objects/biblio/Cit_art.hpp>
#include <objects/biblio/Cit_sub.hpp>
#include <objects/biblio/Cit_book.hpp>
#include <objects/biblio/Cit_let.hpp>
#include <objects/biblio/Cit_pat.hpp>
#include <objects/biblio/Cit_jour.hpp>
#include <objects/biblio/Auth_list.hpp>
#include <objects/biblio/Affil.hpp>
#include <objects/biblio/Author.hpp>
#include <objects/biblio/Imprint.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/pub/Pub_set.hpp>
#include <objects/biblio/ArticleIdSet.hpp>
#include <objects/biblio/ArticleId.hpp>
#include <objmgr/util/sequence.hpp>
//#include <internal/ID/utils/cpubseq.hpp>
#include <dbapi/driver/drivers.hpp>

#include <objtools/flatfile/index.h>
#include <objtools/flatfile/utilfun.h>

#include <objtools/flatfile/flatdefn.h>
#include <objtools/flatfile/ftamain.h>
#include <objtools/flatfile/ref.h>
#include <objtools/flatfile/asci_blk.h>

#include "ftamed.h"

#ifdef THIS_FILE
#    undef THIS_FILE
#endif
#define THIS_FILE "ftanet.cpp"

#define HEALTHY_ACC "U12345"

static KwordBlk PubStatus[] = {
    {"Publication Status: Available-Online prior to print", 51},
    {"Publication Status : Available-Online prior to print", 52},
    {"Publication_Status: Available-Online prior to print", 51},
    {"Publication_Status : Available-Online prior to print", 52},
    {"Publication-Status: Available-Online prior to print", 51},
    {"Publication-Status : Available-Online prior to print", 52},
    {"Publication Status: Online-Only", 31},
    {"Publication Status : Online-Only", 32},
    {"Publication_Status: Online-Only", 31},
    {"Publication_Status : Online-Only", 32},
    {"Publication-Status: Online-Only", 31},
    {"Publication-Status : Online-Only", 32},
    {"Publication Status: Available-Online", 36},
    {"Publication Status : Available-Online", 37},
    {"Publication_Status: Available-Online", 36},
    {"Publication_Status : Available-Online", 37},
    {"Publication-Status: Available-Online", 36},
    {"Publication-Status : Available-Online", 37},
    {NULL, 0}
};

/**********************************************************/
static CharPtr fta_strip_pub_comment(CharPtr comment, KwordBlkPtr kbp)
{
    CharPtr p;
    CharPtr q;

    ShrinkSpaces(comment);
    for(; kbp->str != NULL; kbp++)
    {
        for(;;)
        {
            p = StringIStr(comment, kbp->str);
            if(p == NULL)
                break;
            for(q = p + kbp->len; *q == ' ' || *q == ';';)
                q++;
            fta_StringCpy(p, q);
        }
    }

    ShrinkSpaces(comment);
    p = (*comment == '\0') ? NULL : StringSave(comment);
    MemFree(comment);

    if(p != NULL && (StringNICmp(p, "Publication Status", 18) == 0 ||
                     StringNICmp(p, "Publication_Status", 18) == 0 ||
                     StringNICmp(p, "Publication-Status", 18) == 0))
        ErrPostEx(SEV_WARNING, ERR_REFERENCE_UnusualPubStatus,
                  "An unusual Publication Status comment exists for this record: \"%s\". If it is a new variant of the special comments used to indicate ahead-of-print or online-only articles, then the comment must be added to the appropriate table of the parser.",
                  p);

    return(p);
}

/**********************************************************/
static void fta_fix_last_initials(ncbi::objects::CName_std &namestd,
                                  bool initials)
{
    char *str;
    char *p;

    if(initials)
    {
        if(!namestd.IsSetInitials())
            return;
        str = (char *) namestd.GetInitials().c_str();
    }
    else
    {
        if(!namestd.IsSetLast())
            return;
        str = (char *) namestd.GetLast().c_str();
    }

    size_t i = strlen(str);
    if(i > 5)
    {
        p = &str[i-5];
        if((*p == ' ' || *p == '.') && !strcmp(p + 1, "III."))
        {
            namestd.SetSuffix("III");
            if(*p == '.')
                p++;
            *p = '\0';
            if(initials)
                namestd.SetInitials(str);
            else
                namestd.SetLast(str);
            i = 0;
        }
    }
    if(i > 4)
    {
        p = &str[i-4];
        if((*p == ' ' || *p == '.') &&
           (!strcmp(p + 1, "III") || !strcmp(p + 1, "2nd") ||
            !strcmp(p + 1, "Jr.") || !strcmp(p + 1, "IV.")))
        {
            if(!strcmp(p + 1, "III"))
                namestd.SetSuffix("III");
            else if(!strcmp(p + 1, "2nd"))
                namestd.SetSuffix("II");
            else if(!strcmp(p + 1, "Jr."))
                namestd.SetSuffix("Jr.");
            else
                namestd.SetSuffix("IV");
            if(*p == '.')
                p++;
            *p = '\0';
            if(initials)
                namestd.SetInitials(str);
            else
                namestd.SetLast(str);
            i = 0;
        }
    }
    if(i > 3)
    {
        p = &str[i-3];
        if((*p == ' ' || *p == '.') &&
           (!strcmp(p + 1, "Jr") || !strcmp(p + 1, "IV") ||
            !strcmp(p + 1, "II")))
        {
            if(!strcmp(p + 1, "Jr"))
                namestd.SetSuffix("Jr.");
            else if(!strcmp(p + 1, "IV"))
                namestd.SetSuffix("IV");
            else
                namestd.SetSuffix("II");
            if(*p == '.')
                p++;
            *p = '\0';
            if(initials)
                namestd.SetInitials(str);
            else
                namestd.SetLast(str);
            i = 0;
        }
    }
}

/**********************************************************/
static void fta_fix_affil(TPubList &pub_list, Int2 source)
{
    bool got_pmid = false;

    NON_CONST_ITERATE(ncbi::objects::CPub_equiv::Tdata, pub, pub_list)
    {
        if(!(*pub)->IsPmid())
            continue;
        got_pmid = true;
        break;
    }

    NON_CONST_ITERATE(ncbi::objects::CPub_equiv::Tdata, pub, pub_list)
    {
        ncbi::objects::CAuth_list *authors;
        if((*pub)->IsArticle())
        {
            ncbi::objects::CCit_art &art = (*pub)->SetArticle();
            if(!art.IsSetAuthors() || !art.CanGetAuthors())
                continue;

            authors = &art.SetAuthors();
        }
        else if((*pub)->IsSub())
        {
            ncbi::objects::CCit_sub &sub = (*pub)->SetSub();
            if(!sub.IsSetAuthors() || !sub.CanGetAuthors())
                continue;

            authors = &sub.SetAuthors();
        }
        else if((*pub)->IsGen())
        {
            ncbi::objects::CCit_gen &gen = (*pub)->SetGen();
            if(!gen.IsSetAuthors() || !gen.CanGetAuthors())
                continue;

            authors = &gen.SetAuthors();
        }
        else if((*pub)->IsBook())
        {
            ncbi::objects::CCit_book &book = (*pub)->SetBook();
            if(!book.IsSetAuthors() || !book.CanGetAuthors())
                continue;

            authors = &book.SetAuthors();
        }
        else if((*pub)->IsMan())
        {
            ncbi::objects::CCit_let &man = (*pub)->SetMan();
            if(!man.IsSetCit() || !man.CanGetCit())
                continue;

            ncbi::objects::CCit_book &book = man.SetCit();
            if(!book.IsSetAuthors() || !book.CanGetAuthors())
                continue;

            authors = &book.SetAuthors();
        }
        else if((*pub)->IsPatent())
        {
            ncbi::objects::CCit_pat &pat = (*pub)->SetPatent();
            if(!pat.IsSetAuthors() || !pat.CanGetAuthors())
                continue;

            authors = &pat.SetAuthors();
        }
        else
            continue;


        if(authors->IsSetAffil() && authors->CanGetAffil() &&
           authors->GetAffil().Which() == ncbi::objects::CAffil::e_Str)
        {
            ncbi::objects::CAffil &affil = authors->SetAffil();
            char *aff = (char *) affil.GetStr().c_str();
            ShrinkSpaces(aff);
            affil.SetStr(aff);
        }

        if(authors->IsSetNames() && authors->CanGetNames() &&
           authors->GetNames().Which() == ncbi::objects::CAuth_list::TNames::e_Std)
        {
            ncbi::objects::CAuth_list::TNames &names = authors->SetNames();
            ncbi::objects::CAuth_list::TNames::TStd::iterator it = (names.SetStd()).begin();
            ncbi::objects::CAuth_list::TNames::TStd::iterator it_end = (names.SetStd()).end();
            for(; it != it_end; it++)
            {
                if((*it)->IsSetAffil() && (*it)->CanGetAffil() &&
                   (*it)->GetAffil().Which() == ncbi::objects::CAffil::e_Str)
                {
                    ncbi::objects::CAffil &affil = (*it)->SetAffil();
                    char *aff = (char *) affil.GetStr().c_str();
                    ShrinkSpaces(aff);
                    affil.SetStr(aff);
                }
                if((*it)->IsSetName() && (*it)->CanGetName() &&
                   (*it)->GetName().IsName())
                {
                    ncbi::objects::CName_std &namestd = (*it)->SetName().SetName();
/* bsv: commented out single letter first name population*/
                    if(source != ParFlat_SPROT && source != ParFlat_PIR &&
                       !got_pmid)
                    {
                        if(!namestd.IsSetFirst() && namestd.IsSetInitials())
                        {
                            char *str = (char *) namestd.GetInitials().c_str();
                            if((strlen(str) == 1 || strlen(str) == 2) &&
                               (str[1] == '.' || str[1] == '\0'))
                            {
                                char *p = (CharPtr) MemNew(2);
                                p[0] = str[0];
                                p[1] = '\0';
                                namestd.SetFirst(p);
                                MemFree(p);
                            }
                        }
                        if((*pub)->IsArticle())
                        {
                            ncbi::objects::CCit_art &art1 = (*pub)->SetArticle();
                            if(art1.IsSetAuthors() && art1.CanGetAuthors())
                            {
                                ncbi::objects::CAuth_list *authors1;
                                authors1 = &art1.SetAuthors();
                                if(authors1->IsSetNames() &&
                                   authors1->CanGetNames() &&
                                   authors1->GetNames().Which() == ncbi::objects::CAuth_list::TNames::e_Std)
                                {
                                    ncbi::objects::CAuth_list::TNames &names1 = authors1->SetNames();
                                    ncbi::objects::CAuth_list::TNames::TStd::iterator it1 = (names1.SetStd()).begin();
                                    ncbi::objects::CAuth_list::TNames::TStd::iterator it1_end = (names1.SetStd()).end();
                                    for(; it1 != it1_end; it1++)
                                    {
                                        if((*it1)->IsSetName() &&
                                           (*it1)->CanGetName() &&
                                           (*it1)->GetName().IsName())
                                        {
                                            ncbi::objects::CName_std &namestd1 = (*it1)->SetName().SetName();
                                            if(!namestd1.IsSetFirst() &&
                                               namestd1.IsSetInitials())
                                            {
                                                char *str = (char *) namestd1.GetInitials().c_str();
                                                if((strlen(str) == 1 || strlen(str) == 2) &&
                                                   (str[1] == '.' || str[1] == '\0'))
                                                {
                                                    char *p = (CharPtr) MemNew(2);
                                                    p[0] = str[0];
                                                    p[1] = '\0';
                                                    namestd1.SetFirst(p);
                                                    MemFree(p);
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
/**/

                    if(namestd.IsSetSuffix())
                        continue;
                    fta_fix_last_initials(namestd, true);
                    if(!namestd.IsSetSuffix())
                        fta_fix_last_initials(namestd, false);
                }
            }
        }
    }
}

/**********************************************************/
static void fta_fix_imprint_language(TPubList &pub_list)
{
    NON_CONST_ITERATE(ncbi::objects::CPub_equiv::Tdata, pub, pub_list)
    {
        if(!(*pub)->IsArticle())
            continue;

        ncbi::objects::CCit_art &art = (*pub)->SetArticle();
        if(!art.IsSetFrom() || !art.GetFrom().IsJournal())
            continue;

        ncbi::objects::CCit_jour &journal = art.SetFrom().SetJournal();

        if(journal.IsSetImp() && journal.GetImp().IsSetLanguage())
        {
            string language = journal.GetImp().GetLanguage();
            char *p;
            char *lang = (char *) language.c_str();
            for(p = lang; *p != '\0'; p++)
                if(*p >= 'A' && *p <= 'Z')
                     *p |= 040;
           journal.SetImp().SetLanguage(lang);
      }
    }
}

/**********************************************************/
static void fta_strip_er_remarks(ncbi::objects::CPubdesc& pub_descr)
{
    if (!pub_descr.IsSetComment())
        return;

    ITERATE(ncbi::objects::CPub_equiv::Tdata, pub, pub_descr.GetPub().Get())
    {
        if (!(*pub)->IsArticle())
            continue;

        const ncbi::objects::CCit_art& art = (*pub)->GetArticle();
        if (!art.IsSetFrom() || !art.GetFrom().IsJournal())
            continue;

        const ncbi::objects::CCit_jour& journal = art.GetFrom().GetJournal();
        
        int status = 0;
        if (journal.IsSetImp() && journal.GetImp().IsSetPubstatus())
            status = journal.GetImp().GetPubstatus();

        if (status == 3 ||          /* epublish */
            status == 4 ||          /* ppublish */
            status == 10)           /* aheadofprint */
        {
            CharPtr comment = StringSave(pub_descr.GetComment().c_str());
            comment = fta_strip_pub_comment(comment, PubStatus);
            if (comment != NULL && comment[0] != 0)
                pub_descr.SetComment(comment);
            else
                pub_descr.ResetComment();

            MemFree(comment);
        }
    }
}

/**********************************************************/
static Uint1 fta_init_med_server(void)
{
    if(!MedArchInit())
        return(2);
    return(1);

}

/**********************************************************/
static Uint1 fta_init_tax_server(void)
{
    ncbi::objects::CTaxon1 taxon_srv;
    if (!taxon_srv.Init())
        return(2);
    return(1);
}

/**********************************************************/
void fta_init_servers(ParserPtr pp)
{
    if(pp->taxserver != 0)
    {
        pp->taxserver = fta_init_tax_server();
        if(pp->taxserver == 2)
        {
            ErrPostEx(SEV_WARNING, ERR_SERVER_Failed,
                      "TaxArchInit call failed.");
        }
    }
    else
    {
        ErrPostEx(SEV_WARNING, ERR_SERVER_NoTaxLookup,
                  "No taxonomy lookup will be performed.");
    }

    if(pp->medserver != 0)
    {
        pp->medserver = fta_init_med_server();
        if(pp->medserver == 2)
        {
            ErrPostEx(SEV_ERROR, ERR_SERVER_Failed,
                      "MedArchInit call failed.");
        }
    }
    else
    {
        ErrPostEx(SEV_WARNING, ERR_SERVER_NoPubMedLookup,
                  "No medline lookup will be performed.");
    }
}

// RW-707
//std::shared_ptr<ncbi::CPubseqAccess> s_pubseq;

/**********************************************************/
static Uint1 fta_init_pubseq(void)
{
#if 0 // RW-707
    // C Toolkit's accpubseq.h library gets username/password from
    // the environment.
    // We are now using C++ Toolkit's cpubseq.hpp library which require
    // credentials during the construction of CPubseqAccess.  So read
    // the environment here and pass it along to the constructor.

    ncbi::DBAPI_RegisterDriver_FTDS();
//    ncbi::DBAPI_RegisterDriver_CTLIB();

    char* env_val = getenv("ALTER_OPEN_SERVER");
    string idserver = env_val ? env_val : "";

    env_val = getenv("ALTER_USER_NAME");
    string idusername = env_val ? env_val : "";

    env_val = getenv("ALTER_USER_PASSWORD");
    string idpassword = env_val ? env_val : "";

    s_pubseq.reset(new CPubseqAccess(idserver.empty() ? "PUBSEQ_OS_INTERNAL_GI64" : idserver.c_str(),
        idusername.empty() ? "anyone" : idusername.c_str(),
        idpassword.empty() ? "allowed" : idpassword.c_str()));

    if (s_pubseq == nullptr || !s_pubseq->CheckConnection())
        return(2);
    return(1);
#endif
    return 2;
}

/**********************************************************/
void fta_entrez_fetch_enable(ParserPtr pp)
{
    if(pp->entrez_fetch != 0)
    {
        pp->entrez_fetch = fta_init_pubseq();
        if(pp->entrez_fetch == 2)
        {
            ErrPostEx(SEV_WARNING, ERR_SERVER_Failed,
                      "Failed to connect to PUBSEQ OS.");
        }
    }
    else
    {
        ErrPostEx(SEV_WARNING, ERR_SERVER_NotUsed,
                  "No PUBSEQ Bioseq fetch will be performed.");
    }
}

/**********************************************************/
void fta_fini_servers(ParserPtr pp)
{
    if(pp->medserver == 1)
        MedArchFini();
/*    if(pp->taxserver == 1)
        tax1_fini();*/
}

/**********************************************************/
void fta_entrez_fetch_disable(ParserPtr pp)
{
#if 0 // RW-707
    if(pp->entrez_fetch == 1)
        s_pubseq.reset();
#endif
}

/**********************************************************/
void fta_fill_find_pub_option(ParserPtr pp, bool htag, bool rtag)
{
    pp->fpo = (FindPubOptionPtr) MemNew(sizeof(FindPubOption));
    MemSet((Pointer) pp->fpo, 0, sizeof(FindPubOption));

    /* no lookup if there is muid in the pub of seqentry
     */
    ((FindPubOptionPtr)pp->fpo)->always_look = !htag;
    ((FindPubOptionPtr)pp->fpo)->replace_cit = !rtag;
    ((FindPubOptionPtr) pp->fpo)->merge_ids = true;
}

/**********************************************************/
static void fta_check_pub_ids(TPubList& pub_list)
{
    bool found = false;
    ITERATE(ncbi::objects::CPub_equiv::Tdata, pub, pub_list)
    {
        if ((*pub)->IsArticle())
        {
            found = true;
            break;
        }
    }
        
    if (found)
        return;

    for (ncbi::objects::CPub_equiv::Tdata::iterator pub = pub_list.begin(); pub != pub_list.end();)
    {
        if (!(*pub)->IsMuid() && !(*pub)->IsPmid())
        {
            ++pub;
            continue;
        }

        ErrPostEx(SEV_ERROR, ERR_REFERENCE_ArticleIdDiscarded,
                  "Article identifier was found for an unpublished, direct submission, book or unparsable article reference, and has been discarded : %s %d.",
                  (*pub)->IsMuid() ? "MUID" : "PMID", (*pub)->GetMuid());

        pub = pub_list.erase(pub);
    }
}

/**********************************************************/
static void fta_fix_pub_equiv(TPubList& pub_list, ParserPtr pp, bool er)
{
    FindPubOptionPtr fpop;
    IndexblkPtr      ibp;

    Uint1            drop;
    TEntrezId        oldpmid;
    TEntrezId        oldmuid;
    TEntrezId        pmid;
    TEntrezId        muid;

    if (pp == NULL)
        return;

    fpop = (FindPubOptionPtr) pp->fpo;
    ibp = pp->entrylist[pp->curindx];

    ncbi::objects::CPub_equiv::Tdata cit_arts;
    NON_CONST_ITERATE(ncbi::objects::CPub_equiv::Tdata, pub, pub_list)
    {
        if (!(*pub)->IsGen())
            continue;

        ncbi::objects::CCit_gen& cit_gen = (*pub)->SetGen();

        if (cit_gen.IsSetCit() &&
            (StringNCmp(cit_gen.GetCit().c_str(), "(er)", 4) == 0 || er))
        {
            cit_arts.push_back(*pub);
            break;
        }
    }

    if (cit_arts.empty())
    {
        fta_check_pub_ids(pub_list);
        FixPubEquiv(pub_list, fpop);
        return;
    }

    ncbi::objects::CPub* cit_gen = *cit_arts.begin();

    ncbi::objects::CPub_equiv::Tdata muids,
                                     pmids,
                                     others;

    NON_CONST_ITERATE(ncbi::objects::CPub_equiv::Tdata, pub, pub_list)
    {
        if (cit_gen == *pub)
            continue;

        if ((*pub)->IsMuid() && muids.empty())
            muids.push_back(*pub);
        else if ((*pub)->IsPmid() && pmids.empty())
            pmids.push_back(*pub);
        else if (!(*pub)->IsArticle())
            others.push_back(*pub);
    }

    pub_list.clear();

    ncbi::objects::CPub* muid_ptr = nullptr;
    if (!muids.empty())
        muid_ptr = *muids.begin();

    ncbi::objects::CPub* pmid_ptr = nullptr;
    if (!pmids.empty())
        pmid_ptr = *pmids.begin();

    oldpmid = (pmid_ptr == NULL) ? ZERO_ENTREZ_ID : pmid_ptr->GetPmid();
    oldmuid = (muid_ptr == NULL) ? ZERO_ENTREZ_ID : muid_ptr->GetMuid();

    drop = 0;
    muid = ZERO_ENTREZ_ID;
    pmid = ZERO_ENTREZ_ID;

    ncbi::CRef<ncbi::objects::CCit_art> new_cit_art;
    if(oldpmid > ZERO_ENTREZ_ID)
    {
        new_cit_art = FetchPubPmId(ENTREZ_ID_TO(Int4, oldpmid));
        if (new_cit_art.Empty())
        {
            ErrPostEx(SEV_REJECT, ERR_REFERENCE_InvalidPmid,
                      "MedArch failed to find a Cit-art for reference with pmid \"%d\".",
                      oldpmid);
            drop = 1;
        }
        else
        {
            if (new_cit_art->IsSetIds())
            {
                ITERATE(ncbi::objects::CArticleIdSet::Tdata, id, new_cit_art->GetIds().Get())
                {
                    if ((*id)->IsPubmed())
                        pmid = (*id)->GetPubmed();
                    else if ((*id)->IsMedline())
                        muid = (*id)->GetMedline();
                }
            }

            if(pmid == ZERO_ENTREZ_ID)
            {
                ErrPostEx(SEV_REJECT, ERR_REFERENCE_CitArtLacksPmid,
                          "Cit-art returned by MedArch lacks pmid identifier in its ArticleIdSet.");
                drop = 1;
            }
            else if(pmid != oldpmid)
            {
                ErrPostEx(SEV_REJECT, ERR_REFERENCE_DifferentPmids,
                          "Pmid \"%d\" used for lookup does not match pmid \"%d\" in the ArticleIdSet of the Cit-art returned by MedArch.",
                          oldpmid, pmid);
                drop = 1;
            }
            if(muid > ZERO_ENTREZ_ID && oldmuid > ZERO_ENTREZ_ID && muid != oldmuid)
            {
                ErrPostEx(SEV_ERROR, ERR_REFERENCE_MuidPmidMissMatch,
                          "Reference has supplied Medline UI \"%d\" but it does not match muid \"%d\" in the Cit-art returned by MedArch.",
                          oldmuid, muid);
            }
        }
    }

    if(drop != 0)
        ibp->drop = 1;
    else if (new_cit_art.NotEmpty())
    {
        cit_arts.clear();
        ncbi::CRef<ncbi::objects::CPub> new_pub(new ncbi::objects::CPub);
        new_pub->SetArticle(*new_cit_art);
        cit_arts.push_back(new_pub);
        cit_gen = *cit_arts.begin();

        if (pmid > ZERO_ENTREZ_ID)
        {
            if (pmids.empty())
            {
                ncbi::CRef<ncbi::objects::CPub> pmid_pub(new ncbi::objects::CPub);
                pmids.push_back(pmid_pub);
                pmid_ptr = *pmids.begin();
            }
            pmid_ptr->SetPmid().Set(pmid);
        }

        if(muid > ZERO_ENTREZ_ID)
        {
            if (muids.empty())
            {
                ncbi::CRef<ncbi::objects::CPub> muid_pub(new ncbi::objects::CPub);
                muids.push_back(muid_pub);
                muid_ptr = *muids.begin();
            }
            muid_ptr->SetMuid(muid);
        }
    }

    pub_list.splice(pub_list.begin(), cit_arts);

    if(muid > ZERO_ENTREZ_ID)
        pub_list.splice(pub_list.begin(), muids);

    pub_list.splice(pub_list.begin(), pmids);
    pub_list.splice(pub_list.begin(), others);
}

/**********************************************************/
static void fta_fix_pub_annot(ncbi::CRef<ncbi::objects::CPub>& pub, ParserPtr pp, bool er)
{
    if (pp == NULL)
        return;

    if (pub->IsEquiv())
    {
        fta_fix_pub_equiv(pub->SetEquiv().Set(), pp, er);
        if(pp->qamode)
            fta_fix_imprint_language(pub->SetEquiv().Set());
        fta_fix_affil(pub->SetEquiv().Set(), pp->source);
        return;
    }

    TPubList pub_list;
    pub_list.push_back(pub);

    FixPub(pub_list, (FindPubOptionPtr)pp->fpo);

    if (pub_list.empty())
        pub.Reset();

    if (pub_list.size() > 1) // pub equiv
    {
        pub->Reset();
        pub->SetEquiv().Set().splice(pub->SetEquiv().Set().end(), pub_list);
    }
}

/**********************************************************/
static void find_pub(ParserPtr pp, ncbi::objects::CBioseq::TAnnot& annots, ncbi::objects::CSeq_descr& descrs)
{
    bool er = false;

    ITERATE(TSeqdescList, descr, descrs.Get())
    {
        if (!(*descr)->IsPub())
            continue;
        
        const ncbi::objects::CPubdesc& pub_descr = (*descr)->GetPub();
        if (pub_descr.IsSetComment() && fta_remark_is_er(pub_descr.GetComment().c_str()) != 0)
            er = true;
        break;
    }

    NON_CONST_ITERATE(TSeqdescList, descr, descrs.Set())
    {
        if (!(*descr)->IsPub())
            continue;

        ncbi::objects::CPubdesc& pub_descr = (*descr)->SetPub();

        fta_fix_pub_equiv(pub_descr.SetPub().Set(), pp, er);
        if(pp->qamode)
            fta_fix_imprint_language(pub_descr.SetPub().Set());
        fta_fix_affil(pub_descr.SetPub().Set(), pp->source);
        fta_strip_er_remarks(pub_descr);
    }

    NON_CONST_ITERATE(ncbi::objects::CBioseq::TAnnot, annot, annots)
    {
        if (!(*annot)->IsSetData() || !(*annot)->GetData().IsFtable())              /* feature table */
            continue;


        NON_CONST_ITERATE(ncbi::objects::CSeq_annot::C_Data::TFtable, feat, (*annot)->SetData().SetFtable())
        {
            if ((*feat)->IsSetData() && (*feat)->GetData().IsPub())   /* pub feature */
            {
                fta_fix_pub_equiv((*feat)->SetData().SetPub().SetPub().Set(), pp, er);
                if(pp->qamode)
                    fta_fix_imprint_language((*feat)->SetData().SetPub().SetPub().Set());
                fta_fix_affil((*feat)->SetData().SetPub().SetPub().Set(), pp->source);
                fta_strip_er_remarks((*feat)->SetData().SetPub());
            }

            if (!(*feat)->IsSetCit())
                continue;

            ncbi::objects::CPub_set& pubs = (*feat)->SetCit();

            NON_CONST_ITERATE(ncbi::objects::CPub_set::TPub, pub, pubs.SetPub())
                fta_fix_pub_annot(*pub, pp, er);
        }
    }
}

/**********************************************************/
static void fta_find_pub(ParserPtr pp, TEntryList& seq_entries)
{
    NON_CONST_ITERATE(TEntryList, entry, seq_entries)
    {
        for (ncbi::CTypeIterator<ncbi::objects::CBioseq_set> bio_set(Begin(*(*entry))); bio_set; ++bio_set)
        {
            find_pub(pp, bio_set->SetAnnot(), bio_set->SetDescr());

            if (bio_set->GetDescr().Get().empty())
                bio_set->ResetDescr();

            if (bio_set->SetAnnot().empty())
                bio_set->ResetAnnot();
        }

        for (ncbi::CTypeIterator<ncbi::objects::CBioseq> bioseq(Begin(*(*entry))); bioseq; ++bioseq)
        {
            find_pub(pp, bioseq->SetAnnot(), bioseq->SetDescr());

            if (bioseq->GetDescr().Get().empty())
                bioseq->ResetDescr();

            if (bioseq->SetAnnot().empty())
                bioseq->ResetAnnot();
        }
    }
}

/**********************************************************/
void fta_find_pub_explore(ParserPtr pp, TEntryList& seq_entries)
{
    if(pp->medserver == 0)
        return;

    if(pp->medserver == 2)
        pp->medserver = fta_init_med_server();

    if (pp->medserver == 1)
    {
        fta_find_pub(pp, seq_entries);
    }
}

/**********************************************************/
static void new_synonym(ncbi::objects::COrg_ref& org_ref, ncbi::objects::COrg_ref& tax_org_ref)
{
    if (!org_ref.CanGetSyn() || !tax_org_ref.CanGetSyn())
        return;

    ITERATE(ncbi::objects::COrg_ref::TSyn, org_syn, org_ref.GetSyn())
    {
        bool found = false;
        ITERATE(ncbi::objects::COrg_ref::TSyn, tax_syn, tax_org_ref.GetSyn())
        {
            if (*org_syn == *tax_syn)
            {
                found = true;
                break;
            }
        }

        if (!found)
        {
            ErrPostEx(SEV_INFO, ERR_ORGANISM_NewSynonym,
                      "New synonym: %s for [%s].",
                      org_syn->c_str(), org_ref.GetTaxname().c_str());
        }
    }
}


#define TAX_SERVER_TIMEOUT 3
static const STimeout s_timeout = { TAX_SERVER_TIMEOUT, 0 };

static void fix_synonyms(ncbi::objects::CTaxon1& taxon, ncbi::objects::COrg_ref& org_ref)
{
    bool with_syns = taxon.SetSynonyms(false);
    if (!with_syns)
        org_ref.SetSyn().clear();
    else
        taxon.SetSynonyms(true);
}

/**********************************************************/
static ncbi::CRef<ncbi::objects::COrg_ref> fta_get_orgref_byid(ParserPtr pp, Uint1Ptr drop, Int4 taxid, bool isoh)
{
    ncbi::CConstRef<ncbi::objects::CTaxon2_data> taxdata;

    ncbi::objects::CTaxon1 taxon;

    bool connection_failed = false;
    for (size_t i = 0; i < 3 && taxdata.Empty(); ++i)
    {
        if (taxon.Init(&s_timeout))
        {
            taxdata = taxon.GetById(TAX_ID_FROM(Int4, taxid));
        }
        else
        {
            connection_failed = true;
            break;
        }
    }

    ncbi::CRef<ncbi::objects::COrg_ref> ret;
    if (taxdata.Empty())
    {
        if (connection_failed)
        {
            ErrPostEx(SEV_FATAL, ERR_SERVER_TaxServerDown,
                      "Taxonomy lookup failed for taxid %d, apparently because the server is down. Cannot generate ASN.1 for this entry.",
                      taxid);
            *drop = 1;
        }
        else
        {
            ErrPostEx(SEV_ERROR, ERR_ORGANISM_TaxNameNotFound,
                      "Taxname not found: [taxid %d].", taxid);
        }
        return ret;
    }

    if (taxdata->GetIs_species_level() != 1 && !isoh)
    {
        ErrPostEx(SEV_WARNING, ERR_ORGANISM_TaxIdNotSpecLevel,
                  "Taxarch hit is not on species level: [taxid %d].", taxid);
    }

    ret.Reset(new ncbi::objects::COrg_ref);
    ret->Assign(taxdata->GetOrg());
    fix_synonyms(taxon, *ret);

    if (ret->IsSetSyn() && ret->GetSyn().empty())
        ret->ResetSyn();

    return ret;
}

/**********************************************************/
ncbi::CRef<ncbi::objects::COrg_ref> fta_fix_orgref_byid(ParserPtr pp, Int4 taxid, Uint1Ptr drop, bool isoh)
{
    ncbi::CRef<ncbi::objects::COrg_ref> ret;

    if(taxid < 1 && pp->taxserver == 0)
        return ret;

    if(pp->taxserver == 2)
        pp->taxserver = fta_init_tax_server();

    if(pp->taxserver == 2)
    {
        ErrPostEx(SEV_FATAL, ERR_SERVER_TaxServerDown,
                  "Taxonomy lookup failed for taxid %d, because the server is down. Cannot generate ASN.1 for this entry.",
                  taxid);
        *drop = 1;
        return ret;
    }

    ret = fta_get_orgref_byid(pp, drop, taxid, isoh);
    if (ret.NotEmpty())
    {
        ErrPostEx(SEV_INFO, ERR_SERVER_TaxNameWasFound,
                  "Taxname _was_ found for taxid %d", taxid);
    }

    return ret;
}

/**********************************************************/
static ncbi::CRef<ncbi::objects::COrg_ref> fta_replace_org(ParserPtr pp, Uint1Ptr drop, ncbi::objects::COrg_ref& org_ref,
                                                           const Char* pn, int merge, Int4 attempt)
{
    IndexblkPtr ibp = pp->entrylist[pp->curindx];

    ncbi::CConstRef<ncbi::objects::CTaxon2_data> taxdata;

    ncbi::objects::CTaxon1 taxon;

    bool connection_failed = true;
    for (size_t i = 0; i < 3 && taxdata.Empty(); ++i)
    {
        if (taxon.Init(&s_timeout))
        {
            if (merge)
            {
                taxdata = taxon.LookupMerge(org_ref);
            }
            else
                taxdata = taxon.Lookup(org_ref);
            connection_failed = false;
            break;
        }
        else
            taxon.Fini();
    }

    ncbi::CRef<ncbi::objects::COrg_ref> ret;

    if (taxdata.Empty())
    {
        if(attempt == 1)
            return ret;

        if (connection_failed)
        {
            ErrPostEx(SEV_FATAL, ERR_SERVER_TaxServerDown,
                      "Taxonomy lookup failed for \"%s\", apparently because the server is down. Cannot generate ASN.1 for this entry.",
                      pn);
            *drop = 1;
        }
        else if(taxon.GetTaxIdByOrgRef(org_ref) < ZERO_TAX_ID)
        {
            if((pp->source == ParFlat_DDBJ || pp->source == ParFlat_EMBL) &&
               ibp->is_pat && ibp->taxid > 0 && ibp->organism != NULL)
            {
                ret = fta_fix_orgref_byid(pp, ibp->taxid, &ibp->drop, true);
                if (ret.NotEmpty() && ret->IsSetTaxname() &&
                   ret->GetTaxname() == ibp->organism)
                {
                    ibp->no_gc_warning = true;
                    return ret;
                }
            }
            ErrPostEx(SEV_ERROR, ERR_ORGANISM_TaxIdNotUnique,
                      "Not an unique Taxonomic Id for [%s].", pn);
        }
        else
        {
            ErrPostEx(SEV_ERROR, ERR_ORGANISM_TaxNameNotFound,
                      "Taxon Id not found for [%s].", pn);
        }
        return ret;
    }

    if (taxdata->GetIs_species_level() != 1 && (ibp->is_pat == false ||
       (pp->source != ParFlat_EMBL && pp->source != ParFlat_DDBJ)))
    {
        ErrPostEx(SEV_WARNING, ERR_ORGANISM_TaxIdNotSpecLevel,
                  "Taxarch hit is not on species level for [%s].", pn);
    }

    ret.Reset(new ncbi::objects::COrg_ref);

    if (merge)
        ret->Assign(org_ref);
    else
        ret->Assign(taxdata->GetOrg());

    return ret;
}

/**********************************************************/
void fta_fix_orgref(ParserPtr pp, ncbi::objects::COrg_ref& org_ref, Uint1Ptr drop,
                    CharPtr organelle)
{
    Int4      attempt;
    int       merge;

    if (org_ref.IsSetTaxname())
    {
        std::string taxname = org_ref.GetTaxname();

        size_t last_char = taxname.size();
        for (; last_char; --last_char)
        {
            if (!isspace(taxname[last_char]))
                break;
        }

        if (!isspace(taxname[last_char]))
            ++last_char;
        org_ref.SetTaxname(taxname.substr(0, last_char));
    }

    if(pp->taxserver == 0)
        return;

    if(pp->taxserver == 2)
        pp->taxserver = fta_init_tax_server();

    std::string old_taxname;
    if (organelle != NULL)
    {
        std::string taxname = org_ref.IsSetTaxname() ? org_ref.GetTaxname() : "",
                    organelle_str(organelle),
                    space(taxname.size() ? " " : "");

        old_taxname = taxname;
        taxname = organelle_str + space + taxname;
        org_ref.SetTaxname(taxname);
        attempt = 1;
    }
    else
    {
        attempt = 2;
    }

    std::string taxname = org_ref.IsSetTaxname() ? org_ref.GetTaxname() : "";
    if (pp->taxserver == 2)
    {
        ErrPostEx(SEV_FATAL, ERR_SERVER_TaxServerDown,
                  "Taxonomy lookup failed for \"%s\", because the server is down. Cannot generate ASN.1 for this entry.",
                  taxname.c_str());
        *drop = 1;
    }
    else
    {
        merge = (pp->format == ParFlat_PIR) ? 0 : 1;

        ncbi::CRef<ncbi::objects::COrg_ref> new_org_ref = fta_replace_org(pp, drop, org_ref, taxname.c_str(), merge, attempt);
        if (new_org_ref.Empty() && attempt == 1)
        {
            org_ref.SetTaxname(old_taxname);
            old_taxname.clear();
            new_org_ref = fta_replace_org(pp, drop, org_ref, "", merge, 2);
        }

        if (new_org_ref.NotEmpty())
        {
            ErrPostEx(SEV_INFO, ERR_SERVER_TaxNameWasFound,
                      "Taxon Id _was_ found for [%s]", taxname.c_str());
            if(pp->format == ParFlat_PIR)
                new_synonym(org_ref, *new_org_ref);

            org_ref.Assign(*new_org_ref);
        }
    }

    if (org_ref.IsSetSyn() && org_ref.GetSyn().empty())
        org_ref.ResetSyn();
}

/**********************************************************/
static ncbi::TGi fta_get_gi_for_seq_id(const ncbi::objects::CSeq_id& id)
{
    ncbi::TGi gi = ncbi::objects::sequence::GetGiForId(id, GetScope());
    if(gi > ZERO_GI)
        return(gi);


    ncbi::objects::CSeq_id test_id;
    test_id.SetGenbank().SetAccession(HEALTHY_ACC);

    int i = 0;
    for (; i < 5; i++)
    {
        if (ncbi::objects::sequence::GetGiForId(test_id, GetScope()) > ZERO_GI)
            break;
#ifdef WIN32
        Sleep(3000);
#else
        sleep(3);
#endif
    }

    if(i == 5)
        return GI_CONST(-1);

    gi = ncbi::objects::sequence::GetGiForId(id, GetScope());
    if (gi > ZERO_GI)
        return(gi);

    return ZERO_GI;
}

/**********************************************************
 * returns -1 if couldn't get division;
 *          1 if it's CON;
 *          0 if it's not CON.
 */
Int4 fta_is_con_div(ParserPtr pp, const ncbi::objects::CSeq_id& id, const Char* acc)
{
    if(pp->entrez_fetch == 0)
        return(-1);
    if(pp->entrez_fetch == 2)
        pp->entrez_fetch = fta_init_pubseq();
    if(pp->entrez_fetch == 2)
    {
        ErrPostEx(SEV_ERROR, ERR_ACCESSION_CannotGetDivForSecondary,
                  "Failed to determine division code for secondary accession \"%s\". Entry dropped.",
                  acc);
        pp->entrylist[pp->curindx]->drop = 1;
        return(-1);
    }

    ncbi::TGi gi = fta_get_gi_for_seq_id(id);
    if(gi < ZERO_GI)
    {
        ErrPostEx(SEV_ERROR, ERR_ACCESSION_CannotGetDivForSecondary,
                  "Failed to determine division code for secondary accession \"%s\". Entry dropped.",
                  acc);
        pp->entrylist[pp->curindx]->drop = 1;
        return(-1);
    }

    if (gi == ZERO_GI)
        return(0);
#if 0 // RW-707
    ncbi::CPubseqAccess::IdGiClass id_gi;
    ncbi::CPubseqAccess::IdBlobClass id_blob;

    if (!s_pubseq->GetIdGiClass(gi, id_gi) || !s_pubseq->GetIdBlobClass(id_gi, id_blob) ||
        id_blob.div[0] == '\0')
    {
        ErrPostEx(SEV_ERROR, ERR_ACCESSION_CannotGetDivForSecondary,
                  "Failed to determine division code for secondary accession \"%s\". Entry dropped.",
                  acc);
        pp->entrylist[pp->curindx]->drop = 1;
        return(-1);
    }
    if (ncbi::NStr::EqualNocase(id_blob.div, "CON"))
        return(1);
#endif
    return(0);
}

/**********************************************************/
ncbi::CRef<ncbi::objects::CCit_art> fta_citart_by_pmid(Int4 pmid, bool& done)
{
    ncbi::CRef<ncbi::objects::CCit_art> cit_art;

    done = true;
    if (pmid < 0)
        return cit_art;
    
    cit_art = FetchPubPmId(pmid);
    return cit_art;
}

/**********************************************************/
void fta_init_gbdataloader()
{
    ncbi::objects::CGBDataLoader::RegisterInObjectManager(*ncbi::objects::CObjectManager::GetInstance());
}
