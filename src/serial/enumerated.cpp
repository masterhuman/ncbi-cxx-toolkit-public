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
* Author: Eugene Vasilchenko
*
* File Description:
*   !!! PUT YOUR DESCRIPTION HERE !!!
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.10  2000/07/03 18:42:43  vasilche
* Added interface to typeinfo via CObjectInfo and CConstObjectInfo.
* Reduced header dependency.
*
* Revision 1.9  2000/06/07 19:45:58  vasilche
* Some code cleaning.
* Macros renaming in more clear way.
* BEGIN_NAMED_*_INFO, ADD_*_MEMBER, ADD_NAMED_*_MEMBER.
*
* Revision 1.8  2000/06/01 19:07:02  vasilche
* Added parsing of XML data.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiutil.hpp>
#include <serial/enumvalues.hpp>
#include <serial/enumerated.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>

BEGIN_NCBI_SCOPE

CEnumeratedTypeValues::CEnumeratedTypeValues(const string& name,
                                             bool isInteger)
    : m_Name(name), m_Integer(isInteger)
{
}

CEnumeratedTypeValues::CEnumeratedTypeValues(const char* name,
                                             bool isInteger)
    : m_Name(name), m_Integer(isInteger)
{
}

CEnumeratedTypeValues::~CEnumeratedTypeValues(void)
{
}

long CEnumeratedTypeValues::FindValue(const CLightString& name) const
{
    const TNameToValue& m = NameToValue();
    TNameToValue::const_iterator i = m.find(name);
    if ( i == m.end() ) {
        THROW1_TRACE(runtime_error,
                     "invalid value of enumerated type");
    }
    return i->second;
}

const string& CEnumeratedTypeValues::FindName(long value,
                                              bool allowBadValue) const
{
    const TValueToName& m = ValueToName();
    TValueToName::const_iterator i = m.find(value);
    if ( i == m.end() ) {
        if ( allowBadValue ) {
            return NcbiEmptyString;
        }
        else {
            THROW1_TRACE(runtime_error,
                         "invalid value of enumerated type");
        }
    }
    return *i->second;
}

void CEnumeratedTypeValues::AddValue(const string& name, long value)
{
    if ( name.empty() )
        THROW1_TRACE(runtime_error, "empty enum value name");
    m_Values.push_back(make_pair(name, value));
    m_ValueToName.reset(0);
    m_NameToValue.reset(0);
}

const CEnumeratedTypeValues::TValueToName&
CEnumeratedTypeValues::ValueToName(void) const
{
    TValueToName* m = m_ValueToName.get();
    if ( !m ) {
        m_ValueToName.reset(m = new TValueToName);
        iterate ( TValues, i, m_Values ) {
            (*m)[i->second] = &i->first;
        }
    }
    return *m;
}

const CEnumeratedTypeValues::TNameToValue&
CEnumeratedTypeValues::NameToValue(void) const
{
    TNameToValue* m = m_NameToValue.get();
    if ( !m ) {
        m_NameToValue.reset(m = new TNameToValue);
        iterate ( TValues, i, m_Values ) {
            const string& s = i->first;
            pair<TNameToValue::iterator, bool> p =
                m->insert(TNameToValue::value_type(s, i->second));
            if ( !p.second ) {
                THROW1_TRACE(runtime_error, "duplicated enum value name");
            }
        }
    }
    return *m;
}

void CEnumeratedTypeValues::AddValue(const char* name, long value)
{
    AddValue(string(name), value);
}

CEnumeratedTypeInfo::CEnumeratedTypeInfo(const CEnumeratedTypeValues* values,
                                         size_t size)
    : CParent(values->GetName()),
      m_ValueType(CPrimitiveTypeInfo::GetIntegerTypeInfo(size)),
      m_Values(*values)
{
    _ASSERT(m_ValueType->GetValueType() == eInteger);
}

CEnumeratedTypeInfo::EValueType CEnumeratedTypeInfo::GetValueType(void) const
{
    return eEnum;
}

size_t CEnumeratedTypeInfo::GetSize(void) const
{
    return m_ValueType->GetSize();
}

TObjectPtr CEnumeratedTypeInfo::Create(void) const
{
    return m_ValueType->Create();
}
    
bool CEnumeratedTypeInfo::IsDefault(TConstObjectPtr object) const
{
    return m_ValueType->IsDefault(object);
}

bool CEnumeratedTypeInfo::Equals(TConstObjectPtr object1,
                                 TConstObjectPtr object2) const
{
    return m_ValueType->Equals(object1, object2);
}

void CEnumeratedTypeInfo::SetDefault(TObjectPtr dst) const
{
    m_ValueType->SetDefault(dst);
}

void CEnumeratedTypeInfo::Assign(TObjectPtr dst, TConstObjectPtr src) const
{
    m_ValueType->Assign(dst, src);
}

bool CEnumeratedTypeInfo::IsSigned(void) const
{
    return m_ValueType->IsSigned();
}

long CEnumeratedTypeInfo::GetValueLong(TConstObjectPtr objectPtr) const
{
    return m_ValueType->GetValueLong(objectPtr);
}

unsigned long CEnumeratedTypeInfo::GetValueULong(TConstObjectPtr objectPtr) const
{
    return m_ValueType->GetValueULong(objectPtr);
}

void CEnumeratedTypeInfo::SetValueLong(TObjectPtr objectPtr, long value) const
{
    Values().FindName(value, Values().IsInteger());
    m_ValueType->SetValueLong(objectPtr, value);
}

void CEnumeratedTypeInfo::SetValueULong(TObjectPtr objectPtr,
                                        unsigned long value) const
{
    if ( long(value) < 0 )
        THROW1_TRACE(runtime_error, "overflow error");
    Values().FindName(value, Values().IsInteger());
    m_ValueType->SetValueULong(objectPtr, value);
}

void CEnumeratedTypeInfo::GetValueString(TConstObjectPtr objectPtr,
                                         string& value) const
{
    value = Values().FindName(m_ValueType->GetValueLong(objectPtr), false);
}

void CEnumeratedTypeInfo::SetValueString(TObjectPtr objectPtr,
                                         const string& value) const
{
    m_ValueType->SetValueLong(objectPtr, Values().FindValue(value));
}

void CEnumeratedTypeInfo::SkipData(CObjectIStream& in) const
{
    if ( !in.ReadEnum(Values()).second ) {
        // plain integer
        m_ValueType->SkipData(in);
    }
}

void CEnumeratedTypeInfo::ReadData(CObjectIStream& in, TObjectPtr object) const
{
    pair<long, bool> value = in.ReadEnum(Values());
    if ( value.second ) {
        // value already read
        m_ValueType->SetValueLong(object, value.first);
    }
    else {
        // plain integer
        m_ValueType->ReadData(in, object);
    }
}

void CEnumeratedTypeInfo::WriteData(CObjectOStream& out, TConstObjectPtr object) const
{
    if ( !out.WriteEnum(Values(), m_ValueType->GetValueLong(object)) ) {
        // plain integer
        m_ValueType->WriteData(out, object);
    }
}

END_NCBI_SCOPE
