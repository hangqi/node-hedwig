import Options, Utils
from os import unlink, symlink, chdir
from os.path import exists, lexists

srcdir = "."
blddir = "build"
VERSION = "4.0.0"

def set_options(opt):
    opt.tool_options("compiler_cxx")
    # opt.tool_options('compiler_cxx boost') 
    opt.add_option('--with-hedwig-headers', action='store', default='/usr/include', help='Path to hedwig headers, e.g. /usr/include')
    opt.add_option('--with-hedwig-libs', action='store', default='/usr/lib', help='Path to hedwig library, e.g. /usr/lib')
    opt.add_option('--with-boost-headers', action='store', default='/usr/include', help='Path to boost headers, e.g. /usr/include')
    opt.add_option('--with-boost-libs', action='store', default='/usr/lib', help='Path to boost library, e.g. /usr/lib')
    opt.add_option('--with-boost-system', action='store', default='', help='boost system library, e.g. boost_system_gcc34-mt-1_38')
    opt.add_option('--with-boost-thread', action='store', default='', help='boost thread library, e.g. boost_thread-gcc34-mt-1_38')

def configure(conf):
    conf.check_tool("compiler_cxx")
    conf.check_tool("node_addon")

    # hedwig headers and libs
    conf.env.append_value('CXXFLAGS', '-I' + Options.options.with_hedwig_headers)
    conf.env.append_value('LINKFLAGS', '-L' + Options.options.with_hedwig_libs)
    conf.env.append_value('CXXFLAGS', '-I' + Options.options.with_boost_headers)
    conf.env.append_value('LINKFLAGS', '-L' + Options.options.with_boost_libs)
    conf.env.append_value('CXXFLAGS', '-g')
    conf.env.append_value('CXXFLAGS', '-D_FILE_OFFSET_BITS=64')
    conf.env.append_value('CXXFLAGS', '-DUSE_BOOST_TR1')
    conf.env.append_value('LINKFLAGS', '-lhedwig')

    if not Options.options.with_boost_system:
        conf.env.append_value('LINKFLAGS', '-lboost_system-gcc34-mt-1_38')
    else:
        conf.env.append_value('LINKFLAGS', '-l' + Options.options.with_boost_system)
        
    if not Options.options.with_boost_thread:
        conf.env.append_value('LINKFLAGS', '-lboost_thread-gcc34-mt-1_38')
    else:
        conf.env.append_value('LINKFLAGS', '-l' + Options.options.with_boost_thread)

    conf.env.append_value('LINKFLAGS', '-lprotobuf')
    conf.env.append_value('LINKFLAGS', '-llog4cxx')

    # if not conf.check_cxx(lib="hedwig", libpath=Options.options.with_hedwig_libs):
    #    conf.fatal("Missing hedwig library")
    # else:
    conf.env.append_value('LINKFLAGS', '-lhedwig')

    # if not conf.check_cxx(header_name='hedwig/client.h', includes=Options.options.with_hedwig_headers):
    #    conf.fatal("Missing hedwig/client.h")

def build(bld):
    obj = bld.new_task_gen("cxx", "shlib", "node_addon")

    obj.target = "hedwig"
    obj.source = "./src/hedwig.cpp"
