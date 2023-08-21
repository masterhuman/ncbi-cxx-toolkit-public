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
 * File Name: utilfun.cpp
 *
 * Author: Karl Sirotkin, Hsiu-Chuan Chen
 *
 * File Description:
 *      Utility functions for parser and indexing.
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbitime.hpp>

#include "ftacpp.hpp"

#include <corelib/ncbistr.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/object_manager.hpp>
#include <objects/seq/MolInfo.hpp>
#include <objects/seqloc/PDB_seq_id.hpp>
#include <corelib/tempstr.hpp>

#include "index.h"

#include "ftaerr.hpp"
#include "indx_def.h"
#include "utilfun.h"

#ifdef THIS_FILE
#  undef THIS_FILE
#endif
#define THIS_FILE "utilfun.cpp"

BEGIN_NCBI_SCOPE;

USING_SCOPE(objects);

CScope& GetScope()
{
    static CScope scope(*CObjectManager::GetInstance());
    return scope;
}


static const char* ParFlat_EST_kw_array[] = {
    "EST",
    "EST PROTO((expressed sequence tag)",
    "expressed sequence tag",
    "EST (expressed sequence tag)",
    "EST (expressed sequence tags)",
    "EST(expressed sequence tag)",
    "transcribed sequence fragment",
    nullptr
};

static const char* ParFlat_GSS_kw_array[] = {
    "GSS",
    "GSS (genome survey sequence)",
    "trapped exon",
    nullptr
};

static const char* ParFlat_STS_kw_array[] = {
    "STS",
    "STS(sequence tagged site)",
    "STS (sequence tagged site)",
    "STS sequence",
    "sequence tagged site",
    nullptr
};

static const char* ParFlat_HTC_kw_array[] = {
    "HTC",
    nullptr
};

static const char* ParFlat_FLI_kw_array[] = {
    "FLI_CDNA",
    nullptr
};

static const char* ParFlat_WGS_kw_array[] = {
    "WGS",
    nullptr
};

static const char* ParFlat_MGA_kw_array[] = {
    "MGA",
    "CAGE (Cap Analysis Gene Expression)",
    "5'-SAGE",
    nullptr
};

static const char* ParFlat_MGA_more_kw_array[] = {
    "CAGE (Cap Analysis Gene Expression)",
    "5'-SAGE",
    "5'-end tag",
    "unspecified tag",
    "small RNA",
    nullptr
};

/* Any change of contents of next array below requires proper
 * modifications in function fta_tsa_keywords_check().
 */
static const char* ParFlat_TSA_kw_array[] = {
    "TSA",
    "Transcriptome Shotgun Assembly",
    nullptr
};

/* Any change of contents of next array below requires proper
 * modifications in function fta_tls_keywords_check().
 */
static const char* ParFlat_TLS_kw_array[] = {
    "TLS",
    "Targeted Locus Study",
    nullptr
};

/* Any change of contents of next 2 arrays below requires proper
 * modifications in function fta_tpa_keywords_check().
 */
static const char* ParFlat_TPA_kw_array[] = {
    "TPA",
    "THIRD PARTY ANNOTATION",
    "THIRD PARTY DATA",
    "TPA:INFERENTIAL",
    "TPA:EXPERIMENTAL",
    "TPA:REASSEMBLY",
    "TPA:ASSEMBLY",
    "TPA:SPECIALIST_DB",
    nullptr
};

static const char* ParFlat_TPA_kw_array_to_remove[] = {
    "TPA",
    "THIRD PARTY ANNOTATION",
    "THIRD PARTY DATA",
    nullptr
};

static const char* ParFlat_ENV_kw_array[] = {
    "ENV",
    nullptr
};

static const char* ParFlat_MAG_kw_array[] = {
    "Metagenome Assembled Genome",
    "MAG",
    nullptr
};

/**********************************************************/
static string FTAitoa(Int4 m)
{
    Int4   sign = (m < 0) ? -1 : 1;
    string res;

    for (m *= sign; m > 9; m /= 10)
        res += m % 10 + '0';

    res += m + '0';

    if (sign < 0)
        res += '-';

    std::reverse(res.begin(), res.end());
    return res;
}

/**********************************************************/
void UnwrapAccessionRange(const CGB_block::TExtra_accessions& extra_accs, CGB_block::TExtra_accessions& hist)
{
    Int4 num1;
    Int4 num2;

    CGB_block::TExtra_accessions ret;

    for (const string& acc : extra_accs) {
        if (acc.empty())
            continue;

        size_t dash = acc.find('-');
        if (dash == string::npos) {
            ret.push_back(acc);
            continue;
        }

        string first(acc.begin(), acc.begin() + dash),
            last(acc.begin() + dash + 1, acc.end());
        size_t acclen = first.size();

        const Char* p = first.c_str();
        for (; (*p >= 'A' && *p <= 'Z') || *p == '_';)
            p++;

        size_t preflen = p - first.c_str();

        string prefix = first.substr(0, preflen);
        while (*p == '0')
            p++;

        const Char* q;
        for (q = p; *p >= '0' && *p <= '9';)
            p++;
        num1 = atoi(q);

        for (p = last.c_str() + preflen; *p == '0';)
            p++;
        for (q = p; *p >= '0' && *p <= '9';)
            p++;
        num2 = atoi(q);

        ret.push_back(first);

        if (num1 == num2)
            continue;

        for (num1++; num1 <= num2; num1++) {
            string new_acc = prefix;
            string num_str = FTAitoa(num1);
            size_t j       = acclen - preflen - num_str.size();

            for (size_t i = 0; i < j; i++)
                new_acc += '0';

            new_acc += num_str;
            ret.push_back(new_acc);
        }
    }

    ret.swap(hist);
}

