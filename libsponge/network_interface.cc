#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

#include <iostream>

// Dummy implementation of a network interface
// Translates from {IP datagram, next hop address} to link-layer frame, and from link-layer frame to IP datagram

// For Lab 5, please replace with a real implementation that passes the
// automated checks run by `make check_lab5`.

// You will need to add private members to the class declaration in `network_interface.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface(const EthernetAddress &ethernet_address, const Address &ip_address)
    : _ethernet_address(ethernet_address), _ip_address(ip_address) {
    cerr << "DEBUG: Network interface has Ethernet address " << to_string(_ethernet_address) << " and IP address "
         << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but may also be another host if directly connected to the same network as the destination)
//! (Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) with the Address::ipv4_numeric() method.)
void NetworkInterface::send_datagram(const InternetDatagram &dgram, const Address &next_hop) {
    // convert IP address of next hop to raw 32-bit representation (used in ARP header)
    const uint32_t next_hop_ip = next_hop.ipv4_numeric();
    EthernetFrame frame{};
    if(IP2Ethernet.count(next_hop_ip))
    {
        frame.header().type = EthernetHeader::TYPE_IPv4;
        frame.header().src = _ethernet_address;
        frame.header().dst = IP2Ethernet[next_hop_ip].first;
        frame.payload() = dgram.serialize();
        _frames_out.push(frame);
    }
    else if(!has_broadcast.count(next_hop_ip))
    {
        frame.header().type = EthernetHeader::TYPE_ARP;
        frame.header().src = _ethernet_address;
        frame.header().dst = ETHERNET_BROADCAST;
        ARPMessage arp{};
        arp.opcode = arp.OPCODE_REQUEST;
        arp.sender_ethernet_address = _ethernet_address;
        arp.sender_ip_address = _ip_address.ipv4_numeric();
        arp.target_ip_address = next_hop_ip;
        frame.payload() = arp.serialize();
        _frames_out.push(frame);
        waiting_ip.push({dgram, next_hop});
        has_broadcast[next_hop_ip] = 0;
    }
}

//! \param[in] frame the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
    if(frame.header().dst != ETHERNET_BROADCAST && frame.header().dst != _ethernet_address)
        return std::nullopt;
    if(frame.header().type == EthernetHeader::TYPE_IPv4)
    {
        InternetDatagram ret;
        if (ret.parse(frame.payload()) != ParseResult::NoError)
            return std::nullopt;
        return ret;
    }
    else if(frame.header().type == EthernetHeader::TYPE_ARP)
    {
        ARPMessage arp{};
        if (arp.parse(frame.payload()) != ParseResult::NoError)
            return std::nullopt;
        IP2Ethernet[arp.sender_ip_address] = {arp.sender_ethernet_address, 0};
        send_waiting();
        if(arp.opcode == arp.OPCODE_REQUEST && arp.target_ip_address == _ip_address.ipv4_numeric())
        {
            EthernetFrame send{};
            send.header().type = EthernetHeader::TYPE_ARP;
            send.header().src = _ethernet_address;
            send.header().dst = arp.sender_ethernet_address;
            ARPMessage arp_reply{};
            arp_reply.opcode = arp_reply.OPCODE_REPLY;
            arp_reply.sender_ethernet_address = _ethernet_address;
            arp_reply.sender_ip_address = _ip_address.ipv4_numeric();
            arp_reply.target_ethernet_address = arp.sender_ethernet_address;
            arp_reply.target_ip_address = arp.sender_ip_address;
            send.payload() = arp_reply.serialize();
            _frames_out.push(send);
        }
    }
    return std::nullopt;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) {
    std::map<uint32_t, std::pair<EthernetAddress, size_t>> tmp{};
    for(auto it=IP2Ethernet.begin();it!=IP2Ethernet.end();++it)
    {
        auto &[ip, p] = *it;
        auto &[et, t] = p;
        if(t + ms_since_last_tick >= 30000)continue;
        tmp[ip] = {et, t + ms_since_last_tick};
    }
    IP2Ethernet = std::move(tmp);
    std::map<uint32_t, size_t> tmp1{};
    for(auto it=has_broadcast.begin();it!=has_broadcast.end();++it)
    {
        auto &[ip, t] = *it;
        if(t + ms_since_last_tick >= 5000)continue;
        tmp1[ip] = t + ms_since_last_tick;
    }
    has_broadcast = std::move(tmp1);
}

void NetworkInterface::send_waiting()
{
    int sz = waiting_ip.size();
    while(sz--)
    {
        auto &t = waiting_ip.front();
        send_datagram(t.dgram, t.next_hop);
        waiting_ip.pop();
    }
}