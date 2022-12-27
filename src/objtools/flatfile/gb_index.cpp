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
 * File Name: gb_index.cpp
 *
 * Author: Karl Sirotkin, Hsiu-Chuan Chen
 *
 * File Description:
 *      Parsing genbank to memory blocks. Build Genbank format index block.
 *
 */

#include <ncbi_pch.hpp>

#include "ftacpp.hpp"

#include "index.h"
#include "genbank.h"

#include "ftaerr.hpp"
#include "indx_blk.h"
#include "indx_def.h"
#include "utilfun.h"
#include "entry.h"

#ifdef THIS_FILE
#    undef THIS_FILE
#endif
#define THIS_FILE "gb_index.cpp"


BEGIN_NCBI_SCOPE

vector<string> genbankKeywords = {
    "LOCUS",
    "DEFINITION",
    "ACCESSION",
    "NID",
    "GSDB ID",
    "KEYWORDS",
    "SEGMENT",
    "SOURCE",
    "REFERENCE",
    "COMMENT",
    "FEATURES",
    "BASE COUNT",
    "ORIGIN",
    "//",
    "GSDBID",
    "CONTIG",
    "VERSION",
    "USER",
    "WGS",
    "PRIMARY",
    "MGA",
    "PROJECT",
    "DBLINK",
};


// LCOV_EXCL_START
// Excluded per Mark's request on 12/14/2016
/**********************************************************
 *
 *   static bool DelSegnum(str, segnum, len2):
 *
 *      Strip off segnum which has number of "len1" digits,
 *   then check if any tailing zero existed.
 *      Subroutine return:
 *   TRUE if
 *   - there is no tailing zero or
 *   - the number of the tailing zero is equal or greater
 *     than (len2-len1) (i.e. strip off len2-len1 of "0").
 *   FALSE and no change in the string "str" if
 *   - len2-len1 less than zero or
 *   - there is not enough "len1" digits at end of
 *     the string "str" or
 *   - there is not enough len2-len1 zero at end of
 *     the string "str".
 *
 *                                      February 25 1993
 *
 **********************************************************/
static bool DelSegnum(IndexblkPtr entry, const char* segnum, size_t len2)
{
    char* str;
    const char* p;
    char* q;

    if (! segnum)
        return false;
    size_t len1 = StringLen(segnum);
    if (len2 < len1)
        return false;

    /* check, is there enough digits to delete
     */
    size_t tlen = len1;
    str         = entry->blocusname;
    size_t i    = StringLen(str);
    for (; tlen > 0; tlen--) {
        char c = str[--i];
        if (! ('0' <= c && c <= '9'))
            break;
        if (i <= 0)
            return false;
    }

    if (tlen > 0)
        return false;

    if (len2 > len1 && str[i] == '0') {
        /* check, is there enough "0" appended
         */
        for (tlen = len2 - len1; tlen > 0 && str[i] == '0'; i--)
            tlen--;

        if (tlen != 0)
            return false;
    }

    for (q = &str[i + 1], p = q; *p == '0';)
        p++;

    int j = atoi(segnum);
    if (atoi(p) != j) {
        ErrPostEx(SEV_REJECT, ERR_SEGMENT_BadLocusName, "Segment suffix in locus name \"%s\" does not match number in SEGMENT line = \"%d\". Entry dropped.", str, j);
        entry->drop = true;
    }

    *q = '\0'; /* strip off "len" characters */
    return true;
}

/**********************************************************/
static void GetSegment(char* str, IndexblkPtr entry)
{
    TokenStatBlkPtr stoken;
    TokenBlkPtr     ptr2;
    TokenBlkPtr     ptr4;

    stoken = TokenString(str, ' ');

    if (stoken->num > 3) {
        ptr2          = stoken->list->next;
        ptr4          = ptr2->next->next;
        entry->segnum = (Uint2)atoi(ptr2->str);

        if (! DelSegnum(entry, ptr2->str, StringLen(ptr4->str))) {
            ErrPostEx(SEV_ERROR, ERR_SEGMENT_BadLocusName, "Bad locus name %s in %d", entry->blocusname, entry->linenum);
        }

        entry->segtotal = (Uint2)atoi(ptr4->str);
    } else {
        ErrPostEx(SEV_ERROR, ERR_SEGMENT_IncompSeg, "Incomplete Segment information at linenum %d", entry->linenum);
    }

    FreeTokenstatblk(stoken);
}
// LCOV_EXCL_STOP

