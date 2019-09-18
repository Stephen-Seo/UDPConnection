#include "UDPC_Defines.hpp"
#include "UDPConnection.h"

#include <cassert>
#include <chrono>
#include <cstring>
#include <optional>
#include <vector>
#include <functional>
#include <type_traits>
#include <string>
#include <sstream>
#include <ios>
#include <regex>

static std::regex ipv6_regex = std::regex(R"d((([0-9a-fA-F]{1,4}:){7,7}[0-9a-fA-F]{1,4}|([0-9a-fA-F]{1,4}:){1,7}:|([0-9a-fA-F]{1,4}:){1,6}:[0-9a-fA-F]{1,4}|([0-9a-fA-F]{1,4}:){1,5}(:[0-9a-fA-F]{1,4}){1,2}|([0-9a-fA-F]{1,4}:){1,4}(:[0-9a-fA-F]{1,4}){1,3}|([0-9a-fA-F]{1,4}:){1,3}(:[0-9a-fA-F]{1,4}){1,4}|([0-9a-fA-F]{1,4}:){1,2}(:[0-9a-fA-F]{1,4}){1,5}|[0-9a-fA-F]{1,4}:((:[0-9a-fA-F]{1,4}){1,6})|:((:[0-9a-fA-F]{1,4}){1,7}|:)|fe80:(:[0-9a-fA-F]{0,4}){0,4}%[0-9a-zA-Z]{1,}|::(ffff(:0{1,4}){0,1}:){0,1}((25[0-5]|(2[0-4]|1{0,1}[0-9]){0,1}[0-9])\.){3,3}(25[0-5]|(2[0-4]|1{0,1}[0-9]){0,1}[0-9])|([0-9a-fA-F]{1,4}:){1,4}:((25[0-5]|(2[0-4]|1{0,1}[0-9]){0,1}[0-9])\.){3,3}(25[0-5]|(2[0-4]|1{0,1}[0-9]){0,1}[0-9])))d");
// TODO remove ipv6_regex_nolink when link device is supported
static std::regex ipv6_regex_nolink = std::regex(R"d((([0-9a-fA-F]{1,4}:){7,7}[0-9a-fA-F]{1,4}|([0-9a-fA-F]{1,4}:){1,7}:|([0-9a-fA-F]{1,4}:){1,6}:[0-9a-fA-F]{1,4}|([0-9a-fA-F]{1,4}:){1,5}(:[0-9a-fA-F]{1,4}){1,2}|([0-9a-fA-F]{1,4}:){1,4}(:[0-9a-fA-F]{1,4}){1,3}|([0-9a-fA-F]{1,4}:){1,3}(:[0-9a-fA-F]{1,4}){1,4}|([0-9a-fA-F]{1,4}:){1,2}(:[0-9a-fA-F]{1,4}){1,5}|[0-9a-fA-F]{1,4}:((:[0-9a-fA-F]{1,4}){1,6})|:((:[0-9a-fA-F]{1,4}){1,7}|:)|::(ffff(:0{1,4}){0,1}:){0,1}((25[0-5]|(2[0-4]|1{0,1}[0-9]){0,1}[0-9])\.){3,3}(25[0-5]|(2[0-4]|1{0,1}[0-9]){0,1}[0-9])|([0-9a-fA-F]{1,4}:){1,4}:((25[0-5]|(2[0-4]|1{0,1}[0-9]){0,1}[0-9])\.){3,3}(25[0-5]|(2[0-4]|1{0,1}[0-9]){0,1}[0-9])))d");
static std::regex ipv4_regex = std::regex(R"d((1[0-9][0-9]|2[0-4][0-9]|25[0-5]|[1-9][0-9]|[0-9])\.(1[0-9][0-9]|2[0-4][0-9]|25[0-5]|[1-9][0-9]|[0-9])\.(1[0-9][0-9]|2[0-4][0-9]|25[0-5]|[1-9][0-9]|[0-9])\.(1[0-9][0-9]|2[0-4][0-9]|25[0-5]|[1-9][0-9]|[0-9]))d");

UDPC::SentPktInfo::SentPktInfo() :
id(0),
sentTime(std::chrono::steady_clock::now())
{}

std::size_t UDPC::ConnectionIdHasher::operator()(const UDPC_ConnectionId& key) const {
    std::string value((const char*)key.addr.s6_addr, 16);
    value.push_back((char)((key.port >> 8) & 0xFF));
    value.push_back((char)(key.port & 0xFF));
    return std::hash<std::string>()(value);
}

std::size_t UDPC::IPV6_Hasher::operator()(const struct in6_addr& addr) const {
    return std::hash<std::string>()(std::string((const char*)addr.s6_addr, 16));
}

bool operator ==(const UDPC_ConnectionId& a, const UDPC_ConnectionId& b) {
    return a.addr == b.addr && a.port == b.port;
}

bool operator ==(const struct in6_addr& a, const struct in6_addr& b) {
    for(unsigned int i = 0; i < 16; ++i) {
        if(a.s6_addr[i] != b.s6_addr[i]) {
            return false;
        }
    }
    return true;
}

UDPC::ConnectionData::ConnectionData() :
flags(),
id(0),
lseq(0),
rseq(0),
ack(0xFFFFFFFF),
timer(std::chrono::steady_clock::duration::zero()),
toggleT(UDPC::THIRTY_SECONDS),
toggleTimer(std::chrono::steady_clock::duration::zero()),
toggledTimer(std::chrono::steady_clock::duration::zero()),
addr({0}),
port(0),
sentPkts(),
sendPkts(UDPC_QUEUED_PKTS_MAX_SIZE),
priorityPkts(UDPC_QUEUED_PKTS_MAX_SIZE),
receivedPkts(UDPC_RECEIVED_PKTS_MAX_SIZE),
received(std::chrono::steady_clock::now()),
sent(std::chrono::steady_clock::now()),
rtt(std::chrono::steady_clock::duration::zero())
{
    flags.set(0);
    flags.reset(1);
}

UDPC::ConnectionData::ConnectionData(bool isServer, Context *ctx, struct in6_addr addr, uint16_t port) :
flags(),
id(0),
lseq(0),
rseq(0),
ack(0xFFFFFFFF),
timer(std::chrono::steady_clock::duration::zero()),
toggleT(UDPC::THIRTY_SECONDS),
toggleTimer(std::chrono::steady_clock::duration::zero()),
toggledTimer(std::chrono::steady_clock::duration::zero()),
addr(addr),
port(port),
sentPkts(),
sendPkts(UDPC_QUEUED_PKTS_MAX_SIZE),
priorityPkts(UDPC_QUEUED_PKTS_MAX_SIZE),
receivedPkts(UDPC_RECEIVED_PKTS_MAX_SIZE),
received(std::chrono::steady_clock::now()),
sent(std::chrono::steady_clock::now()),
rtt(std::chrono::steady_clock::duration::zero())
{
    flags.set(3);
    if(isServer) {
        id = UDPC::generateConnectionID(*ctx);
        flags.set(4);
    } else {
        lseq = 1;
    }
}

