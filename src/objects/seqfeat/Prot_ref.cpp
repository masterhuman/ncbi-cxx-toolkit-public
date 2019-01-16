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
 */

// standard includes

// generated includes
#include <ncbi_pch.hpp>
#include <objects/seqfeat/Prot_ref.hpp>

// generated classes

#include <corelib/ncbimtx.hpp>
#include <util/line_reader.hpp>
#include <util/util_misc.hpp>
#include <objects/misc/error_codes.hpp>

#define NCBI_USE_ERRCODE_X  Objects_ProtRef

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

// destructor
CProt_ref::~CProt_ref(void)
{
}

// Appends a label to "label" based on content
void CProt_ref::GetLabel(string* label) const
{
    if (IsSetName() && !GetName().empty()) {
        *label += *GetName().begin();
    } else if (IsSetDesc()) {
        *label += GetDesc();
    } else if (IsSetDb()) {
        GetDb().front()->GetLabel(label);
    }
}


typedef map<string, CProt_ref::EECNumberStatus, PNocase> TECNumberStatusMap;
typedef map<string, string> TECNumberReplacementMap;

static TECNumberStatusMap      s_ECNumberStatusMap;
static TECNumberReplacementMap s_ECNumberReplacementMap;
static bool                    s_ECNumberMapsInitialized = false;
static CProt_ref::EECNumberFileStatus     s_ECNumAmbiguousStatus = CProt_ref::eECFile_not_attempted;
static CProt_ref::EECNumberFileStatus     s_ECNumDeletedStatus = CProt_ref::eECFile_not_attempted;
static CProt_ref::EECNumberFileStatus     s_ECNumReplacedStatus = CProt_ref::eECFile_not_attempted;
static CProt_ref::EECNumberFileStatus     s_ECNumSpecificStatus = CProt_ref::eECFile_not_attempted;

CProt_ref::EECNumberFileStatus CProt_ref::GetECNumAmbiguousStatus() { return s_ECNumAmbiguousStatus; }
CProt_ref::EECNumberFileStatus CProt_ref::GetECNumDeletedStatus() { return s_ECNumDeletedStatus; }
CProt_ref::EECNumberFileStatus CProt_ref::GetECNumReplacedStatus() { return s_ECNumReplacedStatus; }
CProt_ref::EECNumberFileStatus CProt_ref::GetECNumSpecificStatus() { return s_ECNumSpecificStatus; }

DEFINE_STATIC_FAST_MUTEX(s_ECNumberMutex);

#include "ecnum_ambiguous.inc"
#include "ecnum_deleted.inc"
#include "ecnum_replaced.inc"
#include "ecnum_specific.inc"

static void s_ProcessECNumberLine(const CTempString& line,
                                  CProt_ref::EECNumberStatus status)
{
    if (status == CProt_ref::eEC_replaced) {
        SIZE_TYPE tab_pos = line.find('\t');
        if (tab_pos == NPOS) {
            ERR_POST_X(1, Warning << "No tab in ecnum_replaced entry " << line
                       << "; disregarding");
        } else {
            string lhs(line.substr(0, tab_pos)), rhs(line.substr(tab_pos + 1));
            s_ECNumberStatusMap[lhs]      = status;
            s_ECNumberReplacementMap[lhs] = rhs;
        }
    } else {
        SIZE_TYPE tab_pos = line.find('\t');
        if (tab_pos == NPOS) {
            s_ECNumberStatusMap[line] = status;
        } else {
            string lhs(line.substr(0, tab_pos));
            s_ECNumberStatusMap[lhs] = status;
        }
    }
}


static CProt_ref::EECNumberFileStatus s_LoadECNumberTable(const string& dir, const string& name,
                                const char* const *fallback,
                                size_t fallback_count,
                                CProt_ref::EECNumberStatus status)
{
    CRef<ILineReader> lr;
    CProt_ref::EECNumberFileStatus rval = CProt_ref::eECFile_not_attempted;
    string file = kEmptyStr;
    if ( !dir.empty() ) {
        file = CDirEntry::MakePath(dir, "ecnum_" + name, "txt");
        lr.Reset(ILineReader::New
                 (CDirEntry::MakePath(dir, "ecnum_" + name, "txt")));
        rval = CProt_ref::eECFile_not_found;
    }
    if (lr.Empty()) {
        if (getenv("NCBI_DEBUG")) {
            LOG_POST("Reading " + name + " EC number data from built-in table");
        }
        while (fallback_count--) {
            s_ProcessECNumberLine(*fallback++, status);
        }
    } else {
        if (getenv("NCBI_DEBUG")) {
            LOG_POST("Reading " + name + " EC number data from " + file);
        }
        rval = CProt_ref::eECFile_not_read;
        do {
            s_ProcessECNumberLine(*++*lr, status);
            rval = CProt_ref::eECFile_read;
        } while ( !lr->AtEOF() );
    }
    return rval;
}


