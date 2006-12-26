#ifndef UTIL_COMPRESS__COMPRESS__HPP
#define UTIL_COMPRESS__COMPRESS__HPP

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
 * Author:  Vladimir Ivanov
 *
 * File Description:  The Compression API
 *
 */

#include <corelib/ncbistd.hpp>


/** @addtogroup Compression
 *
 * @{
 */

BEGIN_NCBI_SCOPE


// Default compression I/O stream buffer size
const streamsize kCompressionDefaultBufSize = 16*1024;

// Forward declaration
class CCompressionFile;
class CCompressionStreambuf;


//////////////////////////////////////////////////////////////////////////////
//
// CCompression -- abstract base class
//

class NCBI_XUTIL_EXPORT CCompression
{
public:
    /// Compression level.
    ///
    /// It is in range [0..9]. Increase of level might mean better compression
    /// and usualy greater time of compression. Usualy 1 gives best speed,
    /// 9 gives best compression, 0 gives no compression at all.
    /// eDefault value requests a compromise between speed and compression
    /// (according to developers of the corresponding compression algorithm).
    enum ELevel {
        eLevel_Default       = -1,  // default
        eLevel_NoCompression =  0,  // just store data
        eLevel_Lowest        =  1,
        eLevel_VeryLow       =  2,
        eLevel_Low           =  3,
        eLevel_MediumLow     =  4,
        eLevel_Medium        =  5,
        eLevel_MediumHigh    =  6,
        eLevel_High          =  7,
        eLevel_VeryHigh      =  8,
        eLevel_Best          =  9
    };

    // 'ctors
    CCompression(ELevel level = eLevel_Default);
    virtual ~CCompression(void);

    // Get/set compression level.
    // NOTE 1:  Changing compression level after compression has begun will
    //          be ignored.
    // NOTE 2:  If the level is not supported by the underlying algorithm,
    //          then it will be translated to the nearest supported value.
    void SetLevel(ELevel level);
    virtual ELevel GetLevel(void) const;

    // Return the default compression level for current compression algorithm
    virtual ELevel GetDefaultLevel(void) const = 0;

    // Get compressor's internal status/error code and description
    // for the last operation.
    int    GetErrorCode(void) const;
    string GetErrorDescription(void) const;

    /// Compression flags. The flag selection depends from compression
    /// algorithm implementation.
    typedef unsigned int TFlags;    // Bitwise OR of EFlags*

    // Get/set flags
    TFlags GetFlags(void) const;
    void SetFlags(TFlags flags);

    /// Decompression mode (see fAllowTransparentRead flag).
    enum EDecompressMode {
        eMode_Decompress,      ///< Generic decompression
        eMode_TransparentRead, ///< Transparent read, the data is uncompressed
        eMode_Unknown          ///< Not known yet (decompress/transparent read)
    };


    //
    // Utility functions 
    //

    // (De)compress the source buffer into the destination buffer.
    // Return TRUE on success, FALSE on error.
    // The compressor error code can be acquired via GetErrorCode() call.
    // Notice that altogether the total size of the destination buffer must
    // be little more then size of the source buffer. 

    virtual bool CompressBuffer(
        const void* src_buf, size_t  src_len,
        void*       dst_buf, size_t  dst_size,
        /* out */            size_t* dst_len
    ) = 0;
    virtual bool DecompressBuffer(
        const void* src_buf, size_t  src_len,
        void*       dst_buf, size_t  dst_size,
        /* out */            size_t* dst_len
    ) = 0;

    // (De)compress file "src_file" and put result to file "dst_file".
    // Return TRUE on success, FALSE on error.
    virtual bool CompressFile(
        const string&     src_file,
        const string&     dst_file,
        size_t            buf_size = kCompressionDefaultBufSize
    ) = 0;
    virtual bool DecompressFile(
        const string&     src_file,
        const string&     dst_file, 
        size_t            buf_size = kCompressionDefaultBufSize
    ) = 0;

protected:
    // Universal file compression/decompression functions.
    // Return TRUE on success, FALSE on error.
    virtual bool x_CompressFile(
        const string&     src_file,
        CCompressionFile& dst_file,
        size_t            buf_size = kCompressionDefaultBufSize
    );
    virtual bool x_DecompressFile(
        CCompressionFile& src_file,
        const string&     dst_file,
        size_t            buf_size = kCompressionDefaultBufSize
    );

    // Set last action error/status code and description
    void SetError(int status, const char* description = 0);

protected:
    ///< Decompress mode (Decompress/TransparentRead/Unknown).
    EDecompressMode m_DecompressMode;

private:
    ELevel  m_Level;      // Compression level
    int     m_ErrorCode;  // Last compressor action error/status
    string  m_ErrorMsg;   // Last compressor action error message
    TFlags  m_Flags;      // Bitwise OR of flags

    // Friend classes
    friend class CCompressionStreambuf;
};



//////////////////////////////////////////////////////////////////////////////
//
// CCompressionFile -- abstract base class
//

// Class for support work with compressed files.
// Assumed that file on hard disk is always compressed and data in memory
// is uncompressed. 
//