void UDPC::ConnectionData::cleanupSentPkts() {
    uint32_t id;
    while(sentPkts.size() > UDPC_SENT_PKTS_MAX_SIZE) {
        id = ntohl(*((uint32_t*)(sentPkts.front().data + 8)));
        auto iter = sentInfoMap.find(id);
        assert(iter != sentInfoMap.end()
                && "Sent packet must have correspoding entry in sentInfoMap");
        sentInfoMap.erase(iter);
        sentPkts.pop_front();
    }
}

UDPC::Context::Context(bool isThreaded) :
_contextIdentifier(UDPC_CONTEXT_IDENTIFIER),
flags(),
isAcceptNewConnections(true),
protocolID(UDPC_DEFAULT_PROTOCOL_ID),
#ifndef NDEBUG
loggingType(INFO),
#else
loggingType(WARNING),
#endif
atostrBufIndex(0),
rng_engine(),
mutex()
{
    if(isThreaded) {
        flags.set(0);
    } else {
        flags.reset(0);
    }
    flags.set(2);

    rng_engine.seed(std::chrono::system_clock::now().time_since_epoch().count());

    threadRunning.store(true);
}

void UDPC::Context::update_impl() {
    const auto now = std::chrono::steady_clock::now();
    std::chrono::steady_clock::duration dt = now - lastUpdated;
    std::chrono::steady_clock::duration temp_dt_fs;
    lastUpdated = now;

    {
        // check timed out, check good/bad mode with rtt, remove timed out
        std::vector<UDPC_ConnectionId> removed;
        for(auto iter = conMap.begin(); iter != conMap.end(); ++iter) {
            temp_dt_fs = now - iter->second.received;
            if(temp_dt_fs >= UDPC::CONNECTION_TIMEOUT) {
                removed.push_back(iter->first);
                log(
                    UDPC_LoggingType::VERBOSE,
                    "Timed out connection with ",
                    UDPC_atostr((UDPC_HContext)this, iter->first),
                    ", port = ",
                    iter->second.port);
                continue;
            }

            // check good/bad mode
            iter->second.toggleTimer += dt;
            iter->second.toggledTimer += dt;
            if(iter->second.flags.test(1) && !iter->second.flags.test(2)) {
                // good mode, bad rtt
                log(
                    UDPC_LoggingType::INFO,
                    "Switching to bad mode in connection with ",
                    UDPC_atostr((UDPC_HContext)this, iter->first),
                    ", port = ",
                    iter->second.port);
                iter->second.flags.reset(1);
                if(iter->second.toggledTimer <= UDPC::TEN_SECONDS) {
                    iter->second.toggleT *= 2;
                }
                iter->second.toggledTimer = std::chrono::steady_clock::duration::zero();
            } else if(iter->second.flags.test(1)) {
                // good mode, good rtt
                if(iter->second.toggleTimer >= UDPC::TEN_SECONDS) {
                    iter->second.toggleTimer = std::chrono::steady_clock::duration::zero();
                    iter->second.toggleT /= 2;
                    if(iter->second.toggleT < UDPC::ONE_SECOND) {
                        iter->second.toggleT = UDPC::ONE_SECOND;
                    }
                }
            } else if(!iter->second.flags.test(1) &&
                      iter->second.flags.test(2)) {
                // bad mode, good rtt
                if(iter->second.toggledTimer >= iter->second.toggleT) {
                    iter->second.toggleTimer = std::chrono::steady_clock::duration::zero();
                    iter->second.toggledTimer = std::chrono::steady_clock::duration::zero();
                    log(
                        UDPC_LoggingType::INFO,
                        "Switching to good mode in connection with ",
                        UDPC_atostr((UDPC_HContext)this, iter->first),
                        ", port = ",
                        iter->second.port);
                    iter->second.flags.set(1);
                }
            } else {
                // bad mode, bad rtt
                iter->second.toggledTimer = std::chrono::steady_clock::duration::zero();
            }

            iter->second.timer += dt;
            if(iter->second.flags.test(1)) {
                if(iter->second.timer >= UDPC::GOOD_MODE_SEND_RATE) {
                    iter->second.timer -= UDPC::GOOD_MODE_SEND_RATE;
                    iter->second.flags.set(0);
                }
            } else {
                if(iter->second.timer >= UDPC::BAD_MODE_SEND_RATE) {
                    iter->second.timer -= UDPC::BAD_MODE_SEND_RATE;
                    iter->second.flags.set(0);
                }
            }
        }
        for(auto iter = removed.begin(); iter != removed.end(); ++iter) {
            auto addrConIter = addrConMap.find(iter->addr);
            assert(addrConIter != addrConMap.end()
                    && "addrConMap must have an entry for a current connection");
            auto addrConSetIter = addrConIter->second.find(*iter);
            assert(addrConSetIter != addrConIter->second.end()
                    && "nested set in addrConMap must have an entry for a current connection");
            addrConIter->second.erase(addrConSetIter);
            if(addrConIter->second.empty()) {
                addrConMap.erase(addrConIter);
            }

            auto cIter = conMap.find(*iter);
            assert(cIter != conMap.end()
                    && "conMap must have the entry set to be removed");

            if(cIter->second.flags.test(4)) {
                idMap.erase(cIter->second.id);
            }

            conMap.erase(cIter);
        }
    }

    // update send (only if triggerSend flag is set)
    for(auto iter = conMap.begin(); iter != conMap.end(); ++iter) {
        if(!iter->second.flags.test(0)) {
            continue;
        }
        iter->second.flags.reset(0);

        if(iter->second.flags.test(3)) {
            if(flags.test(1)) {
                // is initiating connection to server
                auto initDT = now - iter->second.sent;
                if(initDT < UDPC::INIT_PKT_INTERVAL_DT) {
                    continue;
                }
                iter->second.sent = now;

                std::unique_ptr<char[]> buf = std::make_unique<char[]>(20);
                UDPC::preparePacket(
                    buf.get(),
                    protocolID,
                    0,
                    0,
                    0xFFFFFFFF,
                    nullptr,
                    0x1);

                struct sockaddr_in6 destinationInfo;
                destinationInfo.sin6_family = AF_INET6;
                std::memcpy(destinationInfo.sin6_addr.s6_addr, iter->first.addr.s6_addr, 16);
                destinationInfo.sin6_port = htons(iter->second.port);
                destinationInfo.sin6_flowinfo = 0;
                destinationInfo.sin6_scope_id = 0;
                long int sentBytes = sendto(
                    socketHandle,
                    buf.get(),
                    20,
                    0,
                    (struct sockaddr*) &destinationInfo,
                    sizeof(struct sockaddr_in6));
                if(sentBytes != 20) {
                    log(
                        UDPC_LoggingType::ERROR,
                        "Failed to send packet to initiate connection to ",
                        UDPC_atostr((UDPC_HContext)this, iter->first),
                        ", port = ",
                        iter->second.port);
                    continue;
                } else {
                    log(UDPC_LoggingType::INFO, "Sent initiate connection to ",
                        UDPC_atostr((UDPC_HContext)this, iter->first),
                        ", port = ",
                        iter->second.port);
                }
            } else {
                // is server, initiate connection to client
                iter->second.flags.reset(3);
                iter->second.sent = now;

                std::unique_ptr<char[]> buf = std::make_unique<char[]>(20);
                UDPC::preparePacket(
                    buf.get(),
                    protocolID,
                    iter->second.id,
                    iter->second.rseq,
                    iter->second.ack,
                    &iter->second.lseq,
                    0x1);

                struct sockaddr_in6 destinationInfo;
                destinationInfo.sin6_family = AF_INET6;
                std::memcpy(destinationInfo.sin6_addr.s6_addr, iter->first.addr.s6_addr, 16);
                destinationInfo.sin6_port = htons(iter->second.port);
                destinationInfo.sin6_flowinfo = 0;
                destinationInfo.sin6_scope_id = 0;
                long int sentBytes = sendto(
                    socketHandle,
                    buf.get(),
                    20,
                    0,
                    (struct sockaddr*) &destinationInfo,
                    sizeof(struct sockaddr_in6));
                if(sentBytes != 20) {
                    log(
                        UDPC_LoggingType::ERROR,
                        "Failed to send packet to initiate connection to ",
                        UDPC_atostr((UDPC_HContext)this, iter->first),
                        ", port = ",
                        iter->second.port);
                    continue;
                }
            }
            continue;
        }

        // Not initiating connection, send as normal on current connection
        if(iter->second.sendPkts.empty() && iter->second.priorityPkts.empty()) {
            // nothing in queues, send heartbeat packet
            auto sentDT = now - iter->second.sent;
            if(sentDT < UDPC::HEARTBEAT_PKT_INTERVAL_DT) {
                continue;
            }

            std::unique_ptr<char[]> buf = std::make_unique<char[]>(20);
            UDPC::preparePacket(
                buf.get(),
                protocolID,
                iter->second.id,
                iter->second.rseq,
                iter->second.ack,
                &iter->second.lseq,
                0);

            struct sockaddr_in6 destinationInfo;
            destinationInfo.sin6_family = AF_INET6;
            std::memcpy(destinationInfo.sin6_addr.s6_addr, iter->first.addr.s6_addr, 16);
            destinationInfo.sin6_port = htons(iter->second.port);
            destinationInfo.sin6_flowinfo = 0;
            destinationInfo.sin6_scope_id = 0;
            long int sentBytes = sendto(
                socketHandle,
                buf.get(),
                20,
                0,
                (struct sockaddr*) &destinationInfo,
                sizeof(struct sockaddr_in6));
            if(sentBytes != 20) {
                log(
                    UDPC_LoggingType::ERROR,
                    "Failed to send heartbeat packet to ",
                    UDPC_atostr((UDPC_HContext)this, iter->first),
                    ", port = ",
                    iter->second.port);
                continue;
            }

            UDPC_PacketInfo pInfo = UDPC::get_empty_pinfo();
            pInfo.sender.addr = in6addr_loopback;
            pInfo.receiver.addr = iter->first.addr;
            pInfo.sender.port = socketInfo.sin6_port;
            pInfo.receiver.port = iter->second.port;
            *((uint32_t*)(pInfo.data + 8)) = htonl(iter->second.lseq - 1);

            iter->second.sentPkts.push_back(std::move(pInfo));
            iter->second.cleanupSentPkts();

            // store other pkt info
            UDPC::SentPktInfo::Ptr sentPktInfo = std::make_shared<UDPC::SentPktInfo>();
            sentPktInfo->id = iter->second.lseq - 1;
            iter->second.sentInfoMap.insert(std::make_pair(sentPktInfo->id, sentPktInfo));
        } else {
            // sendPkts or priorityPkts not empty
            UDPC_PacketInfo pInfo = UDPC::get_empty_pinfo();
            bool isResending = false;
            if(!iter->second.priorityPkts.empty()) {
                // TODO verify getting struct copy is valid
                pInfo = std::move(iter->second.priorityPkts.top().value());
                iter->second.priorityPkts.pop();
                isResending = true;
            } else {
                pInfo = std::move(iter->second.sendPkts.top().value());
                iter->second.sendPkts.pop();
            }
            std::unique_ptr<char[]> buf = std::make_unique<char[]>(20 + pInfo.dataSize);
            UDPC::preparePacket(
                buf.get(),
                protocolID,
                iter->second.id,
                iter->second.rseq,
                iter->second.ack,
                &iter->second.lseq,
                (pInfo.flags & 0x4) | (isResending ? 0x8 : 0));
            std::memcpy(buf.get() + 20, pInfo.data, pInfo.dataSize);

            struct sockaddr_in6 destinationInfo;
            destinationInfo.sin6_family = AF_INET6;
            std::memcpy(destinationInfo.sin6_addr.s6_addr, iter->first.addr.s6_addr, 16);
            destinationInfo.sin6_port = htons(iter->second.port);
            long int sentBytes = sendto(
                socketHandle,
                buf.get(),
                pInfo.dataSize + 20,
                0,
                (struct sockaddr*) &destinationInfo,
                sizeof(struct sockaddr_in6));
            if(sentBytes != 20 + pInfo.dataSize) {
                log(
                    UDPC_LoggingType::ERROR,
                    "Failed to send packet to ",
                    UDPC_atostr((UDPC_HContext)this, iter->first),
                    ", port = ",
                    iter->second.port);
                continue;
            }

            if((pInfo.flags & 0x4) == 0) {
                // is check-received, store data in case packet gets lost
                UDPC_PacketInfo sentPInfo = UDPC::get_empty_pinfo();
                std::memcpy(sentPInfo.data, buf.get(), 20 + pInfo.dataSize);
                sentPInfo.flags = 0;
                sentPInfo.dataSize = 20 + pInfo.dataSize;
                sentPInfo.sender.addr = in6addr_loopback;
                sentPInfo.receiver.addr = iter->first.addr;
                sentPInfo.sender.port = socketInfo.sin6_port;
                sentPInfo.receiver.port = iter->second.port;

                iter->second.sentPkts.push_back(std::move(pInfo));
                iter->second.cleanupSentPkts();
            } else {
                // is not check-received, only id stored in data array
                UDPC_PacketInfo sentPInfo = UDPC::get_empty_pinfo();
                sentPInfo.flags = 0x4;
                sentPInfo.dataSize = 0;
                sentPInfo.sender.addr = in6addr_loopback;
                sentPInfo.receiver.addr = iter->first.addr;
                sentPInfo.sender.port = socketInfo.sin6_port;
                sentPInfo.receiver.port = iter->second.port;
                *((uint32_t*)(sentPInfo.data + 8)) = iter->second.lseq - 1;

                iter->second.sentPkts.push_back(std::move(pInfo));
                iter->second.cleanupSentPkts();
            }

            // store other pkt info
            UDPC::SentPktInfo::Ptr sentPktInfo = std::make_shared<UDPC::SentPktInfo>();
            sentPktInfo->id = iter->second.lseq - 1;
            iter->second.sentInfoMap.insert(std::make_pair(sentPktInfo->id, sentPktInfo));
        }
    }

    // receive packet
#if UDPC_PLATFORM == UDPC_PLATFORM_WINDOWS
    typedef int socklen_t;
#endif
    struct sockaddr_in6 receivedData;
    socklen_t receivedDataSize = sizeof(receivedData);
    int bytes = recvfrom(
        socketHandle,
        recvBuf,
        UDPC_PACKET_MAX_SIZE,
        0,
        (struct sockaddr*) &receivedData,
        &receivedDataSize);

    if(bytes == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
        // no packet was received
        return;
    } else if(bytes < 20) {
        // packet size is too small, invalid packet
        log(
            UDPC_LoggingType::INFO,
            "Received packet is smaller than header, ignoring packet from ",
            UDPC_atostr((UDPC_HContext)this, UDPC_ConnectionId{receivedData.sin6_addr, 0}),
            ", port = ",
            receivedData.sin6_port);
        return;
    }

    uint32_t temp = ntohl(*((uint32_t*)recvBuf));
    if(temp != protocolID) {
        // Invalid protocol id in packet
        log(
            UDPC_LoggingType::INFO,
            "Received packet has invalid protocol id, ignoring packet from ",
            UDPC_atostr((UDPC_HContext)this, UDPC_ConnectionId{receivedData.sin6_addr, 0}),
            ", port = ",
            ntohs(receivedData.sin6_port));
        return;
    }

    uint32_t conID = ntohl(*((uint32_t*)(recvBuf + 4)));
    uint32_t seqID = ntohl(*((uint32_t*)(recvBuf + 8)));
    uint32_t rseq = ntohl(*((uint32_t*)(recvBuf + 12)));
    uint32_t ack = ntohl(*((uint32_t*)(recvBuf + 16)));

    bool isConnect = conID & UDPC_ID_CONNECT;
    bool isPing = conID & UDPC_ID_PING;
    bool isNotRecChecked = conID & UDPC_ID_NO_REC_CHK;
    bool isResending = conID & UDPC_ID_RESENDING;
    conID &= 0x0FFFFFFF;

    UDPC_ConnectionId identifier{receivedData.sin6_addr, ntohs(receivedData.sin6_port)};

    if(isConnect && flags.test(2)) {
        // is connect packet and is accepting new connections
        if(!flags.test(1)
                && conMap.find(identifier) == conMap.end()) {
            // is receiving as server, connection did not already exist
            UDPC::ConnectionData newConnection(true, this, receivedData.sin6_addr, ntohs(receivedData.sin6_port));
            log(
                UDPC_LoggingType::VERBOSE,
                "Establishing connection with client ",
                UDPC_atostr((UDPC_HContext)this, UDPC_ConnectionId{receivedData.sin6_addr, 0}),
                ", port = ",
                ntohs(receivedData.sin6_port),
                ", giving client id = ", newConnection.id);

            idMap.insert(std::make_pair(newConnection.id, identifier));
            conMap.insert(std::make_pair(identifier, std::move(newConnection)));
            auto addrConIter = addrConMap.find(identifier.addr);
            if(addrConIter == addrConMap.end()) {
                auto insertResult = addrConMap.insert(
                    std::make_pair(
                        identifier.addr,
                        std::unordered_set<UDPC_ConnectionId, UDPC::ConnectionIdHasher>{}
                    ));
                assert(insertResult.second
                        && "Must successfully insert into addrConMap");
                addrConIter = insertResult.first;
            }
            addrConIter->second.insert(identifier);
            // TODO trigger event server established connection with client
        } else if (flags.test(1)) {
            // is client
            auto iter = conMap.find(identifier);
            if(iter == conMap.end() || !iter->second.flags.test(3)) {
                return;
            }
            iter->second.flags.reset(3);
            iter->second.id = conID;
            iter->second.flags.set(4);
            log(
                UDPC_LoggingType::VERBOSE,
                "Established connection with server ",
                UDPC_atostr((UDPC_HContext)this, UDPC_ConnectionId{receivedData.sin6_addr, 0}),
                ", port = ",
                ntohs(receivedData.sin6_port),
                ", got id = ", conID);
            // TODO trigger event client established connection with server
        }
        return;
    }

    auto iter = conMap.find(identifier);
    if(iter == conMap.end() || iter->second.flags.test(3)
            || !iter->second.flags.test(4) || iter->second.id != conID) {
        return;
    } else if(isPing) {
        iter->second.flags.set(0);
    }

    // packet is valid
    log(
        UDPC_LoggingType::INFO,
        "Received valid packet from ",
        UDPC_atostr((UDPC_HContext)this, UDPC_ConnectionId{receivedData.sin6_addr, 0}),
        ", port = ",
        ntohs(receivedData.sin6_port),
        ", packet id = ", seqID,
        ", good mode = ", iter->second.flags.test(1) ? "yes" : "no",
        isPing ? ", ping" : "");

    // update rtt
    for(auto sentIter = iter->second.sentPkts.rbegin(); sentIter != iter->second.sentPkts.rend(); ++sentIter) {
        uint32_t id = ntohl(*((uint32_t*)(sentIter->data + 8)));
        if(id == rseq) {
            auto sentInfoIter = iter->second.sentInfoMap.find(id);
            assert(sentInfoIter != iter->second.sentInfoMap.end()
                    && "sentInfoMap should have known stored id");
            auto diff = now - sentInfoIter->second->sentTime;
            if(diff > iter->second.rtt) {
                iter->second.rtt += (diff - iter->second.rtt) / 10;
            } else {
                iter->second.rtt -= (iter->second.rtt - diff) / 10;
            }

            iter->second.flags.set(2, iter->second.rtt <= UDPC::GOOD_RTT_LIMIT);

            log(
                UDPC_LoggingType::INFO,
                "RTT: ",
                UDPC::durationToFSec(iter->second.rtt) * 1000.0f,
                " milliseconds");
            break;
        }
    }

    iter->second.received = now;

    // check pkt timeout
    --rseq;
    for(; ack != 0; ack = ack << 1) {
        if((ack & 0x80000000) != 0) {
            --rseq;
            continue;
        }

        // pkt not received yet, find it in sent to check if it timed out
        for(auto sentIter = iter->second.sentPkts.rbegin(); sentIter != iter->second.sentPkts.rend(); ++sentIter) {
            uint32_t sentID = ntohl(*((uint32_t*)(sentIter->data + 8)));
            if(sentID == rseq) {
                if((sentIter->flags & 0x4) != 0 || (sentIter->flags & 0x8) != 0) {
                    // already resent or not rec-checked pkt
                    break;
                }
                auto sentInfoIter = iter->second.sentInfoMap.find(sentID);
                assert(sentInfoIter != iter->second.sentInfoMap.end()
                        && "Every entry in sentPkts must have a corresponding entry in sentInfoMap");
                auto duration = now - sentInfoIter->second->sentTime;
                if(duration > UDPC::PACKET_TIMEOUT_TIME) {
                    if(sentIter->dataSize <= 20) {
                        log(
                            UDPC_LoggingType::INFO,
                            "Timed out packet has no payload (probably "
                            "heartbeat packet), ignoring it");
                        sentIter->flags |= 0x8;
                        break;
                    }

                    UDPC_PacketInfo resendingData = UDPC::get_empty_pinfo();
                    resendingData.dataSize = sentIter->dataSize - 20;
                    std::memcpy(resendingData.data, sentIter->data + 20, resendingData.dataSize);
                    resendingData.flags = 0;
                    iter->second.priorityPkts.push(resendingData);
                }
                break;
            }
        }

        --rseq;
    }

    // calculate sequence and ack
    bool isOutOfOrder = false;
    uint32_t diff = 0;
    if(seqID > iter->second.rseq) {
        diff = seqID - iter->second.rseq;
        if(diff <= 0x7FFFFFFF) {
            // sequence is more recent
            iter->second.rseq = seqID;
            iter->second.ack = (iter->second.ack >> diff) | 0x80000000;
        } else {
            // sequence is older, recalc diff
            diff = 0xFFFFFFFF - seqID + 1 + iter->second.rseq;
            if((iter->second.ack & (0x80000000 >> (diff - 1))) != 0) {
                // already received packet
                log(
                    UDPC_LoggingType::INFO,
                    "Received packet is already marked as received, ignoring it");
                return;
            }
            iter->second.ack |= 0x80000000 >> (diff - 1);
            isOutOfOrder = true;
        }
    } else if(seqID < iter->second.rseq) {
        diff = iter->second.rseq - seqID;
        if(diff <= 0x7FFFFFFF) {
            // sequence is older
            if((iter->second.ack & (0x80000000 >> (diff - 1))) != 0) {
                // already received packet
                log(
                    UDPC_LoggingType::INFO,
                    "Received packet is already marked as received, ignoring it");
                return;
            }
            iter->second.ack |= 0x80000000 >> (diff - 1);
            isOutOfOrder = true;
        } else {
            // sequence is more recent, recalc diff
            diff = 0xFFFFFFFF - iter->second.rseq + 1 + seqID;
            iter->second.rseq = seqID;
            iter->second.ack = (iter->second.ack >> diff) | 0x80000000;
        }
    } else {
        // already received packet
        log(
            UDPC_LoggingType::INFO,
            "Received packet is already marked as received, ignoring it");
        return;
    }

    if(isOutOfOrder) {
        log(
            UDPC_LoggingType::VERBOSE,
            "Received packet is out of order");
    }

    if(bytes > 20) {
        UDPC_PacketInfo recPktInfo = UDPC::get_empty_pinfo();
        std::memcpy(recPktInfo.data, recvBuf, bytes);
        recPktInfo.dataSize = bytes;
        recPktInfo.flags =
            (isConnect ? 0x1 : 0)
            | (isPing ? 0x2 : 0)
            | (isNotRecChecked ? 0x4 : 0)
            | (isResending ? 0x8 : 0);
        recPktInfo.sender.addr = receivedData.sin6_addr;
        recPktInfo.receiver.addr = in6addr_loopback;
        recPktInfo.sender.port = ntohs(receivedData.sin6_port);
        recPktInfo.receiver.port = ntohs(socketInfo.sin6_port);

        if(iter->second.receivedPkts.size() == iter->second.receivedPkts.capacity()) {
            log(
                UDPC_LoggingType::WARNING,
                "receivedPkts is full, removing oldest entry to make room");
            iter->second.receivedPkts.pop();
        }

        iter->second.receivedPkts.push(recPktInfo);
    } else if(bytes == 20) {
        log(
            UDPC_LoggingType::VERBOSE,
            "Received packet has no payload (probably heartbeat packet)");
    }
}

