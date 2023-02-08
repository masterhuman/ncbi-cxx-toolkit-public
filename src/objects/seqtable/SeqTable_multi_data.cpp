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
 * Author:  .......
 *
 * File Description:
 *   .......
 *
 * Remark:
 *   This code was originally generated by application DATATOOL
 *   using the following specifications:
 *   'seqtable.asn'.
 */

// standard includes
#include <ncbi_pch.hpp>

// generated includes
#include <objects/seqtable/SeqTable_multi_data.hpp>
#include <objects/seqtable/CommonString_table.hpp>
#include <objects/seqtable/CommonBytes_table.hpp>
#include <objects/seqtable/Seq_table.hpp>
#include <serial/objhook.hpp>
#include <corelib/ncbi_param.hpp>

#include <util/bitset/ncbi_bitset.hpp>
#include <objects/seqtable/seq_table_exception.hpp>
#include <cmath>

#include <objects/seqtable/impl/delta_cache.hpp>

// generated classes

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

// constructor
CSeqTable_multi_data::CSeqTable_multi_data(void)
{
}


// destructor
CSeqTable_multi_data::~CSeqTable_multi_data(void)
{
}


DEFINE_STATIC_MUTEX(sx_PrepareMutex_multi_data);


void CSeqTable_multi_data::x_ResetCache(void)
{
    m_Cache.Reset();
}


CIntDeltaSumCache& CSeqTable_multi_data::x_GetIntDeltaCache(void) const
{
    CIntDeltaSumCache* info = m_Cache.GetNCPointerOrNull();
    if ( !info ) {
        if ( GetInt_delta().GetIntSize() <= sizeof(Int4) ) {
            info = new CInt4DeltaSumCache(GetInt_delta().GetSize());
        }
        else {
            info = new CInt8DeltaSumCache(GetInt_delta().GetSize());
        }
        m_Cache = info;
    }
    return *info;
}


/////////////////////////////////////////////////////////////////////////////
// CIntDeltaSumCache
/////////////////////////////////////////////////////////////////////////////

CIntDeltaSumCache::~CIntDeltaSumCache(void)
{
}


Int4 CIntDeltaSumCache::GetDeltaSum4(const TDeltas& deltas,
                                     size_t index)
{
    Int8 v8 = GetDeltaSum8(deltas, index);
    Int4 v = Int4(v8);
    if ( v != v8 ) {
        NCBI_THROW(CSeqTableException, eIncompatibleValueType,
                   "CIntDeltaSumCache::GetDeltaSum4(): "
                   "Int8 value doesn't fit in Int4");
    }
    return v;
}


Int8 CIntDeltaSumCache::GetDeltaSum8(const TDeltas& deltas,
                                     size_t index)
{
    return GetDeltaSum4(deltas, index);
}


#define USE_DELTA_CACHE

/////////////////////////////////////////////////////////////////////////////
// CInt4DeltaSumCache
/////////////////////////////////////////////////////////////////////////////

const size_t CInt4DeltaSumCache::kBlockSize = 128;


CInt4DeltaSumCache::CInt4DeltaSumCache(size_t size)
    : m_Blocks(new TValue[(size+kBlockSize-1)/kBlockSize]),
      m_BlocksFilled(0),
#ifdef USE_DELTA_CACHE
      m_CacheBlockInfo(new TValue[kBlockSize]),
#endif
      m_CacheBlockIndex(size_t(0)-1)
{
}


CInt4DeltaSumCache::~CInt4DeltaSumCache(void)
{
}


inline
Int4 CInt4DeltaSumCache::x_GetDeltaSum2(const TDeltas& deltas,
                                        size_t block_index,
                                        size_t block_offset)
{
#ifdef USE_DELTA_CACHE
    _ASSERT(block_index <= m_BlocksFilled);
    if ( block_index != m_CacheBlockIndex ) {
        size_t size = deltas.GetSize();
        size_t block_pos = block_index*kBlockSize;
        _ASSERT(block_pos < size);
        size_t block_size = min(kBlockSize, size-block_pos);
        _ASSERT(block_offset < block_size);
        TValue sum = block_index == 0? 0: m_Blocks[block_index-1];
        for ( size_t i = 0; i < block_size; ++i ) {
            TValue v;
            if ( deltas.TryGetValue(block_pos+i, v) ) {
                sum += v;
            }
            m_CacheBlockInfo[i] = sum;
        }
        m_CacheBlockIndex = block_index;
        if ( block_index == m_BlocksFilled ) {
            m_Blocks[block_index] = sum;
            m_BlocksFilled = block_index+1;
        }
    }
    return m_CacheBlockInfo[block_offset];
#else
    size_t size = deltas.GetSize();
    size_t block_pos = block_index*kBlockSize;
    _ASSERT(block_pos < size);
    size_t block_size = min(kBlockSize, size-block_pos);
    _ASSERT(block_offset < block_size);
    TValue sum = block_index == 0? 0: m_Blocks[block_index-1];
    for ( size_t i = 0; i <= block_offset; ++i ) {
        TValue v;
        if ( deltas.TryGetValue(block_pos+i, v) ) {
            sum += v;
        }
    }
    if ( block_index == m_BlocksFilled ) {
        TValue sum2 = sum;
        for ( size_t i = block_offset+1; i < block_size; ++i ) {
            TValue v;
            if ( deltas.TryGetValue(block_pos+i, v) ) {
                sum2 += v;
            }
        }
        m_Blocks[block_index] = sum2;
        m_BlocksFilled = block_index+1;
    }
    return sum;
#endif
}


