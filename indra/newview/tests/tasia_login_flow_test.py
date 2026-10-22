#!/usr/bin/env python3
"""Source-contract checks for login field transitions (not a live UI test)."""

from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[3]
LOGIN = (ROOT / "indra/newview/llpanellogin.cpp").read_text()
MESSAGES = (ROOT / "indra/newview/llviewermessage.cpp").read_text()
HANDLER = (ROOT / "indra/newview/llloginhandler.cpp").read_text()
LOGIN_INSTANCE = (ROOT / "indra/newview/lllogininstance.cpp").read_text()
STARTUP = (ROOT / "indra/newview/llstartup.cpp").read_text()
STORE = (ROOT / "indra/newview/lltasia_login_store.cpp").read_text()
SECURITY = (ROOT / "indra/newview/llsechandler_basic.cpp").read_text()


def function_body(source: str, signature: str, next_signature: str) -> str:
    start = source.index(signature)
    end = source.index(next_signature, start)
    return source[start:end]


class LoginFlowContracts(unittest.TestCase):
    def test_grid_switch_preserves_only_manually_typed_username(self):
        update = function_body(LOGIN, "void LLPanelLogin::updateServer()",
                               "void LLPanelLogin::updateLoginButtons()")
        capture = update.index("username_combo")
        populate = update.index("populateUserList(credential)")
        restore = update.index("setTextEntry(manual_username)")
        clear_password = update.index('getChild<LLLineEditor>("password_edit")->setValue(std::string())')

        self.assertLess(capture, populate)
        self.assertLess(populate, restore)
        self.assertLess(restore, clear_password)
        self.assertIn("getCurrentIndex() == -1", update)
        self.assertIn("if (restore_manual_username)", update)
        self.assertIn("mUsernameLength", update)
        self.assertIn("mPasswordLength = 0", update)
        self.assertIn("mPasswordModified = false", update)
        self.assertNotIn('getChild<LLLineEditor>("password_edit")->getText()', update)

    def test_login_diagnostics_do_not_emit_typed_identity(self):
        fields = function_body(LOGIN, "void LLPanelLogin::getFields(",
                               "// static\nbool LLPanelLogin::areCredentialFieldsDirty()")
        self.assertNotIn('<< username <<', fields)
        self.assertNotIn('<< first <<', fields)
        self.assertNotIn('<< last <<', fields)
        self.assertIn('"retrieving login fields"', fields)

    def test_movement_completion_checks_session_before_advancing_startup(self):
        handler = function_body(MESSAGES, "void process_agent_movement_complete(",
                                "void process_crossed_region(")
        mismatch = handler.index("gAgent.getSessionID() != session_id")
        reject = handler.index("return; // ignore messages not meant for us")
        advance = handler.index("gAgentMovementCompleted = true")
        self.assertLess(mismatch, reject)
        self.assertLess(reject, advance)

    def test_circuit_diagnostics_do_not_emit_host_or_circuit_code(self):
        complete = function_body(MESSAGES, "void send_complete_agent_movement(",
                                 "void process_logout_reply(")
        self.assertIn('"Sending CompleteAgentMovement"', complete)
        self.assertNotIn('<< sim_host', complete)

        teleport = function_body(MESSAGES, "void process_teleport_finish(",
                                 "// stuff we have to do every time")
        self.assertIn('"process_teleport_finish() sending UseCircuitCode"', teleport)
        self.assertNotIn('<< sim_host', teleport)
        self.assertNotIn('<< msg->mOurCircuitCode', teleport)

    def test_global_legacy_credentials_are_not_rebound_to_custom_grid(self):
        initialize = function_body(HANDLER, "LLPointer<LLCredential> LLLoginHandler::initializeLoginInfo()",
                                   "LLPointer<LLCredential> LLLoginHandler::loadSavedUserLoginInfo()")
        self.assertIn("loadCredential(LLGridManager::getInstance()->getGrid())", initialize)
        self.assertIn("isInSecondLife()", initialize)
        self.assertIn("loadSavedUserLoginInfo()", initialize)

    def test_password_hash_placeholder_is_not_hashed_again(self):
        set_fields = function_body(LOGIN, "void LLPanelLogin::setFields(",
                                  "// static\nvoid LLPanelLogin::getFields(")
        self.assertIn("mPasswordModified = false", set_fields)
        self.assertIn("mPasswordModified = true; // Real text", set_fields)
        self.assertIn("const std::string filler(\"123456789!123456\")", set_fields)

        get_fields = function_body(LOGIN, "void LLPanelLogin::getFields(",
                                   "// static\nbool LLPanelLogin::areCredentialFieldsDirty()")
        self.assertIn("if (LLPanelLogin::sInstance->mPasswordModified)", get_fields)
        self.assertIn("LLMD5 pass((const U8 *)password.c_str())", get_fields)
        self.assertIn("authenticator = credential->getAuthenticator()", get_fields)

    def test_authentication_uses_grid_uri_and_splash_is_separate(self):
        connect = function_body(LOGIN_INSTANCE, "void LLLoginInstance::connect(LLPointer<LLCredential> credentials)",
                                "void LLLoginInstance::connect(const std::string& uri")
        self.assertIn("getLoginURIs(uris)", connect)
        self.assertIn("connect(uris.front(), credentials)", connect)

        splash = function_body(LOGIN, "void LLPanelLogin::loadLoginPage()",
                               "void LLPanelLogin::onClickForgotPassword(")
        self.assertIn("https://startup.tasiaviewer.work/", splash)
        self.assertNotIn("mLoginModule->connect", splash)

    def test_custom_account_sql_save_is_after_successful_response_processing(self):
        login_response = function_body(STARTUP,
                                       "if (STATE_LOGIN_PROCESS_RESPONSE == LLStartUp::getStartupState())",
                                       "// World Init")
        self.assertIn("if (process_login_success_response())", login_response)
        self.assertIn("LLPanelLogin::saveLoginAccount(gUserCredential", login_response)
        self.assertLess(login_response.index("if (process_login_success_response())"),
                        login_response.index("LLPanelLogin::saveLoginAccount(gUserCredential"))

    def test_startup_diagnostics_do_not_log_identity_or_full_failure_response(self):
        self.assertIn('"Attempting login with selected credential"', STARTUP)
        self.assertNotIn('"Attempting login as: " << userid', STARTUP)
        self.assertNotIn('gDebugInfo["LoginName"] = userid', STARTUP)
        self.assertNotIn('<< LLLoginInstance::getInstance()->getResponse()', STARTUP)
        self.assertNotIn('<< cred->getGrid() << ":" << cred->userID()', SECURITY)
        self.assertIn('"Saving credential; authenticator persistence requested: "', SECURITY)

    def test_non_windows_account_extension_is_explicitly_unavailable(self):
        self.assertIn("bool isAvailable() { return false; }", STORE)
        save = function_body(LOGIN, "void LLPanelLogin::saveLoginAccount(",
                             "void LLPanelLogin::onImportLoginAccounts(")
        self.assertIn("if (!TasiaLoginStore::isAvailable()) return", save)
        self.assertIn("button->setVisible(false)", LOGIN)
        self.assertIn("gSecAPIHandler->saveCredential(gUserCredential, gRememberPassword)", STARTUP)
        remember_password = function_body(LOGIN, "void LLPanelLogin::onRememberPasswordCheck(",
                                          "// static\nvoid LLPanelLogin::onPassKey(")
        self.assertIn("TasiaLoginStore::isAvailable() && !remember_password", remember_password)
        self.assertIn("if (TasiaLoginStore::isAvailable())", LOGIN)


if __name__ == "__main__":
    unittest.main()
