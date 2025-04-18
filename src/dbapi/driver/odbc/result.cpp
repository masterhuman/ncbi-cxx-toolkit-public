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
 * Author:  Vladimir Soussov
 *
 * File Description:  ODBC Results
 *
 */

#include <ncbi_pch.hpp>
#include <dbapi/driver/odbc/interfaces.hpp>
#include <dbapi/driver/util/numeric_convert.hpp>
#include <dbapi/error_codes.hpp>

#include "odbc_utils.hpp"


#define NCBI_USE_ERRCODE_X   Dbapi_Odbc_Results

#undef NCBI_DATABASE_THROW
#undef NCBI_DATABASE_RETHROW

#define NCBI_DATABASE_THROW(ex_class, message, err_code, severity) \
    NCBI_ODBC_THROW(ex_class, message, err_code, severity)
#define NCBI_DATABASE_RETHROW(prev_ex, ex_class, message, err_code, severity) \
    NCBI_ODBC_RETHROW(prev_ex, ex_class, message, err_code, severity)

// Accommodate all the code of the form
//     string err_message = "..." + GetDbgInfo();
//     DATABASE_DRIVER_ERROR(err_message, ...);
// which will still pick up the desired context due to
// NCBI_DATABASE_(RE)THROW's above redefinitions.
#define GetDbgInfo() 0


BEGIN_NCBI_SCOPE

/////////////////////////////////////////////////////////////////////////////
static const char* wrong_type = "Wrong type of CDB_Object.";

/////////////////////////////////////////////////////////////////////////////

static EDB_Type s_GetDataType(SQLSMALLINT t, SQLSMALLINT dec_digits,
                              SQLULEN prec)
{
    switch (t) {
    case SQL_WCHAR:
    case SQL_CHAR:         return (prec < 256)? eDB_Char : eDB_LongChar;
    case SQL_WVARCHAR:
    case SQL_VARCHAR:      return eDB_VarChar;
    case SQL_LONGVARCHAR:  return eDB_Text;
    case SQL_LONGVARBINARY:
    case SQL_WLONGVARCHAR:
        return eDB_Image;
    case SQL_DECIMAL:
    case SQL_NUMERIC:      if(prec > 20 || dec_digits > 0) return eDB_Numeric;
    case SQL_BIGINT:       return eDB_BigInt;
    case SQL_SMALLINT:     return eDB_SmallInt;
    case SQL_INTEGER:      return eDB_Int;
    case SQL_FLOAT:        return eDB_Double;
    case SQL_REAL:         return eDB_Float;
    case SQL_DOUBLE:       return eDB_Double;
    case SQL_BINARY:       return (prec < 256)? eDB_Binary : eDB_LongBinary;
    case SQL_BIT:          return eDB_Bit;
    case SQL_TINYINT:      return eDB_TinyInt;
    case SQL_VARBINARY:    return (prec < 256)? eDB_VarBinary : eDB_LongBinary;
    case SQL_TYPE_TIMESTAMP:
        if (prec <= 16  &&  dec_digits <= 0) {
            return eDB_SmallDateTime;
        } else if (prec <= 23  &&  dec_digits <= 3) {
            return eDB_DateTime;
        } else {
            return eDB_BigDateTime;
        }
    default:               return eDB_UnsupportedType;
    }
}


/////////////////////////////////////////////////////////////////////////////
//
//  CODBC_RowResult::
//


CODBC_RowResult::CODBC_RowResult(
    CStatementBase& stmt,
    SQLSMALLINT nof_cols,
    SQLLEN* row_count
    )
    : m_Stmt(stmt)
    , m_CurrItem(-1)
    , m_EOR(false)
    , m_RowCountPtr( row_count )
    , m_HasMoreData(false)
{
    odbc::TSqlChar column_name_buff[eODBC_Column_Name_Size];

    if(m_RowCountPtr) *m_RowCountPtr = 0;

    SQLSMALLINT actual_name_size;
    SQLSMALLINT nullable;

    m_ColFmt = new SODBC_ColDescr[nof_cols];
    for (unsigned int n = 0; n < (unsigned int)nof_cols; ++n) {
        // SQLDescribeCol takes a pointer to a buffer.
        switch(SQLDescribeCol(GetHandle(),
                              n + 1,
                              column_name_buff,
                              eODBC_Column_Name_Size * sizeof(odbc::TSqlChar),
                              &actual_name_size,
                              &m_ColFmt[n].DataType,
                              &m_ColFmt[n].ColumnSize,
                              &m_ColFmt[n].DecimalDigits,
                              &nullable)) {
        case SQL_SUCCESS_WITH_INFO:
            ReportErrors();
        case SQL_SUCCESS:
            m_ColFmt[n].ColumnName =
                CODBCString(column_name_buff,
                            actual_name_size).ConvertTo(GetClientEncoding());
            break;
        case SQL_ERROR:
            ReportErrors();
            {
                string err_message = "SQLDescribeCol failed." + GetDbgInfo();
                DATABASE_DRIVER_ERROR( err_message, 420020 );
            }
        default:
            {
                string err_message = "SQLDescribeCol failed (memory corruption suspected)." + GetDbgInfo();
                DATABASE_DRIVER_ERROR( err_message, 420021 );
            }
        }

        m_CachedRowInfo.Add(
            m_ColFmt[n].ColumnName,
            m_ColFmt[n].ColumnSize,
            s_GetDataType(m_ColFmt[n].DataType,
                          m_ColFmt[n].DecimalDigits,
                          m_ColFmt[n].ColumnSize)
            );
    }
}


EDB_ResType CODBC_RowResult::ResultType() const
{
    return eDB_RowResult;
}


bool CODBC_RowResult::Fetch()
{
    m_CurrItem = -1;
    m_LastReadData.resize(0);
    m_HasMoreData = false;
    if (!m_EOR) {
        switch (SQLFetch(GetHandle())) {
        case SQL_SUCCESS_WITH_INFO:
            ReportErrors();
        case SQL_SUCCESS:
            m_CurrItem = 0;
            m_HasMoreData = true;
            if ( m_RowCountPtr != NULL ) {
                ++(*m_RowCountPtr);
            }
            return true;
        case SQL_NO_DATA:
            m_EOR = true;
            break;
        case SQL_ERROR:
            ReportErrors();
            {
                string err_message = "SQLFetch failed." + GetDbgInfo();
                DATABASE_DRIVER_ERROR( err_message, 430003 );
            }
        default:
            {
                string err_message = "SQLFetch failed (memory corruption suspected)." + GetDbgInfo();
                DATABASE_DRIVER_ERROR( err_message, 430004 );
            }
        }
    }
    return false;
}


