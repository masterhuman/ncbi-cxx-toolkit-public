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
 * File Name: utilref.cpp
 *
 * Author: Karl Sirotkin, Hsiu-Chuan Chen
 *
 * File Description:
 *      Utility routines for parsing reference block of flatfile.
 *
 */

#include <ncbi_pch.hpp>

#include "ftacpp.hpp"

#include <objects/general/Person_id.hpp>
#include <objects/biblio/Auth_list.hpp>
#include <objects/biblio/Cit_gen.hpp>


#include "index.h"

#include <objtools/flatfile/flatdefn.h>

#include "ftaerr.hpp"
#include "asci_blk.h"
#include "utilref.h"
#include "add.h"
#include "utilfun.h"

#ifdef THIS_FILE
#  undef THIS_FILE
#endif
#define THIS_FILE "utilref.cpp"

#define MAX_PAGE     50
#define OTHER_MEDIUM 255

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

/**********************************************************/
ValNodePtr get_tokens(char* pt, const Char* delimeter)
{
    ValNodePtr token;
    ValNodePtr vnp;

    bool more;

    if (! pt || *pt == '\0')
        return nullptr;

    token = ValNodeNew(nullptr);
    vnp   = token;
    for (; *pt != '\0'; pt++) {
        for (; *pt != '\0'; pt++) {
            if (! StringChr(" \n\t\f~,", *pt))
                break;
            *pt = '\0';
        }
        if (*pt == '\0')
            break;

        vnp       = ValNodeNew(vnp);
        vnp->data = pt;
        more      = false;

        for (; *pt != '\0'; pt++) {
            if (! StringEquN(pt, delimeter, StringLen(delimeter)) &&
                ! StringEquN(pt, ",\n", 2) && ! StringEquN(pt, ",~", 2) &&
                ! StringEquN(pt, " and ", 5))
                continue;

            *pt = '\0';

            if (StringEquN(pt + 1, "and ", 4))
                pt += 4;

            more = true;
            break;
        }

        if (! more)
            break;
    } /* for, completed parsing author list */

    vnp = token->next;
    delete token;
    return (vnp);
}

/**********************************************************/
static void RemoveSpacesAndCommas(string& str)
{
    string buf;
    for (string::iterator it = str.begin(); it != str.end(); ++it)
        if (*it != ',' && *it != '\t' && *it != ' ')
            buf.push_back(*it);

    str.swap(buf);
}

/**********************************************************/
void get_auth_from_toks(ValNodePtr token, ERefFormat format, CRef<CAuth_list>& auths)
{
    ValNodePtr  vnp;
    const char* p;

    if (! token)
        return;

    for (vnp = token; vnp; vnp = vnp->next) {
        p = vnp->data;
        if (StringEquN(p, "and ", 4))
            p += 4;

        CRef<CAuthor> author = get_std_auth(p, format);

        if (author.Empty()) {
            FtaErrPost(SEV_WARNING, ERR_REFERENCE_IllegalAuthorName, p);
            continue;
        }
        if (author->GetName().GetName().IsSetLast() &&
            ! author->GetName().GetName().IsSetFirst()) {
            auto& name = author->SetName().SetName();
            auto& last = name.SetLast();
            auto  i    = last.find('|');
            if (i != string::npos) {
                string s = last.substr(i + 1);
                last.resize(i);
                NStr::TruncateSpacesInPlace(s);
                if (! s.empty())
                    name.SetFirst(std::move(s));
            }
        }

        if (author->GetName().GetName().IsSetInitials()) {
            string& initials = author->SetName().SetName().SetInitials();
            RemoveSpacesAndCommas(initials);
        }

        if (author->GetName().GetName().IsSetSuffix()) {
            string& suffix = author->SetName().SetName().SetSuffix();
            RemoveSpacesAndCommas(suffix);
        }

        if (auths.Empty())
            auths.Reset(new CAuth_list);
        auths->SetNames().SetStd().push_back(author);
    }
}

/**********************************************************/
CRef<CAuthor> get_std_auth(const Char* token, ERefFormat format)
{
    const Char* auth;
    const Char* eptr;

    CRef<CAuthor> author;

    if (! token || *token == '\0')
        return author;

    author                = new CAuthor;
    CPerson_id& person_id = author->SetName();
    CName_std&  namestd   = person_id.SetName();

    for (eptr = token + StringLen(token) - 1; eptr > token && *eptr == ' ';)
        eptr--;

    if (format == PIR_REF || format == GB_REF) {
        for (auth = token; *auth != ',' && *auth != '\0';)
            auth++;
        if (*auth == ',') {
            if (auth[1] != '\0')
                namestd.SetInitials(auth + 1);
        }

        namestd.SetLast(string(token, auth));
    } else if (format == PDB_REF) {
        for (auth = eptr; auth > token && *auth != '.';)
            auth--;
        if (*auth == '.') {
            if (auth[1] != '\0' && auth[1] != '.')
                namestd.SetLast(auth + 1);
            namestd.SetInitials(string(token, auth + 1));
        } else
            namestd.SetLast(token);
    } else if (format == EMBL_REF || format == SP_REF) {
        for (auth = eptr; *auth != ' ' && auth > token;)
            auth--;
        if (*auth == ' ') {
            if (*(auth - 1) == '.')
                for (auth--; *auth != ' ' && auth > token;)
                    auth--;
            if (*auth == ' ') {
                if (auth[1] != '\0')
                    namestd.SetInitials(auth + 1);
            }
        } else
            auth = eptr + 1;

        namestd.SetLast(string(token, auth));
    } else if (format == ML_REF) {
        _TROUBLE;
    }

    if (! namestd.IsSetLast()) {
        author.Reset();
        return author;
    }

    return author;
}

/**********************************************************
 *
 *   AuthListPtr get_auth(pt, format, jour):
 *
 *      Get AuthListPtr for the authors. Delimiter between
 *   the authors is ', ' for GenBank and EMBL. Delimiter
 *   between the authors is ';' for PIR. Delimiter between
 *   last name and initials is ',' for GenBank and PIR,
 *   ' ' for EMBL.
 *      Modified from ParseAuthorList (utilref.c).
 *
 *                                              12-4-93
 *
 **********************************************************/
void get_auth(char* pt, ERefFormat format, char* jour, CRef<CAuth_list>& auths)
{
    static const char* delimiter;
    static char*       eptr;
    ValNodePtr         token;

    switch (format) {
    case GB_REF:
    case EMBL_REF:
    case SP_REF:
        delimiter = ", ";
        break;
    case PIR_REF:
    case PDB_REF:
        delimiter = "; ";
        break;
    default:
        break;
    }
    if (! pt || *pt == '\0' || *pt == ';')
        return;

    size_t len = StringLen(pt);
    for (eptr = pt + len - 1; isalnum(*eptr) == 0; eptr--)
        len--;

    if (len > 4 && StringEquN(eptr - 4, "et al", 5)) {
        if (! jour)
            FtaErrPost(SEV_WARNING, ERR_REFERENCE_EtAlInAuthors, "{}", pt);
        else
            FtaErrPost(SEV_WARNING, ERR_REFERENCE_EtAlInAuthors, "{} : {}", pt, jour);
    }

    token = get_tokens(pt, delimiter);
    get_auth_from_toks(token, format, auths);
    ValNodeFree(token);
}

/**********************************************************/
void get_auth_consortium(char* cons, CRef<CAuth_list>& auths)
{
    char* p;
    char* q;

    if (! cons || *cons == '\0')
        return;

    for (q = cons;; q = p) {
        p = StringChr(q, ';');
        if (p)
            *p = '\0';

        CRef<CAuthor> author(new CAuthor);
        author->SetName().SetConsortium(q);

        if (auths.Empty())
            auths.Reset(new CAuth_list);
        auths->SetNames().SetStd().push_front(author);

        if (! p)
            break;

        for (*p++ = ';'; *p == ';' || *p == ' ';)
            p++;

        if (NStr::EqualNocase(p, 0, 4, "and ")) {
            for (p += 4; *p == ' ';)
                p++;
        }
    }
}