Int4 CInt4DeltaSumCache::GetDeltaSum4(const TDeltas& deltas,
                                      size_t index)
{
    _ASSERT(index < deltas.GetSize());
    size_t block_index  = index / kBlockSize;
    size_t block_offset = index % kBlockSize;
    while ( block_index >= m_BlocksFilled ) {
        x_GetDeltaSum2(deltas, m_BlocksFilled, 0);
    }
    return x_GetDeltaSum2(deltas, block_index, block_offset);
}


/////////////////////////////////////////////////////////////////////////////
// CInt8DeltaSumCache
/////////////////////////////////////////////////////////////////////////////

const size_t CInt8DeltaSumCache::kBlockSize = 128;


CInt8DeltaSumCache::CInt8DeltaSumCache(size_t size)
    : m_Blocks(new TValue[(size+kBlockSize-1)/kBlockSize]),
      m_BlocksFilled(0),
#ifdef USE_DELTA_CACHE
      m_CacheBlockInfo(new TValue[kBlockSize]),
#endif
      m_CacheBlockIndex(size_t(0)-1)
{
}


CInt8DeltaSumCache::~CInt8DeltaSumCache(void)
{
}


inline
Int8 CInt8DeltaSumCache::x_GetDeltaSum2(const TDeltas& deltas,
                                        size_t block_index,
                                        size_t block_offset)
{
#ifdef USE_DELTA_CACHE
    _ASSERT(block_index <= m_BlocksFilled);
    if ( block_index != m_CacheBlockIndex ) {
        size_t size = deltas.GetSize();
        size_t block_pos = block_index*kBlockSize;
        _ASSERT(block_pos < size);
        size_t block_size = min(kBlockSize, size-block_pos);
        _ASSERT(block_offset < block_size);
        TValue sum = block_index == 0? 0: m_Blocks[block_index-1];
        for ( size_t i = 0; i < block_size; ++i ) {
            TValue v;
            if ( deltas.TryGetValue(block_pos+i, v) ) {
                sum += v;
            }
            m_CacheBlockInfo[i] = sum;
        }
        m_CacheBlockIndex = block_index;
        if ( block_index == m_BlocksFilled ) {
            m_Blocks[block_index] = sum;
            m_BlocksFilled = block_index+1;
        }
    }
    return m_CacheBlockInfo[block_offset];
#else
    size_t size = deltas.GetSize();
    size_t block_pos = block_index*kBlockSize;
    _ASSERT(block_pos < size);
    size_t block_size = min(kBlockSize, size-block_pos);
    _ASSERT(block_offset < block_size);
    TValue sum = block_index == 0? 0: m_Blocks[block_index-1];
    for ( size_t i = 0; i <= block_offset; ++i ) {
        TValue v;
        if ( deltas.TryGetValue(block_pos+i, v) ) {
            sum += v;
        }
    }
    if ( block_index == m_BlocksFilled ) {
        TValue sum2 = sum;
        for ( size_t i = block_offset+1; i < block_size; ++i ) {
            TValue v;
            if ( deltas.TryGetValue(block_pos+i, v) ) {
                sum2 += v;
            }
        }
        m_Blocks[block_index] = sum2;
        m_BlocksFilled = block_index+1;
    }
    return sum;
#endif
}


Int8 CInt8DeltaSumCache::GetDeltaSum8(const TDeltas& deltas,
                                      size_t index)
{
    _ASSERT(index < deltas.GetSize());
    size_t block_index  = index / kBlockSize;
    size_t block_offset = index % kBlockSize;
    while ( block_index >= m_BlocksFilled ) {
        x_GetDeltaSum2(deltas, m_BlocksFilled, 0);
    }
    return x_GetDeltaSum2(deltas, block_index, block_offset);
}


/////////////////////////////////////////////////////////////////////////////


CSeqTable_multi_data::E_Choice CSeqTable_multi_data::GetValueType(void) const
{
    switch ( Which() ) {
    case e_Bit_bvector:
        return e_Bit;
    case e_Int1:
    case e_Int2:
        return e_Int;
    case e_Int_delta:
        return GetInt_delta().GetValueType();
    case e_Int_scaled:
        return GetInt_scaled().GetIntSize() <= sizeof(Int4)? e_Int: e_Int8;
    case e_Common_string:
        return e_String;
    case e_Common_bytes:
        return e_Bytes;
    default:
        return Which();
    }
}


