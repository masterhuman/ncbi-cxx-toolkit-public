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
 * Author:  Aleksandr Morgulis
 *
 * File Description:
 *   Implementation file for CSymDustMasker class.
 *
 */

#include <ncbi_pch.hpp>

#include <algo/dustmask/symdust.hpp>

BEGIN_NCBI_SCOPE

//------------------------------------------------------------------------------
CSymDustMasker::triplets::triplets( 
    size_type window, Uint1 low_k,
    perfect_list_type & perfect_list, thres_table_type & thresholds )
    : start_( 0 ), stop_( 0 ), max_size_( window - 2 ), low_k_( low_k ),
      L( 0 ), P( perfect_list ), thresholds_( thresholds ),
      c_w( 64, 0 ), c_v( 64, 0 ),
      r_w( 0 ), r_v( 0 )
{}

//------------------------------------------------------------------------------
void CSymDustMasker::triplets::shift_window( triplet_type t )
{
    // shift the left end of the window, if necessary
    if( triplet_list_.size() >= max_size_ ) {
        triplet_type s = triplet_list_.back();
        triplet_list_.pop_back();
        rem_triplet_info( r_w, c_w, s );

        if( L == start_ ) {
            ++L;
            rem_triplet_info( r_v, c_v, s );
        }

        ++start_;
    }

    triplet_list_.push_front( t );
    add_triplet_info( r_w, c_w, t );
    add_triplet_info( r_v, c_v, t );

    if( c_v[t] > low_k_ ) {
        Uint4 off = triplet_list_.size() - (L - start_) - 1;

        do {
            rem_triplet_info( r_v, c_v, triplet_list_[off] );
            ++L;
        }while( triplet_list_[off--] != t );
    }

    ++stop_;
}

//------------------------------------------------------------------------------
inline void CSymDustMasker::triplets::find_perfect()
{
    typedef perfect_list_type::iterator perfect_iter_type;
    static counts_type counts( 64 );

    Uint4 count = stop_ - L; // count is the suffix length

    // we need a local copy of triplet counts
    std::copy( c_v.begin(), c_v.end(), counts.begin() );

    Uint4 score = r_v; // and of the partial sum
    perfect_iter_type perfect_iter = P.begin();
    Uint4 max_perfect_score = 0;
    size_type max_len = 0;
    size_type pos = L - 1; // skipping the suffix
    impl_citer_type it = triplet_list_.begin() + count; // skipping the suffix
    impl_citer_type iend = triplet_list_.end();

    for( ; it != iend; ++it, ++count, --pos ) {
        Uint1 cnt = counts[*it];
        add_triplet_info( score, counts, *it );

        if( cnt > 0 && score*10 > thresholds_[count] ) {
            // found the candidate for the perfect interval
            // get the max score for the existing perfect intervals within
            //      current suffix
            while(    perfect_iter != P.end() 
                   && pos <= perfect_iter->bounds_.first ) {
                if(    max_perfect_score == 0 
                    || max_len*perfect_iter->score_ 
                       > max_perfect_score*perfect_iter->len_ ) {
                    max_perfect_score = perfect_iter->score_;
                    max_len = perfect_iter->len_;
                }

                ++perfect_iter;
            }

            // check if the current suffix score is at least as large
            if(    max_perfect_score == 0 
                || score*max_len >= max_perfect_score*count ) {
                max_perfect_score = score;
                max_len = count;
                perfect_iter = P.insert( 
                        perfect_iter, perfect( pos, stop_ + 1, 
                        max_perfect_score, count ) );
            }
        }
    }
}
    
//------------------------------------------------------------------------------
CSymDustMasker::CSymDustMasker( 
    Uint4 level, size_type window, size_type linker )
    : level_( (level >= 2 && level <= 64) ? level : DEFAULT_LEVEL ), 
      window_( (window >= 8 && window <= 64) ? window : DEFAULT_WINDOW ), 
      linker_( (linker >= 1 && linker <= 32) ? linker : DEFAULT_LINKER ),
      low_k_( level_/5 )
{
    thresholds_.reserve( window_ - 2 );
    thresholds_.push_back( 1 );

    for( size_type i = 1; i < window_ - 2; ++i )
        thresholds_.push_back( i*level_ );
}