int CODBC_RowResult::CurrentItemNo() const
{
    return m_CurrItem;
}

int CODBC_RowResult::GetColumnNum(void) const
{
    return static_cast<int>(GetDefineParams().GetNum());
}

int CODBC_RowResult::xGetData(SQLSMALLINT target_type, SQLPOINTER buffer,
                              SQLINTEGER buffer_size, bool* more)
{
    SQLLEN f;
    bool fake_more;
    if (more == NULL) {
        more = &fake_more;
    } else {
        *more = false;
    }

    switch(SQLGetData(GetHandle(), m_CurrItem+1, target_type, buffer, buffer_size, &f)) {
    case SQL_SUCCESS_WITH_INFO:
        switch(f) {
        case SQL_NO_TOTAL:
            *more = true;
            return buffer_size;
        case SQL_NULL_DATA:
            return 0;
        default:
            if (f < 0) {
                ReportErrors();
            } else {
                *more = true;
            }
            return (int)f;
        }
    case SQL_SUCCESS:
        if(target_type == SQL_C_CHAR) buffer_size--;
        return (f > buffer_size)? buffer_size : (int)f;
    case SQL_NO_DATA:
        return 0;
    case SQL_ERROR:
        ReportErrors();
    default:
        {
            string err_message = "SQLGetData failed." + GetDbgInfo();
            DATABASE_DRIVER_ERROR( err_message, 430027 );
        }
    }
}

ssize_t CODBC_RowResult::x_GetVarLenData(SQLSMALLINT target_type,
                                         TItemBuffer& buffer,
                                         SQLINTEGER buffer_size)
{
    list<string> extra_buffers;
    char * current_buffer = buffer.get();
    SQLINTEGER current_size = buffer_size;
    ssize_t n;
    int nul_size;
    bool more;
    switch (target_type) {
    case SQL_C_CHAR:  nul_size = 1;               break;
    case SQL_C_WCHAR: nul_size = sizeof(wchar_t); break;
    default:          nul_size = 0;               break;
    }
    while ((n = xGetData(target_type, current_buffer, current_size, &more))
           >= current_size  &&  more) {
        if (n > current_size) {
            // Add margin for (possibly wide) NUL-related complications.
            current_size = static_cast<int>(n) + 2 * nul_size - current_size;
        }
        extra_buffers.emplace_back(string(current_size, '\0'));
        current_buffer = const_cast<char*>(extra_buffers.back().data());
    }
    if (extra_buffers.empty()) {
        return n;
    }

    const char * orig_buffer = buffer.get();
    int elided = 0;
    extra_buffers.back().resize(n);
    n = buffer_size;
    for (const auto & it : extra_buffers) {
        n += it.size();
    }
    buffer.reset(new char[n + nul_size]);
    memcpy(buffer.get(), orig_buffer, buffer_size);
    ssize_t pos = buffer_size;
    for (const auto & it : extra_buffers) {
        if (nul_size > 0  &&  pos >= nul_size) {
            bool elide = true;
            for (int i = 1;  i <= nul_size;  ++i) {
                if (buffer.get()[pos - i] != '\0') {
                    elide = false;
                    break;
                }
            }
            if (elide) {
                pos    -= nul_size;
                elided += nul_size;
            }
        }
        memcpy(buffer.get() + pos, it.data(), it.size());
        pos += it.size();
    }
    memset(buffer.get() + pos, '\0', nul_size);
    _ASSERT(pos + elided == n);
    return pos;
}

static void xConvert2CDB_Numeric(CDB_Numeric* d, SQL_NUMERIC_STRUCT& s)
{
    swap_numeric_endian((unsigned int)s.precision, s.val);
    d->Assign((unsigned int)s.precision, (unsigned int)s.scale,
             s.sign == 0, s.val);
}

bool CODBC_RowResult::CheckSIENoD_Text(CDB_Stream* val)
{
    int rc = 0;
    SQLLEN f = 0;

    char buffer[8*1024];

    rc = SQLGetData(GetHandle(), m_CurrItem + 1, SQL_C_CHAR, buffer, sizeof(buffer), &f);

    switch( rc ) {
    case SQL_SUCCESS_WITH_INFO:
        if(f == SQL_NO_TOTAL) {
            f = sizeof(buffer) - 1;
        } else if(f < 0) {
            ReportErrors();
        }
    case SQL_SUCCESS:
        if(f > 0) {
            if(f > SQLLEN(sizeof(buffer) - 1)) {
                f = sizeof(buffer)-1;
            }

            val->Append(buffer, f);
        }
        return true;
    case SQL_NO_DATA:
        break;
    case SQL_ERROR:
        ReportErrors();
    default:
        {
            DATABASE_DRIVER_ERROR(
                string("SQLGetData failed while retrieving BLOB into C") +
                CDB_Object::GetTypeName(val->GetType()) + '.',
                430021);
        }
    }

    return false;
}

#ifdef HAVE_WSTRING
bool CODBC_RowResult::CheckSIENoD_WText(CDB_Stream* val)
{
    int rc = 0;
    SQLLEN f = 0;

    wchar_t buffer[4*1024];

    rc = SQLGetData(GetHandle(), m_CurrItem + 1, SQL_C_WCHAR, buffer, sizeof(buffer), &f);

    switch( rc ) {
    case SQL_SUCCESS_WITH_INFO:
        if(f == SQL_NO_TOTAL) {
            f = sizeof(buffer) - 1;
        } else if(f < 0) {
            ReportErrors();
        }
    case SQL_SUCCESS:
        if(f > 0) {
            if(f > SQLLEN(sizeof(buffer) - 1)) {
                f = sizeof(buffer)-1;
            }

            f = f / sizeof(wchar_t);

            string encoded_value = CODBCString(buffer, f).ConvertTo(GetClientEncoding());
            val->Append(encoded_value.data(), encoded_value.size());
        }
        return true;
    case SQL_NO_DATA:
        break;
    case SQL_ERROR:
        ReportErrors();
    default:
        {
            DATABASE_DRIVER_ERROR(
                string("SQLGetData failed while retrieving BLOB into C") +
                CDB_Object::GetTypeName(val->GetType()) + '.',
                430021);
        }
    }

    return false;
}
#endif

