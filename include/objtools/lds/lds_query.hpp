#ifndef LDS_QUERY_HPP__
#define LDS_QUERY_HPP__
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
 * Author: Anatoliy Kuznetsov
 *
 * File Description: Different query functions to LDS database.
 *
 */

#include <objtools/lds/lds_db.hpp>
#include <objtools/lds/lds_set.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

//////////////////////////////////////////////////////////////////
//
// CLDS_Query different queries to the LDS database.
//

class NCBI_LDS_EXPORT CLDS_Query
{
public:
    CLDS_Query(SLDS_TablesCollection& lds_tables)
    : m_db(lds_tables)
    {}

    // Scan the database, find the file, return TRUE if file exists.
    // Linear scan, no idx optimization.
    bool FindFile(const string& path);

private:
    SLDS_TablesCollection& m_db;
};

END_SCOPE(objects)
END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2003/06/16 14:54:08  kuznets
 * lds splitted into "lds" and "lds_admin"
 *
 * ===========================================================================
 */

#endif
