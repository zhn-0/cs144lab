#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    const TCPHeader &header = seg.header();
    if(header.syn && !_syn) _isn = header.seqno, _syn = true;
    if(!_syn) return;
    uint64_t offset = unwrap(header.seqno, _isn, _reassembler.stream_out().bytes_written());
    if(offset == 0 && !header.syn)return;
    offset -= !header.syn;
    // std::cout << "DEBUG: " << seg.payload().str() << " " << offset << std::endl;
    _reassembler.push_substring(seg.payload().copy(), offset, header.fin);
    if(header.fin)_fin = true;
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    if(!_syn)return std::nullopt;
    return wrap(_reassembler.stream_out().bytes_written() + 1 + (_reassembler.stream_out().input_ended() && _fin), _isn);
 }

size_t TCPReceiver::window_size() const {
    return _reassembler.stream_out().remaining_capacity();
}
