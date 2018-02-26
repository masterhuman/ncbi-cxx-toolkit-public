#ifndef BMSPARSEVEC_H__INCLUDED__
#define BMSPARSEVEC_H__INCLUDED__
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

#include <memory.h>

#ifndef BM_NO_STL
#include <stdexcept>
#endif

#include "bm.h"
#include "bmtrans.h"
#include "bmalgo.h"
#include "bmdef.h"

namespace bm
{

/** \defgroup svector Sparse vector
    Sparse vector for integer types using bit transposition transform
    \ingroup bmagic
 */


/*!
   \brief sparse vector with runtime compression using bit transposition method
 
   Sparse vector implements variable bit-depth storage model.
   Initial data is bit-transposed into bit-planes so initial each element
   may use less memory than the original native data type prescribes.
   For example, 32-bit integer may only use 20 bits.
 
   Another level of compression is provided by bit-vector (BV template parameter)
   used for storing bit planes. bvector<> implements varians of on the fly block
   compression, so if a significant area of a sparse vector uses less bits - it
   will save memory.
 
   Overall it provides variable bit-depth compression, sparse compression in
   bit-plains.
 
   \ingroup svector
*/
template<class Val, class BV>
class sparse_vector
{
public:
    typedef Val                                      value_type;
    typedef bm::id_t                                 size_type;
    typedef BV                                       bvector_type;
    typedef bvector_type*                            bvector_type_ptr;
	typedef const value_type&                        const_reference;
    typedef typename BV::allocator_type              allocator_type;
    typedef typename bvector_type::allocation_policy allocation_policy_type;
    
    /*! Statistical information about  memory allocation details. */
    struct statistics : public bv_statistics
    {};

    /**
         Reference class to access elements via common [] operator
    */
    class reference
    {
    public:
        reference(sparse_vector<Val, BV>& sv, size_type idx) BMNOEXEPT
        : sv_(sv), idx_(idx)
        {}
        operator value_type() const { return sv_.get(idx_); }
        reference& operator=(const reference& ref)
        {
            sv_.set(idx_, (value_type)ref);
            return *this;
        }
        reference& operator=(value_type val)
        {
            sv_.set(idx_, val);
            return *this;
        }
        bool operator==(const reference& ref) const
                                { return bool(*this) == bool(ref); }
    private:
        sparse_vector<Val, BV>& sv_;
        size_type               idx_;
    };

public:
    /*!
        \brief Sparse vector constructor
        \param ap - allocation strategy for underlying bit-vectors
        Default allocation policy uses BM_BIT setting (fastest access)
        \param bv_max_size - maximum possible size of underlying bit-vectors
        Please note, this is NOT size of svector itself, it is dynamic upper limit
        which should be used very carefully if we surely know the ultimate size
        \param alloc - allocator for bit-vectors
        
        \sa bvector<>
        \sa bm::bvector<>::allocation_policy
        \sa bm::startegy
    */
    sparse_vector(allocation_policy_type ap = allocation_policy_type(),
                  size_type bv_max_size = bm::id_max,
                  const allocator_type&   alloc  = allocator_type());
    
    /*! copy-ctor */
    sparse_vector(const sparse_vector<Val, BV>& sv);


#ifndef BM_NO_CXX11
    /*! move-ctor */
    sparse_vector(sparse_vector<Val, BV>&& sv) BMNOEXEPT;


    /*! move assignmment operator */
    sparse_vector<Val,BV>& operator = (sparse_vector<Val, BV>&& sv) BMNOEXEPT
    {
        if (this != &sv)
        {
            clear();
            swap(sv);
        }
        return *this;
    }
#endif

    /*! copy assignmment operator */
    sparse_vector<Val,BV>& operator = (const sparse_vector<Val, BV>& sv)
    {
        if (this != &sv)
        {
            clear();
            resize(sv.size());
            bv_size_ = sv.bv_size_;
            alloc_ = sv.alloc_;
            effective_plains_ = sv.effective_plains_;

            for (size_type i = 0; i < sv.effective_plains(); ++i)
            {
                const bvector_type* bv = sv.plains_[i];
                if (bv)
                {
#ifdef BM_NO_STL   // C compatibility mode
                    void* mem = ::malloc(sizeof(bvector_type));
                    if (mem == 0)
                    {
                        BM_ASSERT_THROW(false, BM_ERR_BADALLOC);
                    }
                    plains[i] = new(mem) bvector_type(*bv);
#else
                    plains_[i] = new bvector_type(*bv);
#endif
                }
            } // for i
        }
        return *this;
    }
    
    
    ~sparse_vector() BMNOEXEPT;
    
