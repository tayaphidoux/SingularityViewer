/** 
 * @file lldarray.h
 * @brief Wrapped std::vector for backward compatibility.
 *
 * $LicenseInfo:firstyear=2001&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 * 
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

#ifndef LL_LLDARRAY_H
#define LL_LLDARRAY_H

#include "llerror.h"

#include <vector>
#include <map>

//--------------------------------------------------------
// LLIndexedVector
//--------------------------------------------------------

template <typename Type, typename Key, int BlockSize = 32> 
class LLIndexedVector
{
public:
	typedef typename std::vector<std::pair<Key, Type> > vec_type_t;
	typedef typename vec_type_t::iterator iterator;
	typedef typename vec_type_t::const_iterator const_iterator;
	typedef typename vec_type_t::reverse_iterator reverse_iterator;
	typedef typename vec_type_t::const_reverse_iterator const_reverse_iterator;
	typedef typename vec_type_t::size_type size_type;
protected:
	std::vector<std::pair<Key, Type> > mVector;
	
public:
	LLIndexedVector() { mVector.reserve(BlockSize); }
	
	iterator begin() { return mVector.begin(); }
	const_iterator begin() const { return mVector.begin(); }
	iterator end() { return mVector.end(); }
	const_iterator end() const { return mVector.end(); }

	reverse_iterator rbegin() { return mVector.rbegin(); }
	const_reverse_iterator rbegin() const { return mVector.rbegin(); }
	reverse_iterator rend() { return mVector.rend(); }
	const_reverse_iterator rend() const { return mVector.rend(); }

	void reset() { mVector.resize(0); }
	bool empty() const { return mVector.empty(); }
	size_type size() const { return mVector.size(); }
	
	Type& operator[](const Key& k)
	{
		return get_val_in_pair_vec(mVector, k);
	}

	const_iterator find(const Key& k) const
	{
		return std::find_if(mVector.begin(), mVector.end(), [&k](const typename vec_type_t::value_type& e) { return e.first == k; });
	}
};

#endif
