/*
 * SRT - Secure, Reliable, Transport
 * Copyright (c) 2018 Haivision Systems Inc.
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 * 
 */

/*****************************************************************************
Copyright (c) 2001 - 2011, The Board of Trustees of the University of Illinois.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

* Redistributions of source code must retain the above
  copyright notice, this list of conditions and the
  following disclaimer.

* Redistributions in binary form must reproduce the
  above copyright notice, this list of conditions
  and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

* Neither the name of the University of Illinois
  nor the names of its contributors may be used to
  endorse or promote products derived from this
  software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*****************************************************************************/

/*****************************************************************************
written by
   Yunhong Gu, last updated 01/22/2011
modified by
   Haivision Systems Inc.
*****************************************************************************/

#ifndef __UDT_WINDOW_H__
#define __UDT_WINDOW_H__


#ifndef _WIN32
   #include <sys/time.h>
   #include <time.h>
#endif
#include "udt.h"
#include "packet.h"

namespace ACKWindowTools
{
   struct Seq
   {
       int32_t iACKSeqNo;       // Seq. No. for the ACK packet
       int32_t iACK;            // Data Seq. No. carried by the ACK packet
       srt::sync::steady_clock::time_point tsTimeStamp;      // The timestamp when the ACK was sent
   };

   void store(Seq* r_aSeq, const size_t size, int& r_iHead, int& r_iTail, int32_t seq, int32_t ack);
   int acknowledge(Seq* r_aSeq, const size_t size, int& r_iHead, int& r_iTail, int32_t seq, int32_t& r_ack);
}

template <size_t SIZE>
class CACKWindow
{
public:
    CACKWindow() :
        m_aSeq(),
        m_iHead(0),
        m_iTail(0)
    {
        m_aSeq[0].iACKSeqNo = -1;
    }

   ~CACKWindow() {}

      /// Write an ACK record into the window.
      /// @param [in] seq ACK seq. no.
      /// @param [in] ack DATA ACK no.

   void store(int32_t seq, int32_t ack)
   {
       return ACKWindowTools::store(m_aSeq, SIZE, m_iHead, m_iTail, seq, ack);
   }

      /// Search the ACK-2 "seq" in the window, find out the DATA "ack" and caluclate RTT .
      /// @param [in] seq ACK-2 seq. no.
      /// @param [out] ack the DATA ACK no. that matches the ACK-2 no.
      /// @return RTT.

   int acknowledge(int32_t seq, int32_t& r_ack)
   {
       return ACKWindowTools::acknowledge(m_aSeq, SIZE, m_iHead, m_iTail, seq, r_ack);
   }

private:

   typedef ACKWindowTools::Seq Seq;

   Seq m_aSeq[SIZE];
   int m_iHead;                 // Pointer to the lastest ACK record
   int m_iTail;                 // Pointer to the oldest ACK record

private:
   CACKWindow(const CACKWindow&);
   CACKWindow& operator=(const CACKWindow&);
};

////////////////////////////////////////////////////////////////////////////////

class CPktTimeWindowTools
{
public:
   static int getPktRcvSpeed_in(const int* window, int* replica, const int* bytes, size_t asize, int& bytesps);
   static int getBandwidth_in(const int* window, int* replica, size_t psize);

   static void initializeWindowArrays(int* r_pktWindow, int* r_probeWindow, int* r_bytesWindow, size_t asize, size_t psize);
};

template <size_t ASIZE = 16, size_t PSIZE = 16>
class CPktTimeWindow: CPktTimeWindowTools
{
public:
    CPktTimeWindow():
        m_aPktWindow(),
        m_aBytesWindow(),
        m_iPktWindowPtr(0),
        m_aProbeWindow(),
        m_iProbeWindowPtr(0),
        m_iLastSentTime(0),
        m_iMinPktSndInt(1000000),
        m_tsLastArrTime(srt::sync::steady_clock::now()),
        m_tsCurrArrTime(),
        m_tsProbeTime(),
        m_Probe1Sequence(-1)
    {
        CPktTimeWindowTools::initializeWindowArrays(m_aPktWindow, m_aProbeWindow, m_aBytesWindow, ASIZE, PSIZE);
    }

   ~CPktTimeWindow()
   {
   }

public:
   /// read the minimum packet sending interval.
   /// @return minimum packet sending interval (microseconds).

   int getMinPktSndInt() const { return m_iMinPktSndInt; }

   /// Calculate the packets arrival speed.
   /// @return Packet arrival speed (packets per second).

   int getPktRcvSpeed(ref_t<int> bytesps) const
   {
       // Lock access to the packet Window
       srt::sync::ScopedLock lock(m_lockPktWindow);

       int pktReplica[ASIZE];          // packet information window (inter-packet time)
       return getPktRcvSpeed_in(m_aPktWindow, pktReplica, m_aBytesWindow, ASIZE, *bytesps);
   }

   int getPktRcvSpeed() const
   {
       int bytesps;
       return getPktRcvSpeed(Ref(bytesps));
   }

   /// Estimate the bandwidth.
   /// @return Estimated bandwidth (packets per second).

   int getBandwidth() const
   {
       // Lock access to the packet Window
       srt::sync::ScopedLock lock(m_lockProbeWindow);

       int probeReplica[PSIZE];
       return getBandwidth_in(m_aProbeWindow, probeReplica, PSIZE);
   }

   /// Record time information of a packet sending.
   /// @param currtime  timestamp of the packet sending.

   void onPktSent(int currtime)
   {
       int interval = currtime - m_iLastSentTime;

       if ((interval < m_iMinPktSndInt) && (interval > 0))
           m_iMinPktSndInt = interval;

       m_iLastSentTime = currtime;
   }

