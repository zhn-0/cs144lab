#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _RTO(retx_timeout)
    , _stream(capacity) {}

uint64_t TCPSender::bytes_in_flight() const { return _bytes_in_flight; }

void TCPSender::fill_window() {
    uint64_t left = _next_seqno, right = _mx_ackno + max(static_cast<uint16_t>(1), _window_size);
    while(left <= right)
    {
        TCPSegment seg;
        TCPHeader &header = seg.header();
        if (_next_seqno == 0) 
            header.syn = true;
        header.seqno = next_seqno();
        size_t sz = right - left;
        seg.payload() = Buffer(_stream.read(min(sz, TCPConfig::MAX_PAYLOAD_SIZE) - header.syn));
        size_t seg_len = seg.length_in_sequence_space();
        if (_stream.eof() && left + seg_len + 1 <= right)
            header.fin = true, ++seg_len;
        if(!seg_len || _fin_sent)break;
        _fin_sent = header.fin;
        left += seg_len; _next_seqno += seg_len;
        _bytes_in_flight += seg_len;
        if (!_timer.running()) 
            _timer.start();
        _segments_out.push(seg);
        _outstanding.push(seg);
        // std::cout << "DEBUG:" << seg.payload().copy() << " " << _next_seqno << std::endl;
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    uint64_t _ackno = unwrap(ackno, _isn, _stream.bytes_read());
    if(_ackno > _next_seqno)return;
    _window_size = window_size;
    bool ok = false;
    while(!_outstanding.empty())
    {
        auto &seg = _outstanding.front();
        // std::cout << "DEBUG: " << unwrap(seg.header().seqno, _isn, _stream.bytes_read()) << " " << _ackno << std::endl;
        uint64_t segno = unwrap(seg.header().seqno, _isn, _stream.bytes_read());
        size_t seg_len = seg.length_in_sequence_space();
        if(segno + seg_len - 1 < _ackno)
        {
            ok = true;
            _bytes_in_flight -= seg_len;
            _mx_ackno = max(_mx_ackno, segno + seg_len);
            _outstanding.pop();
        }
        else break;
    }
    if(ok)
    {
        _RTO = _initial_retransmission_timeout;
        _consecutive_retransmissions = 0;
        if(!_outstanding.empty())_timer.start();
    }
    if(_outstanding.empty())_timer.stop();
    if(!_fin_sent)fill_window();
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) { 
    if(_timer.running())
    {
        _timer.pass_time(ms_since_last_tick, _RTO);
        if(_timer.expired())
        {
            _segments_out.push(_outstanding.front());
            if(_window_size > 0)
            {
                _RTO <<= 1;
                ++_consecutive_retransmissions;
            }
            _timer.start();
        }
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return _consecutive_retransmissions; }

void TCPSender::send_empty_segment() {
    TCPSegment seg;
    seg.header().seqno = next_seqno();
    _segments_out.push(seg);
}