bool CSeqTable_multi_data::CanGetInt(void) const
{
    switch ( GetValueType() ) {
    case e_Bit:
    case e_Int:
    case e_Int8:
        return true;
    default:
        return false;
    }
}


bool CSeqTable_multi_data::CanGetReal(void) const
{
    switch ( GetValueType() ) {
    case e_Bit:
    case e_Int:
    case e_Int8:
    case e_Real:
        return true;
    default:
        return false;
    }
}


size_t CSeqTable_multi_data::GetIntSize(void) const
{
    switch ( Which() ) {
    case e_Int:
        return sizeof(TInt::value_type);
    case e_Int1:
        return sizeof(TInt1::value_type);
    case e_Int2:
        return sizeof(TInt2::value_type);
    case e_Int8:
        return sizeof(TInt8::value_type);
    case e_Bit:
    case e_Bit_bvector: 
        return 1; // one char is enough to store bit value
    case e_Int_delta:
        return GetInt_delta().GetIntSize();
    case e_Int_scaled:
        return GetInt_scaled().GetIntSize();
    default:
        return 0;
    }
}


size_t CSeqTable_multi_data::GetSize(void) const
{
    switch ( Which() ) {
    case e_Int:
        return GetInt().size();
    case e_Int1:
        return GetInt1().size();
    case e_Int2:
        return GetInt2().size();
    case e_Int8:
        return GetInt8().size();
    case e_Real:
        return GetReal().size();
    case e_String:
        return GetString().size();
    case e_Bytes:
        return GetBytes().size();
    case e_Common_string:
        return GetCommon_string().GetIndexes().size();
    case e_Common_bytes:
        return GetCommon_bytes().GetIndexes().size();
    case e_Bit:
        return GetBit().size()*8;
    case e_Loc:
        return GetLoc().size();
    case e_Id:
        return GetId().size();
    case e_Interval:
        return GetInterval().size();
    case e_Bit_bvector: 
        return GetBit_bvector().GetSize();
    case e_Int_delta:
        return GetInt_delta().GetSize();
    case e_Int_scaled:
        return GetInt_scaled().GetData().GetSize();
    case e_Real_scaled:
        return GetReal_scaled().GetData().GetSize();
    default:
        break;
    }
    return 0;
}


template<class DstInt>
static inline
bool sx_DownCastInt8(DstInt& v, const Int8& value, const char* type_name)
{
    v = DstInt(value);
    if ( Int8(v) != value ) {
        NCBI_THROW_FMT(CSeqTableException, eIncompatibleValueType,
                       "CSeqTable_multi_data::TryGet"<<type_name<<"(): "
                       "value is too big for requested type: "<<value);
    }
    return true;
}


template<class DstInt>
static inline
bool sx_Round(DstInt& v, double value, const char* cast_error)
{
    // Round and check range.
    // Function round() is not everywhere available,
    // so we are using either floor() or ceil() depending on value sign.
    bool range_error;
    static constexpr double kMaxPlusOne
        = ((numeric_limits<DstInt>::max() - 1) / 2 + 1) * 2.0;
    if ( value > 0 ) {
        value = floor(value + .5);
        range_error = value >= kMaxPlusOne;
    }
    else {
        value = ceil(value-.5);
        range_error = value < numeric_limits<DstInt>::min();
    }
    if ( range_error ) {
        // range check failed
        NCBI_THROW(CSeqTableException, eIncompatibleValueType, cast_error);
    }
    v = DstInt(value);
    return true;
}


template<class Arr, class Row, class Int>
static inline
bool sx_TryGet(const Arr& arr, Row row, Int& v)
{
    if ( row >= arr.size() ) {
        return false;
    }
    v = arr[row];
    return true;
}


bool CSeqTable_multi_data::TryGetBool(size_t row, bool& v) const
{
    Int8 value;
    return x_TryGetInt8(row, value, "Bool") &&
        sx_DownCastInt8(v, value, "Bool");
}


bool CSeqTable_multi_data::TryGetInt1(size_t row, Int1& v) const
{
    Int8 value;
    return x_TryGetInt8(row, value, "Int1") &&
        sx_DownCastInt8(v, value, "Int1");
}


bool CSeqTable_multi_data::TryGetInt2(size_t row, Int2& v) const
{
    Int8 value;
    return x_TryGetInt8(row, value, "Int2") &&
        sx_DownCastInt8(v, value, "Int2");
}


bool CSeqTable_multi_data::TryGetInt4(size_t row, Int4& v) const
{
    Int8 value;
    return x_TryGetInt8(row, value, "Int4") &&
        sx_DownCastInt8(v, value, "Int4");
}


