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
 *   using specifications from the data definition file
 *   'seqalign.asn'.
 */

// standard includes
#include <objects/seqalign/seqalign_exception.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seqalign/Std_seg.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <serial/iterator.hpp>

// generated includes
#include <objects/seqalign/Seq_align.hpp>

// generated classes

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

// destructor
CSeq_align::~CSeq_align(void)
{
}


CSeq_align::TDim CSeq_align::CheckNumRows(void) const
{
    switch (GetSegs().Which()) {
    case C_Segs::e_Denseg:
        return GetSegs().GetDenseg().CheckNumRows();
    case C_Segs::e_Std:
        {
            TDim numrows = 0;
            ITERATE (C_Segs::TStd, std_i, GetSegs().GetStd()) {
                const TDim& seg_numrows = (*std_i)->CheckNumRows();
                if (numrows) {
                    if (seg_numrows != numrows) {
                        NCBI_THROW(CSeqalignException, eInvalidAlignment,
                                   "CSeq_align::CheckNumRows(): Number of rows "
                                   "is not the same for each std seg.");
                    }
                } else {
                    numrows = seg_numrows;
                }
            }
            return numrows;
        }
    default:
        NCBI_THROW(CSeqalignException, eUnsupported,
                   "CSeq_align::CheckNumRows() currently does not handle "
                   "this type of alignment");
    }
}


CRange<TSeqPos> CSeq_align::GetSeqRange(TDim row) const
{
    switch (GetSegs().Which()) {
    case C_Segs::e_Denseg:
        return GetSegs().GetDenseg().GetSeqRange(row);
    case C_Segs::e_Std:
        {
            CRange<TSeqPos> rng;
            CSeq_id seq_id;
            size_t seg_i = 0;
            ITERATE (C_Segs::TStd, std_i, GetSegs().GetStd()) {
                TDim row_i = 0;
                ITERATE (CStd_seg::TLoc, i, (*std_i)->GetLoc()) {
                    const CSeq_loc& loc = **i;
                    if (row_i++ == row) {
                        if (loc.IsInt()) {
                            if ( !seg_i ) {
                                seq_id.Assign(loc.GetInt().GetId());
                            } else if (seq_id.Compare(loc.GetInt().GetId())
                                       != CSeq_id::e_YES) {
                                NCBI_THROW(CSeqalignException,
                                           eInvalidRowNumber,
                                           "CSeq_align::GetSeqRange():"
                                           " Row seqids not consistent."
                                           " Cannot determine range.");
                            }
                            rng.CombineWith
                                (CRange<TSeqPos>
                                 (loc.GetInt().GetFrom(),
                                  loc.GetInt().GetTo()));
                        }
                    }
                }
                if (row < 0  ||  row >= row_i) {
                    NCBI_THROW(CSeqalignException, eInvalidRowNumber,
                               "CSeq_align::GetSeqRange():"
                               " Invalid row number");
                }
                if (CanGetDim()  &&  row_i != GetDim()) {
                    NCBI_THROW(CSeqalignException, eInvalidAlignment,
                               "CSeq_align::GetSeqRange():"
                               " loc.size is inconsistent with dim");
                }
                seg_i++;
            }
            if (rng.Empty()) {
                NCBI_THROW(CSeqalignException, eInvalidAlignment,
                           "CSeq_align::GetSeqRange(): Row is empty");
            }
            return rng;
        }
    default:
        NCBI_THROW(CSeqalignException, eUnsupported,
                   "CSeq_align::GetSeqRange() currently does not handle "
                   "this type of alignment.");
    }
}


TSeqPos CSeq_align::GetSeqStart(TDim row) const
{
    switch (GetSegs().Which()) {
    case C_Segs::e_Denseg:
        return GetSegs().GetDenseg().GetSeqStart(row);
    case C_Segs::e_Std:
        return GetSeqRange(row).GetFrom();
    default:
        NCBI_THROW(CSeqalignException, eUnsupported,
                   "CSeq_align::GetSeqStart() currently does not handle "
                   "this type of alignment.");
    }
}