static bool sIsPrefixChar(char c)
{
    return ('A' <= c && c <= 'Z') || c == '_';
}
/**********************************************************/
bool ParseAccessionRange(list<string>& tokens, int skip)
{
    bool bad = false;

    if (tokens.empty()) {
        return true;
    }

    if (tokens.size() <= skip + 1) {
        return true;
    }


    auto it = tokens.begin();
    if (skip) {
        advance(it, skip);
    }

    for (; it != tokens.end(); ++it) {
        const auto& token = *it;
        if (token.empty()) {
            continue;
        }

        CTempString first, last;
        if (! NStr::SplitInTwo(token, "-", first, last)) {
            continue;
        }
        if (first.size() != last.size()) {
            bad = true;
            break;
        }

        auto first_it =
            find_if_not(begin(first), end(first), sIsPrefixChar);

        if (first_it == first.end()) {
            bad = true;
            break;
        }


        auto last_it =
            find_if_not(begin(last), end(last), sIsPrefixChar);
        if (last_it == last.end()) {
            bad = true;
            break;
        }

        auto prefixLength = distance(first.begin(), first_it);
        if (prefixLength != distance(last.begin(), last_it) ||
            ! NStr::EqualCase(first, 0, prefixLength, last.substr(0, prefixLength))) {
            ErrPostEx(SEV_REJECT, ERR_ACCESSION_2ndAccPrefixMismatch, "Inconsistent prefix found in secondary accession range \"%s\".", token.c_str());
            break;
        }

        auto num1 = NStr::StringToInt(first.substr(prefixLength));
        auto num2 = NStr::StringToInt(last.substr(prefixLength));

        if (num2 <= num1) {
            ErrPostEx(SEV_REJECT, ERR_ACCESSION_Invalid2ndAccRange, "Invalid start/end values in secondary accession range \"%s\".", token.c_str());
        }

        *it = first;
        it  = tokens.insert(it, "-");
        it  = tokens.insert(it, last);
    }

    if (bad) {
        ErrPostEx(SEV_REJECT, ERR_ACCESSION_Invalid2ndAccRange, "Incorrect secondary accession range provided: \"%s\".", it->c_str());
    }
    return false;
}

/**********************************************************/
bool ParseAccessionRange(TokenStatBlkPtr tsbp, Int4 skip)
{
    TokenBlkPtr tbp;
    TokenBlkPtr tbpnext;
    char*       dash;
    const char* first;
    char*       last;
    const char* p;
    const char* q;
    bool        bad;
    Int4        num1;
    Int4        num2;

    if (! tsbp->list)
        return true;

    tbp = nullptr;
    if (skip == 0)
        tbp = tsbp->list;
    else if (skip == 1) {
        if (tsbp->list)
            tbp = tsbp->list->next;
    } else {
        if (tsbp->list && tsbp->list->next)
            tbp = tsbp->list->next->next;
    }
    if (! tbp)
        return true;

    for (bad = false; tbp; tbp = tbpnext) {
        tbpnext = tbp->next;
        if (! tbp->str)
            continue;
        dash = StringChr(tbp->str, '-');
        if (! dash)
            continue;
        *dash = '\0';
        first = tbp->str;
        last  = dash + 1;
        if (StringLen(first) != StringLen(last) || *first < 'A' ||
            *first > 'Z' || *last < 'A' || *last > 'Z') {
            *dash = '-';
            bad   = true;
            break;
        }

        for (p = first; (*p >= 'A' && *p <= 'Z') || *p == '_';)
            p++;
        if (*p < '0' || *p > '9') {
            *dash = '-';
            bad   = true;
            break;
        }
        for (q = last; (*q >= 'A' && *q <= 'Z') || *q == '_';)
            q++;
        if (*q < '0' || *q > '9') {
            *dash = '-';
            bad   = true;
            break;
        }
        size_t preflen = p - first;
        if (preflen != (size_t)(q - last) || ! StringEquN(first, last, preflen)) {
            *dash = '-';
            ErrPostEx(SEV_REJECT, ERR_ACCESSION_2ndAccPrefixMismatch, "Inconsistent prefix found in secondary accession range \"%s\".", tbp->str);
            break;
        }

        while (*p == '0')
            p++;
        for (q = p; *p >= '0' && *p <= '9';)
            p++;
        if (*p != '\0') {
            *dash = '-';
            bad   = true;
            break;
        }
        num1 = atoi(q);

        for (p = last + preflen; *p == '0';)
            p++;
        for (q = p; *p >= '0' && *p <= '9';)
            p++;
        if (*p != '\0') {
            *dash = '-';
            bad   = true;
            break;
        }
        num2 = atoi(q);

        if (num1 > num2) {
            *dash = '-';
            ErrPostEx(SEV_REJECT, ERR_ACCESSION_Invalid2ndAccRange, "Invalid start/end values in secondary accession range \"%s\".", tbp->str);
            break;
        }

        tbp->next = new TokenBlk;
        tbp       = tbp->next;
        tbp->str  = StringSave("-");
        tbp->next = new TokenBlk;
        tbp       = tbp->next;
        tbp->str  = StringSave(last);
        tsbp->num += 2;

        tbp->next = tbpnext;
    }
    if (! tbp)
        return true;
    if (bad) {
        ErrPostEx(SEV_REJECT, ERR_ACCESSION_Invalid2ndAccRange, "Incorrect secondary accession range provided: \"%s\".", tbp->str);
    }
    return false;
}

/**********************************************************/
static TokenBlkPtr TokenNodeNew(TokenBlkPtr tbp)
{
    TokenBlkPtr newnode = new TokenBlk;

    if (tbp) {
        while (tbp->next)
            tbp = tbp->next;
        tbp->next = newnode;
    }

    return (newnode);
}

/**********************************************************/
static void InsertTokenVal(TokenBlkPtr* tbp, const char* str)
{
    TokenBlkPtr ltbp;

    ltbp      = *tbp;
    ltbp      = TokenNodeNew(ltbp);
    ltbp->str = StringSave(str);

    if (! *tbp)
        *tbp = ltbp;
}

/**********************************************************
 *
 *   TokenStatBlkPtr TokenString(str, delimiter):
 *
 *      Parsing string "str" by delimiter or tab key, blank.
 *      Parsing stop at newline ('\n') or end of string ('\0').
 *      Return a statistics of link list token.
 *
 **********************************************************/