bool CSeqTable_multi_data::x_TryGetInt8(size_t row, Int8& v,
                                        const char* type_name) const
{
    switch ( Which() ) {
    case e_Bit:
    {
        const TBit& bits = GetBit();
        size_t i = row/8;
        if ( i >= bits.size() ) {
            return false;
        }
        size_t j = row%8;
        Uint1 bb = bits[i];
        v = ((bb<<j)&0x80) != 0;
        return true;
    }
    case e_Bit_bvector:
        return sx_TryGet(GetBit_bvector().GetBitVector(),
                         static_cast<bm::bvector<>::size_type>(row), v);
    case e_Int1:
        return sx_TryGet(GetInt1(), row, v);
    case e_Int2:
        return sx_TryGet(GetInt2(), row, v);
    case e_Int:
        return sx_TryGet(GetInt(), row, v);
    case e_Int8:
        return sx_TryGet(GetInt8(), row, v);
    case e_Int_delta:
    {
        const CSeqTable_multi_data& deltas = GetInt_delta();
        if ( row >= deltas.GetSize() ) {
            return false;
        }
        CMutexGuard guard(sx_PrepareMutex_multi_data);
        v = x_GetIntDeltaCache().GetDeltaSum8(deltas, row);
        return true;
    }
    case e_Int_scaled:
        return GetInt_scaled().TryGetInt8(row, v);
    default:
        NCBI_THROW_FMT(CSeqTableException, eIncompatibleValueType,
                       "CSeqTable_multi_data::TryGet"<<type_name<<"(): "
                       "value cannot be converted to requested type");
    }
}


bool CSeqTable_multi_data::TryGetInt8(size_t row, Int8& v) const
{
    return x_TryGetInt8(row, v, "Int8");
}


bool CSeqTable_multi_data::TryGetReal(size_t row, double& v) const
{
    switch ( Which() ) {
    case e_Real:
        return sx_TryGet(GetReal(), row, v);
    case e_Real_scaled:
        return GetReal_scaled().TryGetReal(row, v);
    default:
        Int8 value_int8;
        if ( !x_TryGetInt8(row, value_int8, "Real") ) {
            return false;
        }
        v = double(value_int8);
        return true;
    }
}


const string* CSeqTable_multi_data::GetStringPtr(size_t row) const
{
    if ( IsString() ) {
        const CSeqTable_multi_data::TString& arr = GetString();
        if ( row < arr.size() ) {
            return &arr[row];
        }
    }
    else if ( IsCommon_string() ) {
        const CCommonString_table& common = GetCommon_string();
        const CCommonString_table::TIndexes& indexes = common.GetIndexes();
        if ( row < indexes.size() ) {
            const CCommonString_table::TStrings& arr = common.GetStrings();
            size_t arr_index = indexes[row];
            if ( arr_index < arr.size() ) {
                return &arr[arr_index];
            }
        }
    }
    else {
        NCBI_THROW(CSeqTableException, eIncompatibleValueType,
                   "CSeqTable_multi_data::GetStringPtr() "
                   "data cannot be converted to string");
    }
    return 0;
}


bool CSeqTable_multi_data::TryGetInt1WithRounding(size_t row, Int1& v) const
{
    if ( GetValueType() == e_Real ) {
        double value;
        return TryGetReal(row, value) &&
            sx_Round(v, value,
                     "CSeqTable_multi_data::TryGetInt1WithRounding(): "
                     "real value doesn't fit in Int1");
    }
    return TryGetInt1(row, v);
}


bool CSeqTable_multi_data::TryGetInt2WithRounding(size_t row, Int2& v) const
{
    if ( GetValueType() == e_Real ) {
        double value;
        return TryGetReal(row, value) &&
            sx_Round(v, value,
                     "CSeqTable_multi_data::TryGetInt2WithRounding(): "
                     "real value doesn't fit in Int2");
    }
    return TryGetInt2(row, v);
}


bool CSeqTable_multi_data::TryGetInt4WithRounding(size_t row, int& v) const
{
    if ( GetValueType() == e_Real ) {
        double value;
        return TryGetReal(row, value) &&
            sx_Round(v, value,
                     "CSeqTable_multi_data::TryGetInt4WithRounding(): "
                     "real value doesn't fit in Int4");
    }
    return TryGetInt4(row, v);
}


bool CSeqTable_multi_data::TryGetInt8WithRounding(size_t row, Int8& v) const
{
    if ( GetValueType() == e_Real ) {
        double value;
        return TryGetReal(row, value) &&
            sx_Round(v, value,
                     "CSeqTable_multi_data::TryGetInt8WithRounding(): "
                     "real value doesn't fit in Int8");
    }
    return TryGetInt8(row, v);
}


const vector<char>* CSeqTable_multi_data::GetBytesPtr(size_t row) const
{
    if ( IsBytes() ) {
        const CSeqTable_multi_data::TBytes& arr = GetBytes();
        if ( row < arr.size() ) {
            return arr[row];
        }
    }
    else if ( IsCommon_bytes() ) {
        const CCommonBytes_table& common = GetCommon_bytes();
        const CCommonBytes_table::TIndexes& indexes = common.GetIndexes();
        if ( row < indexes.size() ) {
            const CCommonBytes_table::TBytes& arr = common.GetBytes();
            size_t arr_index = indexes[row];
            if ( arr_index < arr.size() ) {
                return arr[arr_index];
            }
        }
    }
    else {
        NCBI_THROW(CSeqTableException, eIncompatibleValueType,
                   "CSeqTable_multi_data::GetBytesPtr() "
                   "data cannot be converted to OCTET STRING");
    }
    return 0;
}


