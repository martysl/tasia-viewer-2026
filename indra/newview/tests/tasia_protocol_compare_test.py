#!/usr/bin/env python3
"""Compare a reference viewer's templates and actual zero-code implementations.

Usage: python3 tasia_protocol_compare_test.py /path/to/reference-viewer
Offline synthetic tests only: no network, viewer, graphics or real session data.
"""

import os
from pathlib import Path
import re
import shlex
import subprocess
import sys
import tempfile


def messages(path):
    text = re.sub(r"//[^\n]*", "", path.read_text())
    tokens = re.findall(r"\{|\}|[^\s{}]+", text)
    result = {}
    depth = 0
    for index, token in enumerate(tokens):
        if token == "{":
            if depth == 0:
                start = index
            depth += 1
        elif token == "}":
            depth -= 1
            if depth == 0:
                name = tokens[start + 1]
                assert name not in result, name
                result[name] = tokens[start:index + 1]
    assert depth == 0
    return result


def function(text, signature):
    # These functions contain no braces inside string literals. Strip comments
    # first so changes to documentation cannot alter brace matching.
    text = re.sub(r"//[^\n]*|/\*[\s\S]*?\*/", "", text)
    start = text.index(signature)
    opening = text.index("{", start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (text[end] == "{") - (text[end] == "}")
        end += 1
    return text[start:end]


if len(sys.argv) != 2:
    raise SystemExit(__doc__)
root = Path(__file__).resolve().parents[3]
reference = Path(sys.argv[1]).resolve()
current = messages(root / "scripts/messages/message_template.msg")
other = messages(reference / "scripts/messages/message_template.msg")
critical = (
    "UseCircuitCode", "RegionHandshake", "RegionHandshakeReply",
    "CompleteAgentMovement", "AgentMovementComplete", "AgentUpdate",
    "ObjectUpdate", "ObjectUpdateCompressed", "ImprovedTerseObjectUpdate",
    "AvatarAppearance", "AgentSetAppearance", "AgentWearablesRequest",
    "AgentWearablesUpdate", "AgentIsNowWearing", "AgentCachedTexture",
    "AgentCachedTextureResponse", "TeleportFinish", "CrossedRegion",
    "EnableSimulator", "DisableSimulator", "ParcelOverlay", "KillObject",
    "StartPingCheck", "CompletePingCheck", "LogoutReply", "KickUser",
)
for name in critical:
    assert current[name] == other[name], f"Protocol mismatch: {name}"
changed = sorted(name for name in current.keys() & other.keys() if current[name] != other[name])
print(f"Templates: {len(current)} current / {len(other)} reference messages")
print(f"PASS: {len(critical)} avatar/movement/connection definitions match exactly")
print("Other differing definitions:", ", ".join(changed) or "none")
print("Current only:", ", ".join(sorted(current.keys() - other.keys())) or "none")
print("Reference only:", ", ".join(sorted(other.keys() - current.keys())) or "none")

encoder = function((reference / "indra/llmessage/lltemplatemessagebuilder.cpp").read_text(),
                   "static S32 zero_code(")
decoder = function((reference / "indra/llmessage/message.cpp").read_text(),
                   "S32 LLMessageSystem::zeroCodeExpand(")
preamble = r'''
#include <algorithm>
#include <cassert>
#include <cstring>
#include <iostream>
#include <random>
#include <vector>
#include "llzerocode.h"
constexpr U32 MAX_BUFFER_SIZE = 65536, LL_PACKET_ID_SIZE = 6;
constexpr U8 LL_ZERO_CODE_FLAG = 0x80;
constexpr int LL_MINIMUM_VALID_PACKET_SIZE = 7, MX_WROTE_PAST_BUFFER_SIZE = 1;
#define LL_WARNS(...) std::cerr
#define LL_ENDL std::endl
struct LLMessageSystem {
    U32 mTotalBytesIn = 0, mCompressedPacketsIn = 0, mCompressedBytesIn = 0,
        mUncompressedBytesIn = 0;
    U8 mEncodedRecvBuffer[MAX_BUFFER_SIZE];
    void callExceptionFunc(int) { assert(false && "unexpected decoder overflow"); }
    S32 zeroCodeExpand(U8**, S32*);
};
'''
tests = r'''
int main() {
    std::mt19937 rng(20261018);
    LLMessageSystem reference_decoder;
    unsigned tested = 0;
    for (unsigned iteration = 0; iteration < 12000; ++iteration) {
        const unsigned size = 7 + (rng() % 2042);
        std::vector<U8> packet(size);
        for (auto& byte : packet) byte = static_cast<U8>(rng());
        packet[0] &= ~LL_ZERO_CODE_FLAG;
        // Mix fully random, zero-heavy and long-zero-run valid packets.
        if (iteration % 3 == 1) {
            for (unsigned i = LL_PACKET_ID_SIZE; i < size; ++i)
                if (rng() % 4) packet[i] = 0;
        } else if (iteration % 3 == 2) {
            std::fill(packet.begin() + LL_PACKET_ID_SIZE, packet.end(), 0);
        }
        auto old_input = packet;
        U8* old_data = old_input.data();
        U32 old_size = size;
        zero_code(&old_data, &old_size);
        std::vector<U8> encoded(2 * size);
        S32 encoded_size = LLZeroCode::encode(packet.data(), size, encoded.data(),
                                            static_cast<U32>(encoded.size()), LL_PACKET_ID_SIZE);
        if (encoded_size < 0) {
            assert(old_size == size && old_data == old_input.data());
            assert(old_input == packet);
        } else {
            assert(static_cast<U32>(encoded_size) == old_size);
            assert(std::equal(encoded.begin(), encoded.begin() + encoded_size, old_data));
            std::vector<U8> decoded(MAX_BUFFER_SIZE);
            bool overflow = false;
            U32 decoded_size = LLZeroCode::decode(encoded.data(), encoded_size,
                decoded.data(), static_cast<U32>(decoded.size()), LL_PACKET_ID_SIZE, overflow);
            assert(!overflow && decoded_size == size);
            assert(std::equal(packet.begin(), packet.end(), decoded.begin()));
            auto old_encoded = encoded;
            U8* old_decoded = old_encoded.data();
            S32 old_decoded_size = encoded_size;
            reference_decoder.zeroCodeExpand(&old_decoded, &old_decoded_size);
            assert(static_cast<U32>(old_decoded_size) == decoded_size);
            assert(std::equal(decoded.begin(), decoded.begin() + decoded_size, old_decoded));
        }
        ++tested;
    }
    std::cout << "PASS: " << tested << " synthetic valid packets; actual encoders agree, "
                 "compressed packets round-trip and both decoders agree\n";
}
'''
with tempfile.TemporaryDirectory(prefix="tasia-protocol-") as directory:
    source = Path(directory) / "protocol.cpp"
    binary = Path(directory) / "protocol"
    source.write_text(preamble + encoder + decoder + tests)
    subprocess.run(shlex.split(os.environ.get("CXX", "c++")) + [
        "-std=c++17", "-Wall", "-Wextra", "-Werror",
        "-I" + str(root / "indra/llmessage"), "-I" + str(root / "indra/llcommon"),
        str(source), "-o", str(binary)
    ], check=True)
    subprocess.run([str(binary)], check=True)
