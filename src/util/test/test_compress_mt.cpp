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
 * File Description:  Test program for the Compression API
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/test_mt.hpp>
#include <corelib/ncbi_limits.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/ncbi_test.hpp>
#include <util/compress/stream_util.hpp>

#include <common/test_assert.h>  // This header must go last


USING_NCBI_SCOPE;


#define KB * NCBI_CONST_UINT8(1024)

/// Length of data buffers for tests (>5 for overflow test)
const size_t  kTests[] = {20, 16 KB, 41 KB};

/// Output buffer length. ~20% more than maximum value from kTests[].
const size_t  kBufLen = size_t(41 KB * 1.2);

/// Number of tests.
const size_t  kTestCount = sizeof(kTests)/sizeof(kTests[0]);

/// Method type for more compact representation
typedef CCompressStream M;


//////////////////////////////////////////////////////////////////////////////
//
// Test application
//

class CTest : public CThreadedApp
{
public:
    bool TestApp_Init(void);
    bool Thread_Run(int idx);
    bool TestApp_Args(CArgDescriptions& args);

public:
    // Test specified compression method
    template<class TCompression,
             class TCompressionFile,
             class TStreamCompressor,
             class TStreamDecompressor>
    void TestMethod(M::EMethod, int idx, const char* src_buf, size_t src_len, size_t buf_len);

    // Print out compress/decompress status
    enum EPrintType { 
        eCompress,
        eDecompress 
    };
    void PrintResult(EPrintType type, int last_errcode,
                     size_t src_len, size_t dst_len, size_t out_len);

private:
    // Auxiliary methods
    CNcbiIos* x_CreateIStream(const string& filename, const string& src, size_t buf_len);
    void x_CreateFile(const string& filename, const char* buf, size_t len);

private:
    // Available tests
    bool bz2, lzo, z, zcf, zstd;

    // MT test doesn't use "big data" test, so we have next members
    // for compatibility with ST test only
    bool   m_AllowIstrstream;   // allow using CNcbiIstrstream
    bool   m_AllowOstrstream;   // allow using CNcbiOstrstream
    bool   m_AllowStrstream;    // allow using CNcbiStrstream
    string m_SrcFile;           // file with source data
};


#include "test_compress_util.inl"



bool CTest::TestApp_Init()
{
    SetDiagPostLevel(eDiag_Error);
    // To see all output, uncomment next line:
    //SetDiagPostLevel(eDiag_Trace);

    // Define available tests

    // Get arguments
    const CArgs& args = GetArgs();
    string test = args["lib"].AsString();

    bz2  = (test == "all" || test == "bz2");
    lzo  = (test == "all" || test == "lzo");
    z    = (test == "all" || test == "z");
    zcf  = (test == "all" || test == "zcf");
    zstd = (test == "all" || test == "zstd");

    if (bz2) {
        #if !defined(HAVE_LIBBZ2)
            ERR_POST(Warning << "BZ2 is not available on this platform, ignored.");
            bz2 = false;
        #else
            CBZip2Compression::Initialize();
        #endif
    }
    if (lzo) {
        #if !defined(HAVE_LIBLZO)
            ERR_POST(Warning << "LZO is not available on this platform, ignored.");
            lzo = false;
        #else
            CLZOCompression::Initialize();
        #endif
    }
    if (z) {
        #if !defined(HAVE_LIBZ)
            ERR_POST(Warning << "ZLIB is not available on this platform, ignored.");
            z = false;
        #else
            CZipCompression::Initialize();
        #endif
    }
    if (zcf) {
        #if !defined(HAVE_LIBZCF)
            ERR_POST(Warning << "Cloudflare ZLIB is not available on this platform, ignored.");
            zcf = false;
        #else
            CZipCloudflareCompression::Initialize();
        #endif
    }
    if (zstd) {
        #if !defined(HAVE_LIBZSTD)
            ERR_POST(Warning << "ZSTD is not available on this platform, ignored.");
            zstd = false;
        #else
            CZstdCompression::Initialize();
        #endif
    }

    m_AllowIstrstream = true;
    m_AllowOstrstream = true;
    m_AllowStrstream  = true;

    return true;
}


bool CTest::TestApp_Args(CArgDescriptions& args)
{
    args.SetUsageContext(GetArguments().GetProgramBasename(),
                         "Test compression library in MT mode");
    args.AddDefaultPositional
        ("lib", "Compression library to test", CArgDescriptions::eString, "all");
    args.SetConstraint
        ("lib", &(*new CArgAllow_Strings, "all", "bz2", "lzo", "z", "zcf", "zstd"));
    return true;
}


