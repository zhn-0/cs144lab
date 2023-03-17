#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity) : _capacity(capacity), readen(0), written(0), stream("") {  }

size_t ByteStream::write(const string &data) {
    if(_eof)return 0;
    size_t i = remaining_capacity();
    if(i < data.size())
    {
        stream += data.substr(0, i);
        written += i;
        return i;
    }
    stream += data;
    written += data.size();
    return data.size();
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {  return stream.substr(0, len);  }

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) { stream = stream.substr(len); readen += len; }

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    std::string ret = peek_output(len);
    pop_output(ret.size());
    return ret;
}

void ByteStream::end_input() {  _eof = true;  }

bool ByteStream::input_ended() const { return _eof; }

size_t ByteStream::buffer_size() const { return stream.size(); }

bool ByteStream::buffer_empty() const { return stream.empty(); }

bool ByteStream::eof() const { return input_ended() && buffer_empty(); }

size_t ByteStream::bytes_written() const { return written; }

size_t ByteStream::bytes_read() const { return readen; }

size_t ByteStream::remaining_capacity() const { return _capacity - buffer_size(); }