TSeqPos CSeq_align::GetSeqStop (TDim row) const
{
    switch (GetSegs().Which()) {
    case C_Segs::e_Denseg:
        return GetSegs().GetDenseg().GetSeqStop(row);
    case C_Segs::e_Std:
        return GetSeqRange(row).GetTo();
    default:
        NCBI_THROW(CSeqalignException, eUnsupported,
                   "CSeq_align::GetSeqStop() currently does not handle "
                   "this type of alignment.");
    }
}


void CSeq_align::Validate(bool full_test) const
{
    switch (GetSegs().Which()) {
    case C_Segs::e_Denseg:
        GetSegs().GetDenseg().Validate(full_test);
        break;
    case C_Segs::e_Std:
        CheckNumRows();
        break;
    default:
        NCBI_THROW(CSeqalignException, eUnsupported,
                   "CSeq_align::Validate() currently does not handle "
                   "this type of alignment");
    }
}


///---------------------------------------------------------------------------
/// PRE : currently only implemented for dense-seg segments
/// POST: same alignment, opposite orientation
void CSeq_align::Reverse(void)
{
    switch (GetSegs().Which()) {
    case C_Segs::e_Denseg:
        SetSegs().SetDenseg().Reverse();
        break;
    default:
        NCBI_THROW(CSeqalignException, eUnsupported,
                   "CSeq_align::Reverse() currently only handles dense-seg "
                   "alignments");
    }
}

///---------------------------------------------------------------------------
/// PRE : currently only implemented for dense-seg segments; two row numbers
/// POST: same alignment, position of the two rows has been swapped
void CSeq_align::SwapRows(TDim row1, TDim row2)
{
    switch (GetSegs().Which()) {
    case C_Segs::e_Denseg:
        SetSegs().SetDenseg().SwapRows(row1, row2);
        break;
    default:
        NCBI_THROW(CSeqalignException, eUnsupported,
                   "CSeq_align::SwapRows currently only handles dense-seg "
                   "alignments");
    }
}