    reference operator[](size_type idx)
    {
        BM_ASSERT(idx < size_);
        return reference(*this, idx);
    }

    
    /*! \brief content exchange
    */
    void swap(sparse_vector<Val, BV>& sv) BMNOEXEPT;
    
    /*!
        \brief get specified element without bounds checking
        \param idx - element index
        \return value of the element
    */
    value_type operator[](size_type idx) const { return this->get(idx); }
    
    /*!
        \brief Import list of elements from a C-style array
        \param arr  - source array
        \param size - source size
        \param offset - target index in the sparse vector
    */
    void import(const value_type* arr, size_type size, size_type offset = 0);


    /*!
        \brief Bulk export list of elements to a C-style array
     
        For efficiency, this is left as a low level function,
        it does not do any bounds checking on the target array, it will
        override memory and crash if you are not careful with allocation
        and request size.
     
        \param arr  - dest array
        \param idx_from - index in the sparse vector to export from
        \param size - decoding size (array allocation should match)
        \param zero_mem - set to false if target array is pre-initialized
                          with 0s to avoid performance penalty
     
        \return number of actually exported elements (can be less than requested)
     
        \sa decode
     
        @internal
    */
    size_type decode(value_type* arr,
                     size_type   idx_from,
                     size_type   size,
                     bool        zero_mem = true) const;


    
    /*! \brief return size of the vector
        \return size of sparse vector
    */
    size_type size() const;
    
    
    /*! \brief return true if vector is empty
        \return true if empty
    */
    bool empty() const;
    
    
    /*! \brief resize vector
        \param sz - new size
    */
    void resize(size_type sz);
    
    /*! \brief resize to zero, free memory
    */
    void clear() BMNOEXEPT;
    
    /*!
        \brief access specified element with bounds checking
        \param idx - element index
        \return value of the element
    */
    value_type at(size_type idx) const;
    
    /*!
        \brief get specified element without bounds checking
        \param idx - element index
        \return value of the element
    */
    value_type get(bm::id_t idx) const;
    
    /*!
        \brief set specified element with bounds checking and automatic resize
        \param idx - element index
        \param v   - element value
    */
    void set(size_type idx, value_type v);

    /*!
        \brief push value back into vector
        \param v   - element value
    */
    void push_back(value_type v);
    
    /*!
        \brief check if another sparse vector has the same content and size
        \return true, if it is the same
    */
    bool equal(const sparse_vector<Val, BV>& sv) const;


    /*!
        \brief run memory optimization for all vector plains
        \param temp_block - pre-allocated memory block to avoid unnecessary re-allocs
        \param opt_mode - requested compression depth
        \param stat - memory allocation statistics after optimization
    */
    void optimize(bm::word_t* temp_block = 0,
                  typename bvector_type::optmode opt_mode = bvector_type::opt_compress,
                  typename sparse_vector<Val, BV>::statistics* stat = 0);
    /*!
       \brief Optimize sizes of GAP blocks

       This method runs an analysis to find optimal GAP levels for all bit plains
       of the vector.
    */
    void optimize_gap_size();
    
    /*!
        \brief join all with another sparse vector using OR operation
        \param sv - argument vector to join with
        \return slf reference
    */
    sparse_vector<Val, BV>& join(const sparse_vector<Val, BV>& sv);


    /*!
       @brief Calculates memory statistics.

       @param st - pointer on statistics structure to be filled in. 

       Function fills statistics structure containing information about how 
       this vector uses memory and estimation of max. amount of memory 
       bvector needs to serialize itself.

       @sa statistics
    */
    void calc_stat(struct sparse_vector<Val, BV>::statistics* st) const;
    

    /*!
        \brief get access to bit-plain, function checks and creates a plain
    */
    bvector_type_ptr get_plain(unsigned i);
    
    /*!
        \brief get total number of bit-plains in the vector
    */
    static unsigned plains() { return unsigned(sizeof(Val)*8); }
    
    /*!
        \brief get access to bit-plain as is (can return NULL)
    */
    bvector_type_ptr plain(unsigned i) { return plains_[i]; }
    const bvector_type_ptr plain(unsigned i) const { return plains_[i]; }
    
