#!/usr/bin/env python3
"""Compile the production Variable-1 clamp and verify it preserves caller data."""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[3]
SOURCE = (ROOT / 'indra/llmessage/lltemplatemessagebuilder.cpp').read_text()
MARKER = ('void LLTemplateMessageBuilder::addData(const char *varname, '
          'const void *data, EMsgVariableType type, S32 size)')


def production_method():
    start = SOURCE.index(MARKER)
    brace = SOURCE.index('{', start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (SOURCE[end] == '{') - (SOURCE[end] == '}')
        end += 1
    return SOURCE[start:end]


PRELUDE = r'''
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>
using S32 = int;
using U8 = std::uint8_t;
enum EMsgVariableType { MVT_VARIABLE, MVT_FIXED };
#define LL_ERRS() std::cerr
#define LL_WARNS() std::cerr
#define LL_ENDL '\n'
struct LLMessageVariable {
    std::string name = "AboutText";
    EMsgVariableType type = MVT_VARIABLE;
    int prefix_size = 1;
    const char* getName() const { return name.c_str(); }
    EMsgVariableType getType() const { return type; }
    int getSize() const { return prefix_size; }
};
struct LLMessageBlockTemplate {
    LLMessageVariable variable;
    LLMessageVariable* getVariable(char*) { return &variable; }
};
struct LLMessageTemplate {
    std::string mName = "fixture";
    LLMessageBlockTemplate block;
    LLMessageBlockTemplate* getBlock(char*) { return &block; }
};
struct LLMessageDataBlock {
    std::vector<U8> bytes;
    int prefix_size = 0;
    void addData(char*, const void* data, int size, EMsgVariableType, int prefix) {
        const auto* begin = static_cast<const U8*>(data);
        bytes.assign(begin, begin + size);
        prefix_size = prefix;
    }
    void addData(char*, const void*, int, EMsgVariableType) {}
};
struct LLTemplateMessageBuilder {
    LLMessageTemplate* mCurrentSMessageTemplate = nullptr;
    LLMessageDataBlock* mCurrentSDataBlock = nullptr;
    char* mCurrentSBlockName = nullptr;
    int mCurrentSendTotal = 0;
    void addData(const char*, const void*, EMsgVariableType, S32);
};
'''


def main():
    fixture = PRELUDE + production_method() + r'''
int main() {
    LLMessageTemplate message_template;
    LLMessageDataBlock data_block;
    char block_name[] = "PropertiesData";
    LLTemplateMessageBuilder builder;
    builder.mCurrentSMessageTemplate = &message_template;
    builder.mCurrentSDataBlock = &data_block;
    builder.mCurrentSBlockName = block_name;

    std::string description(400, 'x');
    const std::string before = description;
    builder.addData("FLAboutText", description.c_str(), MVT_VARIABLE,
                    static_cast<S32>(description.size() + 1));

    assert(description == before); // serialization must not mutate cached input
    assert(data_block.prefix_size == 1);
    assert(data_block.bytes.size() == 255);
    for (int i = 0; i < 254; ++i) assert(data_block.bytes[i] == 'x');
    assert(data_block.bytes[254] == 0);
    assert(builder.mCurrentSendTotal == 255);

    std::cout << "production Variable-1 clamp preserves input and emits 255 bytes PASS\n";
}
'''
    with tempfile.TemporaryDirectory(dir='/tmp/opencode', prefix='var1-clamp-') as tmp:
        cpp, exe = Path(tmp) / 'test.cpp', Path(tmp) / 'test'
        cpp.write_text(fixture)
        subprocess.run(['c++', '-std=c++17', '-Wall', '-Wextra', '-Werror',
                        '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
                        str(cpp), '-o', str(exe)], check=True)
        subprocess.run([str(exe)], check=True)


if __name__ == '__main__':
    main()