UDPC::Context *UDPC::verifyContext(UDPC_HContext ctx) {
    if(ctx == nullptr) {
        return nullptr;
    }
    UDPC::Context *c = (UDPC::Context *)ctx;
    if(c->_contextIdentifier == UDPC_CONTEXT_IDENTIFIER) {
        return c;
    } else {
        return nullptr;
    }
}

bool UDPC::isBigEndian() {
    static std::optional<bool> isBigEndian = std::nullopt;
    if(isBigEndian) {
        return *isBigEndian;
    }
    union {
        uint32_t i;
        char c[4];
    } bint = {0x01020304};

    isBigEndian = (bint.c[0] == 1);
    return *isBigEndian;
}

void UDPC::preparePacket(char *data, uint32_t protocolID, uint32_t conID,
                         uint32_t rseq, uint32_t ack, uint32_t *seqID,
                         int flags) {
    uint32_t temp;

    temp = htonl(protocolID);
    std::memcpy(data, &temp, 4);
    temp = htonl(conID | ((flags & 0x1) != 0 ? UDPC_ID_CONNECT : 0) |
                 ((flags & 0x2) != 0 ? UDPC_ID_PING : 0) |
                 ((flags & 0x4) != 0 ? UDPC_ID_NO_REC_CHK : 0) |
                 ((flags & 0x8) != 0 ? UDPC_ID_RESENDING : 0));
    std::memcpy(data + 4, &temp, 4);

    if(seqID) {
        temp = htonl(*seqID);
        ++(*seqID);
    } else {
        temp = 0;
    }
    std::memcpy(data + 8, &temp, 4);

    temp = htonl(rseq);
    std::memcpy(data + 12, &temp, 4);
    temp = htonl(ack);
    std::memcpy(data + 16, &temp, 4);
}