    /*!
        \brief free memory in bit-plain
    */
    void free_plain(unsigned i);
    
    /*!
        \brief clear range (assign bit 0 for all plains)
        \param left  - interval start
        \param right - interval end (closed interval)
    */
    sparse_vector<Val, BV>& clear_range(size_type left, size_type right);
    
    
    // -------------------------- auxiliary methods, for internal use
    
    /*!
        \brief Bulk export list of elements to a C-style array
     
        Use of all extract() methods is restricted.
        Please consider decode() for the same purpose.
     
        \param arr  - dest array
        \param size - dest size
        \param offset - target index in the sparse vector to export from
        \param zero_mem - set to false if target array is pre-initialized
                          with 0s to avoid performance penalty
     
        \return number of exported elements
     
        \sa decode
     
        @internal
    */
    size_type extract(value_type* arr,
                      size_type size,
                      size_type offset = 0,
                      bool      zero_mem = true) const;

    /** \brief extract small window without use of masking vector
        \sa decode
        @internal
    */
    size_type extract_range(value_type* arr,
                            size_type size,
                            size_type offset,
                            bool      zero_mem = true) const;
    
    /** \brief extract medium window without use of masking vector
        \sa decode
        @internal
    */
    size_type extract_plains(value_type* arr,
                             size_type size,
                             size_type offset,
                             bool      zero_mem = true) const;

private:

    /*! \brief free all internal vectors
    */
    void free_vectors() BMNOEXEPT;
    
    unsigned effective_plains() const { return effective_plains_ + 1; }


protected:
    /*! \brief set value without checking boundaries
    */
    void set_value(size_type idx, value_type v);
    const bm::word_t* get_block(unsigned p, unsigned i, unsigned j) const;
    void throw_range_error(const char* err_msg) const;

private:
    
    size_type                bv_size_;
    allocator_type           alloc_;
    allocation_policy_type   ap_;
    