bool CTest::Thread_Run(int idx)
{
    // Set randomization seed for the test
    CNcbiTest::SetRandomSeed();

    // Preparing data for compression
    ERR_POST(Trace << "Creating test data...");
    AutoArray<char> src_buf_arr(kBufLen + 1 /* for possible '\0' */);
    char* src_buf = src_buf_arr.get();
    assert(src_buf);

    for (size_t i = 0; i < kBufLen; i += 2) {
        // Use a set of 25 chars [A-Z]
        // NOTE: manipulator tests don't allow '\0'.
        src_buf[i]   = (char)(65+(double)rand()/RAND_MAX*(90-65));
        // Make data more predictable for better compression,
        // especially for LZO, that is bad on a random data.
        src_buf[i+1] = (char)(src_buf[i] + 1);
    }
    // Modify first bytes to fixed value, this possible will prevent decoders
    // to treat random text data as compressed data.
    memcpy(src_buf,"12345",5);

    // Test compressors with different size of data
    for (size_t i = 0; i < kTestCount; i++) {

        // Some test require zero-terminated data (manipulators).
        size_t len   = kTests[i];
        char   saved = src_buf[len];
        src_buf[len] = '\0';

        ERR_POST(Trace << "====================================");
        ERR_POST(Trace << "Data size = " << len);

#if defined(HAVE_LIBBZ2)
        if ( bz2 ) {
            ERR_POST(Trace << "-------------- BZip2 ---------------");
            TestMethod<CBZip2Compression,
                       CBZip2CompressionFile,
                       CBZip2StreamCompressor,
                       CBZip2StreamDecompressor> (M::eBZip2, idx, src_buf, len, kBufLen);
        }
#endif
#if defined(HAVE_LIBLZO)
        if ( lzo ) {
            ERR_POST(Trace << "-------------- LZO -----------------");
            TestMethod<CLZOCompression,
                       CLZOCompressionFile,
                       CLZOStreamCompressor,
                       CLZOStreamDecompressor> (M::eLZO, idx, src_buf, len, kBufLen);
        }
#endif
#if defined(HAVE_LIBZ)
        if ( z ) {
            ERR_POST(Trace << "-------------- Zlib ----------------");
            TestMethod<CZipCompression,
                       CZipCompressionFile,
                       CZipStreamCompressor,
                       CZipStreamDecompressor> (M::eZip, idx, src_buf, len, kBufLen);
        }
#endif
#if defined(HAVE_LIBZCF)
        if ( zcf ) {
            ERR_POST(Trace << "-------------- Zlib Cloudflare -----");
            TestMethod<CZipCloudflareCompression,
                       CZipCloudflareCompressionFile,
                       CZipCloudflareStreamCompressor,
                       CZipCloudflareStreamDecompressor>(M::eZipCloudflare, idx, src_buf, len, kBufLen);
        }
#endif
#if defined(HAVE_LIBZSTD)
        if ( zstd ) {
            ERR_POST(Trace << "-------------- Zstd ----------------");
            TestMethod<CZstdCompression,
                       CZstdCompressionFile,
                       CZstdStreamCompressor,
                       CZstdStreamDecompressor>(M::eZstd, idx, src_buf, len, kBufLen);
        }
#endif

        // Restore saved character
        src_buf[len] = saved;
    }

    _TRACE("\nTEST execution completed successfully!\n");
    return true;
}



//////////////////////////////////////////////////////////////////////////////
//
// Test specified compression method
//

// Print OK message.
#define OK          ERR_POST(Trace << "OK")
#define OK_MSG(msg) ERR_POST(Trace << msg << " - OK")

// Initialize destination buffers.
#define INIT_BUFFERS  memset(dst_buf, 0, buf_len); memset(cmp_buf, 0, buf_len)


template<class TCompression,
         class TCompressionFile,
         class TStreamCompressor,
         class TStreamDecompressor>
void CTest::TestMethod(M::EMethod method, 
                       int idx, const char* src_buf, size_t src_len, size_t buf_len)
{
    const string kFileName_str = "test_compress.compressed.file." + NStr::IntToString(idx);
    const char*  kFileName = kFileName_str.c_str();

#   include "test_compress_run.inl"
}


void CTest::PrintResult(EPrintType type, int last_errcode, 
                       size_t src_len, size_t dst_len, size_t out_len)
{
    ERR_POST(Trace
        << string((type == eCompress) ? "Compress   " : "Decompress ")
        << "errcode = "
        << ((last_errcode == kUnknownErr) ? "?" : NStr::IntToString(last_errcode)) << ", "
        << ((src_len == kUnknown) ?         "?" : NStr::SizetToString(src_len)) << " -> "
        << ((out_len == kUnknown) ?         "?" : NStr::SizetToString(out_len)) << ", limit "
        << ((dst_len == kUnknown) ?         "?" : NStr::SizetToString(dst_len))
    );
}



//////////////////////////////////////////////////////////////////////////////
//
// MAIN
//

int main(int argc, const char* argv[])
{
    // Execute main application function
    return CTest().AppMain(argc, argv);
}
