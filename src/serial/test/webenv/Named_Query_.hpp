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

#ifndef NAMED_QUERY_BASE_HPP
#define NAMED_QUERY_BASE_HPP

// standard includes
#include <serial/serialbase.hpp>

// forward declarations
class CName;
class CQuery_Command;
class CTime;


// generated classes

class CNamed_Query_Base : public ncbi::CSerialObject
{
    typedef ncbi::CSerialObject Tparent;
public:
    // constructor
    CNamed_Query_Base(void);
    // destructor
    virtual ~CNamed_Query_Base(void);

    // type info
    DECLARE_INTERNAL_TYPE_INFO();

    // types
    typedef CName TName;
    typedef CTime TTime;
    typedef CQuery_Command TCommand;

    // getters
    // setters

    // mandatory
    // typedef CName TName
    bool IsSetName(void) const;
    bool CanGetName(void) const;
    void ResetName(void);
    const TName& GetName(void) const;
    void SetName(TName& value);
    TName& SetName(void);

    // mandatory
    // typedef CTime TTime
    bool IsSetTime(void) const;
    bool CanGetTime(void) const;
    void ResetTime(void);
    const TTime& GetTime(void) const;
    void SetTime(TTime& value);
    TTime& SetTime(void);

    // mandatory
    // typedef CQuery_Command TCommand
    bool IsSetCommand(void) const;
    bool CanGetCommand(void) const;
    void ResetCommand(void);
    const TCommand& GetCommand(void) const;
    void SetCommand(TCommand& value);
    TCommand& SetCommand(void);

    // reset whole object
    virtual void Reset(void);


private:
    // Prohibit copy constructor and assignment operator
    CNamed_Query_Base(const CNamed_Query_Base&);
    CNamed_Query_Base& operator=(const CNamed_Query_Base&);

    // data
    Uint4 m_set_State[1];
    ncbi::CRef< TName > m_Name;
    ncbi::CRef< TTime > m_Time;
    ncbi::CRef< TCommand > m_Command;
};






///////////////////////////////////////////////////////////
///////////////////// inline methods //////////////////////
///////////////////////////////////////////////////////////
inline
bool CNamed_Query_Base::IsSetName(void) const
{
    return m_Name.NotEmpty();
}

inline
bool CNamed_Query_Base::CanGetName(void) const
{
    return IsSetName();
}

inline
const CName& CNamed_Query_Base::GetName(void) const
{
    if (!CanGetName()) {
        ThrowUnassigned(0);
    }
    return (*m_Name);
}

inline
CName& CNamed_Query_Base::SetName(void)
{
    return (*m_Name);
}

inline
bool CNamed_Query_Base::IsSetTime(void) const
{
    return m_Time.NotEmpty();
}

inline
bool CNamed_Query_Base::CanGetTime(void) const
{
    return IsSetTime();
}

inline
const CTime& CNamed_Query_Base::GetTime(void) const
{
    if (!CanGetTime()) {
        ThrowUnassigned(1);
    }
    return (*m_Time);
}

inline
CTime& CNamed_Query_Base::SetTime(void)
{
    return (*m_Time);
}

inline
bool CNamed_Query_Base::IsSetCommand(void) const
{
    return m_Command.NotEmpty();
}

inline
bool CNamed_Query_Base::CanGetCommand(void) const
{
    return IsSetCommand();
}

inline
const CQuery_Command& CNamed_Query_Base::GetCommand(void) const
{
    if (!CanGetCommand()) {
        ThrowUnassigned(2);
    }
    return (*m_Command);
}

inline
CQuery_Command& CNamed_Query_Base::SetCommand(void)
{
    return (*m_Command);
}

///////////////////////////////////////////////////////////
////////////////// end of inline methods //////////////////
///////////////////////////////////////////////////////////






#endif // NAMED_QUERY_BASE_HPP