//------------------------------------------------------------------------------
inline void CSymDustMasker::save_masked_regions( 
        TMaskList & res, size_type wstart, size_type start )
{
    if( !P.empty() ) {
        TMaskedInterval b = P.back().bounds_;
        
        if( b.first < wstart ) {
            TMaskedInterval b1( b.first + start, b.second + start );

            if( !res.empty() ) {
                size_type s = res.back().second;

                if( s + linker_ >= b1.first ) {
                    res.back().second = max( s, b1.second );
                }else {
                    res.push_back( b1 );
                }
            }else {
                res.push_back( b1 );
            }

            while( !P.empty() && P.back().bounds_.first < wstart ) {
                P.pop_back();
            }
        }
    }
}

//------------------------------------------------------------------------------
std::auto_ptr< CSymDustMasker::TMaskList > 
CSymDustMasker::operator()( const sequence_type & seq, 
                            size_type start, size_type stop )
{
    std::auto_ptr< TMaskList > res( new TMaskList );

    if( seq.empty() )
        return res;

    if( stop >= seq.size() )
        stop = seq.size() - 1;

    if( start > stop )
        start = stop;

    if( stop - start > 2 )    // there must be at least one triplet
    {
        // initializations
        P.clear();
        triplet_type t
            = (converter_( seq[start] )<<2) 
            + converter_( seq[start + 1] );
        triplets w( window_, low_k_, P, thresholds_ );
        seq_citer_type it = seq.begin() + start + w.stop() + 2;
        const seq_citer_type seq_end = seq.begin() + stop + 1;

        while( it != seq_end )
        {
            save_masked_regions( *res.get(), w.start(), start );

            // shift the window
            t = ((t<<2)&TRIPLET_MASK) + (converter_( *it )&0x3);
            w.shift_window( t );
            
            if( w.needs_processing() ) {
                w.find_perfect();
            }

            ++it;
        }

        // append the rest of the perfect intervals to the result
        {
            size_type wstart = w.start();

            while( !P.empty() ) {
                save_masked_regions( *res.get(), wstart, start );
                ++wstart;
            }
        }
    }

    return res;
}

//------------------------------------------------------------------------------
std::auto_ptr< CSymDustMasker::TMaskList > 
CSymDustMasker::operator()( const sequence_type & seq )
{ return (*this)( seq, 0, seq.size() - 1 ); }

//------------------------------------------------------------------------------
void CSymDustMasker::GetMaskedLocs( 
    objects::CSeq_id & seq_id,
    const sequence_type & seq, 
    std::vector< CConstRef< objects::CSeq_loc > > & locs )
{
    typedef std::vector< CConstRef< objects::CSeq_loc > > locs_type;
    std::auto_ptr< TMaskList > res = (*this)( seq );
    locs.clear();
    locs.reserve( res->size() );

    for( TMaskList::const_iterator it = res->begin(); it != res->end(); ++it )
        locs.push_back( CConstRef< objects::CSeq_loc >( 
            new objects::CSeq_loc( seq_id, it->first, it->second ) ) );
}

//------------------------------------------------------------------------------
CRef< objects::CPacked_seqint > CSymDustMasker::GetMaskedInts( 
        objects::CSeq_id & seq_id, const sequence_type & seq )
{
    CRef< objects::CPacked_seqint > result( new objects::CPacked_seqint );
    std::auto_ptr< TMaskList > res = (*this)( seq );

    for( TMaskList::const_iterator it = res->begin(); it != res->end(); ++it )
        result->AddInterval( seq_id, it->first, it->second );

    return result;
}

END_NCBI_SCOPE

/*
 * ========================================================================
 * $Log$
 * Revision 1.16  2005/10/31 20:55:14  morgulis
 * Refactoring of the library code to better correspond to the pseudocode
 * in the paper text.
 *
 * Revision 1.15  2005/10/21 17:25:54  morgulis
 * Fixed a problem of linker usage in the last window of the sequence.
 *
 * Revision 1.14  2005/09/19 14:37:09  morgulis
 * Added API to return masked intervals as CRef< CPacked_seqint >.
 *
 * Revision 1.13  2005/08/25 11:28:04  morgulis
 * Fixing the memory access problem when the suffix == window.
 *
 * Revision 1.12  2005/07/27 18:40:49  morgulis
 * some code simplification
 *
 * Revision 1.11  2005/07/19 18:59:25  morgulis
 * Simplification of add_k_info().
 *
 * Revision 1.10  2005/07/18 14:55:59  morgulis
 * Removed position lists maintanance.
 *
 * Revision 1.9  2005/07/14 20:39:39  morgulis
 * Fixed offsets bug when masking part of the sequence.
 *
 * Revision 1.8  2005/07/13 18:29:50  morgulis
 * operator() can mask part of the sequence
 *
 *
 */
