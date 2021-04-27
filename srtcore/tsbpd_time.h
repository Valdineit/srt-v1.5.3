/*
 * SRT - Secure, Reliable, Transport
 * Copyright (c) 2021 Haivision Systems Inc.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 */

#ifndef INC_SRT_TSBPD_TIME_H
#define INC_SRT_TSBPD_TIME_H

#include "platform_sys.h"
#include "sync.h"
#include "utilities.h"

namespace srt
{

/// @brief TSBPD time conversion logic for receiver.
class CTsbpdTime
{
    typedef srt::sync::steady_clock  steady_clock;
    typedef steady_clock::time_point time_point;
    typedef steady_clock::duration   duration;
    typedef srt::sync::Mutex         Mutex;

public:
    CTsbpdTime()
        : m_bTsbPdMode(false)
        , m_tdTsbPdDelay(0)
        , m_bTsbPdWrapCheck(false)
    {
    }

    /// Set TimeStamp-Based Packet Delivery Mode (receiver).
    /// @param [in] timebase localtime base (uSec) of packet time stamps including buffering delay.
    /// @param [in] wrap Is in wrapping period.
    /// @param [in] delay aggreed TsbPD delay (buffering latency).
    void setTsbPdMode(const time_point& timebase, bool wrap, duration delay);

    /// @brief Check if TSBPD logic is enabled.
    /// @return true if TSBPD is enabled.
    bool isEnabled() const { return m_bTsbPdMode; }

    /// @brief Apply new state derived from other members of a socket group.
    /// @param timebase TSBPD base time.
    /// @param wrp wrap period (enabled or not).
    /// @param delay TSBPD delay.
    /// @param udrift Clock drift.
    void applyGroupTime(const time_point& timebase, bool wrp, uint32_t delay, const duration& udrift);

    /// @brief Apply new clock state (TSBPD base and drift) derived from other members of a socket group.
    /// @param timebase TSBPD base time.
    /// @param wrp the state of wrapping period (enabled or disabled).
    /// @param udrift Clock drift.
    void applyGroupDrift(const time_point& timebase, bool wrp, const duration& udrift);

    /// @brief Add new drift sample from an ACK-ACKACK pair.
    /// ACKACK pakcets are sent immideatly (except for UDP buffering).
    /// 
    /// @param [in] pktTimestamp Timestamp of the arrived ACKACK packet.
    /// @param [out] w_udrift Current clock drift value
    /// @param [out] w_newtimebase Current TSBPD base time
    /// 
    /// @return true if TSBPD base time was changed, false otherwise.
    bool addDriftSample(uint32_t                  pktTimestamp,
                        steady_clock::duration&   w_udrift,
                        steady_clock::time_point& w_newtimebase);

    /// @brief Get packet TSBPD time with buffering delay.
    /// The target time when to deliver the packet to an upstream application.
    /// @param [in] usPktTimestamp 32-bit value of packet timestamp field (microseconds).
    ///
    /// @returns Packet TSBPD base time with buffering delay.
    time_point getPktTsbPdTime(uint32_t usPktTimestamp) const;

    /// @brief Get packet TSBPD base time without buffering delay.
    /// @param [in] usPktTimestamp 32-bit value of packet timestamp field (microseconds).
    ///
    /// @returns Packet TSBPD base time without buffering delay.
    time_point getPktTsbPdBaseTime(uint32_t usPktTimestamp) const;

    /// @brief Get TSBPD base time considering possible carryover
    /// when packet timestamp is overflown and continues from 0.
    /// @param [in] usPktTimestamp 32-bit value of packet timestamp field (microseconds).
    ///
    /// @returns TSBPD base time for a provided packet timestamp.
    time_point getTsbPdTimeBase(uint32_t usPktTimestamp) const;

    /// @brief Handle timetamp of data packet to handle 32-bit integer carryover is about to happen.
    /// When packet timestamp approaches CPacket::MAX_TIMESTAMP, the TSBPD base time should be
    /// shifted accordingly to correctly handle new packets with timestamps starting from zero.
    /// @param usPktTimestamp timestamp field value of a data packet.
    void updateTsbPdTimeBase(uint32_t usPktTimestamp);

    /// @brief Get current drift value.
    /// @return current drift value.
    int64_t    drift() const { return m_DriftTracer.drift(); }

    /// @brief Get current overdrift value.
    /// @return current overdrift value.
    int64_t    overdrift() const { return m_DriftTracer.overdrift(); }

    /// @brief Get internal state to apply to another member of a socket group.
    /// @param w_tb TsbPd base time.
    /// @param w_udrift drift value.
    /// @param w_wrp wrap check.
    void getInternalTimeBase(time_point& w_tb, bool& w_wrp, duration& w_udrift) const;

private:
    bool       m_bTsbPdMode;      //< Rreceiver buffering and TSBPD is active when true.
    duration   m_tdTsbPdDelay;    //< Negotiated buffering delay.

    /// @brief Local time base for TsbPd.
    /// @note m_tsTsbPdTimeBase is chencged in the following cases:
    /// 1. Initialized upon SRT_CMD_HSREQ packet as the difference with the current time:
    ///    = (NOW - PACKET_TIMESTAMP), at the time of HSREQ reception.
    /// 2. Shifted forward on timestamp overflow (@c CTsbpdTime::updateTsbPdTimeBase), when overflow
    ///    of the timestamp field value of a data packet is detected.
    ///    += CPacket::MAX_TIMESTAMP + 1
    /// 3. Clock drift (@c CTsbpdTime::addDriftSample, executed exclusively
    ///    from ACKACK handler). This is updated with (positive or negative) TSBPD_DRIFT_MAX_VALUE
    ///    once the value of average drift exceeds this value in whatever direction.
    ///    += (+/-)TSBPD_DRIFT_MAX_VALUE
    ///
    /// @note The TSBPD base time is expected to hold the following condition:
    /// (PACKET_TIMESTAMP + m_tsTsbPdTimeBase + drift) == NOW.
    /// Then it can be used to estimate the origin time af a data packet, and calculate its delivery time
    /// with buffering delay applied.
    time_point m_tsTsbPdTimeBase;

    bool                  m_bTsbPdWrapCheck;                  //< true: check packet time stamp wrap around (overflow).
    static const uint32_t TSBPD_WRAP_PERIOD = (30 * 1000000); //< 30 seconds (in usec) for timestamp wrapping period.

    /// Maximum clock drift (microseconds) above which TsbPD base time is already adjusted.
    static const int TSBPD_DRIFT_MAX_VALUE = 5000;
    /// Number of samples (ACKACK packets) to perform drift caclulation and compensation.
    static const int TSBPD_DRIFT_MAX_SAMPLES = 1000;
    DriftTracer<TSBPD_DRIFT_MAX_SAMPLES, TSBPD_DRIFT_MAX_VALUE> m_DriftTracer;

    /// Protect simultaneous change of state (read/write).
    mutable Mutex m_mtxRW;
};

} // namespace srt

#endif // INC_SRT_TSBPD_TIME_H