bool CODBC_RowResult::CheckSIENoD_Binary(CDB_Stream* val)
{
    SQLLEN f = 0;
    char buffer[8*1024];

    switch(SQLGetData(GetHandle(), m_CurrItem+1, SQL_C_BINARY, buffer, sizeof(buffer), &f)) {
    case SQL_SUCCESS_WITH_INFO:
        if(f == SQL_NO_TOTAL || f > SQLLEN(sizeof(buffer))) f = sizeof(buffer);
        else ReportErrors();
    case SQL_SUCCESS:
        if(f > 0) {
            if(f > SQLLEN(sizeof(buffer))) f = sizeof(buffer);
            val->Append(buffer, f);
        }
        return true;
    case SQL_NO_DATA:
        break;
    case SQL_ERROR:
        ReportErrors();
    default:
        {
            DATABASE_DRIVER_ERROR(
                string("SQLGetData failed while retrieving BLOB into C") +
                CDB_Object::GetTypeName(val->GetType()) + '.',
                430022);
        }
    }

    return false;
}

CDB_Object* CODBC_RowResult::x_LoadItem(I_Result::EGetItem policy, CDB_Object* item_buf)
{
    char base_buf[8*1024];
    TItemBuffer buffer(base_buf, eNoOwnership);
    ssize_t outlen;

    switch(m_ColFmt[m_CurrItem].DataType) {
    case SQL_WCHAR:
    case SQL_WVARCHAR:
        switch (item_buf->GetType()) {
        case eDB_VarBinary:
            outlen = x_GetVarLenData(SQL_C_BINARY, buffer, sizeof(base_buf));
            if ( outlen <= 0) item_buf->AssignNULL();
            else ((CDB_VarBinary*)item_buf)->SetValue(buffer.get(), outlen);
            break;
        case eDB_Binary:
            outlen = x_GetVarLenData(SQL_C_BINARY, buffer, sizeof(base_buf));
            if ( outlen <= 0) item_buf->AssignNULL();
            else ((CDB_Binary*)item_buf)->SetValue(buffer.get(), outlen);
            break;
        case eDB_LongBinary:
            outlen = x_GetVarLenData(SQL_C_BINARY, buffer, sizeof(base_buf));
            if ( outlen <= 0) item_buf->AssignNULL();
            else ((CDB_LongBinary*)item_buf)->SetValue(buffer.get(), outlen);
            break;
#ifdef HAVE_WSTRING
        case eDB_VarChar:
            outlen = x_GetVarLenData(SQL_C_WCHAR, buffer, sizeof(base_buf));
            if ( outlen <= 0) item_buf->AssignNULL();
            else *((CDB_VarChar*)item_buf)
                     = CODBCString((wchar_t*)buffer.get())
                     .ConvertTo(GetClientEncoding());
            break;
        case eDB_Char:
            outlen = x_GetVarLenData(SQL_C_WCHAR, buffer, sizeof(base_buf));
            if ( outlen <= 0) item_buf->AssignNULL();
            else *((CDB_Char*)item_buf)
                     = CODBCString((wchar_t*)buffer.get())
                     .ConvertTo(GetClientEncoding());
            break;
        case eDB_LongChar:
            outlen = x_GetVarLenData(SQL_C_WCHAR, buffer, sizeof(base_buf));
            if ( outlen <= 0) item_buf->AssignNULL();
            else *((CDB_LongChar*)item_buf)
                     = CODBCString((wchar_t*)buffer.get())
                     .ConvertTo(GetClientEncoding());
            break;
#endif
        default:
            {
                string err_message = wrong_type + GetDbgInfo();
                DATABASE_DRIVER_ERROR( err_message, 430020 );
            }
        }
        break;
    case SQL_VARCHAR:
    case SQL_CHAR: {
        switch (item_buf->GetType()) {
        case eDB_VarBinary:
            outlen = x_GetVarLenData(SQL_C_BINARY, buffer, sizeof(base_buf));
            if ( outlen <= 0) item_buf->AssignNULL();
            else ((CDB_VarBinary*) item_buf)->SetValue(buffer.get(), outlen);
            break;
        case eDB_Binary:
            outlen = x_GetVarLenData(SQL_C_BINARY, buffer, sizeof(base_buf));
            if ( outlen <= 0) item_buf->AssignNULL();
            else ((CDB_Binary*) item_buf)->SetValue(buffer.get(), outlen);
            break;
        case eDB_LongBinary:
            outlen = x_GetVarLenData(SQL_C_BINARY, buffer, sizeof(base_buf));
            if ( outlen <= 0) item_buf->AssignNULL();
            else ((CDB_LongBinary*) item_buf)->SetValue(buffer.get(), outlen);
            break;
        case eDB_VarChar:
            outlen = x_GetVarLenData(SQL_C_CHAR, buffer, sizeof(base_buf));
            if ( outlen < 0) item_buf->AssignNULL();
            else *((CDB_VarChar*)  item_buf) = buffer.get();
            break;
        case eDB_Char:
            outlen = x_GetVarLenData(SQL_C_CHAR, buffer, sizeof(base_buf));
            if ( outlen < 0) item_buf->AssignNULL();
            else *((CDB_Char*)     item_buf) = buffer.get();
            break;
        case eDB_LongChar:
            outlen = x_GetVarLenData(SQL_C_CHAR, buffer, sizeof(base_buf));
            if ( outlen < 0) item_buf->AssignNULL();
            else *((CDB_LongChar*)     item_buf) = buffer.get();
            break;
        default:
            {
                string err_message = wrong_type + GetDbgInfo();
                DATABASE_DRIVER_ERROR( err_message, 430020 );
            }
        }
        break;
    }

    case SQL_BINARY:
    case SQL_VARBINARY: {
        switch ( item_buf->GetType() ) {
        case eDB_VarBinary:
            outlen = x_GetVarLenData(SQL_C_BINARY, buffer, sizeof(base_buf));
            if ( outlen <= 0) item_buf->AssignNULL();
            else ((CDB_VarBinary*) item_buf)->SetValue(buffer.get(), outlen);
            break;
        case eDB_Binary:
            outlen = x_GetVarLenData(SQL_C_BINARY, buffer, sizeof(base_buf));
            if ( outlen <= 0) item_buf->AssignNULL();
            else ((CDB_Binary*) item_buf)->SetValue(buffer.get(), outlen);
            break;
        case eDB_LongBinary:
            outlen = x_GetVarLenData(SQL_C_BINARY, buffer, sizeof(base_buf));
            if ( outlen <= 0) item_buf->AssignNULL();
            else ((CDB_LongBinary*) item_buf)->SetValue(buffer.get(), outlen);
            break;
        case eDB_VarChar:
            outlen = x_GetVarLenData(SQL_C_CHAR, buffer, sizeof(base_buf));
            if (outlen < 0) item_buf->AssignNULL();
            else *((CDB_VarChar*)  item_buf) = buffer.get();
            break;
        case eDB_Char:
            outlen = x_GetVarLenData(SQL_C_CHAR, buffer, sizeof(base_buf));
            if (outlen < 0) item_buf->AssignNULL();
            else *((CDB_Char*) item_buf) = buffer.get();
            break;
        case eDB_LongChar:
            outlen = x_GetVarLenData(SQL_C_CHAR, buffer, sizeof(base_buf));
            if (outlen < 0) item_buf->AssignNULL();
            else *((CDB_LongChar*) item_buf) = buffer.get();
            break;
        default:
            {
                string err_message = wrong_type + GetDbgInfo();
                DATABASE_DRIVER_ERROR( err_message, 430020 );
            }
        }

        break;
    }

    case SQL_BIT: {
        SQLCHAR v;
        switch (  item_buf->GetType()  ) {
        case eDB_Bit:
            outlen = xGetData(SQL_C_BIT, &v, sizeof(SQLCHAR));
            if (outlen <= 0) item_buf->AssignNULL();
            else *((CDB_Bit*) item_buf) = (int) v;
            break;
        case eDB_TinyInt:
            outlen = xGetData(SQL_C_BIT, &v, sizeof(SQLCHAR));
            if (outlen <= 0) item_buf->AssignNULL();
            else *((CDB_TinyInt*)  item_buf) = v ? 1 : 0;
            break;
        case eDB_SmallInt:
            outlen = xGetData(SQL_C_BIT, &v, sizeof(SQLCHAR));
            if (outlen <= 0) item_buf->AssignNULL();
            else *((CDB_SmallInt*) item_buf) = v ? 1 : 0;
            break;
        case eDB_Int:
            outlen = xGetData(SQL_C_BIT, &v, sizeof(SQLCHAR));
            if (outlen <= 0) item_buf->AssignNULL();
            else *((CDB_Int*)      item_buf) = v ? 1 : 0;
            break;
        default:
            {
                string err_message = wrong_type + GetDbgInfo();
                DATABASE_DRIVER_ERROR( err_message, 430020 );
            }
        }
        break;
    }

    case SQL_TYPE_TIMESTAMP: {
        SQL_TIMESTAMP_STRUCT v;
        switch ( item_buf->GetType() ) {
        case eDB_SmallDateTime:
        case eDB_DateTime:
        case eDB_BigDateTime:
        {
            outlen = xGetData(SQL_C_TYPE_TIMESTAMP, &v, sizeof(SQL_TIMESTAMP_STRUCT));
            if (outlen <= 0) item_buf->AssignNULL();
            else {
                CTime t((int)v.year, (int)v.month, (int)v.day,
                        (int)v.hour, (int)v.minute, (int)v.second,
                        (long)v.fraction);

                switch (item_buf->GetType()) {
                case eDB_SmallDateTime:
                    *((CDB_SmallDateTime*) item_buf) = t;
                    break;
                case eDB_DateTime:
                    *((CDB_DateTime*) item_buf) = t;
                    break;
                case eDB_BigDateTime:
                    // TODO - try to distinguish specific types?
                    *((CDB_BigDateTime*) item_buf) = t;
                    break;
                default:
                    _TROUBLE;
                }
            }
            break;
        }
        default:
            {
                string err_message = wrong_type + GetDbgInfo();
                DATABASE_DRIVER_ERROR( err_message, 430020 );
            }
        }
        break;
    }

    case SQL_TINYINT: {
        SQLCHAR v;
        switch (  item_buf->GetType()  ) {
        case eDB_TinyInt:
            outlen = xGetData(SQL_C_UTINYINT, &v, sizeof(SQLCHAR));
            if (outlen <= 0) item_buf->AssignNULL();
            else *((CDB_TinyInt*)  item_buf) = (Uint1) v;
            break;
        case eDB_SmallInt:
            outlen = xGetData(SQL_C_UTINYINT, &v, sizeof(SQLCHAR));
            if (outlen <= 0) item_buf->AssignNULL();
            else *((CDB_SmallInt*) item_buf) = (Int2) v;
            break;
        case eDB_Int:
            outlen = xGetData(SQL_C_UTINYINT, &v, sizeof(SQLCHAR));
            if (outlen <= 0) item_buf->AssignNULL();
            else *((CDB_Int*)      item_buf) = (Int4) v;
            break;
        default:
            {
                string err_message = wrong_type + GetDbgInfo();
                DATABASE_DRIVER_ERROR( err_message, 430020 );
            }
        }
        break;
    }

    case SQL_SMALLINT: {
        SQLSMALLINT v;
        switch (  item_buf->GetType()  ) {
        case eDB_SmallInt:
            outlen = xGetData(SQL_C_SSHORT, &v, sizeof(SQLSMALLINT));
            if (outlen <= 0) item_buf->AssignNULL();
            else *((CDB_SmallInt*) item_buf) = (Int2) v;
            break;
        case eDB_Int:
            outlen = xGetData(SQL_C_SSHORT, &v, sizeof(SQLSMALLINT));
            if (outlen <= 0) item_buf->AssignNULL();
            else *((CDB_Int*) item_buf) = (Int4) v;
            break;
        default:
            {
                string err_message = wrong_type + GetDbgInfo();
                DATABASE_DRIVER_ERROR( err_message, 430020 );
            }
        }
        break;
    }

    case SQL_INTEGER: {
        SQLINTEGER v;
        switch (  item_buf->GetType()  ) {
        case eDB_Int:
            outlen = xGetData(SQL_C_SLONG, &v, sizeof(SQLINTEGER));
            if (outlen <= 0) item_buf->AssignNULL();
            else *((CDB_Int*) item_buf) = (Int4) v;
            break;
        default:
            {
                string err_message = wrong_type + GetDbgInfo();
                DATABASE_DRIVER_ERROR( err_message, 430020 );
            }
        }
        break;
    }

    case SQL_DOUBLE:
    case SQL_FLOAT: {
        SQLDOUBLE v;
        switch (  item_buf->GetType()  ) {
        case eDB_Double:
            outlen = xGetData(SQL_C_DOUBLE, &v, sizeof(SQLDOUBLE));
            if (outlen <= 0) item_buf->AssignNULL();
            else *((CDB_Double*)      item_buf) = v;
            break;
        default:
            {
                string err_message = wrong_type + GetDbgInfo();
                DATABASE_DRIVER_ERROR( err_message, 430020 );
            }
        }
        break;
    }

    case SQL_REAL: {
        SQLREAL v;
        switch (  item_buf->GetType()  ) {
        case eDB_Float:
            outlen = xGetData(SQL_C_FLOAT, &v, sizeof(SQLREAL));
            if (outlen <= 0) item_buf->AssignNULL();
            else *((CDB_Float*)      item_buf) = v;
            break;
        default:
            {
                string err_message = wrong_type + GetDbgInfo();
                DATABASE_DRIVER_ERROR( err_message, 430020 );
            }
        }
        break;
    }

    case SQL_BIGINT:
    case SQL_DECIMAL:
    case SQL_NUMERIC: {
        switch (  item_buf->GetType()  ) {
        case eDB_Numeric: {
            SQL_NUMERIC_STRUCT v;
            SQLHDESC hdesc;
            SQLGetStmtAttr(GetHandle(), SQL_ATTR_APP_ROW_DESC, &hdesc, 0, NULL);
            SQLSetDescField(hdesc, m_CurrItem + 1, SQL_DESC_TYPE, (VOID*)SQL_C_NUMERIC, 0);
            SQLSetDescField(hdesc, m_CurrItem + 1, SQL_DESC_PRECISION,
                    (VOID*)(m_ColFmt[m_CurrItem].ColumnSize), 0);
            SQLSetDescField(hdesc, m_CurrItem + 1, SQL_DESC_SCALE,
                    reinterpret_cast<VOID*>(m_ColFmt[m_CurrItem].DecimalDigits), 0);

            // outlen = xGetData(SQL_ARD_TYPE, &v, sizeof(SQL_NUMERIC_STRUCT));
            outlen = xGetData(SQL_C_NUMERIC, &v, sizeof(SQL_NUMERIC_STRUCT));
            if (outlen <= 0) item_buf->AssignNULL();
            else xConvert2CDB_Numeric((CDB_Numeric*)item_buf, v);
            break;
        }
        case eDB_BigInt: {
            SQLBIGINT v;
            outlen = xGetData(SQL_C_SBIGINT, &v, sizeof(SQLBIGINT));
            if (outlen <= 0) item_buf->AssignNULL();
            else *((CDB_BigInt*) item_buf) = (Int8) v;
            break;
        }
        default:
            {
                string err_message = wrong_type + GetDbgInfo();
                DATABASE_DRIVER_ERROR( err_message, 430020 );
            }
        }
        break;
    }

    case SQL_WLONGVARCHAR:
        switch(item_buf->GetType()) {
#ifdef HAVE_WSTRING
        case eDB_Text: {
			if (policy == I_Result::eAssignLOB) {
				static_cast<CDB_Stream*>(item_buf)->Truncate();
			}

            while (CheckSIENoD_WText((CDB_Stream*)item_buf)) {
                continue;
            }
            break;
        }
#endif
        case eDB_Image: {
			if (policy == I_Result::eAssignLOB) {
				static_cast<CDB_Stream*>(item_buf)->Truncate();
			}

            while (CheckSIENoD_Binary((CDB_Stream*)item_buf)) {
                continue;
            }
            break;
        }
        default:
            {
                string err_message = wrong_type + GetDbgInfo();
                DATABASE_DRIVER_ERROR( err_message, 430020 );
            }
        }
        break;
    case SQL_LONGVARBINARY:
    case SQL_LONGVARCHAR:
        switch(item_buf->GetType()) {
        case eDB_Text:
        case eDB_VarCharMax: {
			if (policy == I_Result::eAssignLOB) {
				static_cast<CDB_Stream*>(item_buf)->Truncate();
			}

            while (CheckSIENoD_Text((CDB_Stream*)item_buf)) {
                continue;
            }
            break;
        }
        case eDB_Image:
        case eDB_VarBinaryMax: {
			if (policy == I_Result::eAssignLOB) {
				static_cast<CDB_Stream*>(item_buf)->Truncate();
			}

            while (CheckSIENoD_Binary((CDB_Stream*)item_buf)) {
                continue;
            }
            break;
        }
        default:
            {
                string err_message = wrong_type + GetDbgInfo();
                DATABASE_DRIVER_ERROR( err_message, 430020 );
            }
        }
        break;
    default:
        {
            string err_message = "Unsupported column type." + GetDbgInfo();
            DATABASE_DRIVER_ERROR( err_message, 430025 );
        }

    }
    return item_buf;
}