NCBI_PARAM_DECL(bool, OBJECTS, SEQ_TABLE_RESERVE);
NCBI_PARAM_DEF_EX(bool, OBJECTS, SEQ_TABLE_RESERVE, true,
                  eParam_NoThread, OBJECTS_SEQ_TABLE_RESERVE);

void CSeqTable_multi_data::CReserveHook::PreReadChoiceVariant(
    CObjectIStream& in,
    const CObjectInfoCV& variant)
{
    static CSafeStatic<NCBI_PARAM_TYPE(OBJECTS, SEQ_TABLE_RESERVE)> s_Reserve;

    if ( !s_Reserve->Get() ) {
        return;
    }
    if ( CSeq_table* table = CType<CSeq_table>::GetParent(in, 5, 2) ) {
        size_t size = table->GetNum_rows();
        CSeqTable_multi_data* data =
            CType<CSeqTable_multi_data>::Get(variant.GetChoiceObject());
        try {
            switch ( variant.GetVariantIndex() ) {
            case e_Int:
                data->SetInt().reserve(size);
                break;
            case e_Int1:
                data->SetInt1().reserve(size);
                break;
            case e_Int2:
                data->SetInt2().reserve(size);
                break;
            case e_Int8:
                data->SetInt8().reserve(size);
                break;
            case e_Real:
                data->SetReal().reserve(size);
                break;
            case e_String:
                data->SetString().reserve(size);
                break;
            case e_Bytes:
                data->SetBytes().reserve(size);
                break;
            case e_Common_string:
                data->SetCommon_string().SetIndexes().reserve(size);
                break;
            case e_Common_bytes:
                data->SetCommon_bytes().SetIndexes().reserve(size);
                break;
            case e_Bit:
                data->SetBit().reserve((size+7)/8);
                break;
            case e_Loc:
                data->SetLoc().reserve(size);
                break;
            case e_Id:
                data->SetId().reserve(size);
                break;
            case e_Interval:
                data->SetInterval().reserve(size);
                break;
            default:
                break;
            }
        }
        catch ( bad_alloc& /*ignored*/ ) {
            // ignore insufficient memory exception from advisory reserve()
        }
    }
}


void CSeqTable_multi_data::ChangeTo(E_Choice type)
{
    if ( Which() == type ) {
        return;
    }
    switch ( type ) {
    case e_Int:
        ChangeToInt();
        break;
    case e_Int1:
        ChangeToInt1();
        break;
    case e_Int2:
        ChangeToInt2();
        break;
    case e_Int8:
        ChangeToInt8();
        break;
    case e_Real:
        ChangeToReal();
        break;
    case e_String:
        ChangeToString();
        break;
    case e_Common_string:
        ChangeToCommon_string();
        break;
    case e_Bytes:
        ChangeToBytes();
        break;
    case e_Common_bytes:
        ChangeToCommon_bytes();
        break;
    case e_Bit:
        ChangeToBit();
        break;
    case e_Int_delta:
        ChangeToInt_delta();
        break;
    case e_Bit_bvector:
        ChangeToBit_bvector();
        break;
    case e_Int_scaled: // scaling requires extra parameters
        NCBI_THROW(CSeqTableException, eIncompatibleValueType,
                   "CSeqTable_multi_data::ChangeTo(e_Int_scaled): "
                   "scaling parameters are unknown");
    case e_Real_scaled: // scaling requires extra parameters
        NCBI_THROW(CSeqTableException, eIncompatibleValueType,
                   "CSeqTable_multi_data::ChangeTo(e_Real_scaled): "
                   "scaling parameters are unknown");
    default:
        NCBI_THROW(CSeqTableException, eIncompatibleValueType,
                   "CSeqTable_multi_data::ChangeTo(): "
                   "requested multi-data type is invalid");
    }
}


void CSeqTable_multi_data::ChangeToString(const string* omitted_value)
{
    if ( IsString() ) {
        return;
    }
    if ( IsCommon_string() ) {
        const CCommonString_table& common = GetCommon_string();
        const CCommonString_table::TIndexes& indexes = common.GetIndexes();
        size_t size = indexes.size();
        TString arr;
        arr.reserve(size);
        const CCommonString_table::TStrings& src = common.GetStrings();
        ITERATE ( CCommonString_table::TIndexes, it, indexes ) {
            size_t index = *it;
            if ( index < src.size() ) {
                arr.push_back(src[index]);
            }
            else if ( omitted_value ) {
                arr.push_back(*omitted_value);
            }
            else {
                NCBI_THROW(CSeqTableException, eIncompatibleValueType,
                           "CSeqTable_multi_data::ChangeToString(): "
                           "common string table is sparse");
            }
        }
        swap(SetString(), arr);
        return;
    }
    NCBI_THROW(CSeqTableException, eIncompatibleValueType,
               "CSeqTable_multi_data::ChangeToString(): "
               "requested mult-data type is invalid");
}