    bvector_type_ptr    plains_[sizeof(Val)*8];
    size_type           size_;
    unsigned            effective_plains_;
};

//---------------------------------------------------------------------


template<class Val, class BV>
sparse_vector<Val, BV>::sparse_vector(
        allocation_policy_type  ap,
        size_type               bv_max_size,
        const allocator_type&   alloc)
: bv_size_(bv_max_size),
  alloc_(alloc),
  ap_(ap),
  size_(0),
  effective_plains_(0)
{
    ::memset(plains_, 0, sizeof(plains_));
}

//---------------------------------------------------------------------

template<class Val, class BV>
sparse_vector<Val, BV>::sparse_vector(const sparse_vector<Val, BV>& sv)
: bv_size_ (sv.bv_size_),
  alloc_(sv.alloc_),
  ap_(sv.ap_),
  size_(sv.size_),
  effective_plains_(sv.effective_plains_)
{
    if (this != &sv)
    {
        for (size_type i = 0; i < sizeof(Val)*8; ++i)
        {
            const bvector_type* bv = sv.plains_[i];
            if (bv)
                plains_[i] = new bvector_type(*bv);
            else
                plains_[i] = 0;
        } // for i
    }
}

//---------------------------------------------------------------------
#ifndef BM_NO_CXX11

template<class Val, class BV>
sparse_vector<Val, BV>::sparse_vector(sparse_vector<Val, BV>&& sv) BMNOEXEPT
{
    bv_size_ = 0;
    alloc_ = sv.alloc_;
    ap_ = sv.ap_;
    size_ = sv.size_;
    effective_plains_ = sv.effective_plains_;
        
    for (size_type i = 0; i < sizeof(Val)*8; ++i)
    {
        plains_[i] = sv.plains_[i];
        sv.plains_[i] = 0;
    }
    sv.size_ = 0;
}

#endif



//---------------------------------------------------------------------

template<class Val, class BV>
sparse_vector<Val, BV>::~sparse_vector() BMNOEXEPT
{
    free_vectors();
}

//---------------------------------------------------------------------

template<class Val, class BV>
void sparse_vector<Val, BV>::swap(sparse_vector<Val, BV>& sv) BMNOEXEPT
{
    if (this != &sv)
    {
        bm::xor_swap(bv_size_, sv.bv_size_);
        
        allocator_type alloc_tmp = alloc_;
        alloc_ = sv.alloc_;
        sv.alloc_ = alloc_tmp;
        
        allocation_policy_type ap_tmp = ap_;
        ap_ = sv.ap_;
        sv.ap_ = ap_tmp;
        
        for (size_type i = 0; i < sizeof(Val)*8; ++i)
        {
            bvector_type* bv_tmp = plains_[i];
            plains_[i] = sv.plains_[i];
            sv.plains_[i] = bv_tmp;
        } // for i
        
        bm::xor_swap(size_, sv.size_);        
    }
}

//---------------------------------------------------------------------

template<class Val, class BV>
void sparse_vector<Val, BV>::throw_range_error(const char* err_msg) const
{
#ifndef BM_NO_STL
    throw std::range_error(err_msg);
#else
    BM_ASSERT_THROW(false, BM_ERR_RANGE);
#endif
}

//---------------------------------------------------------------------

template<class Val, class BV>
void sparse_vector<Val, BV>::import(const value_type* arr,
                                    size_type         size,
                                    size_type         offset)
{
    unsigned char b_list[sizeof(Val)*8];
    unsigned row_len[sizeof(Val)*8] = {0, };
    
    const unsigned transpose_window = 256;
    bm::tmatrix<bm::id_t, sizeof(Val)*8, transpose_window> tm; // matrix accumulator
    
    if (size == 0)
    {
        throw_range_error("sparse_vector range error (import size 0)");
    }
    
    // clear all plains in the range to provide corrrect import of 0 values
    this->clear_range(offset, offset + size - 1);
    
    // transposition algorithm uses bitscen to find index bits and store it
    // in temporary matrix (list for each bit plain), matrix here works
    // when array gets to big - the list gets loaded into bit-vector using
    // bulk load algorithm, which is faster than single bit access
    //
    
    size_type i;
    for (i = 0; i < size; ++i)
    {
        unsigned bcnt = bm::bitscan(arr[i], b_list);
        const unsigned bit_idx = i + offset;
        
        for (unsigned j = 0; j < bcnt; ++j)
        {
            unsigned p = b_list[j];
            unsigned rl = row_len[p];
            tm.row(p)[rl] = bit_idx;
            row_len[p] = ++rl;
            
            if (rl == transpose_window)
            {
                bvector_type* bv = get_plain(p);
                const bm::id_t* r = tm.row(p);
                bm::combine_or(*bv, r, r + rl);
                row_len[p] = 0;
                tm.row(p)[0] = 0;
            }
        } // for j
    } // for i
    
    // process incomplete transposition lines
    //
    for (unsigned k = 0; k < tm.rows(); ++k)
    {
        unsigned rl = row_len[k];
        if (rl)
        {
            bvector_type* bv = get_plain(k);
            const bm::id_t* r = tm.row(k);
            bm::combine_or(*bv, r, r + rl);
        }
    } // for k
    
    
    if (i + offset > size_)
        size_ = i + offset;
}

//---------------------------------------------------------------------

template<class Val, class BV>
typename sparse_vector<Val, BV>::size_type
sparse_vector<Val, BV>::decode(value_type* arr,
                               size_type   idx_from,
                               size_type   size,
                               bool        zero_mem) const
{
    if (size < 32)
    {
        return extract_range(arr, size, idx_from, zero_mem);
    }
    if (size < 1024)
    {
        return extract_plains(arr, size, idx_from, zero_mem);
    }
    return extract(arr, size, idx_from, zero_mem);
}

//---------------------------------------------------------------------

template<class Val, class BV>
typename sparse_vector<Val, BV>::size_type
sparse_vector<Val, BV>::extract_range(value_type* arr,
                                      size_type size,
                                      size_type offset,
                                      bool      zero_mem) const
{
    if (size == 0)
        return 0;
    if (zero_mem)
        ::memset(arr, 0, sizeof(value_type)*size);

    size_type start = offset;
    size_type end = start + size;
    if (end > size_)
    {
        end = size_;
    }
    
    // calculate logical block coordinates and masks
    //
    unsigned nb = unsigned(start >>  bm::set_block_shift);
    unsigned i0 = nb >> bm::set_array_shift; // top block address
    unsigned j0 = nb &  bm::set_array_mask;  // address in sub-block
    unsigned nbit = unsigned(start & bm::set_block_mask);
    unsigned nword  = unsigned(nbit >> bm::set_word_shift);
    unsigned mask0 = 1u << (nbit & bm::set_word_mask);
    const bm::word_t* blk = 0;
    unsigned is_set;
    
    for (unsigned j = 0; j < sizeof(Val)*8; ++j)
    {
        blk = get_block(j, i0, j0);
        bool is_gap = BM_IS_GAP(blk);
        
        for (unsigned k = start; k < end; ++k)
        {
            unsigned nb1 = unsigned(k >>  bm::set_block_shift);
            if (nb1 != nb) // block switch boundaries
            {
                nb = nb1;
                i0 = nb >> bm::set_array_shift;
                j0 = nb &  bm::set_array_mask;
                blk = get_block(j, i0, j0);
                is_gap = BM_IS_GAP(blk);
            }
        
            if (!blk)
                continue;
            nbit = unsigned(k & bm::set_block_mask);
            if (is_gap)
            {
                is_set = bm::gap_test_unr(BMGAP_PTR(blk), nbit);
            }
            else
            {
                nword  = unsigned(nbit >> bm::set_word_shift);
                mask0 = 1u << (nbit & bm::set_word_mask);
                is_set = (blk[nword] & mask0);
            }
            size_type idx = k - offset;
            value_type vm = (bool) is_set;
            vm <<= j;
            arr[idx] |= vm;
            
        } // for k

    } // for j
    return 0;
}

//---------------------------------------------------------------------

template<class Val, class BV>
typename sparse_vector<Val, BV>::size_type
sparse_vector<Val, BV>::extract_plains(value_type* arr,
                                       size_type   size,
                                       size_type   offset,
                                       bool        zero_mem) const
{
    if (size == 0)
        return 0;

    if (zero_mem)
        ::memset(arr, 0, sizeof(value_type)*size);
    
    size_type start = offset;
    size_type end = start + size;
    if (end > size_)
    {
        end = size_;
    }
    
    for (size_type i = 0; i < sizeof(Val) * 8; ++i)
    {
        const bvector_type* bv = plains_[i];
        if (!bv)
            continue;
       
        value_type mask = 1;
        mask <<= i;
        typename BV::enumerator en(bv, offset);
        for (;en.valid(); ++en)
        {
            size_type idx = *en - offset;
            if (idx >= size)
                break;
            arr[idx] |= mask;
        } // for
        
    } // for i

    return 0;
}


//---------------------------------------------------------------------

template<class Val, class BV>
typename sparse_vector<Val, BV>::size_type
sparse_vector<Val, BV>::extract(value_type* arr,
                                size_type   size,
                                size_type   offset,
                                bool        zero_mem) const
{
    /// Decoder functor
    /// @internal
    ///
    struct sv_decode_visitor_func
    {
        sv_decode_visitor_func(value_type* arr,
                               value_type mask,
                               size_type  offset)
        : arr_(arr), mask_(mask), off_(offset)
        {}
        
        void add_bits(bm::id_t offset, const unsigned char* bits, unsigned size)
        {
            size_type idx_base = offset - off_;
            for (unsigned i = 0; i < size; ++i)
            {
                size_type idx = idx_base + bits[i];
                arr_[idx] |= mask_;
            }
            
        }
        void add_range(bm::id_t offset, unsigned size)
        {
            size_type idx_base = offset - off_;
            for (unsigned i = 0; i < size; ++i)
            {
                arr_[i + idx_base] |= mask_;
            }
        }
        value_type*  arr_;
        value_type   mask_;
        size_type    off_;
    };


    if (size == 0)
        return 0;

    if (zero_mem)
        ::memset(arr, 0, sizeof(value_type)*size);
    
    size_type start = offset;
    size_type end = start + size;
    if (end > size_)
    {
        end = size_;
    }
    
	bool masked_scan = !(offset == 0 && size == this->size());

    if (masked_scan) // use temp vector to decompress the area
    {
        // for large array extraction use logical opartions
        // (faster due to vectorization)
        bvector_type bv_mask;
        for (size_type i = 0; i < sizeof(Val) * 8; ++i)
        {
            const bvector_type* bv = plains_[i];
            if (bv)
            {
                bv_mask.set_range(offset, end - 1);
                bv_mask.bit_and(*bv);

                sv_decode_visitor_func func(arr, (value_type(1) << i), offset);
                bm::for_each_bit(bv_mask, func);
                bv_mask.clear();
            }
        } // for i
    }
    else
    {
        for (size_type i = 0; i < sizeof(Val)*8; ++i)
        {
            const bvector_type* bv = plains_[i];
            if (bv)
            {
                sv_decode_visitor_func func(arr, (value_type(1) << i), 0);
                bm::for_each_bit(*bv, func);
            }
        } // for i
    }

    return 0;
}

//---------------------------------------------------------------------

template<class Val, class BV>
typename sparse_vector<Val, BV>::size_type
sparse_vector<Val, BV>::size() const
{
    return size_;
}

//---------------------------------------------------------------------

template<class Val, class BV>
bool sparse_vector<Val, BV>::empty() const
{
    return (size_ == 0);
}


//---------------------------------------------------------------------

template<class Val, class BV>
void sparse_vector<Val, BV>::resize(typename sparse_vector<Val, BV>::size_type sz)
{
    if (sz == size_)  // nothing to do
        return;
    
    if (!sz) // resize to zero is an equivalent of non-destructive deallocation
    {
        clear();
        return;
    }
    
    if (sz < size_) // vector shrink
    {
        this->clear_range(sz, size_-1);   // clear the tails
    }
    
    size_ = sz;
}

//---------------------------------------------------------------------


template<class Val, class BV>
typename sparse_vector<Val, BV>::bvector_type_ptr
   sparse_vector<Val, BV>::get_plain(unsigned i)
{
    BM_ASSERT(i < (sizeof(Val)*8));

    bvector_type_ptr bv = plains_[i];
    if (!bv)
    {
#ifdef BM_NO_STL   // C compatibility mode
        void* mem = ::malloc(sizeof(bvector_type));
        if (mem == 0)
        {
            BM_ASSERT_THROW(false, BM_ERR_BADALLOC);
        }
        bv = new(mem) bvector_type(ap_.strat, ap_.glevel_len,
                                   bv_size_,
                                   alloc_);

#else
        bv = new bvector_type(ap_.strat, ap_.glevel_len,
                              bv_size_,
                              alloc_);
#endif

        bv->init();
        plains_[i] = bv;
        if (i > effective_plains_)
            effective_plains_ = i;
    }
    return bv;
}

//---------------------------------------------------------------------

template<class Val, class BV>
typename sparse_vector<Val, BV>::value_type
sparse_vector<Val, BV>::at(typename sparse_vector<Val, BV>::size_type idx) const
{
    if (idx >= size_)
        throw_range_error("sparse vector range error");
    return this->get(idx);
}

//---------------------------------------------------------------------

template<class Val, class BV>
const bm::word_t* sparse_vector<Val, BV>::get_block(unsigned p, unsigned i, unsigned j) const
{
    const bvector_type* bv = this->plains_[p];
    if (bv)
    {
        const typename bvector_type::blocks_manager_type& bman = bv->get_blocks_manager();
        return bman.get_block(i, j);
    }
    return 0;
}

//---------------------------------------------------------------------

template<class Val, class BV>
typename sparse_vector<Val, BV>::value_type
sparse_vector<Val, BV>::get(bm::id_t i) const
{
    BM_ASSERT(i < size_);
    
    value_type v = 0;
    
    // calculate logical block coordinates and masks
    //
    unsigned nb = unsigned(i >>  bm::set_block_shift);
    unsigned i0 = nb >> bm::set_array_shift; // top block address
    unsigned j0 = nb &  bm::set_array_mask;  // address in sub-block
    unsigned nbit = unsigned(i & bm::set_block_mask);
    unsigned nword  = unsigned(nbit >> bm::set_word_shift);
    unsigned mask0 = 1u << (nbit & bm::set_word_mask);
    const bm::word_t* blk;
    const bm::word_t* blka[4];
    
    unsigned eff_plains = effective_plains();
    for (unsigned j = 0; j < eff_plains; j+=4)
    {
        bool b = plains_[j+0] || plains_[j+1] || plains_[j+2] || plains_[j+3];
        if (!b)
            continue;

        blka[0] = get_block(j+0, i0, j0);
        blka[1] = get_block(j+1, i0, j0);
        blka[2] = get_block(j+2, i0, j0);
        blka[3] = get_block(j+3, i0, j0);

        if ((blk = blka[0+0])!=0)
        {
            unsigned is_set = (BM_IS_GAP(blk)) ? bm::gap_test_unr(BMGAP_PTR(blk), nbit) : (blk[nword] & mask0);
            value_type vm = (bool) is_set;
            vm <<= (j+0);
            v |= vm;
        }
        if ((blk = blka[0+1])!=0)
        {
            unsigned is_set = (BM_IS_GAP(blk)) ? bm::gap_test_unr(BMGAP_PTR(blk), nbit) : (blk[nword] & mask0);
            value_type vm = (bool) is_set;
            vm <<= (j+1);
            v |= vm;
        }
        if ((blk = blka[0+2])!=0)
        {
            unsigned is_set = (BM_IS_GAP(blk)) ? bm::gap_test_unr(BMGAP_PTR(blk), nbit) : (blk[nword] & mask0);
            value_type vm = (bool) is_set;
            vm <<= (j+2);
            v |= vm;
        }
        if ((blk = blka[0+3])!=0)
        {
            unsigned is_set = (BM_IS_GAP(blk)) ? bm::gap_test_unr(BMGAP_PTR(blk), nbit) : (blk[nword] & mask0);
            value_type vm = (bool) is_set;
            vm <<= (j+3);
            v |= vm;
        }

    } // for j
    
    return v;
}


//---------------------------------------------------------------------

template<class Val, class BV>
void sparse_vector<Val, BV>::set(size_type idx, value_type v)
{ 
    if (idx >= size_)
    {
        size_ = idx+1;
    }
    set_value(idx, v);
}

//---------------------------------------------------------------------

template<class Val, class BV>
void sparse_vector<Val, BV>::push_back(value_type v)
{
    set_value(size_, v);
    ++size_;
}

//---------------------------------------------------------------------

template<class Val, class BV>
void sparse_vector<Val, BV>::set_value(size_type idx, value_type v)
{
    // calculate logical block coordinates and masks
    //
    unsigned nb = unsigned(idx >>  bm::set_block_shift);
    unsigned i0 = nb >> bm::set_array_shift; // top block address
    unsigned j0 = nb &  bm::set_array_mask;  // address in sub-block

    // clear the plains where needed
    unsigned eff_plains = effective_plains();
    for (unsigned i = 0; i < eff_plains; ++i)
    {
        const bm::word_t* blk = get_block(i, i0, j0);
        if (blk)
        {
            BM_ASSERT(plains_[i]);
            bvector_type* bv = plains_[i];
            bv->clear_bit(idx);
        }
    }

    // set bits in plains
    unsigned b_list[sizeof(Val) * 8];
    unsigned bcnt = bm::bitscan(v, b_list);

    for (unsigned j = 0; j < bcnt; ++j)
    {
        unsigned p = b_list[j];
        bvector_type* bv = get_plain(p);
        bv->set_bit_no_check(idx);
    } // for j
}

//---------------------------------------------------------------------

template<class Val, class BV>
void sparse_vector<Val, BV>::clear() BMNOEXEPT
{
    free_vectors();
    size_ = 0;
    effective_plains_ = 0;
    ::memset(plains_, 0, sizeof(plains_));
}

//---------------------------------------------------------------------


template<class Val, class BV>
void sparse_vector<Val, BV>::free_vectors() BMNOEXEPT
{
    for (size_type i = 0; i < sizeof(Val)*8; ++i)
        delete plains_[i];
}

//---------------------------------------------------------------------


template<class Val, class BV>
void sparse_vector<Val, BV>::free_plain(unsigned i)
{
    BM_ASSERT(i < sizeof(Val)*8);
    bvector_type* bv = plains_[i];
    delete bv;
    plains_[i] = 0;
}

//---------------------------------------------------------------------

template<class Val, class BV>
sparse_vector<Val, BV>&
sparse_vector<Val, BV>::clear_range(
    typename sparse_vector<Val, BV>::size_type left,
    typename sparse_vector<Val, BV>::size_type right)
{
    if (right < left)
    {
        return clear_range(right, left);
    }
    unsigned eff_plains = effective_plains();
    for (unsigned i = 0; i < eff_plains; ++i)
    {
        bvector_type* bv = plains_[i];
        if (bv)
        {
            bv->set_range(left, right, false);
        }
    } // for i
    
    return *this;
}

//---------------------------------------------------------------------

template<class Val, class BV>
void sparse_vector<Val, BV>::calc_stat(
     struct sparse_vector<Val, BV>::statistics* st) const
{
    BM_ASSERT(st);
    