TokenStatBlkPtr TokenString(char* str, Char delimiter)
{
    char*           bptr;
    char*           ptr;
    char*           curtoken;
    Int2            num;
    TokenStatBlkPtr token;
    Char            ch;

    token = new TokenStatBlk;

    /* skip first several delimiters if any existed
     */
    for (ptr = str; *ptr == delimiter;)
        ptr++;

    for (num = 0; *ptr != '\0' && *ptr != '\r' && *ptr != '\n';) {
        for (bptr = ptr; *ptr != delimiter && *ptr != '\r' && *ptr != '\n' &&
                         *ptr != '\t' && *ptr != ' ' && *ptr != '\0';)
            ptr++;

        ch       = *ptr;
        *ptr     = '\0';
        curtoken = StringSave(bptr);
        *ptr     = ch;

        InsertTokenVal(&token->list, curtoken);
        num++;
        MemFree(curtoken);

        while (*ptr == delimiter || *ptr == '\t' || *ptr == ' ')
            ptr++;
    }

    token->num = num;

    return (token);
}

/**********************************************************/
void FreeTokenblk(TokenBlkPtr tbp)
{
    TokenBlkPtr temp;

    while (tbp) {
        temp = tbp;
        tbp  = tbp->next;
        MemFree(temp->str);
        delete temp;
    }
}

/**********************************************************/
void FreeTokenstatblk(TokenStatBlkPtr tsbp)
{
    FreeTokenblk(tsbp->list);
    delete tsbp;
}

/**********************************************************
 *
 *   Int2 fta_StringMatch(array, text):
 *
 *      Return array position of the matched length
 *   of string in array.
 *      Return -1 if no match.
 *
 **********************************************************/
Int2 fta_StringMatch(const Char** array, const Char* text)
{
    Int2 i;

    if (! text)
        return (-1);

    for (i = 0; *array; i++, array++) {
        if (NStr::EqualCase(text, 0, StringLen(*array), *array))
            break;
    }

    if (! *array)
        return (-1);

    return (i);
}

/**********************************************************
 *
 *   Int2 StringMatchIcase(array, text):
 *
 *      Return array position of the matched lenght of
 *   string (ignored case) in array.
 *      Return -1 if no match.
 *
 **********************************************************/
Int2 StringMatchIcase(const Char** array, const Char* text)
{
    Int2 i;

    if (! text)
        return (-1);

    for (i = 0; *array; i++, array++) {
        // If string from an array is empty its length == 0 and would be equval to any other string
        // The next 'if' statement will avoid that behavior
        if (text[0] != 0 && *array[0] == 0)
            continue;

        if (NStr::EqualNocase(text, 0, StringLen(*array), *array))
            break;
    }

    if (! *array)
        return (-1);
    return (i);
}

/**********************************************************
 *
 *   Int2 MatchArrayString(array, text):
 *
 *      Return array position of the string in the
 *   array.
 *      Return -1 if no match.
 *
 **********************************************************/
Int2 MatchArrayString(const char** array, const char* text)
{
    Int2 i;

    if (! text)
        return (-1);

    for (i = 0; *array; i++, array++) {
        if (NStr::Equal(*array, text))
            break;
    }

    if (! *array)
        return (-1);
    return (i);
}

/**********************************************************/
Int2 MatchArrayIString(const Char** array, const Char* text)
{
    Int2 i;

    if (! text)
        return (-1);

    for (i = 0; *array; i++, array++) {
        // If string from an array is empty its length == 0 and would be equval to any other string
        // The next 'if' statement will avoid that behavior
        if (text[0] != 0 && *array[0] == 0)
            continue;

        if (NStr::EqualNocase(*array, text))
            break;
    }

    if (! *array)
        return (-1);
    return (i);
}

/**********************************************************
 *
 *   Int2 MatchArraySubString(array, text):
 *
 *      Return array position of the string in the array
 *   if any array is in the substring of "text".
 *      Return -1 if no match.
 *
 **********************************************************/
Int2 MatchArraySubString(const Char** array, const Char* text)
{
    Int2 i;

    if (! text)
        return (-1);

    for (i = 0; *array; i++, array++) {
        if (NStr::Find(text, *array) != NPOS)
            break;
    }

    if (! *array)
        return (-1);
    return (i);
}

/**********************************************************/
Char* StringIStr(const Char* where, const Char* what)
{
    const Char* p;
    const Char* q;

    if (! where || *where == '\0' || ! what || *what == '\0')
        return nullptr;

    q = nullptr;
    for (; *where != '\0'; where++) {
        for (q = what, p = where; *q != '\0' && *p != '\0'; q++, p++) {
            if (*q == *p)
                continue;

            if (*q >= 'A' && *q <= 'Z') {
                if (*q + 32 == *p)
                    continue;
            } else if (*q >= 'a' && *q <= 'z') {
                if (*q - 32 == *p)
                    continue;
            }
            break;
        }
        if (*p == '\0' || *q == '\0')
            break;
    }
    if (q && *q == '\0')
        return const_cast<char*>(where);
    return nullptr;
}

/**********************************************************/
Int2 MatchArrayISubString(const Char** array, const Char* text)
{
    Int2 i;

    if (! text)
        return (-1);

    for (i = 0; *array; i++, array++) {
        if (NStr::FindNoCase(text, *array) != NPOS)
            break;
    }

    if (! *array)
        return (-1);
    return (i);
}

/**********************************************************
 *
 *   char* GetBlkDataReplaceNewLine(bptr, eptr,
 *                                    start_col_data):
 *
 *      Return a string which replace newline to blank
 *   and skip "XX" line data.
 *
 **********************************************************/
char* GetBlkDataReplaceNewLine(char* bptr, char* eptr, Int2 start_col_data)
{
    string instr(bptr, eptr - bptr);
    xGetBlkDataReplaceNewLine(instr, start_col_data);

    char* ptr;

    if (bptr + start_col_data >= eptr)
        return nullptr;

    size_t size   = eptr - bptr;
    char*  retstr = MemNew(size + 1);
    char*  str    = retstr;

    while (bptr < eptr) {
        if (NStr::Equal(bptr, 0, 2, "XX")) /* skip XX line data */
        {
            ptr  = SrchTheChar(bptr, eptr, '\n');
            bptr = ptr + 1;
            continue;
        }

        bptr += start_col_data;
        ptr = SrchTheChar(bptr, eptr, '\n');

        if (ptr) {
            size = ptr - bptr;
            MemCpy(str, bptr, size);
            str += size;
            if (*(ptr - 1) != '-' || *(ptr - 2) == ' ') {
                StringCpy(str, " ");
                str++;
            }
            bptr = ptr;
        }
        bptr++;
    }

    string tstr = NStr::TruncateSpaces(string(retstr), NStr::eTrunc_End);
    MemFree(retstr);
    retstr = StringSave(tstr.c_str());
    return (retstr);
}