class NCBI_XUTIL_EXPORT CCompressionFile
{
public:
    /// Compression file handler
    typedef void* TFile;

    /// File open mode
    enum EMode {
        eMode_Read,         ///< Reading from compressed file
        eMode_Write         ///< Writing compressed data to file
    };

    // 'ctors
    CCompressionFile(void);
    CCompressionFile(const string& path, EMode mode); 
    virtual ~CCompressionFile(void);

    // Opens a compressed file for reading or writing.
    // Return NULL if error has been occurred.
    virtual bool Open(const string& path, EMode mode) = 0; 

    // Read up to "len" uncompressed bytes from the compressed file "file"
    // into the buffer "buf". Return the number of bytes actually read
    // (0 for end of file, -1 for error)
    virtual long Read(void* buf, size_t len) = 0;

    // Writes the given number of uncompressed bytes into the compressed file.
    // Return the number of bytes actually written or -1 for error.
    virtual long Write(const void* buf, size_t len) = 0;

    // Flushes all pending output if necessary, closes the compressed file.
    // Return TRUE on success, FALSE on error.
    virtual bool Close(void) = 0;

protected:
    TFile  m_File;   ///< File handler.
    EMode  m_Mode;   ///< File open mode.
};



//////////////////////////////////////////////////////////////////////////////
//
// CCompressionProcessor -- abstract base class
//
// Contains a functions for service a compression/decompression session.
//

class NCBI_XUTIL_EXPORT CCompressionProcessor
{
public:
    // Type of the result of all basic functions
    enum EStatus {
        // Everything is fine, no errors occurred
        eStatus_Success,
        // Special case of eStatus_Success.
        // Logical end of (compressed) stream is detected, no errors occurred.
        // All subsequent inquiries about data processing should be ignored.
        eStatus_EndOfData,
        // Error has occurred. The error code can be acquired by GetErrorCode.
        eStatus_Error,
        // Output buffer overflow - not enough output space.
        // Buffer must be emptied and the last action repeated.
        eStatus_Overflow,
        // Special value, status is undefined
        eStatus_Unknown
    };

    // 'ctors
    CCompressionProcessor(void);
    virtual ~CCompressionProcessor(void);

    // Return compressor's busy flag. If returns value is true that
    // the current compression object already have being use in other
    // compression session.
    bool IsBusy(void) const;

    // Return number of processed/output bytes.
    unsigned long GetProcessedSize(void);
    unsigned long GetOutputSize(void);

protected:
    // Initialize the internal stream state for compression/decompression.
    // It does not perform any compression, this will be done by Process().
    virtual EStatus Init(void) = 0;

    // Compress/decompress as much data as possible, and stops when the input
    // buffer becomes empty or the output buffer becomes full. It may
    // introduce some output latency (reading input without producing any
    // output).
    virtual EStatus Process
    (const char* in_buf,      // [in]  input buffer 
     size_t      in_len,      // [in]  input data length
     char*       out_buf,     // [in]  output buffer
     size_t      out_size,    // [in]  output buffer size
     size_t*     in_avail,    // [out] count unproc.bytes in input buffer
     size_t*     out_avail    // [out] count bytes putted into out buffer
     ) = 0;

    // Flush compressed/decompressed data from the output buffer. 
    // Flushing may degrade compression for some compression algorithms
    // and so it should be used only when necessary.
    virtual EStatus Flush
    (char*       out_buf,     // [in]  output buffer
     size_t      out_size,    // [in]  output buffer size
     size_t*     out_avail    // [out] count bytes putted into out buffer
     ) = 0;

    // Finish the compression/decompression process.
    // Process pending input, flush pending output.
    // This function slightly like to Flush(), but it must be called only
    // at the end of compression process before End().
    virtual EStatus Finish
    (char*       out_buf,     // [in]  output buffer
     size_t      out_size,    // [in]  output buffer size
     size_t*     out_avail    // [out] count bytes putted into out buffer
     ) = 0;

    // Free all dynamically allocated data structures.
    // This function discards any unprocessed input and does not flush
    // any pending output.
    virtual EStatus End(void) = 0;

protected:
    // Reset internal state
    void Reset(void);

    // Set/unset compressor busy flag
    void SetBusy(bool busy = true);

    // Increase number of processed/output bytes.
    void IncreaseProcessedSize(unsigned long n_bytes);
    void IncreaseOutputSize(unsigned long n_bytes);

private:
    unsigned long m_ProcessedSize; //< The number of processed bytes
    unsigned long m_OutputSize;    //< The number of output bytes
    bool          m_Busy;          //< Is true if compressor is ready to begin
                                   //< next session
    // Friend classes
    friend class CCompressionStream;
    friend class CCompressionStreambuf;
    friend class CCompressionStreamProcessor;
};


/////////////////////////////////////////////////////////////////////////////
//
// CCompressionException
//
//  Exceptions generated by CCompresson and derived classes
//