CDB_Object* CODBC_RowResult::x_MakeItem()
{
    char base_buf[8*1024];
    TItemBuffer buffer(base_buf, eNoOwnership);
    ssize_t outlen;

    switch(m_ColFmt[m_CurrItem].DataType) {
    case SQL_WCHAR:
    case SQL_WVARCHAR:
#ifdef HAVE_WSTRING
    {
        outlen = x_GetVarLenData(SQL_C_WCHAR, buffer, sizeof(base_buf));
        CODBCString odbc_str(reinterpret_cast<wchar_t*>(buffer.get()), outlen);

        if(m_ColFmt[m_CurrItem].ColumnSize < 256) {
            CDB_VarChar* val = (outlen < 0)
                ? new CDB_VarChar() : new CDB_VarChar(odbc_str.ConvertTo(GetClientEncoding()));

            return val;
        }
        else {
            CDB_LongChar* val = (outlen < 0)
                ? new CDB_LongChar(m_ColFmt[m_CurrItem].ColumnSize) :
                new CDB_LongChar(m_ColFmt[m_CurrItem].ColumnSize,
                        odbc_str.ConvertTo(GetClientEncoding()));

            return val;
        }
    }
#endif

    case SQL_VARCHAR:
    case SQL_CHAR: {
        outlen = x_GetVarLenData(SQL_C_CHAR, buffer, sizeof(base_buf));
        if(m_ColFmt[m_CurrItem].ColumnSize < 256) {
            CDB_VarChar* val = (outlen < 0)
                ? new CDB_VarChar() : new CDB_VarChar(buffer.get(),
                                                      (size_t) outlen);

            return val;
        }
        else {
            CDB_LongChar* val = (outlen < 0)
                ? new CDB_LongChar(m_ColFmt[m_CurrItem].ColumnSize) :
                new CDB_LongChar(m_ColFmt[m_CurrItem].ColumnSize,
                                 buffer.get());

            return val;
        }
    }

    case SQL_BINARY:
    case SQL_VARBINARY: {
        outlen = x_GetVarLenData(SQL_C_BINARY, buffer, sizeof(base_buf));
        if(m_ColFmt[m_CurrItem].ColumnSize < 256) {
            CDB_VarBinary* val = (outlen <= 0)
                ? new CDB_VarBinary() : new CDB_VarBinary(buffer.get(),
                                                          (size_t)outlen);

            return val;
        }
        else {
            CDB_LongBinary* val = (outlen < 0)
                ? new CDB_LongBinary(m_ColFmt[m_CurrItem].ColumnSize) :
                new CDB_LongBinary(m_ColFmt[m_CurrItem].ColumnSize,
                                   buffer.get(), (size_t) outlen);

            return val;
        }
    }

    case SQL_BIT: {
        SQLCHAR v;
        outlen = xGetData(SQL_C_BIT, &v, sizeof(SQLCHAR));
        return (outlen <= 0) ? new CDB_Bit() : new CDB_Bit((int) v);
    }

    case SQL_TYPE_TIMESTAMP: {
        SQL_TIMESTAMP_STRUCT v;
        outlen = xGetData(SQL_C_TYPE_TIMESTAMP, &v, sizeof(SQL_TIMESTAMP_STRUCT));
        EDB_Type type = s_GetDataType(SQL_TYPE_TIMESTAMP,
                                      m_ColFmt[m_CurrItem].DecimalDigits,
                                      m_ColFmt[m_CurrItem].ColumnSize);
        if (outlen <= 0) {
            return CDB_Object::Create(type);
        } else {
            CTime t((int)v.year, (int)v.month, (int)v.day,
                    (int)v.hour, (int)v.minute, (int)v.second,
                    (long)v.fraction);
            switch (type) {
            case eDB_SmallDateTime: return new CDB_SmallDateTime(t);
            case eDB_DateTime:      return new CDB_DateTime(t);
            case eDB_BigDateTime:   return new CDB_BigDateTime(t);
            default:                _TROUBLE;
            }
        }
    }

    case SQL_TINYINT: {
        SQLCHAR v;
        outlen = xGetData(SQL_C_UTINYINT, &v, sizeof(SQLCHAR));
        return (outlen <= 0) ? new CDB_TinyInt() : new CDB_TinyInt((Uint1) v);
    }

    case SQL_SMALLINT: {
        SQLSMALLINT v;
        outlen = xGetData(SQL_C_SSHORT, &v, sizeof(SQLSMALLINT));
        return (outlen <= 0) ? new CDB_SmallInt() : new CDB_SmallInt((Int2) v);
    }

    case SQL_INTEGER: {
        SQLINTEGER v;
        outlen = xGetData(SQL_C_SLONG, &v, sizeof(SQLINTEGER));
        return (outlen <= 0) ? new CDB_Int() : new CDB_Int((Int4) v);
    }

    case SQL_DOUBLE:
    case SQL_FLOAT: {
        SQLDOUBLE v;
        outlen = xGetData(SQL_C_DOUBLE, &v, sizeof(SQLDOUBLE));
        return (outlen <= 0) ? new CDB_Double() : new CDB_Double(v);
    }
    case SQL_REAL: {
        SQLREAL v;
        outlen = xGetData(SQL_C_FLOAT, &v, sizeof(SQLREAL));
        return (outlen <= 0) ? new CDB_Float() : new CDB_Float(v);
    }

    case SQL_DECIMAL:
    case SQL_NUMERIC: {
        if((m_ColFmt[m_CurrItem].DecimalDigits > 0) ||
           (m_ColFmt[m_CurrItem].ColumnSize > 20)) { // It should be numeric
            SQL_NUMERIC_STRUCT v;
            outlen = xGetData(SQL_C_NUMERIC, &v, sizeof(SQL_NUMERIC_STRUCT));
            CDB_Numeric* r= new CDB_Numeric;
            if(outlen > 0) {
                xConvert2CDB_Numeric(r, v);
            }
                return r;
        }
        else { // It should be bigint
            SQLBIGINT v;
            outlen = xGetData(SQL_C_SBIGINT, &v, sizeof(SQLBIGINT));
            return (outlen <= 0) ? new CDB_BigInt() : new CDB_BigInt((Int8) v);
        }
    }

    case SQL_WLONGVARCHAR:
#ifdef HAVE_WSTRING
    {
        CDB_Text* val = new CDB_Text;

        // Code below looks strange, but it completely matches original logic.
        for(;;) {
            CheckSIENoD_WText(val);
        }
        return val;
    }
#endif

    case SQL_LONGVARCHAR: {
        CDB_Text* val = new CDB_Text;

        // Code below looks strange, but it completely matches original logic.
        for(;;) {
            CheckSIENoD_Text(val);
        }
        return val;
    }

    case SQL_LONGVARBINARY: {
        CDB_Image* val = new CDB_Image;

        // Code below looks strange, but it completely matches original logic.
        for(;;) {
            CheckSIENoD_Binary(val);
        }
        return val;
    }
    default:
        {
            string err_message = "Unsupported column type." + GetDbgInfo();
            DATABASE_DRIVER_ERROR( err_message, 430025 );
        }

    }
}


