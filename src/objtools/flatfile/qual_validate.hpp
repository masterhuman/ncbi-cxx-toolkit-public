/*
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
* File Name: qual_validate.hpp
*
* Author: Frank Ludwig
*
* File Description:
* -----------------
*
*/

#ifndef FLATFILE__QUAL_VALIDATE__HPP
#define FLATFILE__QUAL_VALIDATE__HPP

BEGIN_NCBI_SCOPE

//  ----------------------------------------------------------------------------
class CQualCleanup
//  ----------------------------------------------------------------------------
{
public:
    static bool CleanAndValidate(
        const string& featKey,
        string& qualKey,
        string& qualVal);

private:
    static bool xCleanAndValidateGeneric(
        const string& featKey,
        string& qualKey,
        string& qualVal);

    static bool xCleanAndValidateSpecificHost(
        const string& featKey,
        string& qualKey,
        string& qualVal);
    static bool xCleanAndValidateTranslation(
        const string& featKey,
        string& qualKey,
        string& qualVal);

    static bool xCleanStripBlanks(
        const string& featKey,
        string& qualKey,
        string& qualVal);
};

END_NCBI_SCOPE

#endif //FLATFILE__QUAL_VALIDATE__HPP