void xGetBlkDataReplaceNewLine(string& instr, int indent)
{
    vector<string> lines;
    NStr::Split(instr, "\n", lines);
    string replaced;
    for (auto line : lines) {
        if (line.empty() || NStr::StartsWith(line, "XX")) {
            continue;
        }
        replaced += line.substr(indent);
        auto last = line.size() - 1;
        if (line[last] != '-') {
            replaced += ' ';
        } else if (line[last - 1] == ' ') {
            replaced += ' ';
        }
    }
    NStr::TruncateSpacesInPlace(replaced);
    instr = replaced;
}


/**********************************************************/
static size_t SeekLastAlphaChar(const Char* str, size_t len)
{
    if (str && len != 0) {
        for (size_t ret = len; ret > 0;) {
            char c = str[--ret];
            if (c != ' ' && c != '\n' && c != '\\' && c != ',' &&
                c != ';' && c != '~' && c != '.' && c != ':') {
                return ret + 1;
            }
        }
    }

    return 0;
}

/**********************************************************/
void CleanTailNoneAlphaCharInString(string& str)
{
    size_t ret = SeekLastAlphaChar(str.c_str(), str.size());
    str        = str.substr(0, ret);
}

/**********************************************************
 *
 *   void CleanTailNoneAlphaChar(str):
 *
 *      Delete any tailing ' ', '\n', '\\', ',', ';', '~',
 *   '.', ':' characters.
 *
 **********************************************************/
void CleanTailNoneAlphaChar(char* str)
{
    if (! str || *str == '\0')
        return;

    size_t last = SeekLastAlphaChar(str, strlen(str));
    str[last]   = '\0';
}

/**********************************************************/
char* PointToNextToken(char* ptr)
{
    if (ptr) {
        while (*ptr != ' ')
            ptr++;
        while (*ptr == ' ')
            ptr++;
    }
    return (ptr);
}

/**********************************************************
 *
 *   char* GetTheCurrentToken(ptr):
 *
 *      Return the current token (also CleanTailNoneAlphaChar)
 *   which ptr points to and ptr will points to next token
 *   after the routine return.
 *
 **********************************************************/
char* GetTheCurrentToken(char** ptr)
{
    char* retptr;
    char* bptr;
    char* str;
    Char  ch;

    bptr = retptr = *ptr;
    if (! retptr || *retptr == '\0')
        return nullptr;

    while (*retptr != '\0' && *retptr != ' ')
        retptr++;

    ch      = *retptr;
    *retptr = '\0';
    str     = StringSave(bptr);
    *retptr = ch;

    while (*retptr != '\0' && *retptr == ' ') /* skip blanks */
        retptr++;
    *ptr = retptr;

    CleanTailNoneAlphaChar(str);
    return (str);
}

/**********************************************************
 *
 *   char* SrchTheChar(bptr, eptr, letter):
 *
 *      Search The character letter.
 *      Return NULL if not found; otherwise, return
 *   a pointer points first occurrence The character.
 *
 **********************************************************/
char* SrchTheChar(char* bptr, char* eptr, Char letter)
{
    while (bptr < eptr && *bptr != letter)
        bptr++;

    if (bptr < eptr)
        return (bptr);

    return nullptr;
}

/**********************************************************
 *
 *   char* SrchTheStr(bptr, eptr, leadstr):
 *
 *      Search The leading string.
 *      Return NULL if not found; otherwise, return
 *   a pointer points first occurrence The leading string.
 *
 **********************************************************/
char* SrchTheStr(char* bptr, char* eptr, const char* leadstr)
{
    char* p;
    Char  c;

    c     = *eptr;
    *eptr = '\0';
    p     = StringStr(bptr, leadstr);
    *eptr = c;
    return (p);
}

/**********************************************************/
void CpSeqId(InfoBioseqPtr ibp, const CSeq_id& id)
{
    const CTextseq_id* text_id = id.GetTextseq_Id();
    if (text_id) {
        if (text_id->IsSetName())
            ibp->mLocus = text_id->GetName();

        CRef<CSeq_id> new_id(new CSeq_id);
        if (text_id->IsSetAccession()) {
            ibp->mAccNum = text_id->GetAccession();

            CRef<CTextseq_id> new_text_id(new CTextseq_id);
            new_text_id->SetAccession(text_id->GetAccession());
            if (text_id->IsSetVersion())
                new_text_id->SetVersion(text_id->GetVersion());

            SetTextId(id.Which(), *new_id, *new_text_id);
        } else {
            new_id->Assign(id);
        }

        ibp->ids.push_back(new_id);
    } else {
        auto pId = Ref(new CSeq_id());
        pId->Assign(id);
        ibp->ids.push_back(move(pId));
    }
}

/**********************************************************
 *
 *   CRef<CDate_std> get_full_date(s, is_ref, source):
 *
 *      Get year, month, day and return CRef<CDate_std>.
 *
 **********************************************************/