void CSeqTable_multi_data::ChangeToCommon_string(const string* omit_value)
{
    if ( IsCommon_string() ) {
        return;
    }
    if ( IsString() ) {
        CRef<CCommonString_table> common(new CCommonString_table);
        CCommonString_table::TIndexes& indexes = common->SetIndexes();
        CCommonString_table::TStrings& arr = common->SetStrings();
        const TString& src = GetString();
        indexes.reserve(src.size());
        typedef map<string, CCommonString_table::TIndexes::value_type>
            TIndexMap;
        TIndexMap index_map;
        if ( omit_value ) {
            index_map[*omit_value] = -1;
        }
        ITERATE ( TString, it, src ) {
            const string& key = *it;
            TIndexMap::iterator iter = index_map.lower_bound(key);
            if ( iter == index_map.end() || iter->first != key ) {
                iter = index_map.insert(iter, TIndexMap::value_type(key, arr.size()));
                arr.push_back(key);
            }
            indexes.push_back(iter->second);
        }
        SetCommon_string(*common);
        return;
    }
    NCBI_THROW(CSeqTableException, eIncompatibleValueType,
               "CSeqTable_multi_data::ChangeToCommon_string(): "
               "requested mult-data type is invalid");
}


void CSeqTable_multi_data::ChangeToBytes(const TBytesValue* omitted_value)
{
    if ( IsBytes() ) {
        return;
    }
    if ( IsCommon_bytes() ) {
        const CCommonBytes_table& common = GetCommon_bytes();
        const CCommonBytes_table::TIndexes& indexes = common.GetIndexes();
        size_t size = indexes.size();
        TBytes arr;
        arr.reserve(size);
        const CCommonBytes_table::TBytes& src = common.GetBytes();
        ITERATE ( CCommonBytes_table::TIndexes, it, indexes ) {
            size_t index = *it;
            const TBytesValue* value;
            if ( index < src.size() ) {
                value = src[index];
            }
            else if ( omitted_value ) {
                value = omitted_value;
            }
            else {
                NCBI_THROW(CSeqTableException, eIncompatibleValueType,
                           "CSeqTable_multi_data::ChangeToBytes(): "
                           "common bytes table is sparse");
            }
            arr.push_back(new TBytesValue(*value));
        }
        swap(SetBytes(), arr);
        return;
    }
    NCBI_THROW(CSeqTableException, eIncompatibleValueType,
               "CSeqTable_multi_data::ChangeToBytes(): "
               "requested mult-data type is invalid");
}


void CSeqTable_multi_data::ChangeToCommon_bytes(const TBytesValue* omit_value)
{
    if ( IsCommon_bytes() ) {
        return;
    }
    if ( IsBytes() ) {
        CRef<CCommonBytes_table> common(new CCommonBytes_table);
        CCommonBytes_table::TIndexes& indexes = common->SetIndexes();
        CCommonBytes_table::TBytes& arr = common->SetBytes();
        const TBytes& src = GetBytes();
        indexes.reserve(src.size());
        typedef map<const TBytesValue*,
                    CCommonBytes_table::TIndexes::value_type,
                    PPtrLess<const TBytesValue*> > TIndexMap;
        TIndexMap index_map;
        if ( omit_value ) {
            index_map[omit_value] = -1;
        }
        ITERATE ( TBytes, it, src ) {
            const TBytesValue* key = *it;
            TIndexMap::iterator iter = index_map.lower_bound(key);
            if ( iter == index_map.end() || *iter->first != *key ) {
                iter = index_map.insert(iter, TIndexMap::value_type(key, arr.size()));
                arr.push_back(new TBytesValue(*key));
            }
            indexes.push_back(iter->second);
        }
        SetCommon_bytes(*common);
        return;
    }
    NCBI_THROW(CSeqTableException, eIncompatibleValueType,
               "CSeqTable_multi_data::ChangeToBytes(): "
               "requested mult-data type is invalid");
}


void CSeqTable_multi_data::ChangeToReal_scaled(double mul, double add)
{
    if ( IsReal_scaled() ) {
        return;
    }
    TReal arr;
    if ( IsReal() ) {
        // in-place
        swap(arr, SetReal());
        NON_CONST_ITERATE ( TReal, it, arr ) {
            TReal::value_type value = *it;
            value -= add;
            *it = value/mul;
        }
    }
    else {
        for ( size_t row = 0; ; ++row ) {
            TReal::value_type value;
            if ( !TryGetReal(row, value) ) {
                break;
            }
            value -= add;
            arr.push_back(value/mul);
        }
    }
    swap(SetReal_scaled().SetData().SetReal(), arr);
}


