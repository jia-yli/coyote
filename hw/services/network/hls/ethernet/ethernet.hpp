/*
 * Copyright (c) 2019, Systems Group, ETH Zurich
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef ETHERNET_HPP
#define ETHERNET_HPP

#include "../packet.hpp"

// #define VLANTAG

#ifndef VLANTAG
const int ETH_HEADER_SIZE = 112;
/**
 * [47:0] MAC destination
 * [95:48] MAC source
 * [111:96] Ethertype
 */ 
template <int W>
class ethHeader : public packetHeader<W, ETH_HEADER_SIZE> {
	using packetHeader<W, ETH_HEADER_SIZE>::header;

public:
    void setDstAddress(ap_uint<48> addr)
    {
        header(47,0) = addr;
    }
    void setSrcAddress(ap_uint<48> addr)
    {
        header(95,48) = addr;
    }
    void setEthertype(ap_uint<16> type)
    {
        header(111,96) = reverse(type);
    }
	ap_uint<16> getEthertype()
	{
		return reverse((ap_uint<16>)header(111,96));
	}
};
#else
const int ETH_HEADER_SIZE = 144;
const int ETH_HEADER_WOVLAN_SIZE = 112;
/**
 * [47:0] MAC destination
 * [95:48] MAC source
 * [127:96] 802.1Q tag
 * [143:128] Ethertype
 */ 
template <int W>
class ethHeader : public packetHeader<W, ETH_HEADER_SIZE> {
    using packetHeader<W, ETH_HEADER_SIZE>::header;

public:
    void setDstAddress(ap_uint<48> addr)
    {
        header(47,0) = addr;
    }
    void setSrcAddress(ap_uint<48> addr)
    {
        header(95,48) = addr;
    }
    void setVlanTag(ap_uint<32> vlantag)
    {
        header(127,96) = reverse(vlantag);
    }
    void setEthertype(ap_uint<16> type)
    {
        header(143,128) = reverse(type);
    }
    /** VLAN tag is removed for receiver on access port*/
    ap_uint<16> getEthertype()
    {
        return reverse((ap_uint<16>)header(111,96));
    }
};

#endif

#endif