   /// Record time information of an arrived packet.

   void onPktArrival(int pktsz = 0)
   {
       srt::sync::ScopedLock lock(m_lockPktWindow);

       m_tsCurrArrTime = srt::sync::steady_clock::now();

       // record the packet interval between the current and the last one
       m_aPktWindow[m_iPktWindowPtr] = count_microseconds(m_tsCurrArrTime - m_tsLastArrTime);
       m_aBytesWindow[m_iPktWindowPtr] = pktsz;

       // the window is logically circular
       ++ m_iPktWindowPtr;
       if (m_iPktWindowPtr == ASIZE)
           m_iPktWindowPtr = 0;

       // remember last packet arrival time
       m_tsLastArrTime = m_tsCurrArrTime;
   }

   /// Shortcut to test a packet for possible probe 1 or 2
   void probeArrival(const CPacket& pkt, bool unordered)
   {
       const int inorder16 = pkt.m_iSeqNo & PUMASK_SEQNO_PROBE;

       // for probe1, we want 16th packet
       if (inorder16 == 0)
       {
           probe1Arrival(pkt, unordered);
       }

       if (unordered)
           return;

       // for probe2, we want 17th packet
       if (inorder16 == 1)
       {
           probe2Arrival(pkt);
       }
   }

   /// Record the arrival time of the first probing packet.
   void probe1Arrival(const CPacket& pkt, bool unordered)
   {
       if (unordered && pkt.m_iSeqNo == m_Probe1Sequence)
       {
           // Reset the starting probe into "undefined", when
           // a packet has come as retransmitted before the
           // measurement at arrival of 17th could be taken.
           m_Probe1Sequence = -1;
           return;
       }

       m_tsProbeTime = srt::sync::steady_clock::now();
       m_Probe1Sequence = pkt.m_iSeqNo; // Record the sequence where 16th packet probe was taken
   }

   /// Record the arrival time of the second probing packet and the interval between packet pairs.

   void probe2Arrival(const CPacket& pkt)
   {
       // Reject probes that don't refer to the very next packet
       // towards the one that was lately notified by probe1Arrival.
       // Otherwise the result can be stupid.

       // Simply, in case when this wasn't called exactly for the
       // expected packet pair, behave as if the 17th packet was lost.

       // no start point yet (or was reset) OR not very next packet
       if (m_Probe1Sequence == -1 || CSeqNo::incseq(m_Probe1Sequence) != pkt.m_iSeqNo)
           return;

       // Grab the current time before trying to acquire
       // a mutex. This might add extra delay and therefore
       // screw up the measurement.
       const srt::sync::steady_clock::time_point now = srt::sync::steady_clock::now();

       // Lock access to the packet Window
       srt::sync::ScopedLock cg(m_lockProbeWindow);

       m_tsCurrArrTime = now;

       // Reset the starting probe to prevent checking if the
       // measurement was already taken.
       m_Probe1Sequence = -1;

       // record the probing packets interval
       // Adjust the time for what a complete packet would have take
       int64_t timediff = count_microseconds(m_tsCurrArrTime - m_tsProbeTime);
       int64_t timediff_times_pl_size = timediff * CPacket::SRT_MAX_PAYLOAD_SIZE;

       // Let's take it simpler than it is coded here:
       // (stating that a packet has never zero size)
       //
       // probe_case = (now - previous_packet_time) * SRT_MAX_PAYLOAD_SIZE / pktsz;
       //
       // Meaning: if the packet is fully packed, probe_case = timediff.
       // Otherwise the timediff will be "converted" to a time that a fully packed packet "would take",
       // provided the arrival time is proportional to the payload size and skipping
       // the ETH+IP+UDP+SRT header part elliminates the constant packet delivery time influence.
       //
       const size_t pktsz = pkt.getLength();
       m_aProbeWindow[m_iProbeWindowPtr] = pktsz ? timediff_times_pl_size / pktsz : int(timediff);

       // OLD CODE BEFORE BSTATS:
       // record the probing packets interval
       // m_aProbeWindow[m_iProbeWindowPtr] = int(m_tsCurrArrTime - m_tsProbeTime);

       // the window is logically circular
       ++ m_iProbeWindowPtr;
       if (m_iProbeWindowPtr == PSIZE)
           m_iProbeWindowPtr = 0;
   }

private:
   int m_aPktWindow[ASIZE];          // packet information window (inter-packet time)
   int m_aBytesWindow[ASIZE];        // 
   int m_iPktWindowPtr;         // position pointer of the packet info. window.
   mutable srt::sync::Mutex m_lockPktWindow; // used to synchronize access to the packet window

   int m_aProbeWindow[PSIZE];        // record inter-packet time for probing packet pairs
   int m_iProbeWindowPtr;       // position pointer to the probing window
   mutable srt::sync::Mutex m_lockProbeWindow; // used to synchronize access to the probe window

   int m_iLastSentTime;         // last packet sending time
   int m_iMinPktSndInt;         // Minimum packet sending interval

   srt::sync::steady_clock::time_point m_tsLastArrTime;      // last packet arrival time
   srt::sync::steady_clock::time_point m_tsCurrArrTime;      // current packet arrival time
   srt::sync::steady_clock::time_point m_tsProbeTime;        // arrival time of the first probing packet
   int32_t m_Probe1Sequence;    // sequence number for which the arrival time was notified

private:
   CPktTimeWindow(const CPktTimeWindow&);
   CPktTimeWindow &operator=(const CPktTimeWindow&);
};


#endif