void CSeqTable_multi_data::ChangeToInt_scaled(int mul, int add)
{
    if ( IsInt_scaled() ) {
        return;
    }
    int min_v = 0, max_v = 0;
    if ( IsInt() ) {
        // in-place
        TInt arr;
        swap(arr, SetInt());
        NON_CONST_ITERATE ( TInt, it, arr ) {
            TInt::value_type value = *it;
            value -= add;
            if ( value % mul != 0 ) {
                // restore already scaled values
                while ( it != arr.begin() ) {
                    TInt::value_type value = *--it;
                    *it = value*mul+add;
                }
                swap(arr, SetInt());
                NCBI_THROW(CSeqTableException, eIncompatibleValueType,
                           "CSeqTable_multi_data::ChangeToInt_scaled(): "
                           "value is not round for scaling");
            }
            value /= mul;
            if ( value < min_v ) {
                min_v = value;
            }
            else if ( value > max_v ) {
                max_v = value;
            }
            *it = value;
        }
        swap(SetInt_scaled().SetData().SetInt(), arr);
    }
    else if ( GetIntSize() <= sizeof(Int4) ) {
        TInt arr;
        for ( size_t row = 0; ; ++row ) {
            TInt::value_type value;
            if ( !TryGetInt(row, value) ) {
                break;
            }
            value -= add;
            if ( value % mul != 0 ) {
                NCBI_THROW(CSeqTableException, eIncompatibleValueType,
                           "CSeqTable_multi_data::ChangeToInt_scaled(): "
                           "value is not round for scaling");
            }
            value /= mul;
            if ( value > max_v ) {
                max_v = value;
            }
            else if ( value < min_v ) {
                min_v = value;
            }
            arr.push_back(value);
        }
        swap(SetInt_scaled().SetData().SetInt(), arr);
    }
    else { // Int8 -> scaled
        TInt8 arr;
        Int8 min_v8 = 0, max_v8 = 0;
        for ( size_t row = 0; ; ++row ) {
            Int8 value;
            if ( !TryGetInt8(row, value) ) {
                break;
            }
            value -= add;
            if ( value % mul != 0 ) {
                NCBI_THROW(CSeqTableException, eIncompatibleValueType,
                           "CSeqTable_multi_data::ChangeToInt_scaled(): "
                           "value is not round for scaling");
            }
            value /= mul;
            if ( value > max_v8 ) {
                max_v8 = value;
            }
            else if ( value < min_v8 ) {
                min_v8 = value;
            }
            arr.push_back(value);
        }
        CScaled_int_multi_data& scaled = SetInt_scaled();
        swap(scaled.SetData().SetInt8(), arr);
        bool is_int8 = false;
        if ( min_v < kMin_Int ) {
            scaled.SetMin(min_v);
            is_int8 = true;
        }
        if ( max_v > kMax_Int ) {
            scaled.SetMax(max_v);
            is_int8 = true;
        }
        if ( is_int8 ) {
            return;
        }
        min_v = int(min_v8);
        max_v = int(max_v8);
    }
    // compact scaled data
    if ( min_v == 0 && max_v <= 1 ) {
        SetInt_scaled().SetData().ChangeToBit();
    }
    else if ( min_v >= kMin_I1 && max_v <= kMax_I1 ) {
        SetInt_scaled().SetData().ChangeToInt1();
    }
    else if ( min_v >= kMin_I2 && max_v <= kMax_I2 ) {
        SetInt_scaled().SetData().ChangeToInt2();
    }
    else {
        SetInt_scaled().SetData().ChangeToInt4();
    }
}


void CSeqTable_multi_data::ChangeToInt_delta(void)
{
    if ( IsInt_delta() ) {
        return;
    }
    TInt arr;
    if ( IsInt() ) {
        // in-place
        swap(arr, SetInt());
        TInt::value_type prev_value = 0;
        NON_CONST_ITERATE ( TInt, it, arr ) {
            TInt::value_type value = *it;
            *it = value - prev_value;
            prev_value = value;
        }
    }
    else {
        TInt::value_type prev_value = 0;
        for ( size_t row = 0; ; ++row ) {
            TInt::value_type value;
            if ( !TryGetInt(row, value) ) {
                break;
            }
            arr.push_back(value-prev_value);
            prev_value = value;
        }
    }
    Reset();
    swap(SetInt_delta().SetInt(), arr);
}


void CSeqTable_multi_data::ChangeToInt4(void)
{
    if ( IsInt() ) {
        return;
    }
    if ( IsInt_delta() ) {
        TInt arr;
        size_t size = GetSize();
        arr.reserve(size);
        for ( size_t row = 0; row < size; ++row ) {
            int value;
            if ( !TryGetInt(row, value) ) {
                break;
            }
            arr.push_back(value);
        }
        SetInt().swap(arr);
        return;
    }
    TInt arr;
    for ( size_t row = 0; ; ++row ) {
        TInt::value_type value;
        if ( !TryGetIntWithRounding(row, value) ) {
            break;
        }
        arr.push_back(value);
    }
    Reset();
    swap(SetInt(), arr);
}


void CSeqTable_multi_data::ChangeToInt1(void)
{
    if ( IsInt1() ) {
        return;
    }
    TInt1 arr;
    for ( size_t row = 0; ; ++row ) {
        Int1 value;
        if ( !TryGetInt1WithRounding(row, value) ) {
            break;
        }
        arr.push_back(value);
    }
    Reset();
    swap(SetInt1(), arr);
}