	st->bit_blocks = st->gap_blocks = 0; 
	st->max_serialize_mem = st->memory_used = 0;

    unsigned eff_plains = effective_plains();
    for (unsigned j = 0; j < eff_plains; ++j)
    {
        const bvector_type* bv = this->plains_[j];
        if (bv)
        {
            typename bvector_type::statistics stbv;
            bv->calc_stat(&stbv);
            
            st->bit_blocks += stbv.bit_blocks;
            st->gap_blocks += stbv.gap_blocks;
            st->max_serialize_mem += stbv.max_serialize_mem + 8;
            st->memory_used += stbv.memory_used;
        }
    } // for j
    // header accounting
    st->max_serialize_mem += 1 + 1 + 1 + 1 + 8 + (8 * sizeof(Val) * 8);

}

//---------------------------------------------------------------------

template<class Val, class BV>
void sparse_vector<Val, BV>::optimize(
    bm::word_t*                                  temp_block, 
    typename bvector_type::optmode               opt_mode,
    typename sparse_vector<Val, BV>::statistics* st)
{
    if (st)
    {
        st->bit_blocks = st->gap_blocks = 0;
        st->max_serialize_mem = st->memory_used = 0;
    }
    unsigned eff_plains = effective_plains();
    for (unsigned j = 0; j < eff_plains; ++j)
    {
        bvector_type* bv = this->plains_[j];
        if (bv)
        {
            if (!bv->any())  // empty vector?
            {
                delete this->plains_[j];
                this->plains_[j] = 0;
                continue;
            }
            
            typename bvector_type::statistics stbv;
            bv->optimize(temp_block, opt_mode, &stbv);
            
            if (st)
            {
                st->bit_blocks += stbv.bit_blocks;
                st->gap_blocks += stbv.gap_blocks;
                st->max_serialize_mem += stbv.max_serialize_mem + 8;
                st->memory_used += stbv.memory_used;
            }

        }
    } // for j

}

//---------------------------------------------------------------------


template<class Val, class BV>
void sparse_vector<Val, BV>::optimize_gap_size()
{
    unsigned eff_plains = effective_plains();
    for (unsigned j = 0; j < eff_plains; ++j)
    {
        bvector_type* bv = this->plains_[j];
        if (bv)
        {
            bv->optimize_gap_size();
        }
    }
}

//---------------------------------------------------------------------

template<class Val, class BV>
sparse_vector<Val, BV>&
sparse_vector<Val, BV>::join(const sparse_vector<Val, BV>& sv)
{
    size_type arg_size = sv.size();
    if (size_ < arg_size)
    {
        resize(arg_size);
    }
    unsigned arg_eff_plains = sv.effective_plains();
    for (unsigned j = 0; j < arg_eff_plains; ++j)
    {
        bvector_type* arg_bv = sv.plains_[j];
        if (arg_bv)
        {
            bvector_type* bv = this->plains_[j];
            if (!bv)
            {
                bv = get_plain(j);
            }
            *bv |= *arg_bv;
        }
    } // for j
    return *this;
}


//---------------------------------------------------------------------

template<class Val, class BV>
bool sparse_vector<Val, BV>::equal(const sparse_vector<Val, BV>& sv) const
{
    size_type arg_size = sv.size();
    if (size_ != arg_size)
    {
        return false;
    }
    
    unsigned eff_plains = effective_plains();
    unsigned arg_eff_plains = sv.effective_plains();
    if (arg_eff_plains > eff_plains)
        eff_plains = arg_eff_plains;
    
    for (unsigned j = 0; j < eff_plains; ++j)
    {
        const bvector_type* bv = plains_[j];
        const bvector_type* arg_bv = sv.plains_[j];
        if (!bv && !arg_bv)
            continue;
        // check if any not NULL and not empty
        if (!bv && arg_bv)
        {
            if (arg_bv->any())
                return false;
        }
        if (bv && !arg_bv)
        {
            if (bv->any())
                return false;
        }
        // both not NULL
        int cmp = bv->compare(*arg_bv);
        if (cmp != 0)
            return false;
    } // for j
    return true;
}

//---------------------------------------------------------------------


} // namespace bm

#include "bmundef.h"


#endif
