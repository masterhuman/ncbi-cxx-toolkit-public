#ifndef SAMPLE__NCBI_SAMPLE_API__HPP
#define SAMPLE__NCBI_SAMPLE_API__HPP

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
 * Authors:  Denis Vakatov
 *
 */

/// @file ncbi_sample_api.hpp
/// CNcbiSample -- base class for the NCBI samples, to be used by
/// the NEW_PROJECT-class utilities.

#include <corelib/ncbistl.hpp>


/** @addtogroup SampleAPI
 *
 * @{
 */

/// CNcbiSample -- base class for the NCBI samples, to be used
/// by the NEW_PROJECT-class utilities.

class CNcbiSample
{
public:
    CNcbiSample() {}
    virtual ~CNcbiSample() {}

    virtual std::string Description(void) = 0;

    virtual void Init(void) = 0;
    virtual int  Run (void) = 0;
    virtual void Exit(void) = 0;
};


/* @} */

#endif // SAMPLE__NCBI_SAMPLE_API__HPP