CRef<CDate_std> get_full_date(const char* s, bool is_ref, Parser::ESource source)
{
    CRef<CDate_std> date;

    if (! s || *s == '\0')
        return date;

    int parse_day = 0;
    if (isdigit(*s) != 0) {
        parse_day = atoi(s);
        s += 3;
        // should we make at least a token effort of validation (like <32)?
    }

    static const vector<string> months{
        "JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"
    };
    CTempString maybe_month(s, 3);
    auto        it = find(months.begin(), months.end(), maybe_month);
    if (it == months.end()) {
        char msg[11];
        StringNCpy(msg, s, 10);
        msg[10] = '\0';
        is_ref ? ErrPostEx(
                     SEV_WARNING, ERR_REFERENCE_IllegalDate, "Unrecognized month: %s", msg)
               : ErrPostEx(
                     SEV_WARNING, ERR_DATE_IllegalDate, "Unrecognized month: %s", msg);
        return date;
    }
    int parse_month = int(it - months.begin()) + 1;

    s += 4;

    int parse_year = atoi(s);
    int cur_year   = CCurrentTime().Year();
    if (1900 <= parse_year && parse_year <= cur_year) {
        // all set
    } else if (0 <= parse_year && parse_year <= 99 && '0' <= s[1] && s[1] <= '9') {
        // insist that short form year has exactly two digits
        (parse_year < 70) ? (parse_year += 2000) : (parse_year += 1900);
    } else {
        if (is_ref) {
            ErrPostEx(
                SEV_ERROR, ERR_REFERENCE_IllegalDate, "Illegal year: %d, current year: %d", parse_year, cur_year);
        } else if (source != Parser::ESource::SPROT || parse_year - cur_year > 1) {
            ErrPostEx(
                SEV_WARNING, ERR_DATE_IllegalDate, "Illegal year: %d, current year: %d", parse_year, cur_year);
        }
        // treat bad year like bad month above:
        return date;
    }
    date.Reset(new CDate_std);
    date->SetYear(parse_year);
    date->SetMonth(parse_month);
    date->SetDay(parse_day);

    return date;
}

/**********************************************************
 *
 *   int SrchKeyword(ptr, kwl):
 *
 *      Compare first kwl.len byte in ptr to kwl.str.
 *      Return the position of keyword block array;
 *   return unknown keyword, UNKW, if not found.
 *
 *                                              3-25-93
 *
 **********************************************************/
int SrchKeyword(const CTempString& ptr, const vector<string>& keywordList)
{
    int keywordCount = keywordList.size();

    for (int i = 0; i < keywordCount; ++i) {
        if (NStr::StartsWith(ptr, keywordList[i])) {
            return i;
        }
    }
    return ParFlat_UNKW;
}

/**********************************************************/
bool CheckLineType(char* ptr, Int4 line, const vector<string>& keywordList, bool after_origin)
{
    char* p;
    Char  msg[51];
    Int2  i;

    if (after_origin) {
        for (p = ptr; *p >= '0' && *p <= '9';)
            p++;
        if (*p == ' ')
            return true;
    }

    auto keywordCount = keywordList.size();
    for (i = 0; i < keywordCount; i++) {
        auto keyword = keywordList[i];
        if (StringEquN(ptr, keyword.c_str(), keyword.size()))
            return true;
    }

    StringNCpy(msg, ptr, 50);
    msg[50] = '\0';
    p       = StringChr(msg, '\n');
    if (p)
        *p = '\0';
    ErrPostEx(SEV_ERROR, ERR_ENTRY_InvalidLineType, "Unknown linetype \"%s\". Line number %d.", msg, line);
    if (p)
        *p = '\n';

    return false;
}

/**********************************************************
 *
 *   char* SrchNodeType(entry, type, len):
 *
 *      Return a memory location of the node which has
 *   the "type".
 *
 **********************************************************/
char* SrchNodeType(DataBlkPtr entry, Int4 type, size_t* len)
{
    DataBlkPtr temp;

    temp = TrackNodeType(*entry, (Int2)type);
    if (temp) {
        *len = temp->len;
        return (temp->mOffset);
    }

    *len = 0;
    return nullptr;
}

char* xSrchNodeType(const DataBlk& entry, Int4 type, size_t* len)
{
    DataBlkPtr temp;

    temp = TrackNodeType(entry, (Int2)type);
    if (temp) {
        *len = temp->len;
        return (temp->mOffset);
    }

    *len = 0;
    return nullptr;
}

string xGetNodeData(const DataBlk& entry, int nodeType)
{
    auto tmp = TrackNodeType(entry, (Int2)nodeType);
    if (! tmp) {
        return "";
    }
    return string(tmp->mOffset, tmp->len);
}

/**********************************************************
 *
 *   DataBlkPtr TrackNodeType(entry, type):
 *
 *      Return a pointer points to the Node which has
 *   the "type".
 *
 **********************************************************/
DataBlkPtr TrackNodeType(const DataBlk& entry, Int2 type)
{
    DataBlkPtr  temp;
    EntryBlkPtr ebp;

    ebp  = static_cast<EntryBlk*>(entry.mpData);
    temp = ebp->chain;
    while (temp && temp->mType != type)
        temp = temp->mpNext;

    return (temp);
}


const SectionPtr xTrackNodeType(const Entry& entry, int type)
{
    for (SectionPtr sectionPtr : entry.mSections) {
        if (sectionPtr->mType == type) {
            return sectionPtr;
        }
    }
    return nullptr;
}


/**********************************************************/
bool fta_tpa_keywords_check(const TKeywordList& kwds)
{
    const char* b[4];

    bool kwd_tpa   = false;
    bool kwd_party = false;
    bool kwd_inf   = false;
    bool kwd_exp   = false;
    bool kwd_asm   = false;
    bool kwd_spedb = false;
    bool ret       = true;

    Int4 j;
    Int2 i;

    if (kwds.empty())
        return true;

    size_t len = 0;
    j          = 0;
    for (const string& key : kwds) {
        if (key.empty())
            continue;

        const char* p = key.c_str();
        i             = MatchArrayIString(ParFlat_TPA_kw_array, p);
        if (i == 0)
            kwd_tpa = true;
        else if (i == 1 || i == 2)
            kwd_party = true;
        else if (i == 3)
            kwd_inf = true;
        else if (i == 4)
            kwd_exp = true;
        else if (i == 5 || i == 6)
            kwd_asm = true;
        else if (i == 7)
            kwd_spedb = true;
        else if (NStr::EqualNocase(p, 0, 3, "TPA")) {
            if (p[3] == ':') {
                ErrPostEx(SEV_REJECT, ERR_KEYWORD_InvalidTPATier, "Keyword \"%s\" is not a valid TPA-tier keyword.", p);
                ret = false;
            } else if (p[3] != '\0' && p[4] != '\0') {
                ErrPostEx(SEV_WARNING, ERR_KEYWORD_UnexpectedTPA, "Keyword \"%s\" looks like it might be TPA-related, but it is not a recognized TPA keyword.", p);
            }
        }
        if (i > 2 && i < 8 && j < 4) {
            b[j] = p;
            ++j;
            len += key.size() + 1;
        }
    }

    if (kwd_tpa && ! kwd_party) {
        ErrPostEx(SEV_REJECT, ERR_KEYWORD_MissingTPAKeywords, "This TPA-record should have keyword \"Third Party Annotation\" or \"Third Party Data\" in addition to \"TPA\".");
        ret = false;
    } else if (! kwd_tpa && kwd_party) {
        ErrPostEx(SEV_REJECT, ERR_KEYWORD_MissingTPAKeywords, "This TPA-record should have keyword \"TPA\" in addition to \"Third Party Annotation\" or \"Third Party Data\".");
        ret = false;
    }
    if (! kwd_tpa && (kwd_inf || kwd_exp)) {
        ErrPostEx(SEV_REJECT, ERR_KEYWORD_MissingTPAKeywords, "This TPA-record should have keyword \"TPA\" in addition to its TPA-tier keyword.");
        ret = false;
    } else if (kwd_tpa && kwd_inf == false && kwd_exp == false &&
               kwd_asm == false && kwd_spedb == false) {
        ErrPostEx(SEV_ERROR, ERR_KEYWORD_MissingTPATier, "This TPA record lacks a keyword to indicate which tier it belongs to: experimental, inferential, reassembly or specialist_db.");
    }
    if (j > 1) {
        string buf;
        for (i = 0; i < j; i++) {
            if (i > 0)
                buf += ';';
            buf += b[i];
        }
        ErrPostEx(SEV_REJECT, ERR_KEYWORD_ConflictingTPATiers, "Keywords for multiple TPA tiers exist on this record: \"%s\". A TPA record can only be in one tier.", buf.c_str());
        ret = false;
    }

    return (ret);
}

/**********************************************************/
bool fta_tsa_keywords_check(const TKeywordList& kwds, Parser::ESource source)
{
    bool kwd_tsa      = false;
    bool kwd_assembly = false;
    bool ret          = true;
    Int2 i;

    if (kwds.empty())
        return true;

    for (const string& key : kwds) {
        if (key.empty())
            continue;
        i = MatchArrayIString(ParFlat_TSA_kw_array, key.c_str());
        if (i == 0)
            kwd_tsa = true;
        else if (i == 1)
            kwd_assembly = true;
        else if (source == Parser::ESource::EMBL &&
                 NStr::EqualNocase(key, "Transcript Shotgun Assembly"))
            kwd_assembly = true;
    }

    if (kwd_tsa && ! kwd_assembly) {
        ErrPostEx(SEV_REJECT, ERR_KEYWORD_MissingTSAKeywords, "This TSA-record should have keyword \"Transcriptome Shotgun Assembly\" in addition to \"TSA\".");
        ret = false;
    } else if (! kwd_tsa && kwd_assembly) {
        ErrPostEx(SEV_REJECT, ERR_KEYWORD_MissingTSAKeywords, "This TSA-record should have keyword \"TSA\" in addition to \"Transcriptome Shotgun Assembly\".");
        ret = false;
    }
    return (ret);
}

/**********************************************************/
bool fta_tls_keywords_check(const TKeywordList& kwds, Parser::ESource source)
{
    bool kwd_tls   = false;
    bool kwd_study = false;
    bool ret       = true;
    Int2 i;

    if (kwds.empty())
        return true;

    for (const string& key : kwds) {
        if (key.empty())
            continue;
        i = MatchArrayIString(ParFlat_TLS_kw_array, key.c_str());
        if (i == 0)
            kwd_tls = true;
        else if (i == 1)
            kwd_study = true;
        else if (source == Parser::ESource::EMBL &&
                 NStr::EqualNocase(key, "Targeted Locus Study"))
            kwd_study = true;
    }

    if (kwd_tls && ! kwd_study) {
        ErrPostEx(SEV_REJECT, ERR_KEYWORD_MissingTLSKeywords, "This TLS-record should have keyword \"Targeted Locus Study\" in addition to \"TLS\".");
        ret = false;
    } else if (! kwd_tls && kwd_study) {
        ErrPostEx(SEV_REJECT, ERR_KEYWORD_MissingTLSKeywords, "This TLS-record should have keyword \"TLS\" in addition to \"Targeted Locus Study\".");
        ret = false;
    }
    return (ret);
}

/**********************************************************/
bool fta_is_tpa_keyword(const char* str)
{
    if (! str || *str == '\0' || MatchArrayIString(ParFlat_TPA_kw_array, str) < 0)
        return false;

    return true;
}

/**********************************************************/
bool fta_is_tsa_keyword(const char* str)
{
    if (! str || *str == '\0' || MatchArrayIString(ParFlat_TSA_kw_array, str) < 0)
        return false;
    return true;
}

/**********************************************************/
bool fta_is_tls_keyword(const char* str)
{
    if (! str || *str == '\0' || MatchArrayIString(ParFlat_TLS_kw_array, str) < 0)
        return false;
    return true;
}

/**********************************************************/
void fta_keywords_check(const char* str, bool* estk, bool* stsk, bool* gssk, bool* htck, bool* flik, bool* wgsk, bool* tpak, bool* envk, bool* mgak, bool* tsak, bool* tlsk)
{
    if (estk && MatchArrayString(ParFlat_EST_kw_array, str) != -1)
        *estk = true;

    if (stsk && MatchArrayString(ParFlat_STS_kw_array, str) != -1)
        *stsk = true;

    if (gssk && MatchArrayString(ParFlat_GSS_kw_array, str) != -1)
        *gssk = true;

    if (htck && MatchArrayString(ParFlat_HTC_kw_array, str) != -1)
        *htck = true;

    if (flik && MatchArrayString(ParFlat_FLI_kw_array, str) != -1)
        *flik = true;

    if (wgsk && MatchArrayString(ParFlat_WGS_kw_array, str) != -1)
        *wgsk = true;

    if (tpak && MatchArrayString(ParFlat_TPA_kw_array, str) != -1)
        *tpak = true;

    if (envk && MatchArrayString(ParFlat_ENV_kw_array, str) != -1)
        *envk = true;

    if (mgak && MatchArrayString(ParFlat_MGA_kw_array, str) != -1)
        *mgak = true;

    if (tsak && MatchArrayString(ParFlat_TSA_kw_array, str) != -1)
        *tsak = true;

    if (tlsk && MatchArrayString(ParFlat_TLS_kw_array, str) != -1)
        *tlsk = true;
}

