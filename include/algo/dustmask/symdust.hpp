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
 *   Header file for CSymDustMasker class.
 *
 */

#ifndef C_SYM_DUST_MASKER_HPP
#define C_SYM_DUST_MASKER_HPP

#include <iostream>
#include <vector>
#include <algorithm>
#include <iterator>
#include <memory>
#include <deque>
#include <list>

#include <corelib/ncbitype.h>
#include <corelib/ncbistr.hpp>
#include <corelib/ncbiobj.hpp>

#include <objects/seqloc/Seq_loc.hpp>
#include <objmgr/seq_vector.hpp>

BEGIN_NCBI_SCOPE

/**
    \brief Looks for low complexity parts of sequences according to the
           symmetric version of DUST algorithm.
 */
class NCBI_XALGODUSTMASK_EXPORT CSymDustMasker
{
    private:

        /** \internal
            \brief Function class responsible for conversion from IUPACNA
                   to NCBI2NA coding.
         */
        struct CIupac2Ncbi2na_converter
        {
            /** \internal
                \brief Operator performing the actual conversion.
                \param r base letter in IOPACNA encoding
                \return the same letter in NCBI2NA encoding
             */
            Uint1 operator()( Uint1 r ) const
            {
                switch( r )
                {
                    case 67: return 1;
                    case 71: return 2;
                    case 84: return 3;
                    default: return 0;
                }
            }
        };

        typedef objects::CSeqVector seq_t;          /**<\internal Sequence type. */
        typedef CIupac2Ncbi2na_converter convert_t; /**<\internal Converter type. */

    public:

        /**\brief Public sequence type. */
        typedef seq_t sequence_type;    
        /**\brief Integer size type corresponding to sequence_type. */
        typedef sequence_type::size_type size_type; 
        /**\brief Type respresenting an interval selected for masking. */
        typedef std::pair< size_type, size_type > TMaskedInterval;
        /**\brief Type representing a list of masked intervals. */
        typedef std::vector< TMaskedInterval > TMaskList;

        static const Uint4 DEFAULT_LEVEL  = 20; /**< Default value of score threshold. */
        static const Uint4 DEFAULT_WINDOW = 64; /**< Default window size. */
        static const Uint4 DEFAULT_LINKER = 1;  /**< Default value of the longest distance between
                                                     consequtive masked intervals at which they
                                                     should be merged. */

        // These (up to constructor) are public to work around the bug in SUN C++ compiler.

        /** \internal
            \brief Type representing a perfect interval.
         */
        struct lcr
        {
            TMaskedInterval bounds_;    /**<\internal The actual interval. */
            Uint4 score_;               /**<\internal The score of the interval. */
            size_type len_;             /**<\internal The length of the interval. */

            /** \internal   
                \brief Object constructor.
                \param start position of the left end
                \param stop position of the right end
                \param score the score
                \param len the length
             */
            lcr( size_type start, size_type stop, Uint4 score, size_type len )
                : bounds_( start, stop ), score_( score ), len_( len )
            {}
        };

        /**\brief Type representing a list of perfect intervals. */
        typedef std::list< lcr > lcr_list_type;
        /**\brief Table type to store score sum thresholds for each window length. */
        typedef std::vector< Uint4 > thres_table_type;
        /**\brief Type representing a triplet value. */
        typedef Uint1 triplet_type;

        /**\brief Selects the significant bits in triplet_type. */
        static const triplet_type TRIPLET_MASK = 0x3F;

        /**
            \brief Object constructor.
            \param level score threshold
            \param window max window size
            \param linker max distance at which to merge consequtive masked intervals
         */
        CSymDustMasker( Uint4 level = DEFAULT_LEVEL, 
                        size_type window 
                            = static_cast< size_type >( DEFAULT_WINDOW ),
                        size_type linker 
                            = static_cast< size_type >( DEFAULT_LINKER ) );

        /**
            \brief Mask a sequence.
            \param seq a sequence to mask
            \return list of masked intervals
         */
        std::auto_ptr< TMaskList > operator()( const sequence_type & seq );

        /**
            \brief Mask a part of the sequence.
            \param seq the sequence to mask
            \param start beginning position of the subsequence to mask
            \param stop ending position of the subsequence to mask
            \return list of masked intervals
         */
        std::auto_ptr< TMaskList > operator()( const sequence_type & seq,
                                               size_type start, size_type stop );