static void s_InitializeECNumberMaps(void)
{
    CFastMutexGuard GUARD(s_ECNumberMutex);
    if (s_ECNumberMapsInitialized) {
        return;
    }
    string dir;
    const char* env_val = NULL;
    env_val = getenv("NCBI_ECNUM_USE_DATA_DIR_FIRST");
    if (env_val != NULL && NStr::EqualNocase(env_val, "TRUE"))
    {
        string file = g_FindDataFile("ecnum_specific.txt");
        if ( !file.empty() ) {
            dir = CDirEntry::AddTrailingPathSeparator(CDirEntry(file).GetDir());
        }
        if (dir.empty()) {
            LOG_POST("s_InitializeECNumberMaps: reading specific EC Numbers from built-in data.");
        } else {
            LOG_POST("s_InitializeECNumberMaps: reading specific EC Numbers from " + file);
        }
    }
#define LOAD_EC(x) s_LoadECNumberTable \
    (dir, #x, kECNum_##x, sizeof(kECNum_##x) / sizeof(*kECNum_##x), \
     CProt_ref::eEC_##x)
    s_ECNumSpecificStatus = LOAD_EC(specific);
    s_ECNumAmbiguousStatus = LOAD_EC(ambiguous);
    s_ECNumReplacedStatus = LOAD_EC(replaced);
    s_ECNumDeletedStatus = LOAD_EC(deleted);
#undef LOAD_EC
    s_ECNumberMapsInitialized = true;
}


CProt_ref::EECNumberStatus CProt_ref::GetECNumberStatus(const string& ecno)
{
    if ( !s_ECNumberMapsInitialized ) {
        s_InitializeECNumberMaps();
    }
    TECNumberStatusMap::const_iterator it = s_ECNumberStatusMap.find(ecno);
    if (it == s_ECNumberStatusMap.end()) {
        return eEC_unknown;
    } else {
        return it->second;
    }
}


const string& CProt_ref::GetECNumberReplacement(const string& old_ecno)
{
    if ( !s_ECNumberMapsInitialized ) {
        s_InitializeECNumberMaps();
    }
    TECNumberReplacementMap::const_iterator it
        = s_ECNumberReplacementMap.find(old_ecno);
    if (it == s_ECNumberReplacementMap.end()) {
        NCBI_THROW(CCoreException, eInvalidArg,
                   "No replacement defined for EC number " + old_ecno);
        // alternatively, could return old_ecno or kEmptyStr
    } else {
        // see if this number has also been replaced
        auto other_it = s_ECNumberReplacementMap.find(it->second);
        while (other_it != s_ECNumberReplacementMap.end()) {
            it = other_it;
            other_it = s_ECNumberReplacementMap.find(it->second);
        }
        return it->second;
    }
}


bool CProt_ref::IsECNumberSplit(const string& old_ecno)
{
    if (GetECNumberStatus(old_ecno) != eEC_replaced) {
        return false;
    }
   
    const string& replacement = GetECNumberReplacement(old_ecno);
    if (NStr::Find(replacement, "\t") != string::npos) {
        return true;
    } else {
        return false;
    }
}


// From the INSDC Feature Table Documentation:
// Valid values for EC numbers are defined in the list prepared by the
// Nomenclature Committee of the International Union of Biochemistry and
// Molecular Biology(NC - IUBMB) (published in Enzyme Nomenclature 1992,
// Academic Press, San Diego, or a more recent revision thereof).
// The format represents a string of four numbers separated by full
// stops; up to three numbers starting from the end of the string can
// be replaced by dash "." to indicate uncertain assignment.
// Symbol "n" can be used in the last position instead of a number
// where the EC number is awaiting assignment.Please note that such
// incomplete EC numbers are not approved by NC - IUBMB.
// 
// Examples:
//     1.1.2.4
//     1.1.2.-
//     1.1.2.n
bool CProt_ref::IsValidECNumberFormat (const string&  ecno)
{
    char     ch;
    bool     is_ambig;
    int      numdashes;
    int      numdigits;
    int      numperiods;
    const char *ptr;

    if (NStr::IsBlank(ecno)) {
        return false;
    }

    is_ambig = false;
    numperiods = 0;
    numdigits = 0;
    numdashes = 0;

    ptr = ecno.c_str();
    ch = *ptr;
    while (ch != '\0') {
        if (isdigit(ch)) {
            numdigits++;
            if (is_ambig) return false;
            ptr++;
            ch = *ptr;
        } else if (ch == '-') {
            numdashes++;
            is_ambig = true;
            ptr++;
            ch = *ptr;
        } else if (ch == 'n') {
            if (numperiods == 3 && numdigits == 0 && isdigit(*(ptr + 1))) {
                // allow/ignore n in first position of fourth number to not mean ambiguous, if followed by digit */
            } else {
                numdashes++;
                is_ambig = true;
            }
            ptr++;
            ch = *ptr;
        } else if (ch == '.') {
            numperiods++;
            if (numdigits > 0 && numdashes > 0) return false;
            if (numdigits == 0 && numdashes == 0) return false;
            if (numdashes > 1) return false;
            numdigits = 0;
            numdashes = 0;
            ptr++;
            ch = *ptr;
        } else {
            ptr++;
            ch = *ptr;
        }
    }

    if (numperiods == 3) {
        if (numdigits > 0 && numdashes > 0) return false;
        if (numdigits > 0 || numdashes == 1) return true;
    }

    return false;
}


void CProt_ref::AutoFixEC()
{
    if (!IsSetEc()) {
        return;
    }
    CProt_ref::TEc::iterator it = SetEc().begin();
    while (it != SetEc().end()) {
        if (GetECNumberStatus(*it) == eEC_replaced) {
            string new_val = GetECNumberReplacement(*it);
            if (!NStr::IsBlank(new_val)) {
                *it = new_val;
            }
        }
        it++;
    }

}


void CProt_ref::RemoveBadEC()
{
    AutoFixEC();
    if (!IsSetEc()) {
        return;
    }
    CProt_ref::TEc::iterator it = SetEc().begin();
    while (it != SetEc().end()) {
        EECNumberStatus status = GetECNumberStatus(*it);
        if (status == eEC_deleted ||
            status == eEC_unknown ||
            status == eEC_replaced) {
            it = SetEc().erase(it);
        } else {
            it++;
        }
    }
    if (SetEc().empty()) {
        ResetEc();
    }
}


END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE

/* Original file checksum: lines: 61, chars: 1885, CRC32: 4ba9347a */
