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
 * Author:  Jim Ostell
 *
 * File Description:
 *   .......
 *
 * Remark:
 *   This code was originally generated by application DATATOOL
 *   using specifications from the ASN data definition file
 *   'seqloc.asn'.
 *
 * ---------------------------------------------------------------------------
 * $Log$
 * Revision 6.8  2004/05/19 17:26:25  gorelenk
 * Added include of PCH - ncbi_pch.hpp
 *
 * Revision 6.7  2003/02/06 22:23:29  vasilche
 * Added CSeq_id::Assign(), CSeq_loc::Assign().
 * Added int CSeq_id::Compare() (not safe).
 * Added caching of CSeq_loc::GetTotalRange().
 *
 * Revision 6.6  2001/08/31 20:05:45  ucko
 * Fix ICC build.
 *
 * Revision 6.5  2001/08/31 16:02:43  clausen
 * Added new constructors for Fasta.
 *
 * Revision 6.4  2000/12/15 19:30:32  ostell
 * Used Upcase() in AsFastaString() and changed to PNocase().Equals() style
 *
 * Revision 6.3  2000/12/08 22:19:45  ostell
 * changed MakeFastString to AsFastaString and to use ostream instead of string
 *
 * Revision 6.2  2000/12/08 20:45:14  ostell
 * added MakeFastaString()
 *
 * Revision 6.1  2000/11/30 18:39:27  ostell
 * added Textseq_id.Match
 *
 *
 * ===========================================================================
 */

// standard includes

// generated includes
#include <ncbi_pch.hpp>
#include <objects/seqloc/Textseq_id.hpp>


// generated classes

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE // namespace ncbi::objects::


// destructor
CTextseq_id::~CTextseq_id(void)
{
    return;
}


// Error/throw function for CTextseq_id:: constructors
static void s_InitThrow
(const string&  message,
 const string&  acc,
 const string&  name,
 int            version,
 const string&  release,
 bool           allow_dot_version)
{
    THROW1_TRACE(invalid_argument,
                 "CTextseq_id:: " + message +
                 "\nacc = "       + acc +
                 "\nname = "      + name +
                 "\nversion: "    + NStr::IntToString(version) +
                 "\nrelease = "   + release +
                 "\nallow_dot_version = " +
                 NStr::BoolToString(allow_dot_version));
}


// Constructor helper function

void CTextseq_id::x_Init
(const string&  acc,
 const string&  name,
 int            version,
 const string&  release,
 bool           allow_dot_version)
{
    if ( ! acc.empty() ) {
        string::size_type idx = string::npos;

        if (allow_dot_version) {
            idx = acc.find('.');
        }

        if (idx == string::npos) {
            // no version within acc
            SetAccession (acc.c_str());

            // standalone version ok, here
            if ( version > 0) {
                SetVersion(version);
 
            } else if ( version < 0 ) {
                s_InitThrow("Unexpected negative version.",
                            acc, name, version, release, allow_dot_version);
            }
        }
        else {
            //accession.version
            string accession = acc.substr(0,idx);
            string acc_ver = acc.substr(idx+1);
            int ver = NStr::StringToNumeric(acc_ver);
 
            //If there is a non-zero version and it differs
            //from ver (the accession version), then throw an error
            if( version > 0 && ver != version) {
                s_InitThrow("Incompatible version information supplied.",
                            acc, name, version, release, allow_dot_version);
            }
 
            SetAccession (accession.c_str());
            if ( ver >  0 ) {
                SetVersion(ver);
            } else if( ver < 0 ) {
                s_InitThrow("Unexpected non-numeric version in accession.",
                            acc, name, version, release, allow_dot_version);
            }   
        }
    }

    if (! name.empty()) {
        SetName(name.c_str());
    }

    if ( (! name.empty()) || (! acc.empty()) ) {
        if (! release.empty()){
            SetRelease(release);
        }
    } else {
        s_InitThrow("Name or accession missing.",
                    acc, name, version, release, allow_dot_version);
    }
}


// Alternative constructors start here Karl Sirotkin 4/23/01
CTextseq_id::CTextseq_id
( const string& acc,
  const string& name,
  const string& version,
  const string& release,
  bool    allow_dot_version )
{
    int ver = 0;

    if ( !version.empty() ) {
        ver = NStr::StringToNumeric(version);
        if (ver < 0 ) {
            THROW1_TRACE(invalid_argument,
                         "Unexpected non-numeric version. "
                         "\naccession = " + acc +
                         "\nname = "      + name +
                         "\nversion = "   + version +
                         "\nrelease = "   + release +
                         "\nallow_dot_version = " +
                         NStr::BoolToString(allow_dot_version));
        }
    }

    x_Init(acc, name, ver, release, allow_dot_version);
}


CTextseq_id::CTextseq_id
( const string& acc,
  const string& name,
  int           version,
  const string& release,
  bool    allow_dot_version )
{
    x_Init(acc, name, version, release, allow_dot_version);
}


// comparison function
bool CTextseq_id::Match(const CTextseq_id& tsip2) const
{
    // Check Accessions first
    if (IsSetAccession()  &&  tsip2.IsSetAccession()) {
        if ( PNocase().Equals(GetAccession(), tsip2.GetAccession()) ) {
            if (IsSetVersion()  &&  tsip2.IsSetVersion()) {
                return GetVersion() == tsip2.GetVersion();
            } else {
                return true;
            }
        } else {
            return false;
        }
    }

    // then try name
    if (IsSetName()  &&  tsip2.IsSetName()) {
        if ( PNocase().Equals(GetName(), tsip2.GetName()) ) {
            if (IsSetVersion()  &&  tsip2.IsSetVersion()) {
                return GetVersion() == tsip2.GetVersion();
            }
            else {
                return true;
            }
        } else {
            return false;
        }
    }

    //nothing to compare
    return false;
}


// comparison function
int CTextseq_id::Compare(const CTextseq_id& tsip2) const
{
    // Check Accessions first
    if (IsSetAccession()  &&  tsip2.IsSetAccession()) {
        int ret = PNocase().Compare(GetAccession(), tsip2.GetAccession());
        if ( ret == 0 && IsSetVersion()  &&  tsip2.IsSetVersion() ) {
            ret = GetVersion() - tsip2.GetVersion();
        }
        return ret;
    }

    // then try name
    if (IsSetName()  &&  tsip2.IsSetName()) {
        int ret = PNocase().Compare(GetName(), tsip2.GetName());
        if ( ret == 0 && IsSetVersion()  &&  tsip2.IsSetVersion() ) {
            ret = GetVersion() - tsip2.GetVersion();
        }
        return ret;
    }

    int ret = IsSetAccession() - tsip2.IsSetAccession();
    if ( ret == 0 ) {
        ret = IsSetName() - tsip2.IsSetName();
        if ( ret == 0 ) {
            ret = IsSetVersion() - tsip2.IsSetVersion();
            if ( ret == 0 )
                ret = this == &tsip2? 0: this < &tsip2? -1: 1;
        }
    }
    return ret;
}


// format the contents FASTA string style
ostream& CTextseq_id::AsFastaString(ostream& s) const
{
    if (IsSetAccession()) {
        s << GetAccession(); // no Upcase per Ostell - Karl 7/2001
        if ( IsSetVersion() ) {
            int version = GetVersion();
            if (version) {
                s << '.' << version;
            }
        }
    }

    s << '|';
    if ( IsSetName() ) {
        s << GetName();  // no Upcase per Ostell - Karl 7/2001
    }
    return s;
}


END_objects_SCOPE // namespace ncbi::objects::
END_NCBI_SCOPE