CDB_Object* CODBC_RowResult::GetItem(CDB_Object* item_buf, I_Result::EGetItem policy)
{
    if ((unsigned int) m_CurrItem >= GetDefineParams().GetNum()  ||  m_CurrItem == -1) {
        return 0;
    }

    CDB_Object* item = item_buf? x_LoadItem(policy, item_buf) : x_MakeItem();

    ++m_CurrItem;
    return item;
}


size_t CODBC_RowResult::ReadItem(void* buffer,size_t buffer_size,bool* is_null)
{
    if ((unsigned int) m_CurrItem >= GetDefineParams().GetNum()  ||  m_CurrItem == -1 ||
        buffer == 0 || buffer_size == 0) {
        return 0;
    }

    SQLLEN f = 0;

    if(is_null) *is_null= false;

    SQLSMALLINT data_type = m_ColFmt[m_CurrItem].DataType;

    while (m_HasMoreData  &&  m_LastReadData.size() < buffer_size) {
        m_HasMoreData = false;

        string next_data;
        size_t next_len = 0;

        switch(SQLGetData(GetHandle(), m_CurrItem + 1, SQL_C_BINARY, buffer, buffer_size, &f)) {
        case SQL_SUCCESS_WITH_INFO:
            switch(f) {
            case SQL_NO_TOTAL:
                next_data.append((char*) buffer, buffer_size);
                m_HasMoreData = true;
                break;
            case SQL_NULL_DATA:
                if(is_null) *is_null= true;
                break;
            default:
                if ( f < 0 ) {
                    ReportErrors();
                    return 0;
                }
                m_HasMoreData = true;
                next_len = static_cast<size_t>(f);
                if (next_len >= buffer_size) {
                    next_len = buffer_size;
                }
                next_data.append((char*) buffer, next_len);
                break;
            }
            break;
        case SQL_SUCCESS:
            if(f == SQL_NULL_DATA) {
                if(is_null) *is_null= true;
            }
            else {
                next_len = (f >= 0)? ((size_t)f) : 0;
                next_data.append((char*) buffer, next_len);
            }
            break;
        case SQL_NO_DATA:
            if(f == SQL_NULL_DATA) {
                if(is_null) *is_null= true;
            }
            break;
        case SQL_ERROR:
            ReportErrors();
            return 0;
        default:
            {
                string err_message = "SQLGetData failed." + GetDbgInfo();
                DATABASE_DRIVER_ERROR( err_message, 430026 );
            }
        }

#ifdef HAVE_WSTRING
        if (data_type == SQL_WCHAR  ||  data_type == SQL_WVARCHAR  ||  data_type == SQL_WLONGVARCHAR) {
            string conv_data
                = (CODBCString((wchar_t*) next_data.data(),
                               next_data.size() / sizeof(wchar_t))
                   .ConvertTo(GetClientEncoding()));
            m_LastReadData += conv_data;
        }
        else
#endif
        {
            m_LastReadData += next_data;
        }
    }

    size_t return_len = m_LastReadData.size();
    if (return_len > buffer_size) {
        return_len = buffer_size;
    }
    memcpy(buffer, m_LastReadData.data(), return_len);
    m_LastReadData = m_LastReadData.substr(return_len);
    if (!m_HasMoreData  &&  return_len <= buffer_size) {
        ++m_CurrItem;
        m_HasMoreData = true;
    }

    return return_len;
}


