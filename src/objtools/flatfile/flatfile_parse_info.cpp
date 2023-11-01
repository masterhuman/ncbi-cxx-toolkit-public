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
 * File Name: flatfile_parse_info.cpp
 *
 * Author:
 *
 * File Description:
 *      Main routines for parsing flat files to ASN.1 file format.
 * Available flat file format are GENBANK (LANL), EMBL, SWISS-PROT.
 *
 */

#include <ncbi_pch.hpp>

#include <objtools/flatfile/flatfile_parse_info.hpp>
#include "keyword_parse.hpp"
#include "indx_blk.h"

BEGIN_NCBI_SCOPE

Parser::Parser() :
    mpKeywordParser(nullptr)
{
}

void Parser::InitializeKeywordParser(
    EFormat fmt)
{
    mpKeywordParser = new CKeywordParser(fmt);
}

CKeywordParser&
Parser::KeywordParser()
{
    _ASSERT(mpKeywordParser);
    return *mpKeywordParser;
}

Parser::~Parser()
{
    delete mpKeywordParser;
    ResetParserStruct(this);
}

END_NCBI_SCOPE