class CCompressionException : public CCoreException
{
public:
    enum EErrCode {
        eCompression,      ///< Compression/decompression error
        eCompressionFile   ///< Compression/decompression file error
    };
    virtual const char* GetErrCodeString(void) const
    {
        switch (GetErrCode()) {
        case eCompression     : return "eCompression";
        case eCompressionFile : return "eCompressionFile";
        default               : return CException::GetErrCodeString();
        }
    }
    NCBI_EXCEPTION_DEFAULT(CCompressionException,CCoreException);
};


/* @} */


//
// Inline functions
//

inline
void CCompression::SetLevel(ELevel level)
{
    m_Level = level;
}

inline
int CCompression::GetErrorCode(void) const
{
    return m_ErrorCode;
}

inline
string CCompression::GetErrorDescription(void) const
{
    return m_ErrorMsg;
}

inline
void CCompression::SetError(int errcode, const char* description)
{
    m_ErrorCode = errcode;
    m_ErrorMsg  = description ? description : kEmptyStr;
}

inline
CCompression::TFlags CCompression::GetFlags(void) const
{
    return m_Flags;
}

inline
void CCompression::SetFlags(TFlags flags)
{
    m_Flags = flags;
}

inline
void CCompressionProcessor::Reset(void)
{
    m_ProcessedSize  = 0;
    m_OutputSize     = 0;
    m_Busy           = false;
}

inline
bool CCompressionProcessor::IsBusy(void) const
{
    return m_Busy;
}

inline
void CCompressionProcessor::SetBusy(bool busy)
{
    if ( busy  &&  m_Busy ) {
        NCBI_THROW(CCompressionException, eCompression,
                   "CCompression::SetBusy(): The compressor is busy now");
    }
    m_Busy = busy;
}

inline
void CCompressionProcessor::IncreaseProcessedSize(unsigned long n_bytes)
{
    if (n_bytes > 0) {
        m_ProcessedSize += n_bytes;
    }
}

inline
void CCompressionProcessor::IncreaseOutputSize(unsigned long n_bytes)
{
    if (n_bytes > 0) {
        m_OutputSize += n_bytes;
    }
}

inline
unsigned long CCompressionProcessor::GetProcessedSize(void)
{
    return m_ProcessedSize;
}

inline
unsigned long CCompressionProcessor::GetOutputSize(void)
{
    return m_OutputSize;
}


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.19  2006/12/26 17:32:26  ivanov
 * Move fAllowTransparentRead flag definition from CCompression class
 * to each compresson algorithm definition.
 *
 * Revision 1.18  2006/12/26 17:06:28  ivanov
 * Removed extra comma in EDecompressMode declaration
 *
 * Revision 1.17  2006/12/26 16:06:53  ivanov
 * Fixed compilation error
 *
 * Revision 1.16  2006/12/26 15:57:16  ivanov
 * Add a possibility to detect a fact that data in the buffer/file/stream
 * is uncompressed, and allow to use transparent reading (instead of
 * decompression) from it. Added flag CCompression::fAllowTransparentRead.
 *
 * Revision 1.15  2006/12/18 19:37:06  ivanov
 * CCompressionProcessor: added friend class CCompressionStreamProcessor
 *
 * Revision 1.14  2006/11/23 03:52:37  ivanov
 * CCompressionProcessor::EStatus += eStatus_Unknown
 *
 * Revision 1.13  2006/10/26 15:34:16  ivanov
 * Added automatic finalization for input streams, if no more data
 * in the underlying stream
 *
 * Revision 1.12  2006/01/20 18:37:29  ivanov
 * Minor comment fix
 *
 * Revision 1.11  2005/08/22 14:25:48  ivanov
 * Cosmetics
 *
 * Revision 1.10  2005/04/25 19:01:44  ivanov
 * Changed parameters and buffer sizes from being 'int', 'unsigned int' or
 * 'unsigned long' to unified 'size_t'
 *
 * Revision 1.9  2004/08/19 13:10:56  dicuccio
 * Dropped export specifier on inlined exceptions
 *
 * Revision 1.8  2004/05/10 11:56:24  ivanov
 * Added gzip file format support
 *
 * Revision 1.7  2004/04/01 14:14:02  lavr
 * Spell "occurred", "occurrence", and "occurring"
 *
 * Revision 1.6  2003/07/15 15:50:00  ivanov
 * Improved error diagnostics. Renamed GetLastError() -> GetErrorCode().
 * Added GetErrorDescription(). Added second parameter for SetError().
 * Added new status value - eStatus_EndOfData.
 *
 * Revision 1.5  2003/07/10 16:19:25  ivanov
 * Added kCompressionDefaultBufSize definition from stream.hpp.
 * Added auxiliary file compression/decompression functions.
 *
 * Revision 1.4  2003/06/03 20:09:54  ivanov
 * The Compression API redesign. Added some new classes, rewritten old.
 *
 * Revision 1.3  2003/04/11 19:57:25  ivanov
 * Move streambuf.hpp from 'include/...' to 'src/...'
 *
 * Revision 1.2  2003/04/08 20:48:51  ivanov
 * Added class-key declaration for friend classes in the CCompression
 *
 * Revision 1.1  2003/04/07 20:42:11  ivanov
 * Initial revision
 *
 * ===========================================================================
 */

#endif  /* UTIL_COMPRESS__COMPRESS__HPP */
