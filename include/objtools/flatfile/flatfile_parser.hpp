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
 * File Name: flatfile_parser.hpp
 *
 * Author: Alexey Dobronadezhdin
 *
 * File Description:
 *
 */

#ifndef __FLATFILE_PARSER_HPP__
#define __FLATFILE_PARSER_HPP__

#include <objtools/flatfile/flatfile_parse_info.hpp>

#define ParFlat_EMBL_AC  "AFVXYZ" /* patent is "A" */
#define ParFlat_LANL_AC  "JKLM"
#define ParFlat_SPROT_AC NULL
#define ParFlat_DDBJ_AC  "CDE"
#define ParFlat_NCBI_AC  "BGHIJKLMRSTUWN" /* backbone = "S" */

BEGIN_NCBI_SCOPE

class CSerialObject;
class CObjectOStream;
namespace objects
{
    class IObjtoolsListener;
}

struct Parser;

NCBI_DEPRECATED Int2 fta_main(Parser* pp, bool already);


class CFlatFileParser
{
public:
    CFlatFileParser(objects::IObjtoolsListener* pMessageListener);
    virtual ~CFlatFileParser(void);
    CRef<CSerialObject> Parse(Parser& parseInfo);
    CRef<CSerialObject> Parse(Parser& parseInfo, CNcbiIstream& istr);
    CRef<CSerialObject> Parse(Parser& parseInfo, const string& filename);
    bool Parse(Parser& parseInfo, CObjectOStream& objOstr);

private:
    objects::IObjtoolsListener* m_pMessageListener = nullptr;
};


END_NCBI_SCOPE

#endif // __FLATFILE_PARSER_HPP__
