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
 * Author:  Michael Kornbluh
 *
 * File Description:
 *   Classes for listening to errors, progress, etc.
 *
 */

#include <ncbi_pch.hpp>

#include <objtools/readers/message_listener.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects) // namespace ncbi::objects::

void
CMessageListenerBase::PutProgress(
    const string & sMessage,
    const Uint8 iNumDone,
    const Uint8 iNumTotal)
{
    if( ! m_pProgressOstrm ) {
        // no stream to write to
        return;
    }
    // NB: Some other classes rely on the message fitting in one line.

    *m_pProgressOstrm << "<message severity=\"INFO\" ";

    if( iNumDone > 0 ) {
        *m_pProgressOstrm << "num_done=\"" << iNumDone << "\" ";
    }

    if( iNumTotal > 0 ) {
        *m_pProgressOstrm << "num_total=\"" << iNumTotal << "\" ";
    }

    if( sMessage.empty() ) {
        *m_pProgressOstrm  << " />";
    } else {
        *m_pProgressOstrm  << " >";
        *m_pProgressOstrm << NStr::XmlEncode(sMessage);
        *m_pProgressOstrm << "</message>" << NcbiEndl;
    }
}

END_SCOPE(objects)
END_NCBI_SCOPE

