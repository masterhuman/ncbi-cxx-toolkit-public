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
 * Authors:  Sergiy Gotvyanskyy
 *           Anton Lavrentiev (Toolkit adaptation)
 *
 * File Description:
 *   Stream buffer based on a fixed size memory area
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistre.hpp>
#include <util/memory_streambuf.hpp>
#include <cstring>


BEGIN_NCBI_SCOPE


CMemory_Streambuf::CMemory_Streambuf(const char* area, size_t size)
    : m_begin(area), m_end(area + size)
{
#ifdef NCBI_OS_MSWIN
    // see RW-2258 and https://github.com/microsoft/STL/blob/main/stl/inc/streambuf
    // MS implementation of std::streambuf has _Gcount of type int
    if (size > bmax)
        size = bmax;
#endif // NCBI_OS_MSWIN

    setg((char*) area, (char*) area, (char*) area + size);
    setp(0, 0);
}


CMemory_Streambuf::CMemory_Streambuf(char* area, size_t size)
    : CMemory_Streambuf((const char*) area, size)
{
    setp(area, egptr());
}


CT_INT_TYPE CMemory_Streambuf::underflow(void)
{
#ifdef NCBI_OS_MSWIN
    auto pos = gptr();

    if (pos >= m_end)
        return CT_EOF;

    streamsize size = m_end - pos;
    if (size > bmax)
        size = bmax;

    setg(pos, pos, pos + size);

    return CT_TO_INT_TYPE(*pos);
#else
    return CT_EOF;
#endif // NCBI_OS_MSWIN
}


streamsize CMemory_Streambuf::xsgetn(CT_CHAR_TYPE* buf, streamsize n)
{
#ifdef NCBI_OS_MSWIN
    auto pos = gptr();

    if (pos >= m_end)
        return 0;

    streamsize size = m_end - pos;
    if (n > size)
        n = size;

    memcpy(buf, pos, n);
    size -= n;
    pos  += n;

    if (size > bmax)
        size = bmax;

    setg(pos, pos, pos + size);

    return n;
#else
    return 0;
#endif // NCBI_OS_MSWIN
}


CT_INT_TYPE CMemory_Streambuf::overflow(CT_INT_TYPE c)
{
    if (CT_EQ_INT_TYPE(c, CT_EOF))
        return pbase() ? CT_NOT_EOF(CT_EOF) : CT_EOF;

#ifdef NCBI_OS_MSWIN
    auto pos = pptr();

    if (!pos  ||  pos >= m_end)
        return CT_EOF;

    *pos++ = CT_TO_CHAR_TYPE(c);

    streamsize size = m_end - pos;
    if (size > bmax)
        size = bmax;

    setp(pos, pos + size);

    return c;
#else
    return CT_EOF;
#endif // NCBI_OS_MSWIN
}


streamsize CMemory_Streambuf::xsputn(const CT_CHAR_TYPE* buf, streamsize n)
{
    auto pos = pptr();
    if (!pos  ||  pos >= m_end)
        return 0;

    streamsize m = m_end - pos;
    if (m > n)
        m = n;
    memcpy(pos, buf, m);
    pos += m;

    streamsize size = m_end - pos;
#ifdef NCBI_OS_MSWIN
    if (size > bmax)
        size = bmax;
#endif // NCBI_OS_MSWIN
    setp(pos, pos + size);

    return m;
}


CT_POS_TYPE CMemory_Streambuf::seekoff(CT_OFF_TYPE off, IOS_BASE::seekdir whence, IOS_BASE::openmode which)
{
    static const CT_POS_TYPE err = (CT_POS_TYPE)((CT_OFF_TYPE)(-1L));
    char* pos;

    if (!(which & (IOS_BASE::in | IOS_BASE::out)))
        return err;
    if ((which & IOS_BASE::out)  &&  !pbase())
        return err;
    switch (whence) {
    case IOS_BASE::beg:
        pos = (char*) m_begin;
        break;
    case IOS_BASE::cur:
        if ((which & (IOS_BASE::in | IOS_BASE::out)) == (IOS_BASE::in | IOS_BASE::out))
            pos = gptr() == pptr() ? gptr() : 0;
        else
            pos = which & IOS_BASE::in ? gptr() : pptr();
        break;
    case IOS_BASE::end:
        pos = (char*) m_end;
        break;
    default:
        return err;
    }
    if (!pos)
        return err;

    if (off  ||  whence != IOS_BASE::cur) {
        pos += off;
        if (pos < m_begin  ||  pos > m_end)
            return err;

        streamsize size = pos - m_end;
#ifdef NCBI_OS_MSWIN
        if (size > bmax)
            size = bmax;
#endif // NCBI_OS_MSWIN

        if (which & IOS_BASE::in)
            setg(pos, pos, pos + size);

        if (which & IOS_BASE::out)
            setp(pos, pos + size);
    }

    off = (CT_OFF_TYPE)(pos - m_begin);
    return (CT_POS_TYPE) off;
}


END_NCBI_SCOPE
