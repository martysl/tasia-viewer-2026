#!/usr/bin/env python3
"""Production grid filename helper, using the production LLMD5 implementation."""
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]
NV = ROOT/'indra/newview'


class GridFilename(unittest.TestCase):
    def test_actual_encoding_is_bounded_and_windows_safe(self):
        common = ROOT/'indra/llcommon'
        md5 = (common/'llmd5.cpp').read_text().replace(
            '#include "linden_common.h"',
            '#include <cassert>\n#include <cstdio>\n#include <cstring>\n#include <string>\n#include <istream>\n'
            'using U32=unsigned;using U8=unsigned char;\n#define LL_COMMON_API')
        fixture = r'''
#include <cassert>
#include <set>
#include "lltasia_grid_filename.h"
int main(){using TasiaGridFilename::component;
 assert(component("i.let-us.cyou")=="i.let-us.cyou");
 assert(component("util.agni.lindenlab.com")=="util.agni.lindenlab.com");
 assert(component("i.let-us.cyou:8002")=="i.let-us.cyou~3a8002");
 assert(component("host:8002")!=component("host_8002"));
 assert(component("host:8002")!=component("host~3a8002"));
 assert(component("con")!="con"&&component("con.xml")!="con.xml");
 assert(component("nul")!="nul"&&component("com1")!="com1");
 std::set<std::string> seen;
 for(int i=1;i<=255;++i){auto name=component(std::string("x")+char(i));
  assert(name.find_first_of("\\/:*?\"<>|")==std::string::npos);
  assert(name.back()!='.'&&name.back()!=' ');assert(seen.insert(name).second);
  for(unsigned char c:name)assert(c>=32&&c<127);
 }
 auto a=component(std::string(300,'a')+"1"),b=component(std::string(300,'a')+"2");
 assert(a.size()<=96&&b.size()<=96&&a!=b);
 assert(component("../elsewhere").find('/')==std::string::npos);
}
'''
        with tempfile.TemporaryDirectory(dir='/tmp/opencode', prefix='grid-filename-') as tmp:
            tmp = Path(tmp)
            (tmp/'md5.cpp').write_text(md5)
            (tmp/'test.cpp').write_text('#define LL_COMMON_API\n'+fixture)
            exe = tmp/'test'
            subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror',
                            '-fsanitize=address,undefined','-I'+str(NV),'-I'+str(common),
                            str(tmp/'test.cpp'),str(tmp/'md5.cpp'),'-o',str(exe)],check=True)
            subprocess.run([str(exe)],check=True)


if __name__ == '__main__':
    unittest.main(verbosity=2)