CDB_BlobDescriptor* CODBC_RowResult::GetBlobDescriptor(int item_no,
                                                       const string& cond)
{
    enum {eNameStrLen = 128};
    SQLSMALLINT slp;

    odbc::TSqlChar buffer[eNameStrLen];

    switch(SQLColAttribute(GetHandle(), item_no + 1,
                           SQL_DESC_BASE_TABLE_NAME,
                           (SQLPOINTER)buffer, sizeof(buffer),
                           &slp, 0)) {
    case SQL_SUCCESS_WITH_INFO:
        ReportErrors();
    case SQL_SUCCESS:
        break;
    case SQL_ERROR:
        ReportErrors();
        return 0;
    default:
        {
            string err_message = "SQLColAttribute failed." + GetDbgInfo();
            DATABASE_DRIVER_ERROR( err_message, 430027 );
        }
    }

    string base_table = CODBCString(buffer, GetClientEncoding()).ConvertTo(GetClientEncoding());

    switch(SQLColAttribute(GetHandle(), item_no + 1,
                           SQL_DESC_BASE_COLUMN_NAME,
                           (SQLPOINTER)buffer, sizeof(buffer),
                           &slp, 0)) {
    case SQL_SUCCESS_WITH_INFO:
        ReportErrors();
    case SQL_SUCCESS:
        break;
    case SQL_ERROR:
        ReportErrors();
        return 0;
    default:
        {
            string err_message = "SQLColAttribute failed." + GetDbgInfo();
            DATABASE_DRIVER_ERROR( err_message, 430027 );
        }
    }

    string base_column = CODBCString(buffer, GetClientEncoding()).ConvertTo(GetClientEncoding());

    SQLLEN column_type = 0;
    switch(SQLColAttribute(GetHandle(), item_no + 1,
                           SQL_COLUMN_TYPE,
                           NULL, sizeof(column_type),
                           &slp, &column_type)) {
    case SQL_SUCCESS_WITH_INFO:
        ReportErrors();
    case SQL_SUCCESS:
        break;
    case SQL_ERROR:
        ReportErrors();
        return 0;
    default:
        {
            string err_message = "SQLColAttribute failed." + GetDbgInfo();
            DATABASE_DRIVER_ERROR( err_message, 430027 );
        }
    }

    CDB_BlobDescriptor::ETDescriptorType type = CDB_BlobDescriptor::eUnknown;
    switch (column_type) {
    case SQL_BINARY:
    case SQL_VARBINARY:
    case SQL_LONGVARBINARY:
        type = CDB_BlobDescriptor::eBinary;
        break;
    case SQL_LONGVARCHAR:
        type = CDB_BlobDescriptor::eText;
        break;
    };

    return new CDB_BlobDescriptor(base_table, base_column, cond, type);
}

