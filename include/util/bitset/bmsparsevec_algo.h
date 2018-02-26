#ifndef BMSPARSEVEC_ALGO__H__INCLUDED__
#define BMSPARSEVEC_ALGO__H__INCLUDED__
/*
Copyright(c) 2002-2017 Anatoliy Kuznetsov(anatoliy_kuznetsov at yahoo.com)

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

For more information please visit:  http://bitmagic.io
*/

#include "bmdef.h"
#include "bmsparsevec.h"

/** \defgroup svalgo Sparse vector algorithms
    Sparse vector algorithms
    \ingroup svector
 */


namespace bm
{

/*!
    \brief Clip dynamic range for signal higher than specified
    
    \param  svect - sparse vector to do clipping
    \param  high_bit - max bit (inclusive) allowed for this signal vector
    
    
    \ingroup svalgo
    \sa dynamic_range_clip_low
*/
template<class SV>
void dynamic_range_clip_high(SV& svect, unsigned high_bit)
{
    unsigned sv_plains = svect.plains();
    
    BM_ASSERT(sv_plains > high_bit && high_bit > 0);
    
    typename SV::bvector_type bv_acc;
    unsigned i;
    
    // combine all the high bits into accumulator vector
    for (i = high_bit+1; i < sv_plains; ++i)
    {
        typename SV::bvector_type* bv_plain = svect.plain(i);
        if (bv_plain)
        {
            bv_acc.bit_or(*bv_plain);
            svect.free_plain(i);
        }
    } // for i
    
    // set all bits ON for all low vectors, which happen to be clipped
    for (i = high_bit; true; --i)
    {
        typename SV::bvector_type* bv_plain = svect.get_plain(i);
        bv_plain->bit_or(bv_acc);
        if (i == 0)
            break;
    } // for i
}


/*!
    \brief Clip dynamic range for signal lower than specified (boost)
    
    \param  svect - sparse vector to do clipping
    \param  low_bit - low bit (inclusive) allowed for this signal vector
    
    \ingroup svalgo
    \sa dynamic_range_clip_high 
*/
template<class SV>
void dynamic_range_clip_low(SV& svect, unsigned low_bit)
{
    if (low_bit == 0) return; // nothing to do
    BM_ASSERT(svect.plains() > low_bit);
    
    unsigned sv_plains = svect.plains();
    typename SV::bvector_type bv_acc1;
    unsigned i;
    
    // combine all the high bits into accumulator vector
    for (i = low_bit+1; i < sv_plains; ++i)
    {
        typename SV::bvector_type* bv_plain = svect.plain(i);
        if (bv_plain)
            bv_acc1.bit_or(*bv_plain);
    } // for i
    
    // accumulate all vectors below the clipping point
    typename SV::bvector_type bv_acc2;
    typename SV::bvector_type* bv_low_plain = svect.get_plain(low_bit);
    
    for (i = low_bit-1; true; --i)
    {
        typename SV::bvector_type* bv_plain = svect.plain(i);
        if (bv_plain)
        {
            bv_acc2.bit_or(*bv_plain);
            svect.free_plain(i);
            if (i == 0)
                break;
        }
    } // for i
    
    // now we want to set 1 in the clipping low plain based on
    // exclusive or (XOR) between upper and lower parts)
    // as a result high signal (any bits in the upper plains) gets
    // slightly lower value (due to clipping) low signal gets amplified
    // (lower contrast algorithm)
    
    bv_acc1.bit_xor(bv_acc2);
    bv_low_plain->bit_or(bv_acc1);
}

/*!
    \brief Compute bit-vector of non-zero elements
 
    \param  svect - input sparse vector to compute non-zero elements
    \param  bvect - output bit-bector of non-zero elements
 
    Output vector is computed as a logical OR (join) of all plains
 
    \ingroup svalgo
*/
template<class SV>
void compute_nonzero_bvector(const SV& svect, typename SV::bvector_type& bvect)
{
    bool first = true;
    for (unsigned i = 0; i < svect.plains(); ++i)
    {
        const typename SV::bvector_type* bv_plain = svect.plain(i);
        if (bv_plain)
        {
            if (first) // first found plain - use simple assignment
            {
                bvect = *bv_plain;
                first = false;
            }
            else // for everything else - use OR
            {
                bvect |= *bv_plain;
            }
        }
    } // for i
    if (first) // no plains were found, just clear the result
    {
        bvect.clear(true);
    }
}
    
} // namespace bm

#include "bmundef.h"

#endif