        /**
            \brief Mask a sequence and return result as a sequence of CSeq_loc
                   objects.
            \param seq_id sequence id
            \param seq the sequence
            \param [out] vector of const (smart) references to CSeq_loc
         */
        void GetMaskedLocs( 
            objects::CSeq_id & seq_id,
            const sequence_type & seq,
            std::vector< CConstRef< objects::CSeq_loc > > & locs );

    private:

        /**\internal Sequence iterator type. */
        typedef sequence_type::const_iterator seq_citer_type;

        /** \internal
            \brief Class representing the set of triplets in a window.
         */
        class triplets
        {
            public:
                
                /** \internal
                    \brief Object constructor.
                    \param first the first triplet value
                    \param window max window size
                    \param low_k max triplet multiplicity that guarantees that
                                 the window score is not above the threshold
                    \param lcr_list [in/out] current list of perfect intervals
                    \param thresholds table of threshold values for each window size
                 */
                triplets( triplet_type first,
                          size_type window, 
                          Uint1 low_k,
                          lcr_list_type & lcr_list,
                          thres_table_type & thresholds );

                size_type start() const { return start_; }  /**<\internal Get position of the first triplet. */
                size_type stop() const { return stop_; }    /**<\internal Get position of the last triplet. */
                size_type size() const { return triplet_list_.size(); } /**<\internal Get the number of triplets. */

                /** \internal
                    \brief Shift the right (and possibly left) end of the window 
                           one position to the right.

                    This function shifts the window one base to the right keeping the left 
                    end if window length is less than max window length. Does necessary
                    processing to keep the list of perfect intervals updated.

                    \param n next base character in the sequence
                    \return true if the left end of the window has shifted;
                            false otherwise
                 */
                bool add( sequence_type::value_type n ); 
                          
            private:
                
                /**\internal Implementation type for triplets list. */
                typedef std::deque< triplet_type > impl_type;
                /**\internal Triplets list iterator type. */
                typedef impl_type::const_iterator impl_citer_type;
                /**\internal Type for triplet counts tables. */
                typedef std::vector< Uint1 > counts_type;

                /** \internal
                    \brief Add triplet to the triplets list and update
                           the position lists.
                    \param t new triplet value
                 */
                void push_triplet( triplet_type t );

                /** \internal
                    \brief Remove the triplet from the beginning of the
                           triplet list and update the position lists.
                 */
                void pop_triplet();

                /** \internal
                    \brief Update the position lists when a new triplet
                           value is added.
                    \param t new triplet value
                 */
                void add_k_info( triplet_type t );

                /** \internal 
                    \brief Update the position lists when a triplet value 
                           is deleted.
                    \param t triplet value being deleted
                 */
                void rem_k_info( triplet_type t );

                impl_type triplet_list_;            /**<\internal The triplet list. */

                size_type start_;                   /**<\internal Position of the first triplet in the window. */
                size_type stop_;                    /**<\internal Position of the last triplet in the window. */
                size_type max_size_;                /**<\internal Maximum window size. */

                Uint1 low_k_;                       /**<\internal Max triplet multiplicity that guarantees that
                                                                  that the window score is not above the threshold. */
                Uint4 high_beg_;                    /**<\internal Position of the start of the window suffix
                                                                  corresponding to low_k_. */

                lcr_list_type & lcr_list_;          /**<\internal Current list of perfect subintervals. */
                thres_table_type & thresholds_;     /**<\internal Table containing thresholds for each 
                                                                  value of window length. */

                counts_type outer_counts_;          /**<\internal Table of triplet counts for the whole window. */
                counts_type inner_counts_;          /**<\internal Table of triplet counts for the window suffix. */
                Uint4 outer_sum_;                   /**<\internal s_w for the whole window. */
                Uint4 inner_sum_;                   /**<\internal s_w for the window suffix. */
        };

        Uint4 level_;       /**<\internal Score threshold. */
        size_type window_;  /**<\internal Max window size. */
        size_type linker_;  /**<\internal Max distance at which consequtive masked intervals should be merged. */

        Uint1 low_k_;   /**<\internal max triplet multiplicity guaranteeing not to exceed score threshold. */

        lcr_list_type lcr_list_;        /**<\internal List of perfect intervals within current window. */
        thres_table_type thresholds_;   /**<\internal Table containing score thresholds for each window size. */

        convert_t converter_;   /**\internal IUPACNA to NCBI2NA converter object. */
};

END_NCBI_SCOPE

#endif

/*
 * ========================================================================
 * $Log$
 * Revision 1.10  2005/07/18 14:55:59  morgulis
 * Removed position lists maintanance.
 *
 * Revision 1.9  2005/07/13 18:29:50  morgulis
 * operator() can mask part of the sequence
 *
 *
 */
