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
 */

/// @remap_client.hpp
/// User-defined methods of the data storage class.
///
/// This file was originally generated by application DATATOOL
/// using the following specifications:
/// 'remap.asn'.
///
/// New methods or data members can be added to it if needed.
/// See also: remap_client_.hpp


#ifndef OBJECTS_REMAP_REMAP_CLIENT_HPP
#define OBJECTS_REMAP_REMAP_CLIENT_HPP


// generated includes
#include <objects/remap/remap_client_.hpp>

// additional includes
#include <objects/seqloc/Seq_loc.hpp>

// generated classes

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

/////////////////////////////////////////////////////////////////////////////
class NCBI_REMAP_EXPORT CRemapClient : public CRemapClient_Base
{
    typedef CRemapClient_Base Tparent;
public:
    // constructor
    CRemapClient(void);
    // destructor
    ~CRemapClient(void);

    // Convenience methods for querying

    /// Remap a single Seq-loc
    CRef<objects::CSeq_loc> Remap(const objects::CSeq_loc& loc,
                                  const string& from_build,
                                  const string& to_build);
    /// Remap multiple Seq-locs
    void                    Remap(const vector<CRef<objects::CSeq_loc> >& locs,
                                  const string& from_build,
                                  const string& to_build,
                                  vector<CRef<objects::CSeq_loc> >& result);

protected:
    // we override this to use a server that's not a named service
    void x_Connect();

private:
    // Prohibit copy constructor and assignment operator
    CRemapClient(const CRemapClient& value);
    CRemapClient& operator=(const CRemapClient& value);

};

/////////////////// CRemapClient inline methods

// constructor
inline
CRemapClient::CRemapClient(void)
{
    SetDefaultRequest().SetVersion(0);
}


/////////////////// end of CRemapClient inline methods


END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
* Revision 1.3  2004/07/29 19:46:59  jcherry
* Added convenience methods for remapping
*
* Revision 1.2  2004/07/29 18:47:40  jcherry
* In constructor, set up default request
*
* Revision 1.1  2004/07/28 15:09:48  jcherry
* Arranged to contact experimental server, which is not a named service
*
*
* ===========================================================================
*/

#endif // OBJECTS_REMAP_REMAP_CLIENT_HPP
/* Original file checksum: lines: 94, chars: 2610, CRC32: 5aa8f76b */
