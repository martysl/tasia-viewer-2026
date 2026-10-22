#pragma once

#include "message.h"
#include "llviewernetwork.h"
#include "llviewercontrol.h"

// Only negotiate from an advertised endpoint on an OpenSim grid. The message
// layer leaves an already established UDP circuit alone, and recovers failures.
inline void tasiaEnableAdvertisedQuic(LLMessageSystem* msg, const LLHost& host,
    const LLSD& info, bool login = false)
{
    if (!LLGridManager::getInstance()->isInOpenSim() ||
        !gSavedSettings.getBOOL("TasiaQuicEnabled")) return;
    const auto& port_value = info[login ? "sim_quic_port" : "SimQuicPort"];
    const auto& host_value = info[login ? "sim_quic_host" : "SimQuicHost"];
    // Login XML-RPC commonly represents port numbers as strings; accept only
    // decimal digits, range-check BEFORE narrowing, never wrap 65536 to zero.
    if (!host_value.isString() || (!port_value.isInteger() && !port_value.isString())) return;
    const std::string port_text = port_value.asString();
    if (port_text.empty() || port_text.size() > 5 ||
        port_text.find_first_not_of("0123456789") != std::string::npos) return;
    const auto port = std::stoul(port_text);
    if (port == 0 || port > 65535) return;
    msg->enableQuicCircuit(host, host_value.asString(), static_cast<U16>(port), true,
        gSavedSettings.getS32("TasiaQuicConnectionRetries"));
}

inline void tasiaEnableEventQuic(LLMessageSystem* msg, const LLHost& host, const char* block)
{
    if (const LLSD* body = msg->getCurrentLLSDMessageBody())
    {
        const auto& rows = (*body)[block];
        if (rows.isArray() && rows.size() > 0)
            tasiaEnableAdvertisedQuic(msg, host, rows[0]);
    }
}
