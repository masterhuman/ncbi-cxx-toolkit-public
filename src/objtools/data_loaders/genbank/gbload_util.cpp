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
*  Author: Michael Kimelman
*
*  File Description: GenBank Data loader
*
*/

#include <corelib/ncbistd.hpp>
#include <objects/objmgr/impl/handle_range.hpp>
#include <objects/objmgr/gbloader.hpp>
#include "gbload_util.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

//===============================================================================
// Support Classes
//

/////////////////////////////////////////////////////////////////////////////////
//
// CTimer 

CTimer::CTimer() :
  m_RequestsDevider(0), m_Requests(0)
{
  m_ReasonableRefreshDelay = 0;
  m_LastCalibrated = m_Time= time(0);
};

time_t
CTimer::Time()
{
  if(--m_Requests>0)
    return m_Time;
  m_RequestsLock.Lock();
  if(m_Requests<=0)
    {
      time_t x = time(0);
      if(x==m_Time) {
        m_Requests += m_RequestsDevider + 1;
        m_RequestsDevider = m_RequestsDevider*2 + 1;
      } else {
        m_Requests = m_RequestsDevider / ( x - m_Time );
        m_Time=x;
      }
    }
  m_RequestsLock.Unlock();
  return m_Time;
};  

void
CTimer::Start()
{
  m_TimerLock.Lock();
  m_StartTime = Time();
};

void
CTimer::Stop()
{
  time_t x = Time() - m_StartTime; // test request timing in seconds
  m_ReasonableRefreshDelay = 60 /*sec*/ * 
    (x==0 ? 5 /*min*/ : x*50 /* 50 min per sec of test request*/);
  m_LastCalibrated = m_Time;
  m_TimerLock.Unlock();
};

time_t
CTimer::RetryTime()
{
  return Time() + (m_ReasonableRefreshDelay>0?m_ReasonableRefreshDelay:24*60*60); /* 24 hours */
};

bool
CTimer::NeedCalibration()
{
  return
    (m_ReasonableRefreshDelay==0) ||
    (m_Time-m_LastCalibrated>100*m_ReasonableRefreshDelay);
};

/* =========================================================================== */
// MutexPool
//

#if defined(_REENTRANT)
CMutexPool::CMutexPool()
{
  m_size =0;
  m_Locks=0;
  spread =0;
}

void
CMutexPool::SetSize(int size)
{
  _VERIFY(m_size==0 && !m_Locks);
  m_size=size;
  m_Locks = new CMutex[m_size];
  spread  = new int[m_size];
  for(int i=0;i<m_size;++i) spread[i]=0;
}

CMutexPool::~CMutexPool(void)
{
  if(m_Locks) delete [] m_Locks;
  if(spread)  {
    for(int i=0;i<m_size;++i) GBLOG_POST("PoolMutex " << i << " used "<< spread[i] << " times");
    delete [] spread;
  }
}
#endif

/* =========================================================================== */
// CGBLGuard 
//
CGBLGuard::CGBLGuard(TLMutex& lm,EState orig,const char *loc,int select)
  : m_Locks(&lm),m_Loc(loc),m_orig(orig),m_current(orig),m_select(select)
{
}

CGBLGuard::CGBLGuard(TLMutex &lm,const char *loc) // assume orig=eNone, switch to e.Main in constructor
  : m_Locks(&lm),m_Loc(loc),m_orig(eNone),m_current(eNone),m_select(-1)
{
  Switch(eMain);
}

CGBLGuard::CGBLGuard(CGBLGuard &g,const char *loc)
  : m_Locks(g.m_Locks),m_Loc(g.m_Loc),m_orig(g.m_current),m_current(g.m_current),m_select(g.m_select)
{
  if(loc) m_Loc = loc;
  _VERIFY(m_Locks);
}

CGBLGuard::~CGBLGuard()
{
  Switch(m_orig);
}

#if defined(_REENTRANT)
void CGBLGuard::Select(int s)
{
  if(m_current==eMain) m_select=s;
  _VERIFY(m_select==s);
}

//#define LOCK_POST(x) GBLOG_POST(x) 
#define LOCK_POST(x) 
void CGBLGuard::MLock()
{
  LOCK_POST(&m_Locks << ":: MainLock tried   @ " << m_Loc);
  m_Locks->m_Lookup.Lock();
  LOCK_POST(&m_Locks << ":: MainLock locked  @ " << m_Loc);
}

void CGBLGuard::MUnlock()
{
  LOCK_POST(&m_Locks << ":: MainLock unlocked@ " << m_Loc);
  m_Locks->m_Lookup.Unlock();
}

