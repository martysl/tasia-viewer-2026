#!/usr/bin/env python3
"""Empty name overrides preserve identity; website page_name never replaces it."""
from pathlib import Path
import unittest
from tasia_portable_branding_test import method
from tasia_sound_globe_test import compile_run

NV = Path(__file__).resolve().parents[1]


class NamePreservation(unittest.TestCase):
    def test_production_name_formatters_preserve_empty_fields_individually(self):
        source = (NV/'lltasia_user_config.cpp').read_text()
        fixture = r'''
#include <cassert>
#include <string>
using LLUUID=int;
struct LLAvatarName {
 std::string getDisplayName()const{return "Real Display Name";}
 std::string getUserName()const{return "real.account";}
 std::string getCompleteName()const{return "Real Display Name (real.account)";}
};
struct LLTasiaUserConfig {
 struct User {std::string cosmetic_display_name,cosmetic_username,cosmetic_account_type;};
 static User& remote(){static User user;return user;}
 static bool getUser(const LLUUID&,User& user){user=remote();return true;}
 static bool hasCosmeticAlias(const LLUUID&);
 static std::string renderDisplayName(const LLUUID&,const LLAvatarName&);
 static std::string renderUsername(const LLUUID&,const LLAvatarName&);
 static std::string renderCompleteName(const LLUUID&,const LLAvatarName&);
 static std::string renderAccountType(const LLUUID&,const std::string&);
};
'''
        for signature in ('bool LLTasiaUserConfig::hasCosmeticAlias(',
                          'std::string LLTasiaUserConfig::renderDisplayName(',
                          'std::string LLTasiaUserConfig::renderUsername(',
                          'std::string LLTasiaUserConfig::renderCompleteName(',
                          'std::string LLTasiaUserConfig::renderAccountType('):
            fixture += method(source, signature)
        fixture += r'''
int main(){LLAvatarName real;
 assert(!LLTasiaUserConfig::hasCosmeticAlias(1));
 assert(LLTasiaUserConfig::renderDisplayName(1,real)==real.getDisplayName());
 assert(LLTasiaUserConfig::renderUsername(1,real)==real.getUserName());
 assert(LLTasiaUserConfig::renderCompleteName(1,real)==real.getCompleteName());
 LLTasiaUserConfig::remote().cosmetic_account_type="Grid Admin";
 assert(LLTasiaUserConfig::renderAccountType(1,"Resident")=="Resident");
 LLTasiaUserConfig::remote().cosmetic_display_name="Explicit Display";
 assert(LLTasiaUserConfig::hasCosmeticAlias(1));
 assert(LLTasiaUserConfig::renderDisplayName(1,real)=="Explicit Display");
 assert(LLTasiaUserConfig::renderUsername(1,real)==real.getUserName());
 LLTasiaUserConfig::remote().cosmetic_display_name.clear();
 LLTasiaUserConfig::remote().cosmetic_username="explicit.account";
 assert(LLTasiaUserConfig::renderDisplayName(1,real)==real.getDisplayName());
 assert(LLTasiaUserConfig::renderUsername(1,real)=="explicit.account");
 LLTasiaUserConfig::remote().cosmetic_username.clear();
 assert(LLTasiaUserConfig::renderCompleteName(1,real)==real.getCompleteName());
}
'''
        compile_run(fixture)

    def test_website_name_is_not_a_name_fallback(self):
        avatar = (NV/'llvoavatar.cpp').read_text()
        self.assertFalse('page_name' in avatar)
        parser = method((NV/'lltasia_user_config.cpp').read_text(), 'LLTasiaUserConfig::User parseUser(')
        self.assertNotIn('getCleanString(item, "page_name"', parser)
        self.assertNotIn('getFirstString(item, "display_name"', parser)
        self.assertIn('getCleanString(item, "display_name", MAX_SHORT_TEXT)', parser)
        self.assertIn('getCleanString(item, "username", MAX_SHORT_TEXT)', parser)
        profile = method((NV/'llpanelprofile.cpp').read_text(),
                         'void LLPanelProfileSecondLife::onAvatarNameCache(')
        self.assertIn('if (!presentation.cosmetic_display_name.empty())', profile)
        self.assertIn('if (!presentation.cosmetic_username.empty())', profile)
        self.assertIn('av_name.getDisplayName()', profile)
        self.assertIn('av_name.getAccountName()', profile)
        self.assertIn('presentation.tag_color', profile)

    def test_explicit_username_alias_is_rendered_when_grid_has_no_display_names(self):
        avatar = (NV/'llvoavatar.cpp').read_text()
        legacy_name_path = avatar[avatar.index('else\n        {', avatar.index('if (LLAvatarName::useDisplayNames())')):]
        legacy_name_path = legacy_name_path[:legacy_name_path.index('\n        }')]
        self.assertIn('LLCacheName::buildFullName( firstname->getString(), lastname->getString() )', legacy_name_path)
        self.assertIn('if (show_usernames && has_tasia_user && !tasia_user.cosmetic_username.empty())', legacy_name_path)
        self.assertIn('addNameTagLine(tasia_user.cosmetic_username, username_color', legacy_name_path)

    def test_production_profile_keeps_names_and_restores_original_color(self):
        fixture = r'''
#include <cassert>
#include <string>
using LLUUID=int;using LLColor4=int;
struct LLUIColor{int value;explicit LLUIColor(int v):value(v){} };
struct LLAvatarName{std::string getDisplayName()const{return "Real Name";}std::string getAccountName()const{return "real.account";} };
struct LLLineEditor{std::string value;int color=0;void setValue(std::string v){value=v;}void setReadOnlyFgColor(LLUIColor c){color=c.value;} };
struct LLTasiaUserConfig{struct User{bool has_tag_color=true;int tag_color=7;std::string cosmetic_display_name,cosmetic_username;};
 static User& remote(){static User user;return user;}
 static bool& enabled(){static bool value=true;return value;}
 static bool getUser(LLUUID,User& user){user=remote();return enabled();} };
struct Connection{void disconnect(){} };
struct LLPanelProfileSecondLife{Connection mAvatarNameCacheConnection;LLLineEditor display,user;
 int mDisplayNameDefaultColor=2,mUserNameDefaultColor=3;
 template<class T>T* getChild(const char* name){return std::string(name)=="display_name"?&display:&user;}
 void onAvatarNameCache(const LLUUID&,const LLAvatarName&);};
'''
        fixture += method((NV/'llpanelprofile.cpp').read_text(),
                          'void LLPanelProfileSecondLife::onAvatarNameCache(')
        fixture += r'''
int main(){LLPanelProfileSecondLife panel;LLAvatarName real;
 panel.onAvatarNameCache(1,real);assert(panel.display.value=="Real Name"&&panel.user.value=="real.account");
 assert(panel.display.color==7&&panel.user.color==7);
 LLTasiaUserConfig::remote().cosmetic_display_name="Explicit Display";panel.onAvatarNameCache(1,real);
 assert(panel.display.value=="Explicit Display"&&panel.user.value=="real.account");
 LLTasiaUserConfig::remote().cosmetic_display_name.clear();
 LLTasiaUserConfig::remote().cosmetic_username="explicit.account";panel.onAvatarNameCache(1,real);
 assert(panel.display.value=="Real Name"&&panel.user.value=="explicit.account");
 LLTasiaUserConfig::enabled()=false;panel.onAvatarNameCache(2,real);
 assert(panel.display.value=="Real Name"&&panel.user.value=="real.account");
 assert(panel.display.color==2&&panel.user.color==3);
}
'''
        compile_run(fixture)


if __name__ == '__main__':
    unittest.main(verbosity=2)