uint32_t UDPC::generateConnectionID(Context &ctx) {
    auto dist = std::uniform_int_distribution<uint32_t>(0, 0x0FFFFFFF);
    uint32_t id = dist(ctx.rng_engine);
    while(ctx.idMap.find(id) != ctx.idMap.end()) {
        id = dist(ctx.rng_engine);
    }
    return id;
}

float UDPC::durationToFSec(const std::chrono::steady_clock::duration& duration) {
    return (float)duration.count()
        * (float)std::decay_t<decltype(duration)>::period::num
        / (float)std::decay_t<decltype(duration)>::period::den;
}

float UDPC::timePointsToFSec(
        const std::chrono::steady_clock::time_point& older,
        const std::chrono::steady_clock::time_point& newer) {
    const auto dt = newer - older;
    return (float)dt.count()
        * (float)decltype(dt)::period::num / (float)decltype(dt)::period::den;
}

UDPC_PacketInfo UDPC::get_empty_pinfo() {
    return UDPC_PacketInfo {
        {0},     // data (array)
        0,       // flags
        0,       // dataSize
        {        // sender
            {0},   // ipv6 addr
            0      // port
        },
        {        // receiver
            {0},   // ipv6 addr
            0      // port
        },
    };
}

void UDPC::threadedUpdate(Context *ctx) {
    auto now = std::chrono::steady_clock::now();
    decltype(now) nextNow;
    while(ctx->threadRunning.load()) {
        now = std::chrono::steady_clock::now();
        ctx->mutex.lock();
        ctx->update_impl();
        ctx->mutex.unlock();
        nextNow = std::chrono::steady_clock::now();
        std::this_thread::sleep_for(std::chrono::milliseconds(33) - (nextNow - now));
    }
}

