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
#include <iomanip>
#include <regex>
#include <cstdlib>

#if UDPC_PLATFORM == UDPC_PLATFORM_WINDOWS
#include <netioapi.h>

static const UDPC_IPV6_ADDR_TYPE in6addr_any = UDPC_IPV6_ADDR_TYPE{{
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0
}};
static const UDPC_IPV6_ADDR_TYPE in6addr_loopback = UDPC_IPV6_ADDR_TYPE{{
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 1
}};

typedef int socklen_t;
#elif UDPC_PLATFORM == UDPC_PLATFORM_MAC || UDPC_PLATFORM == UDPC_PLATFORM_LINUX
#include <sys/ioctl.h>
#include <net/if.h>
#endif

//static const std::regex ipv6_regex = std::regex(R"d((([0-9a-fA-F]{1,4}:){7,7}[0-9a-fA-F]{1,4}|([0-9a-fA-F]{1,4}:){1,7}:|([0-9a-fA-F]{1,4}:){1,6}:[0-9a-fA-F]{1,4}|([0-9a-fA-F]{1,4}:){1,5}(:[0-9a-fA-F]{1,4}){1,2}|([0-9a-fA-F]{1,4}:){1,4}(:[0-9a-fA-F]{1,4}){1,3}|([0-9a-fA-F]{1,4}:){1,3}(:[0-9a-fA-F]{1,4}){1,4}|([0-9a-fA-F]{1,4}:){1,2}(:[0-9a-fA-F]{1,4}){1,5}|[0-9a-fA-F]{1,4}:((:[0-9a-fA-F]{1,4}){1,6})|:((:[0-9a-fA-F]{1,4}){1,7}|:)|fe80:(:[0-9a-fA-F]{0,4}){0,4}%[0-9a-zA-Z]{1,}|::(ffff(:0{1,4}){0,1}:){0,1}((25[0-5]|(2[0-4]|1{0,1}[0-9]){0,1}[0-9])\.){3,3}(25[0-5]|(2[0-4]|1{0,1}[0-9]){0,1}[0-9])|([0-9a-fA-F]{1,4}:){1,4}:((25[0-5]|(2[0-4]|1{0,1}[0-9]){0,1}[0-9])\.){3,3}(25[0-5]|(2[0-4]|1{0,1}[0-9]){0,1}[0-9])))d");
static const std::regex ipv6_regex_nolink = std::regex(R"d((([0-9a-fA-F]{1,4}:){7,7}[0-9a-fA-F]{1,4}|([0-9a-fA-F]{1,4}:){1,7}:|([0-9a-fA-F]{1,4}:){1,6}:[0-9a-fA-F]{1,4}|([0-9a-fA-F]{1,4}:){1,5}(:[0-9a-fA-F]{1,4}){1,2}|([0-9a-fA-F]{1,4}:){1,4}(:[0-9a-fA-F]{1,4}){1,3}|([0-9a-fA-F]{1,4}:){1,3}(:[0-9a-fA-F]{1,4}){1,4}|([0-9a-fA-F]{1,4}:){1,2}(:[0-9a-fA-F]{1,4}){1,5}|[0-9a-fA-F]{1,4}:((:[0-9a-fA-F]{1,4}){1,6})|:((:[0-9a-fA-F]{1,4}){1,7}|:)|::(ffff(:0{1,4}){0,1}:){0,1}((25[0-5]|(2[0-4]|1{0,1}[0-9]){0,1}[0-9])\.){3,3}(25[0-5]|(2[0-4]|1{0,1}[0-9]){0,1}[0-9])|([0-9a-fA-F]{1,4}:){1,4}:((25[0-5]|(2[0-4]|1{0,1}[0-9]){0,1}[0-9])\.){3,3}(25[0-5]|(2[0-4]|1{0,1}[0-9]){0,1}[0-9])))d");
static const std::regex ipv6_regex_linkonly = std::regex(R"d(fe80:(:[0-9a-fA-F]{0,4}){0,4}%([0-9a-zA-Z]+))d");
static const std::regex ipv4_regex = std::regex(R"d((1[0-9][0-9]|2[0-4][0-9]|25[0-5]|[1-9][0-9]|[0-9])\.(1[0-9][0-9]|2[0-4][0-9]|25[0-5]|[1-9][0-9]|[0-9])\.(1[0-9][0-9]|2[0-4][0-9]|25[0-5]|[1-9][0-9]|[0-9])\.(1[0-9][0-9]|2[0-4][0-9]|25[0-5]|[1-9][0-9]|[0-9]))d");
static const std::regex regex_numeric = std::regex("[0-9]+");

UDPC::SentPktInfo::SentPktInfo() :
id(0),
sentTime(std::chrono::steady_clock::now())
{}

std::size_t UDPC::ConnectionIdHasher::operator()(const UDPC_ConnectionId& key) const {
    std::string value((const char*)UDPC_IPV6_ADDR_SUB(key.addr), 16);
    value.push_back((char)((key.scope_id >> 24) & 0xFF));
    value.push_back((char)((key.scope_id >> 16) & 0xFF));
    value.push_back((char)((key.scope_id >> 8) & 0xFF));
    value.push_back((char)(key.scope_id & 0xFF));
    value.push_back((char)((key.port >> 8) & 0xFF));
    value.push_back((char)(key.port & 0xFF));
    return std::hash<std::string>()(value);
}

std::size_t UDPC::IPV6_Hasher::operator()(const UDPC_IPV6_ADDR_TYPE& addr) const {
    return std::hash<std::string>()(std::string((const char*)UDPC_IPV6_ADDR_SUB(addr), 16));
}

bool operator ==(const UDPC_ConnectionId& a, const UDPC_ConnectionId& b) {
    return a.addr == b.addr && a.scope_id == b.scope_id && a.port == b.port;
}