/**********************************************************/
static Int4 check_mix_pages_range(char* pages)
{
    char* page1;
    char* page2;
    char* dash;
    char* p;
    char* q;
    Char  ch1;
    Char  ch2;
    Int4  i;

    dash = StringChr(pages, '-');
    if (! dash)
        return (0);

    *dash = '\0';
    page1 = pages;
    page2 = dash + 1;

    if ((*page1 >= 'a' && *page1 <= 'z') || (*page1 >= 'A' && *page1 <= 'Z')) {
        for (p = page1; (*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z');)
            p++;

        if ((*page2 < 'a' || *page2 > 'z') && (*page2 < 'A' || *page2 > 'Z')) {
            *dash = '-';
            return (-1);
        }

        for (q = page2; (*q >= 'a' && *q <= 'z') || (*q >= 'A' && *q <= 'Z');)
            q++;
        ch1 = *p;
        *p  = '\0';
        ch2 = *q;
        *q  = '\0';

        bool j = StringEqu(page1, page2);

        *p = ch1;
        *q = ch2;
        if (! j) {
            *dash = '-';
            return (-1);
        }
        for (page1 = p; *p >= '0' && *p <= '9';)
            p++;
        for (page2 = q; *q >= '0' && *q <= '9';)
            q++;

        i = atoi(page1) - atoi(page2);

        if (*p != '\0' || *q != '\0') {
            *dash = '-';
            return (-1);
        }
        *dash = '-';
        if (i > 0)
            return (1);
        return (0);
    }

    if (*page1 < '0' || *page1 > '9' || *page2 < '0' || *page2 > '9') {
        *dash = '-';
        return (-1);
    }

    for (p = page1; *p >= '0' && *p <= '9';)
        p++;
    for (q = page2; *q >= '0' && *q <= '9';)
        q++;
    ch1 = *p;
    *p  = '\0';
    ch2 = *q;
    *q  = '\0';
    i   = atoi(page2) - atoi(page1);
    *p  = ch1;
    *q  = ch2;

    for (page1 = p; (*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z');)
        p++;
    for (page2 = q; (*q >= 'a' && *q <= 'z') || (*q >= 'A' && *q <= 'Z');)
        q++;
    if (*p != '\0' || *q != '\0' || ! StringEqu(page1, page2)) {
        *dash = '-';
        return (-1);
    }

    *dash = '-';
    if (i < 0)
        return (1);
    return (0);
}

/**********************************************************/
Int4 valid_pages_range(char* pages, const Char* title, Int4 er, bool inpress)
{
    char* p;
    char* q;
    char* s;
    Int4  fps;
    Int4  lps;
    Int4  i;

    if (! pages || *pages == '\0')
        return (-1);

    if (! title)
        title = (char*)"";
    while (*pages == ' ' || *pages == ';' || *pages == '\t' || *pages == ',')
        pages++;
    if (*pages == '\0')
        return (-1);

    for (s = pages; *s != '\0';)
        s++;
    for (s--; *s == ' ' || *s == ';' || *s == ',' || *s == '\t';)
        s--;
    *++s = '\0';

    p = StringChr(pages, '-');
    if (! p) {
        for (q = pages; (*q >= 'a' && *q <= 'z') || (*q >= 'A' && *q <= 'Z') ||
                        (*q >= '0' && *q <= '9');)
            q++;
        if (*q == '\0')
            return (0);
        if ((er & 01) == 01)
            return (0);
        else if (er > 0)
            return (-1);
        return (1);
    }

    if (p == pages || p[1] == '\0') {
        if (er == 0)
            FtaErrPost(SEV_WARNING, ERR_REFERENCE_IllegPageRange, "Incorrect pages range provided: \"{}\".", pages);
        return (-1);
    }

    if (inpress && (*(p - 1) == ' ' || *(p - 1) == '\t' ||
                    p[1] == ' ' || p[1] == '\t'))
        return (1);

    for (q = p + 1; *q >= '0' && *q <= '9';)
        q++;
    for (p = pages; *p >= '0' && *p <= '9';)
        p++;
    if (*p == '-' && *q == '\0') {
        *p  = '\0';
        fps = atoi(pages);
        *p  = '-';
        lps = atoi(p + 1);

        if (lps - fps >= MAX_PAGE) {
            FtaErrPost(SEV_WARNING, ERR_REFERENCE_LargePageRange, "Total pages exceed {}: {}: {}", MAX_PAGE, pages, title);
        } else if (fps > lps) {
            FtaErrPost(SEV_WARNING, ERR_REFERENCE_InvertPageRange, "Page numbers may be inverted, {}: {}", pages, title);
        }
    } else {
        i = check_mix_pages_range(pages);
        if (i == -1) {
            if (er > 0 && (er & 01) != 01)
                return (-1);
            FtaErrPost(SEV_WARNING, ERR_REFERENCE_UnusualPageNumber, "Pages numbers are not digits, letter+digits, or digits_letter: \"{}\": \"{}\".", pages, title);
        } else if (i == 1) {
            FtaErrPost(SEV_WARNING, ERR_REFERENCE_InvertPageRange, "Page numbers may be inverted, {}: {}", pages, title);
        }
    }
    return (0);
}

/**********************************************************
 *
 *   NCBI_DatePtr get_date(year):
 *
 *      Gets only year and return NCBI_DatePtr.
 *
 **********************************************************/
CRef<CDate> get_date(const Char* year)
{
    CRef<CDate> ret;

    if (! year || *year == '\0') {
        FtaErrPost(SEV_ERROR, ERR_REFERENCE_IllegalDate, "No year in reference.");
        return ret;
    }

    if (year[0] < '0' || year[0] > '9' || year[1] < '0' || year[1] > '9' ||
        year[2] < '0' || year[2] > '9' || year[3] < '0' || year[3] > '9') {
        FtaErrPost(SEV_ERROR, ERR_REFERENCE_IllegalDate, "Illegal year: \"{}\".", year);
        return ret;
    }

    string year_str(year, year + 4);
    time_t now = 0;
    time(&now);
    struct tm* tm = localtime(&now);

    Int4 i = NStr::StringToInt(year_str, NStr::fAllowTrailingSymbols);

    if (i < 1900) {
        FtaErrPost(SEV_ERROR, ERR_REFERENCE_YearPrecedes1900, "Reference's year is extremely far in past: \"{}\".", year_str);
        return ret;
    } else if (i < 1950) {
        FtaErrPost(SEV_WARNING, ERR_REFERENCE_YearPrecedes1950, "Reference's year is too far in past: \"{}\".", year_str);
    } else if (i > tm->tm_year + 1900 + 2) {
        FtaErrPost(SEV_WARNING, ERR_REFERENCE_ImpendingYear, "Reference's year is too far in future: \"{}\"", year_str);
    }

    ret.Reset(new CDate);
    ret->SetStd().SetYear(i);

    return ret;
}

/**********************************************************/
CRef<CCit_gen> get_error(char* bptr, CRef<CAuth_list>& auth_list, CRef<CTitle::C_E>& title)
{
    CRef<CCit_gen> cit_gen(new CCit_gen);

    char* s;
    bool  zero_year = false;
    char* end_tit;
    char* eptr;

    size_t len = StringLen(bptr);
    eptr       = bptr + len - 1;
    while (*eptr == ' ' || *eptr == '\t' || *eptr == '.')
        *eptr-- = '\0';

    if (*eptr == ')') {
        for (s = eptr - 1; s >= bptr && *s != '(';)
            s--;
        if (*s == '(' && s[1] == '0') {
            zero_year = true;
            for (end_tit = bptr; isdigit((int)*end_tit) == 0;)
                end_tit++;
            *end_tit = '\0';
        }
    }

    if (zero_year) {
        CRef<CTitle::C_E> journal_title(new CTitle::C_E);
        if (StringEquN(bptr, "(re)", 4))
            journal_title->SetName(NStr::Sanitize(bptr));
        else
            journal_title->SetIso_jta(NStr::Sanitize(bptr));

        cit_gen->SetJournal().Set().push_back(journal_title);
        cit_gen->SetCit("In press");
    } else if (bptr) {
        cit_gen->SetCit(NStr::Sanitize(bptr));
    }

    if (auth_list.NotEmpty())
        cit_gen->SetAuthors(*auth_list);

    if (title.NotEmpty())
        cit_gen->SetTitle(title->GetName());

    return cit_gen;
}

END_NCBI_SCOPE