///----------------------------------------------------------------------------
/// PRE : the Seq-align has StdSeg segs
/// POST: Seq_align of type Dense-seg is created with m_Widths if necessary
CRef<CSeq_align> 
CSeq_align::CreateDensegFromStdseg(SSeqIdChooser* SeqIdChooser) const
{
    if ( !GetSegs().IsStd() ) {
        NCBI_THROW(CSeqalignException, eInvalidInputAlignment,
                   "CSeq_align::CreateDensegFromStdseg(): "
                   "Input Seq-align should have segs of type StdSeg!");
    }

    CRef<CSeq_align> sa(new CSeq_align);
    sa->SetType(eType_not_set);
    CDense_seg& ds = sa->SetSegs().SetDenseg();

    typedef CDense_seg::TDim    TNumrow;
    typedef CDense_seg::TNumseg TNumseg;

    vector<TSeqPos>       row_lens;
    CDense_seg::TLens&    seg_lens = ds.SetLens();
    CDense_seg::TStarts&  starts   = ds.SetStarts();
    CDense_seg::TStrands& strands  = ds.SetStrands();
    CDense_seg::TWidths&  widths   = ds.SetWidths();
    vector<bool>          widths_determined;

    TSeqPos row_len;
    TSeqPos from, to;
    int width;
    ENa_strand strand;


    TNumseg seg = 0;
    TNumrow dim = 0, row = 0;
    ITERATE (C_Segs::TStd, std_i, GetSegs().GetStd()) {

        const CStd_seg& ss = **std_i;

        seg_lens.push_back(0);
        TSeqPos& seg_len = seg_lens.back();
        row_len = 0;
        row_lens.clear();
        widths_determined.push_back(false);

        row = 0;
        const CSeq_id* seq_id;
        ITERATE (CStd_seg::TLoc, i, ss.GetLoc()) {

            // push back initialization values
            if (seg == 0) {
                widths.push_back(0);
                strands.push_back(eNa_strand_unknown);
            }

            if ((*i)->IsInt()) {
                const CSeq_interval& interval = (*i)->GetInt();
                
                // determine start and len
                from = interval.GetFrom();
                to = interval.GetTo();
                starts.push_back(from);
                row_len = to - from + 1;
                row_lens.push_back(row_len);
                
                // try to determine/check the seg_len and width
                if (!seg_len) {
                    width = 0;
                    seg_len = row_len;
                } else {
                    if (row_len * 3 == seg_len) {
                        seg_len /= 3;
                        width = 1;
                    } else if (row_len / 3 == seg_len) {
                        width = 3;
                    } else if (row_len != seg_len) {
                        NCBI_THROW(CSeqalignException, eInvalidInputAlignment,
                                   "CreateDensegFromStdseg(): "
                                   "Std-seg segment lengths not accurate!");
                    }
                }
                if (width > 0) {
                    widths_determined[seg] = true;
                    if (widths[row] > 0  &&  widths[row] != width) {
                        NCBI_THROW(CSeqalignException, eInvalidInputAlignment,
                                   "CreateDensegFromStdseg(): "
                                   "Std-seg segment lengths not accurate!");
                    } else {
                        widths[row] = width;
                    }
                }

                // get the id
                seq_id = &(*i)->GetInt().GetId();

                // determine/check the strand
                if (interval.CanGetStrand()) {
                    strand = interval.GetStrand();
                    if (seg == 0  ||  strands[row] == eNa_strand_unknown) {
                        strands[row] = strand;
                    } else {
                        if (strands[row] != strand) {
                            NCBI_THROW(CSeqalignException,
                                       eInvalidInputAlignment,
                                       "CreateDensegFromStdseg(): "
                                       "Inconsistent strands!");
                        }
                    }
                } else {
                    strand = eNa_strand_unknown;
                }

                    
            } else if ((*i)->IsEmpty()) {
                starts.push_back(-1);
                if (seg == 0) {
                    strands[row] = eNa_strand_unknown;
                }
                seq_id = &(*i)->GetEmpty();
                row_lens.push_back(0);

            }

            // determine/check the id
            if (seg == 0) {
                CRef<CSeq_id> id(new CSeq_id);
                SerialAssign(*id, *seq_id);
                ds.SetIds().push_back(id);
            } else {
                CSeq_id& id = *ds.SetIds()[row];
                if (!SerialEquals(id, *seq_id)) {
                    if (SeqIdChooser) {
                        SeqIdChooser->ChooseSeqId(id, *seq_id);
                    } else {
                        string errstr =
                            string("CreateDensegFromStdseg(): Seq-ids: ") +
                            id.AsFastaString() + " and " +
                            seq_id->AsFastaString() + " are not identical!" +
                            " Without the OM it cannot be determined if they belong to"
                            " the same sequence."
                            " Define and pass ChooseSeqId to resolve seq-ids.";
                        NCBI_THROW(CSeqalignException, eInvalidInputAlignment, errstr);
                    }
                }
            }

            // next row
            row++;
            if (seg == 0) {
                dim++;
            }
        }
        if (dim != ss.GetDim()  ||  row != dim) {
            NCBI_THROW(CSeqalignException, eInvalidInputAlignment,
                       "CreateDensegFromStdseg(): "
                       "Inconsistent dimentions!");
        }

        if (widths_determined[seg]) {
            // go back and determine/check widths
            for (row = 0; row < dim; row++) {
                if ((row_len = row_lens[row]) > 0) {
                    if (row_len == seg_len * 3) {
                        width = 3;
                    } else if (row_len == seg_len) {
                        width = 1;
                    }
                    if (widths[row] > 0  &&  widths[row] != width) {
                        NCBI_THROW(CSeqalignException, eInvalidInputAlignment,
                                   "CreateDensegFromStdseg(): "
                                   "Std-seg segment lengths not accurate!");
                    } else {
                        widths[row] = width;
                    }
                }
            }
        }

        // next seg
        seg++;
    }

    ds.SetDim(dim);
    ds.SetNumseg(seg);

    // go back and finish lens determination
    bool widths_failure = false;
    bool widths_success = false;
    for (seg = 0; seg < ds.GetNumseg(); seg++) {
        if (!widths_determined[seg]) {
            for(row = 0; row < dim; row++) {
                if (starts[seg * dim + row] >= 0) {
                    width = widths[row];
                    if (width == 3) {
                        seg_lens[seg] /= 3;
                    } else if (width == 0) {
                        widths_failure = true;
                    }
                    break;
                }
            }
        } else {
            widths_success = true;
        }
    }

    if (widths_failure) {
        if (widths_success) {
            NCBI_THROW(CSeqalignException, eInvalidInputAlignment,
                       "CreateDensegFromStdseg(): "
                       "Some widths cannot be determined!");
        } else {
            ds.ResetWidths();
        }
    }
    
    // finish the strands
    for (seg = 1; seg < ds.GetNumseg(); seg++) {
        for (row = 0; row < dim; row++) {
            strands.push_back(strands[row]);
        }
    }

    return sa;
}