bool operator ==(const UDPC_IPV6_ADDR_TYPE& a, const UDPC_IPV6_ADDR_TYPE& b) {
    for(unsigned int i = 0; i < 16; ++i) {
        if(UDPC_IPV6_ADDR_SUB(a)[i] != UDPC_IPV6_ADDR_SUB(b)[i]) {
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
sendPkts(),
priorityPkts(),
received(std::chrono::steady_clock::now()),
sent(std::chrono::steady_clock::now()),
rtt(std::chrono::steady_clock::duration::zero())
{
    flags.set(0);
    flags.reset(1);

    if(sodium_init() >= 0) {
        crypto_sign_keypair(pk, sk);
        flags.reset(5);
    } else {
        flags.set(5);
    }
}

UDPC::ConnectionData::ConnectionData(
        bool isServer,
        Context *ctx,
        UDPC_IPV6_ADDR_TYPE addr,
        uint32_t scope_id,
        uint16_t port) :
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
scope_id(scope_id),
port(port),
sentPkts(),
sendPkts(),
priorityPkts(),
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

    if(sodium_init() >= 0) {
        crypto_sign_keypair(pk, sk);
        flags.reset(5);
    } else {
        flags.set(5);
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
isReceivingEvents(false),
protocolID(UDPC_DEFAULT_PROTOCOL_ID),
#ifndef NDEBUG
loggingType(UDPC_DEBUG),
#else
loggingType(UDPC_WARNING),
#endif
atostrBufIndex(0),
receivedPkts(),
cSendPkts(),
rng_engine(),
mutex()
{
    for(unsigned int i = 0; i < UDPC_ATOSTR_SIZE; ++i) {
        atostrBuf[i] = 0;
    }

    if(isThreaded) {
        flags.set(0);
    } else {
        flags.reset(0);
    }

    rng_engine.seed(std::chrono::system_clock::now().time_since_epoch().count());

    threadRunning.store(true);
}

bool UDPC::Context::willLog(UDPC_LoggingType type) {
    switch(loggingType.load()) {
    case UDPC_LoggingType::UDPC_SILENT:
        return false;
    case UDPC_LoggingType::UDPC_ERROR:
        return type == UDPC_LoggingType::UDPC_ERROR;
    case UDPC_LoggingType::UDPC_WARNING:
        return type == UDPC_LoggingType::UDPC_ERROR
            || type == UDPC_LoggingType::UDPC_WARNING;
    case UDPC_LoggingType::UDPC_INFO:
        return type == UDPC_LoggingType::UDPC_ERROR
            || type == UDPC_LoggingType::UDPC_WARNING
            || type == UDPC_LoggingType::UDPC_INFO;
    case UDPC_LoggingType::UDPC_VERBOSE:
        return type == UDPC_LoggingType::UDPC_ERROR
            || type == UDPC_LoggingType::UDPC_WARNING
            || type == UDPC_LoggingType::UDPC_INFO
            || type == UDPC_LoggingType::UDPC_VERBOSE;
    case UDPC_LoggingType::UDPC_DEBUG:
        return type == UDPC_LoggingType::UDPC_ERROR
            || type == UDPC_LoggingType::UDPC_WARNING
            || type == UDPC_LoggingType::UDPC_INFO
            || type == UDPC_LoggingType::UDPC_VERBOSE
            || type == UDPC_LoggingType::UDPC_DEBUG;
    default:
        return false;
    }
}

void UDPC::Context::update_impl() {
    const auto now = std::chrono::steady_clock::now();
    std::chrono::steady_clock::duration dt = now - lastUpdated;
    std::chrono::steady_clock::duration temp_dt_fs;
    lastUpdated = now;

    // handle internalEvents
    do {
        auto optE = internalEvents.top_and_pop();
        if(optE.has_value()) {
            switch(optE.value().type) {
            case UDPC_ET_REQUEST_CONNECT:
            {
                UDPC::ConnectionData newCon(
                    false,
                    this,
                    optE.value().conId.addr,
                    optE.value().conId.scope_id,
                    optE.value().conId.port);
                if(newCon.flags.test(5)) {
                    UDPC_CHECK_LOG(this,
                        UDPC_LoggingType::UDPC_ERROR,
                        "Failed to init ConnectionData instance (libsodium "
                        "init fail) while client establishing connection with ",
                        UDPC_atostr((UDPC_HContext)this, optE.value().conId.addr),
                        " port ",
                        optE.value().conId.port);
                    continue;
                }
                newCon.sent = std::chrono::steady_clock::now() - UDPC::INIT_PKT_INTERVAL_DT;

                if(conMap.find(optE.value().conId) == conMap.end()) {
                    conMap.insert(std::make_pair(
                        optE.value().conId,
                        std::move(newCon)));
                    auto addrConIter = addrConMap.find(optE.value().conId.addr);
                    if(addrConIter == addrConMap.end()) {
                        auto insertResult = addrConMap.insert(std::make_pair(
                            optE.value().conId.addr,
                            std::unordered_set<UDPC_ConnectionId, UDPC::ConnectionIdHasher>{}));
                        assert(insertResult.second &&
                            "new connection insert into addrConMap must not fail");
                        addrConIter = insertResult.first;
                    }
                    addrConIter->second.insert(optE.value().conId);
                    UDPC_CHECK_LOG(this,
                        UDPC_LoggingType::UDPC_INFO,
                        "Client initiating connection to ",
                        UDPC_atostr((UDPC_HContext)this, optE.value().conId.addr),
                        " port ",
                        optE.value().conId.port,
                        " ...");
                } else {
                    UDPC_CHECK_LOG(this,
                        UDPC_LoggingType::UDPC_WARNING,
                        "Client initiate connection, already connected to peer ",
                        UDPC_atostr((UDPC_HContext)this, optE.value().conId.addr),
                        " port ",
                        optE.value().conId.port);
                }
            }
                break;
            case UDPC_ET_REQUEST_DISCONNECT:
                if(optE.value().dropAllWithAddr != 0) {
                    // drop all connections with same address
                    auto addrConIter = addrConMap.find(optE.value().conId.addr);
                    if(addrConIter != addrConMap.end()) {
                        for(auto identIter = addrConIter->second.begin();
                                identIter != addrConIter->second.end();
                                ++identIter) {
                            auto conIter = conMap.find(*identIter);
                            assert(conIter != conMap.end() &&
                                "conMap must have connection listed in addrConMap");
                            if(conIter->second.flags.test(4)) {
                                idMap.erase(conIter->second.id);
                            }
                            if(isReceivingEvents.load()) {
                                externalEvents.push(UDPC_Event{
                                    UDPC_ET_DISCONNECTED, conIter->first, false});
                            }
                            conMap.erase(conIter);
                        }
                        addrConMap.erase(addrConIter);
                    }
                } else {
                    // drop only specific connection with addr and port
                    auto iter = conMap.find(optE.value().conId);
                    if(iter != conMap.end()) {
                        if(iter->second.flags.test(4)) {
                            idMap.erase(iter->second.id);
                        }
                        auto addrConIter = addrConMap.find(optE.value().conId.addr);
                        if(addrConIter != addrConMap.end()) {
                            addrConIter->second.erase(optE.value().conId);
                            if(addrConIter->second.empty()) {
                                addrConMap.erase(addrConIter);
                            }
                        }
                        if(isReceivingEvents.load()) {
                            externalEvents.push(UDPC_Event{
                                UDPC_ET_DISCONNECTED, iter->first, false});
                        }
                        conMap.erase(iter);
                    }
                }
                break;
            default:
                assert(!"internalEvents got invalid type");
                break;
            }
        }
    } while(!internalEvents.empty());

    {
        // check timed out, check good/bad mode with rtt, remove timed out
        std::vector<UDPC_ConnectionId> removed;
        for(auto iter = conMap.begin(); iter != conMap.end(); ++iter) {
            temp_dt_fs = now - iter->second.received;
            if(temp_dt_fs >= UDPC::CONNECTION_TIMEOUT) {
                removed.push_back(iter->first);
                UDPC_CHECK_LOG(this,
                    UDPC_LoggingType::UDPC_VERBOSE,
                    "Timed out connection with ",
                    UDPC_atostr((UDPC_HContext)this, iter->first.addr),
                    ", port = ",
                    iter->second.port);
                continue;
            }

            // check good/bad mode
            iter->second.toggleTimer += dt;
            iter->second.toggledTimer += dt;
            if(iter->second.flags.test(1) && !iter->second.flags.test(2)) {
                // good mode, bad rtt
                UDPC_CHECK_LOG(this,
                    UDPC_LoggingType::UDPC_VERBOSE,
                    "Switching to bad mode in connection with ",
                    UDPC_atostr((UDPC_HContext)this, iter->first.addr),
                    ", port = ",
                    iter->second.port);
                iter->second.flags.reset(1);
                if(iter->second.toggledTimer <= UDPC::TEN_SECONDS) {
                    iter->second.toggleT *= 2;
                }
                iter->second.toggledTimer = std::chrono::steady_clock::duration::zero();
                if(isReceivingEvents.load()) {
                    externalEvents.push(UDPC_Event{
                        UDPC_ET_BAD_MODE, iter->first, false});
                }
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
                    UDPC_CHECK_LOG(this,
                        UDPC_LoggingType::UDPC_VERBOSE,
                        "Switching to good mode in connection with ",
                        UDPC_atostr((UDPC_HContext)this, iter->first.addr),
                        ", port = ",
                        iter->second.port);
                    iter->second.flags.set(1);
                    if(isReceivingEvents.load()) {
                        externalEvents.push(UDPC_Event{
                            UDPC_ET_GOOD_MODE, iter->first, false});
                    }
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
            if(isReceivingEvents.load()) {
                externalEvents.push(UDPC_Event{
                    UDPC_ET_DISCONNECTED, *iter, false});
            }

            conMap.erase(cIter);
        }
    }

    // move queued in cSendPkts to existing connection's sendPkts
    {
        auto sendIter = cSendPkts.begin();
        std::unordered_set<UDPC_ConnectionId, UDPC::ConnectionIdHasher> dropped;
        while(true) {
            auto next = sendIter.current();
            if(next) {
                if(auto iter = conMap.find(next.value().receiver);
                        iter != conMap.end()) {
                    if(iter->second.sendPkts.size() >= UDPC_QUEUED_PKTS_MAX_SIZE) {
                        UDPC_CHECK_LOG(this,
                            UDPC_LoggingType::UDPC_DEBUG,
                            "Not queueing packet to ",
                            UDPC_atostr((UDPC_HContext)this,
                                next.value().receiver.addr),
                            ", port = ",
                            next.value().receiver.port,
                            ", connection's queue reached max size");
                        if(sendIter.next()) {
                            continue;
                        } else {
                            break;
                        }
                    }
                    iter->second.sendPkts.push_back(next.value());
                    if(sendIter.remove()) {
                        continue;
                    } else {
                        break;
                    }
                } else {
                    if(dropped.find(next.value().receiver) == dropped.end()) {
                        UDPC_CHECK_LOG(this,
                            UDPC_LoggingType::UDPC_WARNING,
                            "Dropped queued packets to ",
                            UDPC_atostr(
                                (UDPC_HContext)this,
                                next.value().receiver.addr),
                            ", port = ",
                            next.value().receiver.port,
                            " due to connection not existing");
                        dropped.insert(next.value().receiver);
                    }
                    if(sendIter.remove()) {
                        continue;
                    } else {
                        break;
                    }
                }
            } else {
                break;
            }
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

                std::unique_ptr<char[]> buf = std::make_unique<char[]>(
                    UDPC_CON_HEADER_SIZE);
                UDPC::preparePacket(
                    buf.get(),
                    protocolID,
                    0,
                    0,
                    0xFFFFFFFF,
                    nullptr,
                    0x1,
                    iter->second.pk,
                    iter->second.sk);

                UDPC_IPV6_SOCKADDR_TYPE destinationInfo;
                destinationInfo.sin6_family = AF_INET6;
                std::memcpy(UDPC_IPV6_ADDR_SUB(destinationInfo.sin6_addr), UDPC_IPV6_ADDR_SUB(iter->first.addr), 16);
                destinationInfo.sin6_port = htons(iter->second.port);
                destinationInfo.sin6_flowinfo = 0;
                destinationInfo.sin6_scope_id = iter->first.scope_id;
                long int sentBytes = sendto(
                    socketHandle,
                    buf.get(),
                    UDPC_CON_HEADER_SIZE,
                    0,
                    (struct sockaddr*) &destinationInfo,
                    sizeof(UDPC_IPV6_SOCKADDR_TYPE));
                if(sentBytes != UDPC_CON_HEADER_SIZE) {
                    UDPC_CHECK_LOG(this,
                        UDPC_LoggingType::UDPC_ERROR,
                        "Failed to send packet to initiate connection to ",
                        UDPC_atostr((UDPC_HContext)this, iter->first.addr),
                        ", port = ",
                        iter->second.port);
                    continue;
                } else {
                    UDPC_CHECK_LOG(this, UDPC_LoggingType::UDPC_INFO, "Sent initiate connection to ",
                        UDPC_atostr((UDPC_HContext)this, iter->first.addr),
                        ", port = ",
                        iter->second.port);
                }
            } else {
                // is server, initiate connection to client
                iter->second.flags.reset(3);
                iter->second.sent = now;

                std::unique_ptr<char[]> buf = std::make_unique<char[]>(
                    UDPC_CON_HEADER_SIZE);
                UDPC::preparePacket(
                    buf.get(),
                    protocolID,
                    iter->second.id,
                    iter->second.rseq,
                    iter->second.ack,
                    &iter->second.lseq,
                    0x1,
                    iter->second.pk,
                    iter->second.sk);

                UDPC_IPV6_SOCKADDR_TYPE destinationInfo;
                destinationInfo.sin6_family = AF_INET6;
                std::memcpy(UDPC_IPV6_ADDR_SUB(destinationInfo.sin6_addr), UDPC_IPV6_ADDR_SUB(iter->first.addr), 16);
                destinationInfo.sin6_port = htons(iter->second.port);
                destinationInfo.sin6_flowinfo = 0;
                destinationInfo.sin6_scope_id = iter->first.scope_id;
                long int sentBytes = sendto(
                    socketHandle,
                    buf.get(),
                    UDPC_CON_HEADER_SIZE,
                    0,
                    (struct sockaddr*) &destinationInfo,
                    sizeof(UDPC_IPV6_SOCKADDR_TYPE));
                if(sentBytes != UDPC_CON_HEADER_SIZE) {
                    UDPC_CHECK_LOG(this,
                        UDPC_LoggingType::UDPC_ERROR,
                        "Failed to send packet to initiate connection to ",
                        UDPC_atostr((UDPC_HContext)this, iter->first.addr),
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

            std::unique_ptr<char[]> buf = std::make_unique<char[]>(
                UDPC_FULL_HEADER_SIZE);
            UDPC::preparePacket(
                buf.get(),
                protocolID,
                iter->second.id,
                iter->second.rseq,
                iter->second.ack,
                &iter->second.lseq,
                0,
                iter->second.pk,
                iter->second.sk);

            UDPC_IPV6_SOCKADDR_TYPE destinationInfo;
            destinationInfo.sin6_family = AF_INET6;
            std::memcpy(
                UDPC_IPV6_ADDR_SUB(destinationInfo.sin6_addr),
                UDPC_IPV6_ADDR_SUB(iter->first.addr),
                16);
            destinationInfo.sin6_port = htons(iter->second.port);
            destinationInfo.sin6_flowinfo = 0;
            destinationInfo.sin6_scope_id = iter->first.scope_id;
            long int sentBytes = sendto(
                socketHandle,
                buf.get(),
                UDPC_FULL_HEADER_SIZE,
                0,
                (struct sockaddr*) &destinationInfo,
                sizeof(UDPC_IPV6_SOCKADDR_TYPE));
            if(sentBytes != UDPC_FULL_HEADER_SIZE) {
                UDPC_CHECK_LOG(this,
                    UDPC_LoggingType::UDPC_ERROR,
                    "Failed to send heartbeat packet to ",
                    UDPC_atostr((UDPC_HContext)this, iter->first.addr),
                    ", port = ",
                    iter->second.port);
                continue;
            }

            UDPC_PacketInfo pInfo = UDPC::get_empty_pinfo();
            pInfo.flags = 0x4;
            pInfo.sender.addr = in6addr_loopback;
            pInfo.receiver.addr = iter->first.addr;
            pInfo.sender.port = ntohs(socketInfo.sin6_port);
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
                pInfo = iter->second.priorityPkts.front();
                iter->second.priorityPkts.pop_front();
                isResending = true;
            } else {
                pInfo = iter->second.sendPkts.front();
                iter->second.sendPkts.pop_front();
            }
            std::unique_ptr<char[]> buf = std::make_unique<char[]>(UDPC_FULL_HEADER_SIZE + pInfo.dataSize);
            UDPC::preparePacket(
                buf.get(),
                protocolID,
                iter->second.id,
                iter->second.rseq,
                iter->second.ack,
                &iter->second.lseq,
                (pInfo.flags & 0x4) | (isResending ? 0x8 : 0),
                iter->second.pk,
                iter->second.sk);
            std::memcpy(buf.get() + UDPC_FULL_HEADER_SIZE, pInfo.data, pInfo.dataSize);

            UDPC_IPV6_SOCKADDR_TYPE destinationInfo;
            destinationInfo.sin6_family = AF_INET6;
            std::memcpy(
                UDPC_IPV6_ADDR_SUB(destinationInfo.sin6_addr),
                UDPC_IPV6_ADDR_SUB(iter->first.addr),
                16);
            destinationInfo.sin6_port = htons(iter->second.port);
            destinationInfo.sin6_flowinfo = 0;
            destinationInfo.sin6_scope_id = iter->first.scope_id;
            long int sentBytes = sendto(
                socketHandle,
                buf.get(),
                pInfo.dataSize + UDPC_FULL_HEADER_SIZE,
                0,
                (struct sockaddr*) &destinationInfo,
                sizeof(UDPC_IPV6_SOCKADDR_TYPE));
            if(sentBytes != UDPC_FULL_HEADER_SIZE + pInfo.dataSize) {
                UDPC_CHECK_LOG(this,
                    UDPC_LoggingType::UDPC_ERROR,
                    "Failed to send packet to ",
                    UDPC_atostr((UDPC_HContext)this, iter->first.addr),
                    ", port = ",
                    iter->second.port);
                continue;
            }

            if((pInfo.flags & 0x4) == 0) {
                // is check-received, store data in case packet gets lost
                UDPC_PacketInfo sentPInfo = UDPC::get_empty_pinfo();
                std::memcpy(sentPInfo.data, buf.get(), UDPC_FULL_HEADER_SIZE + pInfo.dataSize);
                sentPInfo.flags = 0;
                sentPInfo.dataSize = UDPC_FULL_HEADER_SIZE + pInfo.dataSize;
                sentPInfo.sender.addr = in6addr_loopback;
                sentPInfo.receiver.addr = iter->first.addr;
                sentPInfo.sender.port = ntohs(socketInfo.sin6_port);
                sentPInfo.receiver.port = iter->second.port;

                iter->second.sentPkts.push_back(std::move(sentPInfo));
                iter->second.cleanupSentPkts();
            } else {
                // is not check-received, only id stored in data array
                UDPC_PacketInfo sentPInfo = UDPC::get_empty_pinfo();
                sentPInfo.flags = 0x4;
                sentPInfo.dataSize = 0;
                sentPInfo.sender.addr = in6addr_loopback;
                sentPInfo.receiver.addr = iter->first.addr;
                sentPInfo.sender.port = ntohs(socketInfo.sin6_port);
                sentPInfo.receiver.port = iter->second.port;
                *((uint32_t*)(sentPInfo.data + 8)) = htonl(iter->second.lseq - 1);

                iter->second.sentPkts.push_back(std::move(sentPInfo));
                iter->second.cleanupSentPkts();
            }

            // store other pkt info
            UDPC::SentPktInfo::Ptr sentPktInfo = std::make_shared<UDPC::SentPktInfo>();
            sentPktInfo->id = iter->second.lseq - 1;
            iter->second.sentInfoMap.insert(std::make_pair(sentPktInfo->id, sentPktInfo));
        }
        iter->second.sent = now;
    }

    // receive packet
    UDPC_IPV6_SOCKADDR_TYPE receivedData;
    socklen_t receivedDataSize = sizeof(receivedData);
    int bytes = recvfrom(
        socketHandle,
        recvBuf,
        UDPC_PACKET_MAX_SIZE,
        0,
        (struct sockaddr*) &receivedData,
        &receivedDataSize);

#if UDPC_PLATFORM == UDPC_PLATFORM_WINDOWS
    if(bytes == 0) {
        // connection closed
        return;
    } else if(bytes == SOCKET_ERROR) {
        int error = WSAGetLastError();
        if(error != WSAEWOULDBLOCK) {
            UDPC_CHECK_LOG(this, UDPC_LoggingType::UDPC_ERROR,
                "Error receiving packet, ", error);
        }
        return;
    }
#else
    if(bytes == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
        // no packet was received
        return;
    }
#endif
    else if(bytes < UDPC_MIN_HEADER_SIZE) {
        // packet size is too small, invalid packet
        UDPC_CHECK_LOG(this,
            UDPC_LoggingType::UDPC_VERBOSE,
            "Received packet is smaller than header, ignoring packet from ",
            UDPC_atostr((UDPC_HContext)this, receivedData.sin6_addr),
            ", port = ",
            ntohs(receivedData.sin6_port));
        return;
    }

    uint32_t temp = ntohl(*((uint32_t*)recvBuf));
    if(temp != protocolID) {
        // Invalid protocol id in packet
        UDPC_CHECK_LOG(this,
            UDPC_LoggingType::UDPC_VERBOSE,
            "Received packet has invalid protocol id, ignoring packet from ",
            UDPC_atostr((UDPC_HContext)this, receivedData.sin6_addr),
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

    if(isConnect && bytes != UDPC_CON_HEADER_SIZE) {
        // invalid packet size
        UDPC_CHECK_LOG(this,
            UDPC_LoggingType::UDPC_VERBOSE,
            "Got connect packet of invalid size from ",
            UDPC_atostr((UDPC_HContext)this, receivedData.sin6_addr),
            ", port = ",
            ntohs(receivedData.sin6_port),
            ", ignoring");
        return;
    } else if (!isConnect && bytes < (int)UDPC_FULL_HEADER_SIZE) {
        // packet is too small
        UDPC_CHECK_LOG(this,
            UDPC_LoggingType::UDPC_VERBOSE,
            "Got non-connect packet of invalid size from ",
            UDPC_atostr((UDPC_HContext)this, receivedData.sin6_addr),
            ", port = ",
            ntohs(receivedData.sin6_port),
            ", ignoring");
        return;
    }

    UDPC_ConnectionId identifier{receivedData.sin6_addr, receivedData.sin6_scope_id, ntohs(receivedData.sin6_port)};

    if(isConnect && isAcceptNewConnections.load()) {
        // is connect packet and is accepting new connections
        if(!flags.test(1)
                && conMap.find(identifier) == conMap.end()) {
            // is receiving as server, connection did not already exist
            UDPC::ConnectionData newConnection(true, this, receivedData.sin6_addr, receivedData.sin6_scope_id, ntohs(receivedData.sin6_port));
            if(newConnection.flags.test(5)) {
                UDPC_CHECK_LOG(this,
                    UDPC_LoggingType::UDPC_ERROR,
                    "Failed to init ConnectionData instance (libsodium init "
                    "fail) while server establishing connection with ",
                    UDPC_atostr((UDPC_HContext)this, receivedData.sin6_addr),
                    ", port = ",
                    ntohs(receivedData.sin6_port));
                return;
            }
            std::memcpy(newConnection.peer_pk, recvBuf + UDPC_MIN_HEADER_SIZE,
                crypto_sign_PUBLICKEYBYTES);
            UDPC_CHECK_LOG(this,
                UDPC_LoggingType::UDPC_INFO,
                "Establishing connection with client ",
                UDPC_atostr((UDPC_HContext)this, receivedData.sin6_addr),
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
            if(isReceivingEvents.load()) {
                externalEvents.push(UDPC_Event{
                    UDPC_ET_CONNECTED,
                    identifier,
                    false});
            }
        } else if (flags.test(1)) {
            // is client
            auto iter = conMap.find(identifier);
            if(iter == conMap.end() || !iter->second.flags.test(3)) {
                return;
            }
            iter->second.flags.reset(3);
            iter->second.id = conID;
            iter->second.flags.set(4);
            std::memcpy(iter->second.peer_pk, recvBuf + UDPC_MIN_HEADER_SIZE,
                crypto_sign_PUBLICKEYBYTES);
            UDPC_CHECK_LOG(this,
                UDPC_LoggingType::UDPC_INFO,
                "Established connection with server ",
                UDPC_atostr((UDPC_HContext)this, receivedData.sin6_addr),
                ", port = ",
                ntohs(receivedData.sin6_port),
                ", got id = ", conID);
            if(isReceivingEvents.load()) {
                externalEvents.push(UDPC_Event{
                    UDPC_ET_CONNECTED,
                    identifier,
                    false});
            }
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

    // verify signature of header
    if(crypto_sign_verify_detached(
        (unsigned char*)(recvBuf + UDPC_MIN_HEADER_SIZE),
        (unsigned char*)recvBuf,
        UDPC_MIN_HEADER_SIZE,
        iter->second.peer_pk) != 0) {
        UDPC_CHECK_LOG(
            this,
            UDPC_LoggingType::UDPC_INFO,
            "Failed to verify received packet from",
            UDPC_atostr((UDPC_HContext)this, receivedData.sin6_addr),
            ", port = ",
            ntohs(receivedData.sin6_port),
            ", ignoring");
        return;
    }

    // packet is valid
    UDPC_CHECK_LOG(this,
        UDPC_LoggingType::UDPC_VERBOSE,
        "Received valid packet from ",
        UDPC_atostr((UDPC_HContext)this, receivedData.sin6_addr),
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

            UDPC_CHECK_LOG(this,
                UDPC_LoggingType::UDPC_VERBOSE,
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
                    if(sentIter->dataSize <= UDPC_FULL_HEADER_SIZE) {
                        UDPC_CHECK_LOG(this,
                            UDPC_LoggingType::UDPC_VERBOSE,
                            "Timed out packet has no payload (probably "
                            "heartbeat packet), ignoring it");
                        sentIter->flags |= 0x8;
                        break;
                    }

                    UDPC_PacketInfo resendingData = UDPC::get_empty_pinfo();
                    resendingData.dataSize = sentIter->dataSize - UDPC_FULL_HEADER_SIZE;
                    std::memcpy(resendingData.data, sentIter->data + UDPC_FULL_HEADER_SIZE, resendingData.dataSize);
                    resendingData.flags = 0;
                    iter->second.priorityPkts.push_back(resendingData);
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
                UDPC_CHECK_LOG(this,
                    UDPC_LoggingType::UDPC_VERBOSE,
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
                UDPC_CHECK_LOG(this,
                    UDPC_LoggingType::UDPC_VERBOSE,
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
        UDPC_CHECK_LOG(this,
            UDPC_LoggingType::UDPC_VERBOSE,
            "Received packet is already marked as received, ignoring it");
        return;
    }

    if(isOutOfOrder) {
        UDPC_CHECK_LOG(this,
            UDPC_LoggingType::UDPC_INFO,
            "Received packet is out of order");
    }

    if(bytes > (int)UDPC_FULL_HEADER_SIZE) {
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

        receivedPkts.push(recPktInfo);
    } else if(bytes == UDPC_FULL_HEADER_SIZE) {
        UDPC_CHECK_LOG(this,
            UDPC_LoggingType::UDPC_VERBOSE,
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

void UDPC::preparePacket(
        char *data, uint32_t protocolID, uint32_t conID, uint32_t rseq,
        uint32_t ack, uint32_t *seqID, int flags,
        const unsigned char *pk, const unsigned char *sk) {
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

    if((flags & 0x1) != 0) {
        std::memcpy(data + UDPC_MIN_HEADER_SIZE, pk, crypto_sign_PUBLICKEYBYTES);
    } else {
        crypto_sign_detached(
            (unsigned char*)(data + UDPC_MIN_HEADER_SIZE), // destination of detached sig
            nullptr,                                       // unused length, assume max
            (unsigned char*)data,                          // header to sign
            UDPC_MIN_HEADER_SIZE,                          // size of header
            sk);                                           // secret key
    }
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
            0,     // scope_id
            0      // port
        },
        {        // receiver
            {0},   // ipv6 addr
            0,     // scope_id
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
        std::this_thread::sleep_for(std::chrono::milliseconds(8) - (nextNow - now));
    }
}

UDPC_ConnectionId UDPC_create_id(UDPC_IPV6_ADDR_TYPE addr, uint16_t port) {
    return UDPC_ConnectionId{addr, 0, port};
}

UDPC_ConnectionId UDPC_create_id_full(UDPC_IPV6_ADDR_TYPE addr, uint32_t scope_id, uint16_t port) {
    return UDPC_ConnectionId{addr, scope_id, port};
}

UDPC_ConnectionId UDPC_create_id_anyaddr(uint16_t port) {
    return UDPC_ConnectionId{in6addr_any, 0, port};
}

UDPC_HContext UDPC_init(UDPC_ConnectionId listenId, int isClient) {
    UDPC::Context *ctx = new UDPC::Context(false);
    ctx->flags.set(1, isClient != 0);

    UDPC_CHECK_LOG(ctx, UDPC_LoggingType::UDPC_INFO, "Got listen addr ",
        UDPC_atostr((UDPC_HContext)ctx, listenId.addr));

    // initialize libsodium
    if(sodium_init() < 0) {
        UDPC_CHECK_LOG(ctx, UDPC_LoggingType::UDPC_ERROR,
            "Failed to initialize libsodium");
        delete ctx;
        return nullptr;
    }

#if UDPC_PLATFORM == UDPC_PLATFORM_WINDOWS
    // Initialize Winsock
    WORD wVersionRequested = MAKEWORD(2, 2);
    WSADATA wsaData;
    if(WSAStartup(wVersionRequested, &wsaData) != 0) {
        UDPC_CHECK_LOG(ctx, UDPC_LoggingType::UDPC_ERROR, "Failed to initialize Winsock");
        delete ctx;
        return nullptr;
    }
#endif

    // create socket
    ctx->socketHandle = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
    if(UDPC_SOCKET_RETURN_ERROR(ctx->socketHandle)) {
        // TODO maybe different way of handling init fail
#if UDPC_PLATFORM == UDPC_PLATFORM_WINDOWS
        UDPC_CHECK_LOG(ctx, UDPC_LoggingType::UDPC_ERROR, "Failed to create socket, ",
            WSAGetLastError());
#else
        UDPC_CHECK_LOG(ctx, UDPC_LoggingType::UDPC_ERROR, "Failed to create socket");
#endif
        delete ctx;
        return nullptr;
    }

    // allow ipv4 connections on ipv6 socket
    {
#if UDPC_PLATFORM == UDPC_PLATFORM_WINDOWS
        char no = 0;
#else
        int no = 0;
#endif
        setsockopt(ctx->socketHandle, IPPROTO_IPV6, IPV6_V6ONLY, &no, sizeof(no));
    }

    // bind socket
    ctx->socketInfo.sin6_family = AF_INET6;
    ctx->socketInfo.sin6_addr = listenId.addr;
    ctx->socketInfo.sin6_port = htons(listenId.port);
    ctx->socketInfo.sin6_flowinfo = 0;
    ctx->socketInfo.sin6_scope_id = listenId.scope_id;
    if(bind(ctx->socketHandle, (const struct sockaddr *)&ctx->socketInfo,
            sizeof(UDPC_IPV6_SOCKADDR_TYPE)) < 0) {
        // TODO maybe different way of handling init fail
        UDPC_CHECK_LOG(ctx, UDPC_LoggingType::UDPC_ERROR, "Failed to bind socket");
        UDPC_CLEANUPSOCKET(ctx->socketHandle);
        delete ctx;
        return nullptr;
    }

    // TODO verify this is necessary to get the listen port
    if(ctx->socketInfo.sin6_port == 0) {
        UDPC_IPV6_SOCKADDR_TYPE getInfo;
        socklen_t size = sizeof(UDPC_IPV6_SOCKADDR_TYPE);
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
        UDPC_CHECK_LOG(ctx, UDPC_LoggingType::UDPC_ERROR, "Failed to set nonblocking on socket");
        UDPC_CLEANUPSOCKET(ctx->socketHandle);
        delete ctx;
        return nullptr;
    }

    UDPC_CHECK_LOG(ctx, UDPC_LoggingType::UDPC_INFO, "Initialized UDPC");

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

    UDPC_CHECK_LOG(ctx, UDPC_LoggingType::UDPC_INFO, "Initialized threaded UDPC");

    return (UDPC_HContext) ctx;
}

void UDPC_destroy(UDPC_HContext ctx) {
    UDPC::Context *UDPC_ctx = UDPC::verifyContext(ctx);
    if(UDPC_ctx) {
        if(UDPC_ctx->flags.test(0)) {
            UDPC_ctx->threadRunning.store(false);
            UDPC_ctx->thread.join();
        }
#if UDPC_PLATFORM == UDPC_PLATFORM_WINDOWS
        WSACleanup();
#endif
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

    c->internalEvents.push(UDPC_Event{UDPC_ET_REQUEST_CONNECT, connectionId, false});
}

void UDPC_queue_send(UDPC_HContext ctx, UDPC_ConnectionId destinationId,
                     int isChecked, void *data, uint32_t size) {
    if(size == 0 || !data) {
        return;
    }

    UDPC::Context *c = UDPC::verifyContext(ctx);
    if(!c) {
        return;
    }

    UDPC_PacketInfo sendInfo = UDPC::get_empty_pinfo();
    std::memcpy(sendInfo.data, data, size);
    sendInfo.dataSize = size;
    sendInfo.sender.addr = in6addr_loopback;
    sendInfo.sender.port = ntohs(c->socketInfo.sin6_port);
    sendInfo.receiver.addr = destinationId.addr;
    sendInfo.receiver.port = destinationId.port;
    sendInfo.flags = (isChecked != 0 ? 0x0 : 0x4);

    c->cSendPkts.push(sendInfo);
}

unsigned long UDPC_get_queue_send_current_size(UDPC_HContext ctx) {
    UDPC::Context *c = UDPC::verifyContext(ctx);
    if(!c) {
        return 0;
    }

    return c->cSendPkts.size();
}

int UDPC_set_accept_new_connections(UDPC_HContext ctx, int isAccepting) {
    UDPC::Context *c = UDPC::verifyContext(ctx);
    if(!c) {
        return 0;
    }
    return c->isAcceptNewConnections.exchange(isAccepting == 0 ? false : true);
}

void UDPC_drop_connection(UDPC_HContext ctx, UDPC_ConnectionId connectionId, int dropAllWithAddr) {
    UDPC::Context *c = UDPC::verifyContext(ctx);
    if(!c) {
        return;
    }

    c->internalEvents.push(UDPC_Event{UDPC_ET_REQUEST_DISCONNECT, connectionId, dropAllWithAddr});
    return;
}

int UDPC_has_connection(UDPC_HContext ctx, UDPC_ConnectionId connectionId) {
    UDPC::Context *c = UDPC::verifyContext(ctx);
    if(!c) {
        return 0;
    }

    std::lock_guard<std::mutex> lock(c->mutex);

    return c->conMap.find(connectionId) == c->conMap.end() ? 0 : 1;
}

UDPC_ConnectionId* UDPC_get_list_connected(UDPC_HContext ctx, unsigned int *size) {
    UDPC::Context *c = UDPC::verifyContext(ctx);
    if(!c) {
        return nullptr;
    }

    std::lock_guard<std::mutex> lock(c->mutex);

    if(c->conMap.empty()) {
        return nullptr;
    }
    if(size) {
        *size = c->conMap.size();
    }

    UDPC_ConnectionId *list = (UDPC_ConnectionId*)std::malloc(
            sizeof(UDPC_ConnectionId) * (c->conMap.size() + 1));
    UDPC_ConnectionId *current = list;
    for(auto iter = c->conMap.begin(); iter != c->conMap.end(); ++iter) {
        *current = iter->first;
        ++current;
    }
    *current = UDPC_ConnectionId{{0}, 0, 0};
    return list;
}

void UDPC_free_list_connected(UDPC_ConnectionId *list) {
    std::free(list);
}

uint32_t UDPC_get_protocol_id(UDPC_HContext ctx) {
    UDPC::Context *c = UDPC::verifyContext(ctx);
    if(!c) {
        return 0;
    }

    return c->protocolID.load();
}

uint32_t UDPC_set_protocol_id(UDPC_HContext ctx, uint32_t id) {
    UDPC::Context *c = UDPC::verifyContext(ctx);
    if(!c) {
        return 0;
    }

    return c->protocolID.exchange(id);
}

UDPC_LoggingType UDPC_get_logging_type(UDPC_HContext ctx) {
    UDPC::Context *c = UDPC::verifyContext(ctx);
    if(!c) {
        return UDPC_LoggingType::UDPC_SILENT;
    }
    return static_cast<UDPC_LoggingType>(c->loggingType.load());
}

UDPC_LoggingType UDPC_set_logging_type(UDPC_HContext ctx, UDPC_LoggingType loggingType) {
    UDPC::Context *c = UDPC::verifyContext(ctx);
    if(!c) {
        return UDPC_LoggingType::UDPC_SILENT;
    }
    return static_cast<UDPC_LoggingType>(c->loggingType.exchange(loggingType));
}

int UDPC_get_receiving_events(UDPC_HContext ctx) {
    UDPC::Context *c = UDPC::verifyContext(ctx);
    if(!c) {
        return 0;
    }

    return c->isReceivingEvents.load() ? 1 : 0;
}

int UDPC_set_receiving_events(UDPC_HContext ctx, int isReceivingEvents) {
    UDPC::Context *c = UDPC::verifyContext(ctx);
    if(!c) {
        return 0;
    }

    return c->isReceivingEvents.exchange(isReceivingEvents != 0);
}

UDPC_Event UDPC_get_event(UDPC_HContext ctx, unsigned long *remaining) {
    UDPC::Context *c = UDPC::verifyContext(ctx);
    if(!c) {
        return UDPC_Event{UDPC_ET_NONE, UDPC_create_id_anyaddr(0), 0};
    }

    auto optE = c->externalEvents.top_and_pop_and_rsize(remaining);
    if(optE) {
        return optE.value();
    } else {
        return UDPC_Event{UDPC_ET_NONE, UDPC_create_id_anyaddr(0), 0};
    }
}

UDPC_PacketInfo UDPC_get_received(UDPC_HContext ctx, unsigned long *remaining) {
    UDPC::Context *c = UDPC::verifyContext(ctx);
    if(!c) {
        return UDPC::get_empty_pinfo();
    }

    auto opt_pinfo = c->receivedPkts.top_and_pop_and_rsize(remaining);
    if(opt_pinfo) {
        return *opt_pinfo;
    }
    return UDPC::get_empty_pinfo();
}

const char *UDPC_atostr_cid(UDPC_HContext ctx, UDPC_ConnectionId connectionId) {
    return UDPC_atostr(ctx, connectionId.addr);
}

const char *UDPC_atostr(UDPC_HContext ctx, UDPC_IPV6_ADDR_TYPE addr) {
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

        if(UDPC_IPV6_ADDR_SUB(addr)[i] == 0
                && (headIndex - index <= 1 || c->atostrBuf[index] == ':')) {
            continue;
        } else {
            std::stringstream sstream;
            sstream << std::setw(2) << std::setfill('0')
                << std::hex << (unsigned int) UDPC_IPV6_ADDR_SUB(addr)[i];
            std::string out(sstream.str());
            unsigned int outOffset = 0;
            if(headIndex - index <= 1 || c->atostrBuf[index - 1] == ':') {
                if(out[0] == '0') {
                    if(out[1] == '0') {
                        outOffset = 2;
                    } else {
                        outOffset = 1;
                    }
                }
            }
            if(outOffset == 2) {
                continue;
            } else if(outOffset == 1) {
                if(out[outOffset] != '0') {
                    std::memcpy(c->atostrBuf + index, out.c_str() + outOffset, 1);
                    ++index;
                }
            } else {
                std::memcpy(c->atostrBuf + index, out.c_str() + outOffset, 2);
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

UDPC_IPV6_ADDR_TYPE UDPC_strtoa(const char *addrStr) {
    UDPC_IPV6_ADDR_TYPE result = in6addr_loopback;
    std::cmatch matchResults;
    if(std::regex_match(addrStr, matchResults, ipv6_regex_nolink)) {
        unsigned int index = 0;
        unsigned int strIndex = 0;
        int doubleColonIndex = -1;
        unsigned char bytes[2] = {0, 0};
        unsigned char bytesState = 0;
        bool prevColon = false;

        const auto checkInc = [&result, &index, &bytes] () -> bool {
            if(index < 15) {
                UDPC_IPV6_ADDR_SUB(result)[index++] = bytes[0];
                UDPC_IPV6_ADDR_SUB(result)[index++] = bytes[1];
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
                UDPC_IPV6_ADDR_SUB(result)[i] = UDPC_IPV6_ADDR_SUB(result)[i - strIndex];
                UDPC_IPV6_ADDR_SUB(result)[i - strIndex] = 0;
            }
        }
    } else if(std::regex_match(addrStr, matchResults, ipv4_regex)) {
        for(unsigned int i = 0; i < 10; ++i) {
            UDPC_IPV6_ADDR_SUB(result)[i] = 0;
        }
        UDPC_IPV6_ADDR_SUB(result)[10] = 0xFF;
        UDPC_IPV6_ADDR_SUB(result)[11] = 0xFF;
        for(unsigned int i = 0; i < 4; ++i) {
            UDPC_IPV6_ADDR_SUB(result)[12 + i] = std::stoi(matchResults[i + 1].str());
        }
    }
    return result;
}

UDPC_IPV6_ADDR_TYPE UDPC_strtoa_link(const char *addrStr, uint32_t *linkId_out) {
    const auto checkSetOut = [&linkId_out] (uint32_t val) {
        if(linkId_out) {
            *linkId_out = val;
        }
    };

    UDPC_IPV6_ADDR_TYPE result({0});
    std::cmatch matchResults;
    const char *linkName = nullptr;

    if(std::regex_match(addrStr, matchResults, ipv6_regex_linkonly)) {
        unsigned int index = 0;
        unsigned int strIndex = 0;
        int doubleColonIndex = -1;
        unsigned char bytes[2] = {0, 0};
        unsigned char bytesState = 0;
        bool prevColon = false;

        const auto checkInc = [&result, &index, &bytes] () -> bool {
            if(index < 15) {
                UDPC_IPV6_ADDR_SUB(result)[index++] = bytes[0];
                UDPC_IPV6_ADDR_SUB(result)[index++] = bytes[1];
                bytes[0] = 0;
                bytes[1] = 0;
                return false;
            }
            return true;
        };

        while(addrStr[strIndex] != '%') {
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
                        checkSetOut(0);
                        return in6addr_loopback;
                    }
                    break;
                default:
                    checkSetOut(0);
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
                        checkSetOut(0);
                        return in6addr_loopback;
                    }
                    break;
                default:
                    checkSetOut(0);
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
                        checkSetOut(0);
                        return in6addr_loopback;
                    }
                    break;
                default:
                    checkSetOut(0);
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
                        checkSetOut(0);
                        return in6addr_loopback;
                    }
                    break;
                case 3:
                    bytes[1] |= (bytes[0] & 0xF) << 4;
                    bytes[0] = bytes[0] >> 4;
                    if(checkInc()) {
                        checkSetOut(0);
                        return in6addr_loopback;
                    }
                    break;
                case 0:
                    break;
                default:
                    checkSetOut(0);
                    return in6addr_loopback;
                }
                bytesState = 0;
                if(prevColon) {
                    if(doubleColonIndex >= 0) {
                        checkSetOut(0);
                        return in6addr_loopback;
                    } else {
                        doubleColonIndex = index;
                    }
                } else {
                    prevColon = true;
                }
            } else {
                checkSetOut(0);
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
                checkSetOut(0);
                return in6addr_loopback;
            }
            break;
        case 3:
            bytes[1] |= (bytes[0] & 0xF) << 4;
            bytes[0] = bytes[0] >> 4;
            if(checkInc()) {
                checkSetOut(0);
                return in6addr_loopback;
            }
            break;
        case 0:
            break;
        default:
            checkSetOut(0);
            return in6addr_loopback;
        }
        linkName = addrStr + strIndex + 1;

        if(doubleColonIndex >= 0) {
            strIndex = 16 - index;
            if(strIndex < 2) {
                checkSetOut(0);
                return in6addr_loopback;
            }
            for(unsigned int i = 16; i-- > (unsigned int)doubleColonIndex + strIndex; ) {
                UDPC_IPV6_ADDR_SUB(result)[i] = UDPC_IPV6_ADDR_SUB(result)[i - strIndex];
                UDPC_IPV6_ADDR_SUB(result)[i - strIndex] = 0;
            }
        }
    }

    uint32_t scope_id;
    if(std::regex_match(linkName, regex_numeric)) {
        scope_id = std::atoi(linkName);
    }
#if UDPC_PLATFORM == UDPC_PLATFORM_WINDOWS
    else {
        scope_id = if_nametoindex(linkName);
        if(scope_id == 0) {
            checkSetOut(0);
            return in6addr_loopback;
        }
    }
#elif UDPC_PLATFORM == UDPC_PLATFORM_MAC || UDPC_PLATFORM == UDPC_PLATFORM_LINUX
    else {
        struct ifreq req{{0}, 0, 0};
        std::strncpy(req.ifr_name, linkName, IFNAMSIZ);
        int socketHandle = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
        if(ioctl(socketHandle, SIOCGIFINDEX, &req) < 0) {
            UDPC_CLEANUPSOCKET(socketHandle);
            checkSetOut(0);
            return in6addr_loopback;
        }
        scope_id = req.ifr_ifindex;
        UDPC_CLEANUPSOCKET(socketHandle);
    }
#endif

    checkSetOut(scope_id);
    return result;
}