void CSeqTable_multi_data::ChangeToInt2(void)
{
    if ( IsInt2() ) {
        return;
    }
    TInt2 arr;
    for ( size_t row = 0; ; ++row ) {
        Int2 value;
        if ( !TryGetInt2WithRounding(row, value) ) {
            break;
        }
        arr.push_back(value);
    }
    Reset();
    swap(SetInt2(), arr);
}


void CSeqTable_multi_data::ChangeToInt8(void)
{
    if ( IsInt8() ) {
        return;
    }
    TInt8 arr;
    for ( size_t row = 0; ; ++row ) {
        Int8 value;
        if ( !TryGetInt8WithRounding(row, value) ) {
            break;
        }
        arr.push_back(value);
    }
    Reset();
    swap(SetInt8(), arr);
}


void CSeqTable_multi_data::ChangeToReal(void)
{
    if ( IsReal() ) {
        return;
    }
    TReal arr;
    for ( size_t row = 0; ; ++row ) {
        double value;
        if ( !TryGetReal(row, value) ) {
            break;
        }
        arr.push_back(TInt::value_type(value));
    }
    Reset();
    swap(SetReal(), arr);
}


void CSeqTable_multi_data::ChangeToBit(void)
{
    if ( IsBit() ) {
        return;
    }
    TBit arr;
    if ( IsBit_bvector() ) {
        const bm::bvector<>& bv = GetBit_bvector().GetBitVector();
        arr.reserve((bv.size()+7)/8);
        if ( bv.any() ) {
            size_t last_byte_index = 0;
            Uint1 last_byte = 0;
            bm::id_t i = bv.get_first();
            do {
                size_t byte_index = i / 8;
                if ( byte_index != last_byte_index ) {
                    arr.resize(last_byte_index);
                    arr.push_back(last_byte);
                    last_byte_index = byte_index;
                    last_byte = 0;
                }
                size_t bit_index = i % 8;
                last_byte |= 0x80 >> bit_index;
            } while ( (i=bv.get_next(i)) );
            if ( last_byte ) {
                arr.resize(last_byte_index);
                arr.push_back(last_byte);
            }
        }
        arr.resize((bv.size()+7)/8);
    }
    else if ( CanGetInt() ) {
        size_t size = GetSize();
        arr.resize((size+7)/8);
        for ( size_t i = 0; i < size; ++i ) {
            int v;
            if ( !TryGetInt(i, v) ) {
                NCBI_THROW(CSeqTableException, eIncompatibleValueType,
                           "CSeqTable_multi_data::ChangeToBit(): "
                           "multi-data value cannot be converted to int");
            }
            if ( v < 0 || v > 1 ) {
                NCBI_THROW(CSeqTableException, eIncompatibleValueType,
                           "CSeqTable_multi_data::ChangeToBit(): "
                           "multi-data value is not 0 or 1");
            }
            if ( v ) {
                arr[i/8] |= 0x80 >> i%8;
            }
        }
    }
    else {
        NCBI_THROW(CSeqTableException, eIncompatibleValueType,
                   "CSeqTable_multi_data::ChangeToBit(): "
                   "requested mult-data type is invalid");
    }
    Reset();
    swap(SetBit(), arr);
}


void CSeqTable_multi_data::ChangeToBit_bvector(void)
{
    if ( IsBit_bvector() ) {
        return;
    }
    typedef bm::bvector<>::size_type TBVSize;
    TBVSize size = static_cast<TBVSize>(GetSize());
    AutoPtr<bm::bvector<> > bv(new bm::bvector<>(size));
    if ( IsBit() ) {
        const TBit& src = GetBit();
        for ( TBVSize i = 0; i < size; i += 8 ) {
            for ( Uint1 b = src[i/8], j = 0; b & 0xff; ++j, b <<= 1 ) {
                if ( b&0x80 ) {
                    bv->set_bit(i+j);
                }
            }
        }
    }
    else if ( CanGetInt() ) {
        for ( TBVSize i = 0; i < size; ++i ) {
            int v;
            if ( !TryGetInt(i, v) ) {
                NCBI_THROW(CSeqTableException, eIncompatibleValueType,
                           "CSeqTable_multi_data::ChangeToBit_bvector(): "
                           "multi-data value cannot be converted to int");
            }
            if ( v < 0 || v > 1 ) {
                NCBI_THROW(CSeqTableException, eIncompatibleValueType,
                           "CSeqTable_multi_data::ChangeToBit_bvector(): "
                           "multi-data value is not 0 or 1");
            }
            if ( v ) {
                bv->set_bit(i);
            }
        }
    }
    else {
        NCBI_THROW(CSeqTableException, eIncompatibleValueType,
                   "CSeqTable_multi_data::ChangeToBit_bvector(): "
                   "requested multi-data type is invalid");
    }
    bv->optimize();
    SetBit_bvector().SetBitVector(bv.release());
}


END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE
