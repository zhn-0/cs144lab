#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPConnection::send()
{
    while(!_sender.segments_out().empty())
    {
        TCPSegment &seg = _sender.segments_out().front();
        if(_rst)
        {
            seg.header().rst = true;
        }
        if(_receiver.ackno().has_value())
        {
            seg.header().ack = true;
            seg.header().ackno = _receiver.ackno().value();
            seg.header().win = _receiver.window_size();
        }
        _segments_out.push(seg);
        _sender.segments_out().pop();
        if(_rst)break;
    }
    while(!_sender.segments_out().empty())
        _sender.segments_out().pop();
}

void TCPConnection::set_error()
{
    _active = false; _rst = true;
    _sender.stream_in().set_error();
    _receiver.stream_out().set_error();
}

size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return _time_since_last_segment_received; }

void TCPConnection::segment_received(const TCPSegment &seg) { 
    _time_since_last_segment_received = 0;
    if(seg.header().rst)
    {
        set_error();
        return;
    }
    _receiver.segment_received(seg);
    if(_receiver.stream_out().input_ended() && !_sender.stream_in().input_ended())
    {
        _linger_after_streams_finish = false;
    }
    if(seg.header().ack)
    {
        _sender.ack_received(seg.header().ackno, seg.header().win);
    }
    if(seg.header().syn && _sender.next_seqno_absolute() == 0)
    {
        _sender.fill_window();
    }
    else if (seg.length_in_sequence_space() > 0 || 
            (_receiver.ackno().has_value() && (seg.length_in_sequence_space() == 0) &&
            seg.header().seqno == _receiver.ackno().value() - 1)) {
        _sender.send_empty_segment();
    }
    send();
}

bool TCPConnection::active() const { return _active; }

size_t TCPConnection::write(const string &data) {
    size_t ret = _sender.stream_in().write(data);
    _sender.fill_window();
    send();
    return ret;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) { 
    _sender.tick(ms_since_last_tick);
    _time_since_last_segment_received += ms_since_last_tick;
    if(_receiver.stream_out().input_ended() && _sender.stream_in().input_ended() && _sender.bytes_in_flight() == 0)
    {
        if(!_linger_after_streams_finish)_active = false;
        else if(_time_since_last_segment_received >= 10 * _cfg.rt_timeout)_active = false;
        if(!_active)return;
    }
    if(_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS)
    {
        // need to send a RST segment to the peer
        set_error();
        _sender.send_empty_segment();
    }
    send();
}

void TCPConnection::end_input_stream() { 
    _sender.stream_in().end_input();
    _sender.fill_window();
    send();
}

void TCPConnection::connect() {
    _sender.fill_window();
    send();
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";
            // Your code here: need to send a RST segment to the peer
            set_error();
            _sender.send_empty_segment();
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}