///----------------------------------------------------------------------------
/// PRE : the Seq-align is a Dense-seg of aligned nucleotide sequences
/// POST: Seq_align of type Dense-seg is created with each of the m_Widths 
///       equal to 3 and m_Lengths devided by 3.
CRef<CSeq_align> 
CSeq_align::CreateTranslatedDensegFromNADenseg() const
{
    if ( !GetSegs().IsDenseg() ) {
        NCBI_THROW(CSeqalignException, eInvalidInputAlignment,
                   "CSeq_align::CreateTranslatedDensegFromNADenseg(): "
                   "Input Seq-align should have segs of type Dense-seg!");
    }
    
    CRef<CSeq_align> sa(new CSeq_align);
    sa->SetType(eType_not_set);

    if (GetSegs().GetDenseg().IsSetWidths()) {
        NCBI_THROW(CSeqalignException, eInvalidInputAlignment,
                   "CSeq_align::CreateTranslatedDensegFromNADenseg(): "
                   "Widths already exist for the original alignment");
    }

    // copy from the original
    sa->Assign(*this);

    CDense_seg& ds = sa->SetSegs().SetDenseg();

    // fix the lengths
    const CDense_seg::TLens& orig_lens = GetSegs().GetDenseg().GetLens();
    CDense_seg::TLens&       lens      = ds.SetLens();

    for (CDense_seg::TNumseg numseg = 0; numseg < ds.GetNumseg(); numseg++) {
        if (orig_lens[numseg] % 3) {
            string errstr =
                string("CSeq_align::CreateTranslatedDensegFromNADenseg(): ") +
                "Length of segment " + NStr::IntToString(numseg) +
                " is not divisible by 3.";
            NCBI_THROW(CSeqalignException, eInvalidInputAlignment, errstr);
        } else {
            lens[numseg] = orig_lens[numseg] / 3;
        }
    }

    // add the widths
    ds.SetWidths().resize(ds.GetDim(), 3);

#if _DEBUG
    ds.Validate(true);
#endif

    return sa;
}


END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
* Revision 1.11  2004/03/09 17:27:42  kuznets
* Bug fix(compilation): CSeq_align::CreateDensegFromStdseg MSVC
*
* Revision 1.10  2004/03/09 17:14:33  todorov
* changed the C-style callback to a functor
*
* Revision 1.9  2004/02/23 15:31:24  todorov
* +TChooseSeqIdCallback to abstract resolving seq-ids in CreateDensegFromStdseg()
*
* Revision 1.8  2004/02/13 17:10:24  todorov
* - inconsistent ids exception in CreateDensegFromStdseg
*
* Revision 1.7  2004/01/09 16:32:06  todorov
* +SetType(eType_not_set)
*
* Revision 1.6  2003/12/16 22:54:31  todorov
* +CreateTranslatedDensegFromNADenseg
*
* Revision 1.5  2003/12/11 22:34:05  todorov
* Implementation of GetSeq{Start,Stop}
*
* Revision 1.4  2003/09/16 15:31:14  todorov
* Added validation methods. Added seq range methods
*
* Revision 1.3  2003/08/26 20:28:38  johnson
* added 'SwapRows' method
*
* Revision 1.2  2003/08/19 21:11:13  todorov
* +CreateDensegFromStdseg
*
* Revision 1.1  2003/08/13 18:12:03  johnson
* added 'Reverse' method
*
*
* ===========================================================================
*/
/* Original file checksum: lines: 64, chars: 1885, CRC32: 4e5d1825 */
