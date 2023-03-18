#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) : _output(capacity), _capacity(capacity) {  }

void StreamReassembler::insert_to_unassembled(const std::string &data, const size_t index, bool eof)
{
    if(unassembled.count(index))
    {
        auto &[str, _] = unassembled[index];
        if(data.size() > str.size())str = data, _ = _ || eof;
    }
    else unassembled[index] = {data, eof};
}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    first_unread = _output.bytes_read();
    first_unacceptable = first_unread + _capacity;
    if(index + data.size() <= first_unacceptable)
        insert_to_unassembled(std::move(data), index, eof);
    else
        insert_to_unassembled(std::move(data.substr(0, first_unacceptable - index)), index, false);
    while(!unassembled.empty())
    {
        auto &[idx, pair] = *unassembled.begin();
        if(idx <= first_unassembled)
        {
            if(idx + pair.first.size() > first_unassembled)
            {
                _output.write(pair.first.substr(first_unassembled - idx));
                first_unassembled = idx + pair.first.size();
            }
            if(pair.second) _output.end_input();
            unassembled.erase(unassembled.begin());
        }
        else break;
    }
    vector<std::pair<size_t, std::pair<std::string, bool>>> vec;
    num_of_unassembled_bytes = 0;
    for(auto it = unassembled.begin(); it != unassembled.end(); ++it)
    {
        if(vec.empty() || it->first > vec.back().first + vec.back().second.first.size())
        {
            vec.push_back(*it);
            num_of_unassembled_bytes += it->second.first.size();
            continue;
        }
        auto &back = vec.back();
        auto &idx = back.first; auto &str = back.second.first;
        assert(idx < it->first && it->first < idx + str.size());
        if(it->first + it->second.first.size() < idx + str.size())continue;
        num_of_unassembled_bytes += it->second.first.size() - idx - str.size() + it->first;
        str += it->second.first.substr(idx + str.size() - it->first);
    }
    unassembled = std::move(std::map<size_t, std::pair<std::string, bool>>(vec.begin(), vec.end()));
}

size_t StreamReassembler::unassembled_bytes() const { return num_of_unassembled_bytes; }

bool StreamReassembler::empty() const { return _output.buffer_empty(); }