UDPC_ConnectionId UDPC_create_id(struct in6_addr addr, uint16_t port) {
    return UDPC_ConnectionId{addr, port};
}

UDPC_ConnectionId UDPC_create_id_anyaddr(uint16_t port) {
    return UDPC_ConnectionId{in6addr_any, port};
}

UDPC_HContext UDPC_init(UDPC_ConnectionId listenId, int isClient) {
    UDPC::Context *ctx = new UDPC::Context(false);
    ctx->flags.set(1, isClient != 0);

    ctx->log(UDPC_LoggingType::INFO, "Got listen addr ",
        UDPC_atostr((UDPC_HContext)ctx, listenId));

    // create socket
    ctx->socketHandle = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
    if(ctx->socketHandle <= 0) {
        // TODO maybe different way of handling init fail
        ctx->log(UDPC_LoggingType::ERROR, "Failed to create socket");
        delete ctx;
        return nullptr;
    }

    // allow ipv4 connections on ipv6 socket
    {
        int no = 0;
        setsockopt(ctx->socketHandle, IPPROTO_IPV6, IPV6_V6ONLY, &no, sizeof(no));
    }

    // bind socket
    ctx->socketInfo.sin6_family = AF_INET6;
    ctx->socketInfo.sin6_addr = listenId.addr;
    ctx->socketInfo.sin6_port = htons(listenId.port);
    ctx->socketInfo.sin6_flowinfo = 0;
    ctx->socketInfo.sin6_scope_id = 0;
    if(bind(ctx->socketHandle, (const struct sockaddr *)&ctx->socketInfo,
            sizeof(struct sockaddr_in6)) < 0) {
        // TODO maybe different way of handling init fail
        ctx->log(UDPC_LoggingType::ERROR, "Failed to bind socket");
        CleanupSocket(ctx->socketHandle);
        delete ctx;
        return nullptr;
    }

    // TODO verify this is necessary to get the listen port
    if(ctx->socketInfo.sin6_port == 0) {
        struct sockaddr_in6 getInfo;
        socklen_t size = sizeof(struct sockaddr_in6);
        if(getsockname(ctx->socketHandle, (struct sockaddr *)&getInfo, &size) == 0) {
            ctx->socketInfo.sin6_port = getInfo.sin6_port;
        }
    }

    // set non-blocking on socket
#if UDPC_PLATFORM == UDPC_PLATFORM_MAC || UDPC_PLATFORM == UDPC_PLATFORM_LINUX
    int nonblocking = 1;
    if(fcntl(ctx->socketHandle, F_SETFL, O_NONBLOCK, nonblocking) == -1) {
#elif UDPC_PLATFORM == UDPC_PLATFORM_WINDOWS
    DWORD nonblocking = 1;
    if(ioctlsocket(ctx->socketHandle, FIONBIO, &nonblocking) != 0) {
#else
    {
#endif
        // TODO maybe different way of handling init fail
        ctx->log(UDPC_LoggingType::ERROR, "Failed to set nonblocking on socket");
        CleanupSocket(ctx->socketHandle);
        delete ctx;
        return nullptr;
    }

    ctx->log(UDPC_LoggingType::INFO, "Initialized UDPC");

    return (UDPC_HContext) ctx;
}

UDPC_HContext UDPC_init_threaded_update(UDPC_ConnectionId listenId,
                                int isClient) {
    UDPC::Context *ctx = (UDPC::Context *)UDPC_init(listenId, isClient);
    if(!ctx) {
        return nullptr;
    }
    ctx->flags.set(0);
    ctx->thread = std::thread(UDPC::threadedUpdate, ctx);

    ctx->log(UDPC_LoggingType::INFO, "Initialized threaded UDPC");

    return (UDPC_HContext) ctx;
}

void UDPC_destroy(UDPC_HContext ctx) {
    UDPC::Context *UDPC_ctx = UDPC::verifyContext(ctx);
    if(UDPC_ctx) {
        if(UDPC_ctx->flags.test(0)) {
            UDPC_ctx->threadRunning.store(false);
            UDPC_ctx->thread.join();
        }
        delete UDPC_ctx;
    }
}

void UDPC_update(UDPC_HContext ctx) {
    UDPC::Context *c = UDPC::verifyContext(ctx);
    if(!c || c->flags.test(0)) {
        // invalid or is threaded, update should not be called
        return;
    }

    c->update_impl();
}

void UDPC_client_initiate_connection(UDPC_HContext ctx, UDPC_ConnectionId connectionId) {
    UDPC::Context *c = UDPC::verifyContext(ctx);
    if(!c || !c->flags.test(1)) {
        return;
    }

    c->log(UDPC_LoggingType::INFO, "client_initiate_connection: Got peer a = ",
        UDPC_atostr((UDPC_HContext)ctx, connectionId),
        ", p = ", connectionId.port);

    std::lock_guard<std::mutex> lock(c->mutex);

    UDPC::ConnectionData newCon(false, c, connectionId.addr, connectionId.port);
    newCon.sent = std::chrono::steady_clock::now() - UDPC::INIT_PKT_INTERVAL_DT;

    if(c->conMap.find(connectionId) == c->conMap.end()) {
        c->conMap.insert(std::make_pair(connectionId, std::move(newCon)));
        auto addrConIter = c->addrConMap.find(connectionId.addr);
        if(addrConIter == c->addrConMap.end()) {
            auto insertResult = c->addrConMap.insert(std::make_pair(
                    connectionId.addr,
                    std::unordered_set<UDPC_ConnectionId, UDPC::ConnectionIdHasher>{}
                ));
            assert(insertResult.second);
            addrConIter = insertResult.first;
        }
        addrConIter->second.insert(connectionId);
        c->log(UDPC_LoggingType::VERBOSE, "client_initiate_connection: Initiating connection...");
    } else {
        c->log(UDPC_LoggingType::ERROR, "client_initiate_connection: Already connected to peer");
    }
}

int UDPC_get_queue_send_available(UDPC_HContext ctx, UDPC_ConnectionId connectionId) {
    UDPC::Context *c = UDPC::verifyContext(ctx);
    if(!c) {
        return 0;
    }

    std::lock_guard<std::mutex> lock(c->mutex);

    auto iter = c->conMap.find(connectionId);
    if(iter != c->conMap.end()) {
        return iter->second.sendPkts.capacity() - iter->second.sendPkts.size();
    } else {
        return 0;
    }
}

void UDPC_queue_send(UDPC_HContext ctx, UDPC_ConnectionId destinationId,
                     uint32_t isChecked, void *data, uint32_t size) {
    if(size == 0 || !data) {
        return;
    }

    UDPC::Context *c = UDPC::verifyContext(ctx);
    if(!c) {
        return;
    }

    std::lock_guard<std::mutex> lock(c->mutex);

    auto iter = c->conMap.find(destinationId);
    if(iter == c->conMap.end()) {
        c->log(
            UDPC_LoggingType::ERROR,
            "Failed to add packet to queue, no established connection "
            "with recipient");
        return;
    }

    UDPC_PacketInfo sendInfo = UDPC::get_empty_pinfo();
    std::memcpy(sendInfo.data, data, size);
    sendInfo.dataSize = size;
    sendInfo.sender.addr = in6addr_loopback;
    sendInfo.sender.port = c->socketInfo.sin6_port;
    sendInfo.receiver.addr = destinationId.addr;
    sendInfo.receiver.port = iter->second.port;
    sendInfo.flags = (isChecked ? 0x0 : 0x4);

    iter->second.sendPkts.push(sendInfo);
}

int UDPC_set_accept_new_connections(UDPC_HContext ctx, int isAccepting) {
    UDPC::Context *c = UDPC::verifyContext(ctx);
    if(!c) {
        return 0;
    }
    std::lock_guard<std::mutex> lock(c->mutex);
    return c->isAcceptNewConnections.exchange(isAccepting == 0 ? false : true);
}

int UDPC_drop_connection(UDPC_HContext ctx, UDPC_ConnectionId connectionId, bool dropAllWithAddr) {
    UDPC::Context *c = UDPC::verifyContext(ctx);
    if(!c) {
        return 0;
    }

    std::lock_guard<std::mutex> lock(c->mutex);

    if(dropAllWithAddr) {
        auto addrConIter = c->addrConMap.find(connectionId.addr);
        if(addrConIter != c->addrConMap.end()) {
            for(auto identIter = addrConIter->second.begin();
                    identIter != addrConIter->second.end();
                    ++identIter) {
                auto conIter = c->conMap.find(*identIter);
                assert(conIter != c->conMap.end());
                if(conIter->second.flags.test(4)) {
                    c->idMap.erase(conIter->second.id);
                }
                c->conMap.erase(conIter);
            }
            c->addrConMap.erase(addrConIter);
            return 1;
        }
    } else {
        auto iter = c->conMap.find(connectionId);
        if(iter != c->conMap.end()) {
            if(iter->second.flags.test(4)) {
                c->idMap.erase(iter->second.id);
            }
            auto addrConIter = c->addrConMap.find(connectionId.addr);
            if(addrConIter != c->addrConMap.end()) {
                addrConIter->second.erase(connectionId);
                if(addrConIter->second.empty()) {
                    c->addrConMap.erase(addrConIter);
                }
            }
            c->conMap.erase(iter);
            return 1;
        }
    }

    return 0;
}

int UDPC_has_connection(UDPC_HContext ctx, UDPC_ConnectionId connectionId) {
    UDPC::Context *c = UDPC::verifyContext(ctx);
    if(!c) {
        return 0;
    }

    std::lock_guard<std::mutex> lock(c->mutex);

    return c->conMap.find(connectionId) == c->conMap.end() ? 0 : 1;
}

uint32_t UDPC_set_protocol_id(UDPC_HContext ctx, uint32_t id) {
    UDPC::Context *c = UDPC::verifyContext(ctx);
    if(!c) {
        return 0;
    }
    std::lock_guard<std::mutex> lock(c->mutex);
    return c->protocolID.exchange(id);
}

UDPC_LoggingType UDPC_set_logging_type(UDPC_HContext ctx, UDPC_LoggingType loggingType) {
    UDPC::Context *c = UDPC::verifyContext(ctx);
    if(!c) {
        return UDPC_LoggingType::SILENT;
    }
    return static_cast<UDPC_LoggingType>(c->loggingType.exchange(loggingType));
}

UDPC_PacketInfo UDPC_get_received(UDPC_HContext ctx) {
    UDPC::Context *c = UDPC::verifyContext(ctx);
    if(!c) {
        return UDPC::get_empty_pinfo();
    }

    std::lock_guard<std::mutex> lock(c->mutex);

    // TODO impl
    return UDPC::get_empty_pinfo();
}

const char *UDPC_atostr(UDPC_HContext ctx, UDPC_ConnectionId connectionId) {
    UDPC::Context *c = UDPC::verifyContext(ctx);
    if(!c) {
        return nullptr;
    }
    const uint32_t headIndex =
        c->atostrBufIndex.fetch_add(UDPC_ATOSTR_BUFSIZE) % UDPC_ATOSTR_SIZE;
    uint32_t index = headIndex;
    bool usedDouble = false;

    for(unsigned int i = 0; i < 16; ++i) {
        if(i != 0 && i % 2 == 0) {
            if(headIndex - index > 1 && c->atostrBuf[index - 1] == ':') {
                if(usedDouble) {
                    if(c->atostrBuf[index - 2] != ':') {
                        c->atostrBuf[index++] = '0';
                        c->atostrBuf[index++] = ':';
                    } else {
                        // continue use of double :, do nothing here
                    }
                } else {
                    usedDouble = true;
                    c->atostrBuf[index++] = ':';
                }
            } else {
                c->atostrBuf[index++] = ':';
            }
        }

        if(connectionId.addr.s6_addr[i] == 0) {
            continue;
        } else {
            std::stringstream sstream;
            sstream << std::hex << (unsigned int) connectionId.addr.s6_addr[i];
            std::string out(sstream.str());
            if(out.size() == 1) {
                if(out[0] != '0') {
                    std::memcpy(c->atostrBuf + index, out.c_str(), 1);
                    ++index;
                }
            } else {
                std::memcpy(c->atostrBuf + index, out.c_str(), 2);
                index += 2;
            }
        }
    }
    if(c->atostrBuf[index - 1] == ':'
            && (headIndex - index <= 2 || c->atostrBuf[index - 2] != ':')) {
        c->atostrBuf[index++] = '0';
    }

    c->atostrBuf[index] = 0;

    return c->atostrBuf + headIndex;
}

struct in6_addr UDPC_strtoa(const char *addrStr) {
    struct in6_addr result = in6addr_loopback;
    std::cmatch matchResults;
    // TODO switch regex to ipv6_regex when link device is supported
    if(std::regex_match(addrStr, matchResults, ipv6_regex_nolink)) {
        unsigned int index = 0;
        unsigned int strIndex = 0;
        int doubleColonIndex = -1;
        unsigned char bytes[2] = {0, 0};
        unsigned char bytesState = 0;
        bool prevColon = false;

        const auto checkInc = [&result, &index, &bytes] () -> bool {
            if(index < 15) {
                result.s6_addr[index++] = bytes[0];
                result.s6_addr[index++] = bytes[1];
                bytes[0] = 0;
                bytes[1] = 0;
                return false;
            }
            return true;
        };

        while(addrStr[strIndex] != '\0') {
            if(addrStr[strIndex] >= '0' && addrStr[strIndex] <= '9') {
                switch(bytesState) {
                case 0:
                    bytes[0] = (addrStr[strIndex] - '0');
                    bytesState = 1;
                    break;
                case 1:
                    bytes[0] = (bytes[0] << 4) | (addrStr[strIndex] - '0');
                    bytesState = 2;
                    break;
                case 2:
                    bytes[1] = (addrStr[strIndex] - '0');
                    bytesState = 3;
                    break;
                case 3:
                    bytes[1] = (bytes[1] << 4) | (addrStr[strIndex] - '0');
                    bytesState = 0;
                    if(checkInc()) {
                        return in6addr_loopback;
                    }
                    break;
                default:
                    return in6addr_loopback;
                }
                prevColon = false;
            } else if(addrStr[strIndex] >= 'a' && addrStr[strIndex] <= 'f') {
                switch(bytesState) {
                case 0:
                    bytes[0] = (addrStr[strIndex] - 'a' + 10);
                    bytesState = 1;
                    break;
                case 1:
                    bytes[0] = (bytes[0] << 4) | (addrStr[strIndex] - 'a' + 10);
                    bytesState = 2;
                    break;
                case 2:
                    bytes[1] = (addrStr[strIndex] - 'a' + 10);
                    bytesState = 3;
                    break;
                case 3:
                    bytes[1] = (bytes[1] << 4) | (addrStr[strIndex] - 'a' + 10);
                    bytesState = 0;
                    if(checkInc()) {
                        return in6addr_loopback;
                    }
                    break;
                default:
                    return in6addr_loopback;
                }
                prevColon = false;
            } else if(addrStr[strIndex] >= 'A' && addrStr[strIndex] <= 'F') {
                switch(bytesState) {
                case 0:
                    bytes[0] = (addrStr[strIndex] - 'A' + 10);
                    bytesState = 1;
                    break;
                case 1:
                    bytes[0] = (bytes[0] << 4) | (addrStr[strIndex] - 'A' + 10);
                    bytesState = 2;
                    break;
                case 2:
                    bytes[1] = (addrStr[strIndex] - 'A' + 10);
                    bytesState = 3;
                    break;
                case 3:
                    bytes[1] = (bytes[1] << 4) | (addrStr[strIndex] - 'A' + 10);
                    bytesState = 0;
                    if(checkInc()) {
                        return in6addr_loopback;
                    }
                    break;
                default:
                    return in6addr_loopback;
                }
                prevColon = false;
            } else if(addrStr[strIndex] == ':') {
                switch(bytesState) {
                case 1:
                case 2:
                    bytes[1] = bytes[0];
                    bytes[0] = 0;
                    if(checkInc()) {
                        return in6addr_loopback;
                    }
                    break;
                case 3:
                    bytes[1] |= (bytes[0] & 0xF) << 4;
                    bytes[0] = bytes[0] >> 4;
                    if(checkInc()) {
                        return in6addr_loopback;
                    }
                    break;
                case 0:
                    break;
                default:
                    return in6addr_loopback;
                }
                bytesState = 0;
                if(prevColon) {
                    if(doubleColonIndex >= 0) {
                        return in6addr_loopback;
                    } else {
                        doubleColonIndex = index;
                    }
                } else {
                    prevColon = true;
                }
            } else {
                return in6addr_loopback;
            }

            ++strIndex;
        }
        switch(bytesState) {
        case 1:
        case 2:
            bytes[1] = bytes[0];
            bytes[0] = 0;
            if(checkInc()) {
                return in6addr_loopback;
            }
            break;
        case 3:
            bytes[1] |= (bytes[0] & 0xF) << 4;
            bytes[0] = bytes[0] >> 4;
            if(checkInc()) {
                return in6addr_loopback;
            }
            break;
        case 0:
            break;
        default:
            return in6addr_loopback;
        }

        if(doubleColonIndex >= 0) {
            strIndex = 16 - index;
            if(strIndex < 2) {
                return in6addr_loopback;
            }
            for(unsigned int i = 16; i-- > (unsigned int)doubleColonIndex + strIndex; ) {
                result.s6_addr[i] = result.s6_addr[i - strIndex];
                result.s6_addr[i - strIndex] = 0;
            }
        }
    } else if(std::regex_match(addrStr, matchResults, ipv4_regex)) {
        for(unsigned int i = 0; i < 10; ++i) {
            result.s6_addr[i] = 0;
        }
        result.s6_addr[10] = 0xFF;
        result.s6_addr[11] = 0xFF;
        for(unsigned int i = 0; i < 4; ++i) {
            result.s6_addr[12 + i] = std::stoi(matchResults[i + 1].str());
        }
    }
    return result;
}
