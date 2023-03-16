#ifndef PACKET_UTIL_EVENT_T
#define PACKET_UTIL_EVENT_T

#include "pq_index.h"

#include <sst/core/event.h>

class PacketUtilEvent : public SST::Event
{

public:
    unsigned int pkt_id;
    unsigned int pkt_size;
    unsigned int src_port;
    unsigned int dest_port;
    unsigned int priority;
    uint8_t      ip_ecn;

    int mg_index;
    int mg_util;

    int qg_index;
    int qg_util;

    unsigned int pq_index;
    int          pq_util;

    bool is_tcp = false;

private:
    /**
     * @brief Serialize the attributes to send them through the link
     *
     * @param ser
     */
    void serialize_order(SST::Core::Serialization::serializer& ser) override
    {
        Event::serialize_order(ser);
        ser& pkt_id;
        ser& pkt_size;
        ser& src_port;
        ser& dest_port;
        ser& priority;
        ser& ip_ecn;

        ser& mg_index;
        ser& mg_util;

        ser& qg_index;
        ser& qg_util;

        ser& pq_index;
        ser& pq_util;

        ser& is_tcp;
    }

    /**
     * @brief Construct a new Implement Serializable object
     *
     */
    ImplementSerializable(PacketUtilEvent);
};

/**
 * @brief Packet utilization event with TCP information
 **/
class TCPPacketUtilEvent : public PacketUtilEvent
{

public:
    TCPPacketUtilEvent() { is_tcp = true; }

    // TCP header
    uint16_t src_tcp_port;
    uint16_t dest_tcp_port;
    uint32_t ack_number;
    uint32_t seq_number;
    uint8_t  flags;
    uint16_t window_size;
    // IP header
    uint32_t src_ip_addr;
    uint32_t dest_ip_addr;
    // Ethernet header
    uint64_t src_mac_addr;
    uint64_t dest_mac_addr;
    uint16_t length_type;

private:
    /**
     * @brief Serialize the attributes to send them through the link
     *
     * @param ser
     */
    void serialize_order(SST::Core::Serialization::serializer& ser) override
    {
        Event::serialize_order(ser);
        ser& pkt_id;
        ser& pkt_size;
        ser& src_port;
        ser& dest_port;
        ser& priority;
        ser& ip_ecn;

        ser& mg_index;
        ser& mg_util;

        ser& qg_index;
        ser& qg_util;

        ser& pq_index;
        ser& pq_util;

        ser& is_tcp;

        ser& src_tcp_port;
        ser& dest_tcp_port;
        ser& ack_number;
        ser& seq_number;
        ser& flags;
        ser& window_size;
        ser& src_ip_addr;
        ser& dest_ip_addr;
        ser& src_mac_addr;
        ser& dest_mac_addr;
        ser& length_type;
    }

    /**
     * @brief Construct a new Implement Serializable object
     *
     */
    ImplementSerializable(TCPPacketUtilEvent);
};

class DualQPacketUtilEvent : public PacketUtilEvent
{

public:
    int lq_index;
    int lq_util;

private:
    /**
     * @brief Serialize the attributes to send them through the link
     *
     * @param ser
     */
    void serialize_order(SST::Core::Serialization::serializer& ser) override
    {
        Event::serialize_order(ser);
        ser& pkt_id;
        ser& pkt_size;
        ser& src_port;
        ser& dest_port;
        ser& priority;
        ser& ip_ecn;

        ser& mg_index;
        ser& mg_util;

        ser& qg_index;
        ser& qg_util;

        ser& pq_index;
        ser& pq_util;

        ser& lq_index;
        ser& lq_util;

        ser& is_tcp;
    }

    /**
     * @brief Construct a new Implement Serializable object
     *
     */
    ImplementSerializable(PacketUtilEvent);
};

/**
 * @brief Packet utilization event with TCP information
 **/
class DualQTCPPacketUtilEvent : public DualQPacketUtilEvent
{

public:
    DualQTCPPacketUtilEvent() { is_tcp = true; }

    // TCP header
    uint16_t src_tcp_port;
    uint16_t dest_tcp_port;
    uint32_t ack_number;
    uint32_t seq_number;
    uint8_t  flags;
    uint16_t window_size;
    // IP header
    uint32_t src_ip_addr;
    uint32_t dest_ip_addr;
    // Ethernet header
    uint64_t src_mac_addr;
    uint64_t dest_mac_addr;
    uint16_t length_type;

private:
    /**
     * @brief Serialize the attributes to send them through the link
     *
     * @param ser
     */
    void serialize_order(SST::Core::Serialization::serializer& ser) override
    {
        Event::serialize_order(ser);
        ser& pkt_id;
        ser& pkt_size;
        ser& src_port;
        ser& dest_port;
        ser& priority;
        ser& ip_ecn;

        ser& mg_index;
        ser& mg_util;

        ser& qg_index;
        ser& qg_util;

        ser& pq_index;
        ser& pq_util;

        ser& lq_index;
        ser& lq_util;

        ser& is_tcp;

        ser& src_tcp_port;
        ser& dest_tcp_port;
        ser& ack_number;
        ser& seq_number;
        ser& flags;
        ser& window_size;
        ser& src_ip_addr;
        ser& dest_ip_addr;
        ser& src_mac_addr;
        ser& dest_mac_addr;
        ser& length_type;
    }

    /**
     * @brief Construct a new Implement Serializable object
     *
     */
    ImplementSerializable(TCPPacketUtilEvent);
};

#endif