I_BlobDescriptor* CODBC_RowResult::GetBlobDescriptor()
{
    return (I_BlobDescriptor*) GetBlobDescriptor(m_CurrItem, "don't use me");
}

bool CODBC_RowResult::SkipItem()
{
    if ((unsigned int) m_CurrItem < GetDefineParams().GetNum()) {
        ++m_CurrItem;
        return true;
    }
    return false;
}


CODBC_RowResult::~CODBC_RowResult()
{
    try {
        if (m_ColFmt) {
            delete[] m_ColFmt;
            m_ColFmt = 0;
        }
        if (!m_EOR) {
            Close();
        }
    }
    NCBI_CATCH_ALL_X( 1, NCBI_CURRENT_FUNCTION )
}


/////////////////////////////////////////////////////////////////////////////
//
//  CTL_ParamResult::
//  CTL_StatusResult::
//  CTL_CursorResult::
//

CODBC_StatusResult::CODBC_StatusResult(CStatementBase& stmt)
: CODBC_RowResult(stmt, 1, NULL)
{
}

CODBC_StatusResult::~CODBC_StatusResult()
{
}

EDB_ResType CODBC_StatusResult::ResultType() const
{
    return eDB_StatusResult;
}

/////////////////////////////////////////////////////////////////////////////
CODBC_ParamResult::CODBC_ParamResult(
    CStatementBase& stmt,
    SQLSMALLINT nof_cols)