/**********************************************************/
static bool gb_err_field(const char* str)
{
    ErrPostEx(SEV_ERROR, ERR_FORMAT_MissingField, "No %s data in GenBank format file, entry dropped", str);
    return true;
}

/**********************************************************/
static void ParseGenBankVersion(IndexblkPtr entry, char* line, char* nid, Parser::ESource source, Parser::EMode mode, bool ign_toks)
{
    bool  gi;
    char* p;
    char* q;
    char* r;
    Char  ch;
    Char  ch1;

    if (! line)
        return;

    for (p = line; *p != '\0' && *p != ' ' && *p != '\t';)
        p++;
    gi = (*p == '\0') ? false : true;

    ch1 = *p;
    *p  = '\0';
    q   = StringRChr(line, '.');
    if (! q) {
        if (mode != Parser::EMode::Relaxed) {
            *p = ch1;
            ErrPostEx(SEV_FATAL, ERR_VERSION_MissingVerNum, "Missing VERSION number in VERSION line: \"%s\".", line);
            entry->drop = true;
        }
        return;
    }

    for (r = q + 1; *r >= '0' && *r <= '9';)
        r++;
    if (*r != '\0') {
        if (mode != Parser::EMode::Relaxed) {
            *p = ch1;
            ErrPostEx(SEV_FATAL, ERR_VERSION_NonDigitVerNum, "Incorrect VERSION number in VERSION line: \"%s\".", line);
            entry->drop = true;
        }
        return;
    }
    ch = *q;
    *q = '\0';
    if (StringCmp(entry->acnum, line) != 0) {
        *q = ch;
        *p = ch1;
        if (mode != Parser::EMode::Relaxed) {
            ErrPostEx(SEV_FATAL, ERR_VERSION_AccessionsDontMatch, "Accessions in VERSION and ACCESSION lines don't match: \"%s\" vs \"%s\".", line, entry->acnum);
            entry->drop = true;
        }
        return;
    }
    entry->vernum = atoi(q + 1);
    *q            = ch;

    if (entry->vernum < 1) {
        *p = ch1;
        ErrPostEx(SEV_FATAL, ERR_VERSION_InvalidVersion, "Version number \"%d\" from Accession.Version value \"%s.%d\" is not a positive integer.", entry->vernum, entry->acnum, entry->vernum);
        entry->drop = true;
        return;
    }

    if (ch1 != '\0')
        for (*p++ = ch1; *p == ' ' || *p == '\t';)
            p++;

    if (source == Parser::ESource::DDBJ) {
        if (*p != '\0' && ! ign_toks) {
            ErrPostEx(SEV_ERROR, ERR_VERSION_BadVersionLine, "DDBJ's VERSION line has too many tokens: \"%s\".", line);
        }
        return;
    }

    if (! gi)
        return;

    if (! StringEquN(p, "GI:", 3)) {
        ErrPostEx(SEV_FATAL, ERR_VERSION_IncorrectGIInVersion, "Incorrect GI entry in VERSION line: \"%s\".", line);
        entry->drop = true;
        return;
    }
    p += 3;
    for (q = p; *q >= '0' && *q <= '9';)
        q++;
    if (*q != '\0') {
        ErrPostEx(SEV_FATAL, ERR_VERSION_NonDigitGI, "Incorrect GI number in VERSION line: \"%s\".", line);
        entry->drop = true;
    }
}

