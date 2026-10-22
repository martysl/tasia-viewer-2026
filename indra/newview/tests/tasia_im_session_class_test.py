#!/usr/bin/env python3
"""OpenSim one-to-one IM must not be classified as an ad-hoc conference.

Compiles a fixture classifier and checks production guard expressions.
This is a classification contract test; no live IM or network.
"""
from pathlib import Path
import re
import subprocess
import tempfile
import unittest

from tasia_portable_branding_test import method

ROOT = Path(__file__).resolve().parents[3]
VIEWER = ROOT / 'indra/newview'
SOURCE = (VIEWER / 'llimview.cpp').read_text()

# Fixture rule; production guard-expression checks below detect missing guards.
RULE = """
inline bool tasiaOpenSimP2PInvite(bool opensim, bool in_group,
                                  int dialog, std::size_t targets,
                                  bool canonical_pair, bool voice_invite)
{
    return opensim && dialog == IM_SESSION_INVITE && !in_group && targets <= 1 &&
        canonical_pair && !voice_invite;
}
"""


def compile_and_run(cpp):
    with tempfile.TemporaryDirectory(prefix='im-class-', dir='/tmp/opencode') as tmp:
        path = Path(tmp) / 'test.cpp'
        exe = Path(tmp) / 'test'
        path.write_text(cpp)
        subprocess.run(['c++', '-std=c++17', '-Wall', '-Werror', str(path), '-o', str(exe)], check=True)
        return subprocess.run([str(exe)], capture_output=True, text=True).returncode


class OpenSimP2PClassification(unittest.TestCase):
    def test_production_rule_matches_classifier(self):
        constructor = method(SOURCE, 'LLIMModel::LLIMSession::LLIMSession(')
        for fragment in ('LLGridManager::getInstance()->isInOpenSim()', 'IM_SESSION_INVITE == mType',
                         '!gAgent.isInGroup(mSessionID)', 'mInitialTargetIDs.size() <= 1',
                         'mOpenSimP2PInvite = opensim_p2p_invite',
                         'voice_channel_info.isUndefined()',
                         'mSessionID == LLIMMgr::computeSessionID(IM_NOTHING_SPECIAL, mOtherParticipantID)'):
            self.assertIn(fragment, constructor)
        # Every predicate that classifies a session must honour the flag.
        self.assertIn('!mOpenSimP2PInvite', method(SOURCE, 'bool LLIMModel::LLIMSession::isAdHoc()'))
        self.assertIn('IM_NOTHING_SPECIAL == mType || mOpenSimP2PInvite',
                      method(SOURCE, 'bool LLIMModel::LLIMSession::isP2P()'))
        # findAdHocIMSession() only considers ad-hoc sessions, so it inherits the fix.
        self.assertIn('if (!session->isAdHoc()) continue;',
                      method(SOURCE, 'LLIMModel::LLIMSession* LLIMModel::findAdHocIMSession('))

    def test_second_life_and_group_chats_unchanged(self):
        fixture = r'''
#include <cassert>
#include <cstddef>
enum EInstantMessage { IM_NOTHING_SPECIAL = 0, IM_SESSION_INVITE, IM_SESSION_P2P_INVITE,
                       IM_SESSION_GROUP_START, IM_SESSION_CONFERENCE_START };
''' + RULE + r'''
// Mirrors LLIMSession classification after the fix.
enum SType { P2P_SESSION, GROUP_SESSION, ADHOC_SESSION, NONE_SESSION };
struct Session {
    EInstantMessage type; bool in_group; std::size_t targets; bool opensim_p2p_invite; SType session_type;
    Session(bool opensim, EInstantMessage t, bool g, std::size_t n,
            bool canonical_pair = true, bool voice_invite = false)
        : type(t), in_group(g), targets(n), opensim_p2p_invite(false)
    {
        session_type = P2P_SESSION;
        opensim_p2p_invite = tasiaOpenSimP2PInvite(opensim, in_group, type, targets, canonical_pair, voice_invite);
        if (type == IM_NOTHING_SPECIAL || type == IM_SESSION_P2P_INVITE || opensim_p2p_invite) {}
        else session_type = in_group ? GROUP_SESSION : ADHOC_SESSION;
    }
    bool isAdHoc() const {
        return type == IM_SESSION_CONFERENCE_START ||
            (type == IM_SESSION_INVITE && !opensim_p2p_invite && !in_group);
    }
    bool isP2P() const { return type == IM_NOTHING_SPECIAL || opensim_p2p_invite; }
};
int main(){
    // OpenSim incoming one-to-one IM: normal IM, NOT a conference.
    Session os_p2p(true, IM_SESSION_INVITE, false, 1);
    assert(os_p2p.isP2P() && !os_p2p.isAdHoc() && os_p2p.session_type == P2P_SESSION);
    // Second Life keeps upstream behaviour: same dialog type stays ad-hoc.
    Session sl_p2p(false, IM_SESSION_INVITE, false, 1);
    assert(!sl_p2p.isP2P() && sl_p2p.isAdHoc() && sl_p2p.session_type == ADHOC_SESSION);
    // Real OpenSim group chat still a group session (session id == group id).
    Session os_group(true, IM_SESSION_INVITE, true, 1);
    assert(!os_group.isP2P() && !os_group.isAdHoc() && os_group.session_type == GROUP_SESSION);
    // Real conference unchanged.
    Session conf(true, IM_SESSION_CONFERENCE_START, false, 3);
    assert(!conf.isP2P() && conf.isAdHoc() && conf.session_type == ADHOC_SESSION);
    // Multi-recipient OpenSim invite is still an ad-hoc conference, not a P2P.
    Session os_multi(true, IM_SESSION_INVITE, false, 2);
    assert(!os_multi.isP2P() && os_multi.isAdHoc() && os_multi.session_type == ADHOC_SESSION);
    // One-peer conferences are real conferences too; count alone is not proof.
    Session os_single_conf(true, IM_SESSION_INVITE, false, 1, false);
    assert(!os_single_conf.isP2P() && os_single_conf.isAdHoc());
    Session os_voice_conf(true, IM_SESSION_INVITE, false, 1, true, true);
    assert(!os_voice_conf.isP2P() && os_voice_conf.isAdHoc());
    // Outgoing one-to-one IM unaffected on both grids.
    Session out_os(true, IM_NOTHING_SPECIAL, false, 1);
    Session out_sl(false, IM_NOTHING_SPECIAL, false, 1);
    assert(out_os.isP2P() && out_sl.isP2P() && !out_os.isAdHoc() && !out_sl.isAdHoc());
}
'''
        self.assertEqual(compile_and_run(fixture), 0)

    def test_grid_check_present_and_scoped(self):
        self.assertIn('#include "llviewernetwork.h"', SOURCE)
        self.assertIn('mOpenSimP2PInvite;', (VIEWER / 'llimview.h').read_text())


if __name__ == '__main__':
    unittest.main(verbosity=2)
