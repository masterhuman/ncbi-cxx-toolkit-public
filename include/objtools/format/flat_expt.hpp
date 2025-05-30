#ifndef OBJTOOLS_FORMAT___FLAT_EXPT__HPP
#define OBJTOOLS_FORMAT___FLAT_EXPT__HPP

/*  $Id$
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
* Author:  Mati Shomrat
*
* File Description:
*   Flat-File generator exception class.
*
*/
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiexpt.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


class NCBI_FORMAT_EXPORT CFlatException : public CException
{
public:

    enum EErrCode {
        eNotSupported,
        eInternal,
        eInvalidParam,
        eHaltRequested,
        eUnknown
    };

    // Translate the specific error code into a string representations of
    // that error code.
    const char* GetErrCodeString() const override
    {
        switch (GetErrCode()) {
        case eNotSupported:  return "eNotSupported";
        case eInternal:      return "eInternal";
        case eInvalidParam:  return "eInvalidParam";
        case eUnknown:       return "eUnknown";
        case eHaltRequested: return "eHaltRequested";
        default:             return CException::GetErrCodeString();
        }
    }

    NCBI_EXCEPTION_DEFAULT(CFlatException, CException);
};


END_SCOPE(objects)
END_NCBI_SCOPE

#endif  /* OBJTOOLS_FORMAT___FLAT_EXPT__HPP */