: CODBC_RowResult(stmt, nof_cols, NULL)
{
}

CODBC_ParamResult::~CODBC_ParamResult()
{
}

EDB_ResType CODBC_ParamResult::ResultType() const
{
    return eDB_ParamResult;
}


/////////////////////////////////////////////////////////////////////////////
//
//  CODBC_CursorResult::
//

CODBC_CursorResult::CODBC_CursorResult(CODBC_LangCmd* cmd)
: m_Cmd(cmd)
, m_Res(NULL)
, m_EOR(false)
{
    try {
        m_Cmd->Send();
        m_EOR = true;

        while (m_Cmd->HasMoreResults()) {
            m_Res = m_Cmd->Result();

            if (m_Res && m_Res->ResultType() == eDB_RowResult) {
                m_EOR = false;
                return;
            }

            if (m_Res) {
                while (m_Res->Fetch())
                    ;
                delete m_Res;
                m_Res = 0;
            }
        }
    } catch (const CDB_Exception& e) {
        string err_message = "Failed to get the results." + GetDbgInfo();
        DATABASE_DRIVER_ERROR_EX( e, err_message, 422010 );
    }
}


EDB_ResType CODBC_CursorResult::ResultType() const
{
    return eDB_CursorResult;
}


const CDBParams& CODBC_CursorResult::GetDefineParams(void) const
{
    _ASSERT(m_Res);
    return m_Res->GetDefineParams();
}


bool CODBC_CursorResult::Fetch()
{

    if( m_EOR ) {
        return false;
    }

    try {
        if (m_Res && m_Res->Fetch()) {
            return true;
        }
    } catch ( const CDB_Exception& ) {
        delete m_Res;
        m_Res = 0;
    }

    try {
        // finish this command
        m_EOR = true;
        if( m_Res ) {
            delete m_Res;
            m_Res = 0;
            while (m_Cmd->HasMoreResults()) {
                m_Res = m_Cmd->Result();
                if (m_Res) {
                    while (m_Res->Fetch()) {
                        continue;
                    }
                    delete m_Res;
                    m_Res = 0;
                }
            }
        }
    } catch (const CDB_Exception& e) {
        string err_message = "Failed to fetch the results." + GetDbgInfo();
        DATABASE_DRIVER_ERROR_EX( e, err_message, 422011 );
    }
    return false;
}


int CODBC_CursorResult::CurrentItemNo() const
{
    return m_Res ? m_Res->CurrentItemNo() : -1;
}


int CODBC_CursorResult::GetColumnNum(void) const
{
    return m_Res ? m_Res->GetColumnNum() : -1;
}


CDB_Object* CODBC_CursorResult::GetItem(CDB_Object* item_buff, I_Result::EGetItem policy)
{
    return m_Res ? m_Res->GetItem(item_buff, policy) : 0;
}


size_t CODBC_CursorResult::ReadItem(void* buffer, size_t buffer_size,
                                   bool* is_null)
{
    if (m_Res) {
        return m_Res->ReadItem(buffer, buffer_size, is_null);
    }
    if (is_null)
        *is_null = true;
    return 0;
}


NCBI_SUSPEND_DEPRECATION_WARNINGS
I_BlobDescriptor* CODBC_CursorResult::GetBlobDescriptor()
{
    return m_Res ? m_Res->GetBlobDescriptor() : 0;
}
NCBI_RESUME_DEPRECATION_WARNINGS


bool CODBC_CursorResult::SkipItem()
{
    return m_Res ? m_Res->SkipItem() : false;
}


CODBC_CursorResult::~CODBC_CursorResult()
{
    try {
        if (m_Res) {
            delete m_Res;
            m_Res = 0;
        }
    }
    NCBI_CATCH_ALL_X( 2, NCBI_CURRENT_FUNCTION )
}


///////////////////////////////////////////////////////////////////////////////
CODBC_CursorResultExpl::CODBC_CursorResultExpl(CODBC_LangCmd* cmd) :
    CODBC_CursorResult(cmd)
{
}

CODBC_CursorResultExpl::~CODBC_CursorResultExpl(void)
{
}


bool CODBC_CursorResultExpl::Fetch(void)
{

    if( m_EOR ) {
        return false;
    }

    try {
        if (m_Res && m_Res->Fetch()) {
            return true;
        }
    } catch ( const CDB_Exception& ) {
        delete m_Res;
        m_Res = 0;
    }

    try {
        // finish this command
        m_EOR = true;
        if( m_Res ) {
            delete m_Res;
            m_Res = 0;
            while (m_Cmd->HasMoreResults()) {
                m_Res = m_Cmd->Result();
                if (m_Res) {
                    while (m_Res->Fetch()) {
                        continue;
                    }
                    delete m_Res;
                    m_Res = 0;
                }
            }
        }

        // send the another "fetch cursor_name" command
        m_Cmd->Send();
        while (m_Cmd->HasMoreResults()) {
            m_Res = m_Cmd->Result();
            if (m_Res && m_Res->ResultType() == eDB_RowResult) {
                m_EOR = false;
                return m_Res->Fetch();
            }
            if ( m_Res ) {
                while (m_Res->Fetch()) {
                    continue;
                }
                delete m_Res;
                m_Res = 0;
            }
        }
    } catch (const CDB_Exception& e) {
        string err_message = "Failed to fetch the results." + GetDbgInfo();
        DATABASE_DRIVER_ERROR_EX( e, err_message, 422011 );
    }
    return false;
}


END_NCBI_SCOPE