/**********************************************************/
static bool fta_check_mga_line(char* line, IndexblkPtr ibp)
{
    char* p;
    char* q;
    char* str;
    Int4  from;
    Int4  to;

    if (! line || ! ibp)
        return false;

    for (p = line; *p == ' ' || *p == '\t';)
        p++;
    str = StringSave(p);
    p   = StringChr(str, '\n');
    if (p)
        *p = '\0';
    p = StringChr(str, '-');
    if (! p) {
        MemFree(str);
        return false;
    }
    *p++ = '\0';

    if (StringLen(str) != 12 || StringLen(p) != 12 ||
        ! StringEquN(str, ibp->acnum, 5) ||
        ! StringEquN(p, ibp->acnum, 5)) {
        MemFree(str);
        return false;
    }

    for (q = str + 5; *q >= '0' && *q <= '9';)
        q++;
    if (*q != '\0') {
        MemFree(str);
        return false;
    }
    for (q = p + 5; *q >= '0' && *q <= '9';)
        q++;
    if (*q != '\0') {
        MemFree(str);
        return false;
    }

    for (q = str + 5; *q == '0';)
        q++;
    from = atoi(q);
    for (q = p + 5; *q == '0';)
        q++;
    to = atoi(q);

    if (from > to) {
        MemFree(str);
        return false;
    }

    ibp->bases = to - from + 1;
    MemFree(str);
    return true;
}


