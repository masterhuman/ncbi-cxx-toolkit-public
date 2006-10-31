
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
* Author: Andrei Gourianov
*
* File Description:
*   DTD parser's auxiliary stuff:
*       DTDEntity
*       DTDAttribute
*       DTDElement
*       DTDEntityLexer
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <serial/datatool/dtdaux.hpp>

BEGIN_NCBI_SCOPE

/////////////////////////////////////////////////////////////////////////////
// DTDEntity

DTDEntity::DTDEntity(void)
{
    m_External = false;
}
DTDEntity::DTDEntity(const DTDEntity& other)
{
    m_Name = other.m_Name;
    m_Data = other.m_Data;
    m_External = other.m_External;
}
DTDEntity::~DTDEntity(void)
{
}

void DTDEntity::SetName(const string& name)
{
    m_Name = name;
}
const string& DTDEntity::GetName(void) const
{
    return m_Name;
}

void DTDEntity::SetData(const string& data)
{
    m_Data = data;
}
const string& DTDEntity::GetData(void) const
{
    return m_Data;
}

void DTDEntity::SetExternal(void)
{
    m_External = true;
}
bool DTDEntity::IsExternal(void) const
{
    return m_External;
}

/////////////////////////////////////////////////////////////////////////////
// DTDAttribute

DTDAttribute::DTDAttribute(void)
{
    m_SourceLine = 0;
    m_Type = eUnknown;
    m_ValueType = eImplied;
}
DTDAttribute::DTDAttribute(const DTDAttribute& other)
{
    m_SourceLine= other.m_SourceLine;
    m_Name      = other.m_Name;
    m_TypeName  = other.m_TypeName;
    m_Type      = other.m_Type;
    m_ValueType = other.m_ValueType;
    m_Value     = other.m_Value;
    m_ListEnum  = other.m_ListEnum;
    m_ValueSourceLine = other.m_ValueSourceLine;
    m_Comments  = other.m_Comments;
}
DTDAttribute::~DTDAttribute(void)
{
}

DTDAttribute& DTDAttribute::operator= (const DTDAttribute& other)
{
    m_SourceLine= other.m_SourceLine;
    m_Name      = other.m_Name;
    m_TypeName  = other.m_TypeName;
    m_Type      = other.m_Type;
    m_ValueType = other.m_ValueType;
    m_Value     = other.m_Value;
    m_ListEnum  = other.m_ListEnum;
    m_ValueSourceLine = other.m_ValueSourceLine;
    m_Comments  = other.m_Comments;
    return *this;
}

void DTDAttribute::Merge(const DTDAttribute& other)
{
    m_Name      = other.m_Name;
    m_TypeName  = other.m_TypeName;
    m_Type      = other.m_Type;
    if (m_ValueType == eDefault) {
        m_ValueType = other.m_ValueType;
    }
    m_Value     = other.m_Value;
    m_ListEnum  = other.m_ListEnum;
    m_ValueSourceLine = other.m_ValueSourceLine;
}

void DTDAttribute::SetSourceLine(int line)
{
    m_SourceLine = line;
}

int DTDAttribute::GetSourceLine(void) const
{
    return m_SourceLine;
}

void DTDAttribute::SetName(const string& name)
{
    m_Name = name;
}
const string& DTDAttribute::GetName(void) const
{
    return m_Name;
}

void DTDAttribute::SetType(EType type)
{
    m_Type = type;
}
DTDAttribute::EType DTDAttribute::GetType(void) const
{
    return m_Type;
}

void DTDAttribute::SetTypeName( const string& name)
{
    m_TypeName = name;
}

const string& DTDAttribute::GetTypeName( void) const
{
    return m_TypeName;
}

void DTDAttribute::SetValueType(EValueType valueType)
{
    m_ValueType = valueType;
}
DTDAttribute::EValueType DTDAttribute::GetValueType(void) const
{
    return m_ValueType;
}

void DTDAttribute::SetValue(const string& value)
{
    m_Value = value;
}
const string& DTDAttribute::GetValue(void) const
{
    return m_Value;
}

void DTDAttribute::AddEnumValue(const string& value, int line)
{
    m_ListEnum.push_back(value);
    m_ValueSourceLine[value] = line;
}
const list<string>& DTDAttribute::GetEnumValues(void) const
{
    return m_ListEnum;
}

int DTDAttribute::GetEnumValueSourceLine(const string& value) const
{
    if (m_ValueSourceLine.find(value) != m_ValueSourceLine.end()) {
        return m_ValueSourceLine.find(value)->second;
    }
    return 0;
}

/////////////////////////////////////////////////////////////////////////////
// DTDElement

DTDElement::DTDElement(void)
{
    m_SourceLine = 0;
    m_Type = eUnknown;
    m_Occ  = eOne;
    m_Refd = false;
    m_Embd = false;
    m_Named= false;
}

DTDElement::DTDElement(const DTDElement& other)
{
    m_SourceLine = other.m_SourceLine;
    m_Name     = other.m_Name;
    m_TypeName = other.m_TypeName;
    m_NamespaceName = other.m_NamespaceName;
    m_Type     = other.eUnknown;
    m_Occ      = other.m_Occ;
    m_Refd     = other.m_Refd;
    m_Embd     = other.m_Embd;
    m_Refs     = other.m_Refs;
    m_RefOcc   = other.m_RefOcc;
    m_Attrib   = other.m_Attrib;
    m_Named    = other.m_Named;
    m_Comments = other.m_Comments;
    m_AttribComments = other.m_AttribComments;
}