void CGBLGuard::PLock()
{
  _VERIFY(m_select>=0);
  LOCK_POST(&m_Locks << ":: Pool["<< setw(2) << m_select << "] tried   @ " << m_Loc);
  m_Locks->m_Pool.GetMutex(m_select).Lock();
  LOCK_POST(&m_Locks << ":: Pool["<< setw(2) << m_select << "] locked  @ " << m_Loc);
}

void CGBLGuard::PUnlock()
{
  _VERIFY(m_select>=0);
   LOCK_POST(&m_Locks << ":: Pool["<< setw(2) << m_select << "] unlocked@ " << m_Loc);
  m_Locks->m_Pool.GetMutex(m_select).Unlock();
}

void CGBLGuard::Switch(EState newstate)
{
  if(newstate==m_current) return;
  switch(newstate)
    {
    case eNone:
      if(m_current!=eMain) Switch(eMain);
      _ASSERT(m_current==eMain);
      //LOCK_POST(&m_Locks << ":: switch 'main' to 'none'");
      MUnlock();
      m_current=eNone;
      return;
      
    case eBoth:
      if(m_current!=eMain) Switch(eMain);
      _ASSERT(m_current==eMain);
      //LOCK_POST(&m_Locks << ":: switch 'main' to 'both'");
      if(m_Locks->m_SlowTraverseMode>0) PLock();
      m_current=eBoth;
      return;
      
    case eLocal:
      if(m_current!=eBoth) Switch(eBoth);
      _ASSERT(m_current==eBoth);
      //LOCK_POST(&m_Locks << ":: switch 'both' to 'local'");
      if(m_Locks->m_SlowTraverseMode==0) PLock();
      try {
        m_Locks->m_SlowTraverseMode++;
        MUnlock();
      } catch(...) {
        m_Locks->m_SlowTraverseMode--;
        if(m_Locks->m_SlowTraverseMode==0) PUnlock();
        throw;
      }
      m_current=eLocal;
      return;
    case eMain:
      switch(m_current)
        {
        case eNone:
          m_select=-1;
          //LOCK_POST(&m_Locks << ":: switch 'none' to 'main'");
          MLock();
          m_current=eMain;
          return;
        case eBoth:
          //LOCK_POST(&m_Locks << ":: switch 'both' to 'main'");
          if(m_Locks->m_SlowTraverseMode>0) PUnlock();
          m_select=-1;
          m_current=eMain;
          return;
        case eLocal:
          //LOCK_POST(&m_Locks << ":: switch 'local' to 'none2main'");
          PUnlock();
          m_current=eNoneToMain;
        case eNoneToMain:
          //LOCK_POST(&m_Locks << ":: switch 'none2main' to 'main'");
          MLock();
          m_Locks->m_SlowTraverseMode--;
          m_select=-1;
          m_current=eMain;
          return;
        default:
          break;
      }
    default:
      break;
    }
  runtime_error("CGBLGuard::Switch - state desynchronized");
}
#endif // if(_REENTRANT)	

END_SCOPE(objects)
END_NCBI_SCOPE

/* ---------------------------------------------------------------------------
* $Log$
* Revision 1.12  2003/03/01 22:26:56  kimelman
* performance fixes
*
* Revision 1.11  2003/02/05 17:59:17  dicuccio
* Moved formerly private headers into include/objects/objmgr/impl
*
* Revision 1.10  2002/07/22 22:53:24  kimelman
* exception handling fixed: 2level mutexing moved to Guard class + added
* handling of confidential data.
*
* Revision 1.9  2002/05/06 03:28:47  vakatov
* OM/OM1 renaming
*
* Revision 1.8  2002/05/03 21:28:09  ucko
* Introduce T(Signed)SeqPos.
*
* Revision 1.7  2002/04/04 01:35:35  kimelman
* more MT tests
*
* Revision 1.6  2002/04/02 16:02:30  kimelman
* MT testing
*
* Revision 1.5  2002/03/29 02:47:03  kimelman
* gbloader: MT scalability fixes
*
* Revision 1.4  2002/03/27 20:23:49  butanaev
* Added connection pool.
*
* Revision 1.3  2002/03/20 21:24:59  gouriano
* *** empty log message ***
*
* Revision 1.2  2002/03/20 17:03:24  gouriano
* minor changes to make it compilable on MS Windows
*
* Revision 1.1  2002/03/20 04:50:13  kimelman
* GB loader added
*
* ===========================================================================
*/
