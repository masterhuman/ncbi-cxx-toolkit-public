#ifndef BMFWD__H__INCLUDED__
#define BMFWD__H__INCLUDED__
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

#include "bmconst.h"

namespace bm
{

class block_allocator;
class ptr_allocator;

template<class BA = block_allocator, class PA = ptr_allocator> class mem_alloc;

template <class A, size_t N> class miniset;
template<size_t N> class bvmini;

typedef mem_alloc<block_allocator, ptr_allocator> standard_allocator;

template<class A = bm::standard_allocator> class bvector;



} // namespace

#endif
