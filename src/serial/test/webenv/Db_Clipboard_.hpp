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
 * File Description:
 *   This code is generated by application DATATOOL
 *   using specifications from the data definition file
 *   'twebenv.asn'.
 *
 * ATTENTION:
 *   Don't edit or check-in this file to the CVS as this file will
 *   be overridden (by DATATOOL) without warning!
 * ===========================================================================
 */

#ifndef DB_CLIPBOARD_BASE_HPP
#define DB_CLIPBOARD_BASE_HPP

// standard includes
#include <serial/serialbase.hpp>

// generated includes
#include <string>


// forward declarations
class CItem_Set;


// generated classes

class CDb_Clipboard_Base : public ncbi::CSerialObject
{
    typedef ncbi::CSerialObject Tparent;
public:
    // constructor
    CDb_Clipboard_Base(void);
    // destructor
    virtual ~CDb_Clipboard_Base(void);

    // type info
    DECLARE_INTERNAL_TYPE_INFO();

    // types
    typedef std::string TName;
    typedef int TCount;
    typedef CItem_Set TItems;

    // getters
    // setters

    // mandatory
    // typedef std::string TName
    bool IsSetName(void) const;
    bool CanGetName(void) const;
    void ResetName(void);
    const TName& GetName(void) const;
    void SetName(const TName& value);
    TName& SetName(void);

    // mandatory
    // typedef int TCount
    bool IsSetCount(void) const;
    bool CanGetCount(void) const;
    void ResetCount(void);
    TCount GetCount(void) const;
    void SetCount(const TCount& value);
    TCount& SetCount(void);

    // mandatory
    // typedef CItem_Set TItems
    bool IsSetItems(void) const;
    bool CanGetItems(void) const;
    void ResetItems(void);
    const TItems& GetItems(void) const;
    void SetItems(TItems& value);
    TItems& SetItems(void);

    // reset whole object
    virtual void Reset(void);


private:
    // Prohibit copy constructor and assignment operator
    CDb_Clipboard_Base(const CDb_Clipboard_Base&);
    CDb_Clipboard_Base& operator=(const CDb_Clipboard_Base&);

    // data
    Uint4 m_set_State[1];
    TName m_Name;
    TCount m_Count;
    ncbi::CRef< TItems > m_Items;
};






///////////////////////////////////////////////////////////
///////////////////// inline methods //////////////////////
///////////////////////////////////////////////////////////
inline
bool CDb_Clipboard_Base::IsSetName(void) const
{
    return ((m_set_State[0] & 0x3) != 0);
}

inline
bool CDb_Clipboard_Base::CanGetName(void) const
{
    return IsSetName();
}

inline
const std::string& CDb_Clipboard_Base::GetName(void) const
{
    if (!CanGetName()) {
        ThrowUnassigned(0);
    }
    return m_Name;
}

inline
void CDb_Clipboard_Base::SetName(const std::string& value)
{
    m_Name = value;
    m_set_State[0] |= 0x3;
}

inline
std::string& CDb_Clipboard_Base::SetName(void)
{
#ifdef _DEBUG
    if (!IsSetName()) {
        m_Name = ms_UnassignedStr;
    }
#endif
    m_set_State[0] |= 0x1;
    return m_Name;
}

inline
bool CDb_Clipboard_Base::IsSetCount(void) const
{
    return ((m_set_State[0] & 0xc) != 0);
}

inline
bool CDb_Clipboard_Base::CanGetCount(void) const
{
    return IsSetCount();
}

inline
void CDb_Clipboard_Base::ResetCount(void)
{
    m_Count = 0;
    m_set_State[0] &= ~0xc;
}

inline
int CDb_Clipboard_Base::GetCount(void) const
{
    if (!CanGetCount()) {
        ThrowUnassigned(1);
    }
    return m_Count;
}

inline
void CDb_Clipboard_Base::SetCount(const int& value)
{
    m_Count = value;
    m_set_State[0] |= 0xc;
}

inline
int& CDb_Clipboard_Base::SetCount(void)
{
#ifdef _DEBUG
    if (!IsSetCount()) {
        memset(&m_Count,ms_UnassignedByte,sizeof(m_Count));
    }
#endif
    m_set_State[0] |= 0x4;
    return m_Count;
}

inline
bool CDb_Clipboard_Base::IsSetItems(void) const
{
    return m_Items.NotEmpty();
}

inline
bool CDb_Clipboard_Base::CanGetItems(void) const
{
    return IsSetItems();
}

inline
const CItem_Set& CDb_Clipboard_Base::GetItems(void) const
{
    if (!CanGetItems()) {
        ThrowUnassigned(2);
    }
    return (*m_Items);
}

inline
CItem_Set& CDb_Clipboard_Base::SetItems(void)
{
    return (*m_Items);
}

///////////////////////////////////////////////////////////
////////////////// end of inline methods //////////////////
///////////////////////////////////////////////////////////






#endif // DB_CLIPBOARD_BASE_HPP