DTDElement::~DTDElement(void)
{
}

void DTDElement::SetSourceLine(int line)
{
    m_SourceLine = line;
}
int DTDElement::GetSourceLine(void) const
{
    return m_SourceLine;
}

void DTDElement::SetName(const string& name)
{
    m_Name = name;
}
const string& DTDElement::GetName(void) const
{
    return m_Name;
}
void DTDElement::SetNamed(bool named)
{
    m_Named = named;
}
bool DTDElement::IsNamed(void) const
{
    return m_Named;
}

void DTDElement::SetType( EType type)
{
    _ASSERT(m_Type == eUnknown || m_Type == type);
    m_Type = type;
}

void DTDElement::SetTypeIfUnknown( EType type)
{
    if (m_Type == eUnknown) {
        m_Type = type;
    }
}

DTDElement::EType DTDElement::GetType(void) const
{
    return (EType)m_Type;
}

void DTDElement::SetTypeName( const string& name)
{
    m_TypeName = name;
}
const string& DTDElement::GetTypeName( void) const
{
    return m_TypeName;
}


void DTDElement::SetOccurrence( const string& ref_name, EOccurrence occ)
{
    m_RefOcc[ref_name] = occ;
}
DTDElement::EOccurrence DTDElement::GetOccurrence(
    const string& ref_name) const
{
    map<string,EOccurrence>::const_iterator i = m_RefOcc.find(ref_name);
    return (i != m_RefOcc.end()) ? i->second : eOne;
}


void DTDElement::SetOccurrence( EOccurrence occ)
{
    m_Occ = occ;
}
DTDElement::EOccurrence DTDElement::GetOccurrence(void) const
{
    return m_Occ;
}


void DTDElement::AddContent( const string& ref_name)
{
    m_Refs.push_back( ref_name);
}

const list<string>& DTDElement::GetContent(void) const
{
    return m_Refs;
}


void DTDElement::SetReferenced(void)
{
    m_Refd = true;
}
bool DTDElement::IsReferenced(void) const
{
    return m_Refd;
}


void DTDElement::SetEmbedded(void)
{
    m_Embd = true;
}
bool DTDElement::IsEmbedded(void) const
{
    return m_Embd;
}
string DTDElement::CreateEmbeddedName(int depth) const
{
    string name, tmp;
    list<string>::const_iterator i;
    for ( i = m_Refs.begin(); i != m_Refs.end(); ++i) {
        tmp = i->substr(0,depth);
        tmp[0] = toupper((unsigned char) tmp[0]);
        name += tmp;
    }
    return name;
}

void DTDElement::AddAttribute(DTDAttribute& attrib)
{
    m_Attrib.push_back(attrib);
}
bool DTDElement::HasAttributes(void) const
{
    return !m_Attrib.empty();
}
const list<DTDAttribute>& DTDElement::GetAttributes(void) const
{
    return m_Attrib;
}
list<DTDAttribute>& DTDElement::GetNonconstAttributes(void)
{
    return m_Attrib;
}

void DTDElement::SetNamespaceName(const string& name)
{
    m_NamespaceName = name;
}

const string& DTDElement::GetNamespaceName(void) const
{
    return m_NamespaceName;
}


/////////////////////////////////////////////////////////////////////////////
// DTDEntityLexer

DTDEntityLexer::DTDEntityLexer(CNcbiIstream& in, const string& name, bool autoDelete)
    : DTDLexer(in,name)
{
    m_Str = &in;
    m_AutoDelete = autoDelete;
}
DTDEntityLexer::~DTDEntityLexer(void)
{
    if (m_AutoDelete) {
        delete m_Str;
    }
}

/////////////////////////////////////////////////////////////////////////////
// XSDEntityLexer

XSDEntityLexer::XSDEntityLexer(CNcbiIstream& in, const string& name, bool autoDelete)
    : XSDLexer(in,name)
{
    m_Str = &in;
    m_AutoDelete = autoDelete;
}
XSDEntityLexer::~XSDEntityLexer(void)
{
    if (m_AutoDelete) {
        delete m_Str;
    }
}


END_NCBI_SCOPE


/*
 * ==========================================================================
 * $Log$
 * Revision 1.11  2006/10/31 20:01:33  gouriano
 * Added data spec source line info
 *
 * Revision 1.10  2006/07/24 18:57:39  gouriano
 * Preserve comments when parsing DTD
 *
 * Revision 1.9  2006/06/05 15:33:14  gouriano
 * Implemented local elements when parsing XML schema
 *
 * Revision 1.8  2006/05/23 18:24:48  gouriano
 * Make XML attributes optional by default
 *
 * Revision 1.7  2006/05/09 15:16:43  gouriano
 * Added XML namespace definition possibility
 *
 * Revision 1.6  2006/05/03 14:38:08  gouriano
 * Added parsing attribute definition and include
 *
 * Revision 1.5  2006/04/20 14:00:11  gouriano
 * Added XML schema parsing
 *
 * Revision 1.4  2005/06/03 17:05:33  lavr
 * Explicit (unsigned char) casts in ctype routines
 *
 * Revision 1.3  2005/01/06 20:22:14  gouriano
 * Added name property to lexers - for better diagnostics
 *
 * Revision 1.2  2004/05/17 21:03:13  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.1  2002/11/14 21:02:15  gouriano
 * auxiliary classes to use by DTD parser
 *
 *
 *
 * ==========================================================================
 */