/**********************************************************/
void fta_remove_keywords(CMolInfo::TTech tech, TKeywordList& kwds)
{
    const char** b;

    if (kwds.empty())
        return;

    if (tech == CMolInfo::eTech_est)
        b = ParFlat_EST_kw_array;
    else if (tech == CMolInfo::eTech_sts)
        b = ParFlat_STS_kw_array;
    else if (tech == CMolInfo::eTech_survey)
        b = ParFlat_GSS_kw_array;
    else if (tech == CMolInfo::eTech_htc)
        b = ParFlat_HTC_kw_array;
    else if (tech == CMolInfo::eTech_fli_cdna)
        b = ParFlat_FLI_kw_array;
    else if (tech == CMolInfo::eTech_wgs)
        b = ParFlat_WGS_kw_array;
    else
        return;

    for (TKeywordList::iterator key = kwds.begin(); key != kwds.end();) {
        if (key->empty() || MatchArrayString(b, key->c_str()) != -1) {
            key = kwds.erase(key);
        } else
            ++key;
    }
}

/**********************************************************/
void fta_remove_tpa_keywords(TKeywordList& kwds)
{
    if (kwds.empty())
        return;

    for (TKeywordList::iterator key = kwds.begin(); key != kwds.end();) {
        if (key->empty() || MatchArrayIString(ParFlat_TPA_kw_array_to_remove, key->c_str()) != -1) {
            key = kwds.erase(key);
        } else
            ++key;
    }
}

/**********************************************************/
void fta_remove_tsa_keywords(TKeywordList& kwds, Parser::ESource source)
{
    if (kwds.empty())
        return;

    for (TKeywordList::iterator key = kwds.begin(); key != kwds.end();) {
        if (key->empty() || MatchArrayIString(ParFlat_TSA_kw_array, key->c_str()) != -1 ||
            (source == Parser::ESource::EMBL && NStr::EqualNocase(*key, "Transcript Shotgun Assembly"))) {
            key = kwds.erase(key);
        } else
            ++key;
    }
}

/**********************************************************/
void fta_remove_tls_keywords(TKeywordList& kwds, Parser::ESource source)
{
    if (kwds.empty())
        return;

    for (TKeywordList::iterator key = kwds.begin(); key != kwds.end();) {
        if (key->empty() || MatchArrayIString(ParFlat_TLS_kw_array, key->c_str()) != -1 ||
            (source == Parser::ESource::EMBL && NStr::EqualNocase(*key, "Targeted Locus Study"))) {
            key = kwds.erase(key);
        } else
            ++key;
    }
}

/**********************************************************/
void fta_remove_env_keywords(TKeywordList& kwds)
{
    if (kwds.empty())
        return;

    for (TKeywordList::iterator key = kwds.begin(); key != kwds.end();) {
        if (key->empty() || MatchArrayIString(ParFlat_ENV_kw_array, key->c_str()) != -1) {
            key = kwds.erase(key);
        } else
            ++key;
    }
}

/**********************************************************/
void fta_remove_mag_keywords(TKeywordList& kwds)
{
    if (kwds.empty())
        return;

    for (TKeywordList::iterator key = kwds.begin(); key != kwds.end();) {
        if (key->empty() || MatchArrayIString(ParFlat_MAG_kw_array, key->c_str()) != -1) {
            key = kwds.erase(key);
        } else
            ++key;
    }
}

/**********************************************************/
void xCheckEstStsGssTpaKeywords(
    const list<string> keywordList,
    bool               tpa_check,
    IndexblkPtr        entry
    // bool& specialist_db,
    // bool& inferential,
    // bool& experimental,
    // bool& assembly
)
{
    if (keywordList.empty()) {
        return;
    }
    for (auto keyword : keywordList) {
        fta_keywords_check(
            keyword.c_str(), &entry->EST, &entry->STS, &entry->GSS, &entry->HTC, nullptr, nullptr, (tpa_check ? &entry->is_tpa : nullptr), nullptr, nullptr, nullptr, nullptr);
        if (NStr::EqualNocase(keyword, "TPA:assembly")) {
            entry->specialist_db = true;
            entry->assembly      = true;
            continue;
        }
        if (NStr::EqualNocase(keyword, "TPA:specialist_db")) {
            entry->specialist_db = true;
            continue;
        }
        if (NStr::EqualNocase(keyword, "TPA:inferential")) {
            entry->inferential = true;
            continue;
        }
        if (NStr::EqualNocase(keyword, "TPA:experimental")) {
            entry->experimental = true;
            continue;
        }
    }
}

