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
* Author:  Mati Shomrat, NCBI
*
* File Description:
*   Contig item for flat-file
*
*/
#include <corelib/ncbistd.hpp>

#include <objects/seq/Seqdesc.hpp>
#include <objects/seqblock/GB_block.hpp>
#include <objmgr/seqdesc_ci.hpp>

#include <objtools/format/formatter.hpp>
#include <objtools/format/text_ostream.hpp>
#include <objtools/format/items/origin_item.hpp>
#include <objtools/format/context.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


COriginItem::COriginItem(CFFContext& ctx) :
    CFlatItem(ctx), m_Origin(kEmptyStr)
{
    x_GatherInfo(ctx);
}


void COriginItem::Format
(IFormatter& formatter,
 IFlatTextOStream& text_os) const
{
    formatter.FormatOrigin(*this, text_os);
}


void COriginItem::x_GatherInfo(CFFContext& ctx)
{
    CSeqdesc_CI gb(ctx.GetHandle(), CSeqdesc::e_Genbank);
    if ( gb ) {
        const CGB_block& gbb = gb->GetGenbank();
        if ( gbb.CanGetOrigin() ) {
            x_SetObject(*gb);
            m_Origin = gbb.GetOrigin();
        }
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
* Revision 1.1  2004/02/19 18:15:55  shomrat
* Initial Revision
*
*
* ===========================================================================
*/
