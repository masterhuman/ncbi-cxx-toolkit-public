#ifndef _RS_IMPL_HPP_
#define _RS_IMPL_HPP_

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
* File Name:  $Id$
*
* Author:  Michael Kholodov
*   
* File Description:  Resultset implementation
*
*
* $Log$
* Revision 1.5  2002/07/08 16:08:19  kholodov
* Modified: moved initialization code to Init() method
*
* Revision 1.4  2002/06/11 16:22:53  kholodov
* Fixed the incorrect declaration of GetMetaData() method
*
* Revision 1.3  2002/05/14 19:53:17  kholodov
* Modified: Read() returns 0 to signal end of column
*
* Revision 1.2  2002/04/05 19:29:50  kholodov
* Modified: GetBlobIStream() returns one and the same object, which is created
* on the first call.
* Added: GetVariant(const string& colName) to retrieve column value by
* column name
*
* Revision 1.1  2002/01/30 14:51:23  kholodov
* User DBAPI implementation, first commit
*
*
*
*/

//#include <vector>
#include <dbapi/dbapi.hpp>

#include "active_obj.hpp"
#include "array.hpp"
#include "dbexception.hpp"
#include "blobstream.hpp"

BEGIN_NCBI_SCOPE

class CResultSet : public CActiveObject, 
                   public IEventListener,
                   public IResultSet
{
public:
    CResultSet(class CConnection* conn, CDB_Result *rs);

    virtual ~CResultSet();
  
    void Init();
    virtual EDB_ResType GetResultType();

    virtual bool Next();

    virtual const CVariant& GetVariant(unsigned int idx);
    virtual const CVariant& GetVariant(const string& colName);
  
    virtual size_t Read(void* buf, size_t size);
    virtual void Close();
    virtual const IResultSetMetaData* GetMetaData();
    virtual istream& GetBlobIStream(size_t buf_size);
    virtual ostream& GetBlobOStream(size_t blob_size, 
                                    size_t buf_size);

    // Interface IEventListener implementation
    virtual void Action(const CDbapiEvent& e);


private:
    
    int GetColNum(const string& name);
    
    class CConnection* m_conn;
    CDB_Result *m_rs;
    //CResultSetMetaDataImpl *m_metaData;
    CDynArray<CVariant> m_data;
    CBlobIStream *m_istr;
    CBlobOStream *m_ostr;
    int m_column;

};

//====================================================================
inline
const CVariant& CResultSet::GetVariant(unsigned int idx) 
{
    return m_data[idx-1];
}

inline
const CVariant& CResultSet::GetVariant(const string& name) 
{
    return m_data[GetColNum(name)];
}

END_NCBI_SCOPE

#endif // _RS_IMPL_HPP_