void check_est_sts_gss_tpa_kwds(ValNodePtr kwds, size_t len, IndexblkPtr entry, bool tpa_check, bool& specialist_db, bool& inferential, bool& experimental, bool& assembly)
{
    char* line;
    char* p;
    char* q;

    if (! kwds || ! kwds->data || len < 1)
        return;

    line    = MemNew(len + 1);
    line[0] = '\0';
    for (; kwds; kwds = kwds->next) {
        StringCat(line, kwds->data);
    }
    for (p = line; *p != '\0'; p++)
        if (*p == '\n' || *p == '\t')
            *p = ' ';
    for (p = line; *p == ' ' || *p == '.' || *p == ';';)
        p++;
    if (*p == '\0') {
        MemFree(line);
        return;
    }
    for (q = p; *q != '\0';)
        q++;
    for (q--; *q == ' ' || *q == '.' || *q == ';'; q--)
        *q = '\0';
    for (q = p, p = line; *q != '\0';) {
        if (*q != ' ' && *q != ';') {
            *p++ = *q++;
            continue;
        }
        if (*q == ' ') {
            for (q++; *q == ' ';)
                q++;
            if (*q != ';')
                *p++ = ' ';
        }
        if (*q == ';') {
            *p++ = *q++;
            while (*q == ' ' || *q == ';')
                q++;
        }
    }
    *p++ = ';';
    *p   = '\0';
    for (p = line;; p = q + 1) {
        q = StringChr(p, ';');
        if (! q)
            break;
        *q = '\0';
        fta_keywords_check(p, &entry->EST, &entry->STS, &entry->GSS, &entry->HTC, nullptr, nullptr, (tpa_check ? &entry->is_tpa : nullptr), nullptr, nullptr, nullptr, nullptr);
        if (NStr::EqualNocase(p, "TPA:specialist_db") ||
            NStr::EqualNocase(p, "TPA:assembly")) {
            specialist_db = true;
            if (NStr::EqualNocase(p, "TPA:assembly"))
                assembly = true;
        } else if (NStr::EqualNocase(p, "TPA:inferential"))
            inferential = true;
        else if (NStr::EqualNocase(p, "TPA:experimental"))
            experimental = true;
    }
    MemFree(line);
}

/**********************************************************/
ValNodePtr ConstructValNode(CSeq_id::E_Choice choice, const char* data)
{
    ValNodePtr res;

    res         = ValNodeNew(nullptr, data);
    res->choice = choice;
    return (res);
}

/**********************************************************/
bool fta_check_mga_keywords(CMolInfo& mol_info, const TKeywordList& kwds)
{
    bool is_cage;
    bool is_sage;

    TKeywordList::const_iterator key_it = kwds.end();

    bool got = false;
    if (! kwds.empty() && NStr::EqualNocase(kwds.front(), "MGA")) {
        for (TKeywordList::const_iterator key = kwds.begin(); key != kwds.end(); ++key) {
            if (MatchArrayIString(ParFlat_MGA_more_kw_array,
                                  key->c_str()) < 0)
                continue;
            got    = true;
            key_it = key;
            break;
        }
    }

    if (! got) {
        ErrPostEx(SEV_REJECT, ERR_KEYWORD_MissingMGAKeywords, "This is apparently a CAGE record, but it lacks the required keywords. Entry dropped.");
        return false;
    }

    if (! mol_info.IsSetTechexp() || ! kwds.empty() ||
        mol_info.GetTechexp() != "cage")
        return true;

    for (is_sage = false, is_cage = false; key_it != kwds.end(); ++key_it) {
        const char* p = key_it->c_str();

        if (NStr::EqualNocase(p, "5'-SAGE"))
            is_sage = true;
        else if (NStr::EqualNocase(p, "CAGE (Cap Analysis Gene Expression)"))
            is_cage = true;
    }

    if (is_sage) {
        if (is_cage) {
            ErrPostEx(SEV_REJECT, ERR_KEYWORD_ConflictingMGAKeywords, "This MGA record contains more than one of the special keywords indicating different techniques.");
            return false;
        }
        mol_info.SetTechexp("5'-sage");
    }

    return true;
}

/**********************************************************/
void fta_StringCpy(char* dst, const char* src)
{
    const char* p;
    char*       q;

    for (q = dst, p = src; *p != '\0';)
        *q++ = *p++;
    *q = '\0';
}

/**********************************************************/
bool SetTextId(Uint1 seqtype, CSeq_id& seqId, CTextseq_id& textId)
{
    bool wasSet = true;

    switch (seqtype) {
    case CSeq_id::e_Genbank:
        seqId.SetGenbank(textId);
        break;
    case CSeq_id::e_Embl:
        seqId.SetEmbl(textId);
        break;
    case CSeq_id::e_Pir:
        seqId.SetPir(textId);
        break;
    case CSeq_id::e_Swissprot:
        seqId.SetSwissprot(textId);
        break;
    case CSeq_id::e_Other:
        seqId.SetOther(textId);
        break;
    case CSeq_id::e_Ddbj:
        seqId.SetDdbj(textId);
        break;
    case CSeq_id::e_Prf:
        seqId.SetPrf(textId);
        break;
    case CSeq_id::e_Pdb: {
        // TODO: test this branch
        CPDB_seq_id pdbId;
        pdbId.SetChain_id();
        seqId.SetPdb(pdbId);
    } break;
    case CSeq_id::e_Tpg:
        seqId.SetTpg(textId);
        break;
    case CSeq_id::e_Tpe:
        seqId.SetTpe(textId);
        break;
    case CSeq_id::e_Tpd:
        seqId.SetTpd(textId);
        break;
    case CSeq_id::e_Gpipe:
        seqId.SetGpipe(textId);
        break;
    case CSeq_id::e_Named_annot_track:
        seqId.SetNamed_annot_track(textId);
        break;

    default:
        wasSet = false;
    }

    return wasSet;
}

/**********************************************************/
bool IsCancelled(const TKeywordList& keywords)
{
    for (const string& key : keywords) {
        if (NStr::EqualNocase(key, "HTGS_CANCELLED"))
            return true;
    }

    return false;
}

/**********************************************************/
bool HasHtg(const TKeywordList& keywords)
{
    for (const string& key : keywords) {
        if (key == "HTG" || key == "HTGS_PHASE0" ||
            key == "HTGS_PHASE1" || key == "HTGS_PHASE2" ||
            key == "HTGS_PHASE3") {
            return true;
        }
    }

    return false;
}

/**********************************************************/
void RemoveHtgPhase(TKeywordList& keywords)
{
    for (TKeywordList::iterator key = keywords.begin(); key != keywords.end();) {
        const char* p = key->c_str();
        if (NStr::EqualNocase(p, 0, 10, "HTGS_PHASE") &&
            (p[10] == '0' || p[10] == '1' || p[10] == '2' ||
             p[10] == '3') &&
            p[11] == '\0') {
            key = keywords.erase(key);
        } else
            ++key;
    }
}

/**********************************************************/
bool HasHtc(const TKeywordList& keywords)
{
    for (const string& key : keywords) {
        if (NStr::EqualNocase(key, "HTC")) {
            return true;
        }
    }

    return false;
}

END_NCBI_SCOPE
