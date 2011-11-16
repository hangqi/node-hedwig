import Options, Utils
from os import unlink, symlink, chdir
from os.path import exists, lexists

srcdir = "."
blddir = "build"
VERSION = "4.0.0"

def set_options(opt):
    opt.tool_options("compiler_cxx")
    # opt.add_option('--with-hedwig-headers', action='store', default='/usr/include', help='Path to hedwig headers, e.g. /usr/include')
    # opt.add_option('--with-hedwig-libs', action='store', default='/usr/lib', help='Path to hedwig library, e.g. /usr/lib')

def configure(conf):
    conf.check_tool("compiler_cxx")
    conf.check_tool("node_addon")

    # hedwig headers and libs
    # conf.env.append_unique('CXXFLAGS', '-I' + Options.options.with_hedwig_headers)
    # conf.env.append_unique('CXXFLAGS', '-I/opt/local/include')
    # conf.env.append_unique('LINKFLAGS', '-L' + Options.options.with_hedwig_libs)
    # conf.env.append_unique('LINKFLAGS', '-L/opt/local/lib')
    # conf.env.append_unique('LINKFLAGS', '-lhedwig01 -lboost_system-mt -lboost_thread-mt -lprotobuf -llog4cxx')

    # if not conf.check_cxx(lib="hedwig01"):
    #    conf.fatal("Missing hedwig library")
    # else:
    #    conf.env.append_unique('LINKFLAGS', '-lhedwig01')

    # if not conf.check_cxx(header_name='hedwig/client.h'):
    #     conf.fatal("Missing hedwig/client.h")

def build(bld):
    obj = bld.new_task_gen("cxx", "shlib", "node_addon")
    obj.cxxflags = ["-g", "-D_FILE_OFFSET_BITS=64", "-D_LARGEFILE_SOURCE", "-I/opt/local/include", "-I/Users/sijie/Projects/bookkeeper_git/hedwig-client/src/main/cpp/inc/"]
    obj.linkflags = ["-L/opt/local/lib", "-L/Users/sijie/Projects/bookkeeper_git/hedwig-client/src/main/cpp/lib/.libs", "-lhedwig01", "-lboost_system-mt", "-lboost_thread-mt", "-lprotobuf", "-llog4cxx"]
    obj.target = "hedwig"
    obj.source = "./src/hedwig.cpp"
    obj.uselib = "HEDWIG01"