/**********************************************************/
bool GenBankIndex(ParserPtr pp)
{
    FinfoBlk finfo;

    bool acwflag;
    bool end_of_file;
    bool after_LOCUS;
    bool after_DEFNTN;
    bool after_SOURCE;
    bool after_REFER;
    bool after_FEAT;
    bool after_ORIGIN;
    bool after_COMMENT;
    bool after_VERSION;
    bool after_MGA;

    IndexblkPtr   entry;
    int           currentKeyword;
    Int4          indx = 0;
    IndBlkNextPtr ibnp;
    IndBlkNextPtr tibnp;
    char*         p;
    char*         q;
    char*         line_ver;
    char*         line_nid;
    char*         line_locus;
    size_t        i;
    ValNodePtr    kwds;
    ValNodePtr    tkwds;
    ValNodePtr    dbl;
    ValNodePtr    tdbl;

    end_of_file = SkipTitleBuf(pp->ffbuf, &finfo, "LOCUS");

    if (end_of_file) {
        MsgSkipTitleFail("GenBank", &finfo);
        return false;
    }

    bool tpa_check = (pp->source == Parser::ESource::EMBL);

    ibnp  = new IndBlkNode(nullptr);
    tibnp = ibnp;

    pp->num_drop = 0;
    kwds         = nullptr;
    dbl          = nullptr;
    while (! end_of_file) {
        entry = InitialEntry(pp, &finfo);
        if (entry) {
            pp->curindx = indx;
            tibnp->next = new IndBlkNode(entry);
            tibnp       = tibnp->next;

            indx++;

            entry->is_contig = false;
            entry->origin    = false;
            entry->is_mga    = false;
            acwflag          = false;
            after_LOCUS      = false;
            after_DEFNTN     = false;
            after_SOURCE     = false;
            after_REFER      = false;
            after_FEAT       = false;
            after_ORIGIN     = false;
            after_COMMENT    = false;
            after_VERSION    = false;
            after_MGA        = false;

            currentKeyword = ParFlat_LOCUS;
            line_ver       = nullptr;
            line_nid       = nullptr;
            line_locus     = nullptr;
            if (kwds)
                kwds = ValNodeFreeData(kwds);
            tkwds           = nullptr;
            size_t kwds_len = 0;
            if (dbl)
                dbl = ValNodeFreeData(dbl);
            tdbl           = nullptr;
            size_t dbl_len = 0;
            while (currentKeyword != ParFlat_END && ! end_of_file) {
                switch (currentKeyword) {
                case ParFlat_LOCUS:
                    if (after_LOCUS) {
                        ErrPostEx(SEV_ERROR, ERR_FORMAT_LineTypeOrder, "More than two lines LOCUS in one entry");
                        entry->drop = true;
                    } else {
                        after_LOCUS = true;
                        line_locus  = StringSave(finfo.str);
                    }
                    break;
                case ParFlat_COMMENT:
                    if (after_COMMENT) {
                        ErrPostEx(SEV_ERROR, ERR_FORMAT_LineTypeOrder, "Multiple COMMENT lines in one entry");
                        entry->drop = true;
                    } else
                        after_COMMENT = true;

                    break;
                case ParFlat_VERSION:
                    p = StringStr(finfo.str + ParFlat_COL_DATA, "GI:");
                    if (p && atol(p + 3) > 0)
                        entry->wgs_and_gi |= 01;
                    if (pp->accver == false)
                        break;
                    if (after_VERSION) {
                        ErrPostEx(SEV_ERROR, ERR_FORMAT_LineTypeOrder, "Multiple VERSION lines in one entry");
                        entry->drop = true;
                        break;
                    }
                    after_VERSION = true;
                    p             = finfo.str + ParFlat_COL_DATA;
                    while (*p == ' ' || *p == '\t')
                        p++;
                    for (q = p; *q != '\0' && *q != '\r' && *q != '\n';)
                        q++;
                    while (q > p) {
                        q--;
                        if (*q != ' ' && *q != '\t') {
                            q++;
                            break;
                        }
                    }
                    i        = q - p;
                    line_ver = MemNew(i + 1);
                    StringNCpy(line_ver, p, i);
                    line_ver[i] = '\0';
                    break;
                case ParFlat_NCBI_GI:
                    if (pp->source == Parser::ESource::DDBJ || pp->accver == false ||
                        line_nid)
                        break;
                    p = finfo.str + ParFlat_COL_DATA;
                    while (*p == ' ' || *p == '\t')
                        p++;
                    for (q = p; *q != '\0' && *q != ' ' && *q != '\t' &&
                                *q != '\r' && *q != '\n';)
                        q++;
                    i        = q - p;
                    line_nid = MemNew(i + 1);
                    StringNCpy(line_nid, p, i);
                    line_nid[i] = '\0';
                    break;
                case ParFlat_DEFINITION:
                    if (after_DEFNTN) {
                        ErrPostEx(SEV_ERROR, ERR_FORMAT_LineTypeOrder, "More than two lines 'DEFINITION'");
                        entry->drop = true;
                    } else if (after_LOCUS == false) {
                        ErrPostEx(SEV_ERROR, ERR_FORMAT_LineTypeOrder, "DEFINITION field out of order");
                        entry->drop = true;
                    } else
                        after_DEFNTN = true;

                    break;
                case ParFlat_SOURCE:
                    if (after_SOURCE) {
                        ErrPostEx(SEV_ERROR, ERR_FORMAT_LineTypeOrder, "More than two lines 'SOURCE'");
                        entry->drop = true;
                    } else if (after_LOCUS == false || after_DEFNTN == false) {
                        ErrPostEx(SEV_ERROR, ERR_FORMAT_LineTypeOrder, "SOURCE field out of order");
                        entry->drop = true;
                    } else
                        after_SOURCE = true;

                    break;
                case ParFlat_REFERENCE:
                    after_REFER = true;
                    break;
                case ParFlat_CONTIG:
                    if (entry->is_contig) {
                        ErrPostEx(SEV_ERROR, ERR_FORMAT_LineTypeOrder, "More than one line CONTIG in one entry");
                        entry->drop = true;
                    } else
                        entry->is_contig = true;
                    break;
                case ParFlat_MGA:
                    if (entry->is_mga == false) {
                        ErrPostEx(SEV_ERROR, ERR_ENTRY_InvalidLineType, "Line type \"MGA\" is allowed for CAGE records only. Entry dropped.");
                        entry->drop = true;
                    }
                    if (fta_check_mga_line(finfo.str + ParFlat_COL_DATA, entry) == false) {
                        ErrPostEx(SEV_REJECT, ERR_FORMAT_IncorrectMGALine, "Incorrect range of accessions supplied in MGA line of CAGE record. Entry dropped.");
                        entry->drop = true;
                    }
                    after_MGA = true;
                    break;
                case ParFlat_FEATURES:
                    if (after_FEAT) {
                        ErrPostEx(SEV_ERROR, ERR_FORMAT_LineTypeOrder, "More than two lines 'FEATURES'");
                        entry->drop = true;
                    } else if (pp->mode != Parser::EMode::Relaxed &&
                               (after_LOCUS == false ||
                                after_DEFNTN == false ||
                                after_SOURCE == false)) {
                        ErrPostEx(SEV_ERROR, ERR_FORMAT_LineTypeOrder, "FEATURES field out of order");
                        entry->drop = true;
                    } else
                        after_FEAT = true;

                    break;
                case ParFlat_ORIGIN:
                    if (after_ORIGIN) {
                        ErrPostEx(SEV_ERROR, ERR_FORMAT_LineTypeOrder, "More than two lines 'ORIGIN'");
                        entry->drop = true;
                    } else if (
                        pp->mode != Parser::EMode::Relaxed &&
                        (after_LOCUS == false ||
                         after_DEFNTN == false ||
                         after_SOURCE == false ||
                         after_FEAT == false)) {
                        ErrPostEx(SEV_ERROR, ERR_FORMAT_LineTypeOrder, "ORIGIN field out of order");
                        entry->drop = true;
                    } else {
                        after_ORIGIN  = true;
                        entry->origin = true;
                    }
                    break;
                case ParFlat_ACCESSION:
                    if (acwflag == false) /* first accession line */
                    {
                        acwflag = true;
                        if (! GetAccession(pp, finfo.str, entry, 2)) {
                            if (pp->mode != Parser::EMode::Relaxed) {
                                pp->num_drop++;
                            }
                        }
                    }
                    break;
                case ParFlat_SEGMENT:
                    // LCOV_EXCL_START
                    // Excluded per Mark's request on 12/14/2016
                    GetSegment(finfo.str, entry);
                    // LCOV_EXCL_STOP
                    break;
                case ParFlat_USER:
                    if (pp->source != Parser::ESource::Flybase) {
                        ErrPostEx(SEV_ERROR, ERR_ENTRY_InvalidLineType, "Line type \"USER\" is allowed for source \"FLYBASE\" only. Entry dropped.");
                        entry->drop = true;
                    }
                    break;
                case ParFlat_PRIMARY:
                    if (entry->is_tpa == false &&
                        entry->tsa_allowed == false &&
                        pp->source != Parser::ESource::Refseq) {
                        ErrPostEx(SEV_ERROR, ERR_ENTRY_InvalidLineType, "Line type \"PRIMARY\" is allowed for TPA or TSA records only. Continue anyway.");
                    }
                    break;
                case ParFlat_KEYWORDS:
                    if (pp->source != Parser::ESource::DDBJ &&
                        pp->source != Parser::ESource::EMBL)
                        break;
                    if (kwds)
                        ValNodeFreeData(kwds);
                    kwds     = ConstructValNode(objects::CSeq_id::e_not_set, finfo.str + 8);
                    tkwds    = kwds;
                    kwds_len = StringLen(finfo.str) - 8;
                    break;
                case ParFlat_DBLINK:
                    if (dbl)
                        ValNodeFreeData(dbl);
                    dbl     = ConstructValNode(objects::CSeq_id::e_not_set, finfo.str + 8);
                    tdbl    = dbl;
                    dbl_len = StringLen(finfo.str) - 8;
                    break;
                default:
                    break;
                } /* switch */

                end_of_file = XReadFileBuf(pp->ffbuf, &finfo);

                while (! end_of_file && (finfo.str[0] == ' ' || finfo.str[0] == '\t')) {
                    if (currentKeyword == ParFlat_KEYWORDS && tkwds) {
                        tkwds->next = ValNodeNew(nullptr);
                        tkwds       = tkwds->next;
                        tkwds->data = StringSave(finfo.str);
                        kwds_len += StringLen(finfo.str);
                    }

                    if (currentKeyword == ParFlat_DBLINK && tdbl) {
                        tdbl->next = ValNodeNew(nullptr);
                        tdbl       = tdbl->next;
                        tdbl->data = StringSave(finfo.str);
                        dbl_len += StringLen(finfo.str);
                    }

                    if (currentKeyword == ParFlat_ACCESSION && ! entry->drop &&
                        GetAccession(pp, finfo.str, entry, 0) == false)
                        pp->num_drop++;

                    end_of_file = XReadFileBuf(pp->ffbuf, &finfo);
                }


                if (kwds) {
                    check_est_sts_gss_tpa_kwds(kwds, kwds_len, entry, tpa_check, entry->specialist_db, entry->inferential, entry->experimental, entry->assembly);
                    kwds     = ValNodeFreeData(kwds);
                    kwds_len = 0;
                }

                if (pp->mode == Parser::EMode::Relaxed &&
                    NStr::IsBlank(finfo.str)) {
                    currentKeyword = ParFlat_UNKW;
                    continue;
                }

                currentKeyword = SrchKeyword(finfo.str, genbankKeywords);

                if (finfo.str[0] != ' ' && finfo.str[0] != '\t' &&
                    CheckLineType(finfo.str, finfo.line, genbankKeywords, after_ORIGIN) == false)
                    entry->drop = true;

            } /* while, end of one entry */

            entry->is_tpa_wgs_con = (entry->is_contig && entry->is_wgs && entry->is_tpa);

            if (! entry->drop) {

                if (pp->mode != Parser::EMode::Relaxed) {
                    if (line_locus &&
                        CkLocusLinePos(line_locus, pp->source, &entry->lc, entry->is_mga) == false)
                        entry->drop = true;

                    if (entry->is_mga && after_MGA == false)
                        entry->drop = gb_err_field("MGA");

                    if (after_LOCUS == false)
                        entry->drop = gb_err_field("LOCUS");

                    if (after_VERSION == false && pp->accver)
                        entry->drop = gb_err_field("VERSION");

                    if (after_DEFNTN == false)
                        entry->drop = gb_err_field("DEFINITION");

                    if (after_SOURCE == false)
                        entry->drop = gb_err_field("SOURCE");

                    if (after_REFER == false && pp->source != Parser::ESource::Flybase &&
                        entry->is_wgs == false &&
                        (pp->source != Parser::ESource::Refseq ||
                         ! StringEquN(entry->acnum, "NW_", 3))) {
                        entry->drop = gb_err_field("REFERENCE");
                    }

                    if (after_FEAT == false) {
                        entry->drop = gb_err_field("FEATURES");
                    }
                } // !Parser::EMode::Relaxed

                if (entry->is_contig && entry->segnum != 0) {
                    ErrPostEx(SEV_ERROR, ERR_FORMAT_ContigInSegset, "CONTIG data are not allowed for members of segmented sets, entry dropped.");
                    entry->drop = true;
                }
            }
            if (pp->accver) {
                if (pp->mode == Parser::EMode::HTGSCON)
                    entry->vernum = 1;
                else
                    ParseGenBankVersion(
                        entry,
                        line_ver,
                        line_nid,
                        pp->source,
                        pp->mode,
                        pp->ign_toks);
            }
            if (line_locus) {
                MemFree(line_locus);
                line_locus = nullptr;
            }
            if (line_ver) {
                MemFree(line_ver);
                line_ver = nullptr;
            }
            if (line_nid) {
                MemFree(line_nid);
                line_nid = nullptr;
            }
            entry->len = (size_t)(pp->ffbuf.current - pp->ffbuf.start) - entry->offset;

            if (acwflag == false &&
                pp->mode != Parser::EMode::Relaxed) {
                ErrPostEx(SEV_ERROR, ERR_ACCESSION_NoAccessNum, "No accession # for this entry, about line %ld", (long int)entry->linenum);
            }

            if (dbl) {
                dbl = ValNodeFreeData(dbl);
                // dbl_len = 0;
            }
        } /* if, entry */
        else {
            end_of_file = FindNextEntryBuf(end_of_file, pp->ffbuf, &finfo, "//");
        }

        end_of_file = FindNextEntryBuf(end_of_file, pp->ffbuf, &finfo, "LOCUS");

    } /* while, end_of_file */

    pp->indx = indx;

    FtaDeletePrefix(PREFIX_LOCUS | PREFIX_ACCESSION);

    if (pp->qsfd && QSIndex(pp, ibnp->next) == false)
        return false;

    pp->entrylist.resize(indx);
    tibnp = ibnp->next;
    delete ibnp;
    for (int j = 0; j < indx && tibnp; j++, tibnp = ibnp) {
        pp->entrylist[j] = tibnp->ibp;
        ibnp             = tibnp->next;
        delete tibnp;
    }

    return (end_of_file);
}

END_NCBI_SCOPE
