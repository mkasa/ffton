
# -*- python -*-
APPNAME = 'ffton'
VERSION = '1.02'

def options(opt):
    opt.load('compiler_cxx python')
    opt.add_option('--with-udt', action='store', default='', dest='udt', help="Specify the directory of udtlib. --withudt=/usr/local when you have /usr/local/include/udt.h and /usr/local/lib/libudt.so")

def configure(conf):
    conf.load('compiler_cxx python')
    conf.check_python_version((2,4,2))
    conf.env.append_unique('CXXFLAGS', ['-O1'])
    if conf.options.udt != '':
        conf.env.INCLUDES += ["%s/include" % conf.options.udt]
        conf.env.LIBPATH += ["%s/lib" % conf.options.udt]
    conf.env.INCLUDES += '.'
    conf.env.LIB += ['pthread']
    conf.env.CXXFLAGS += ['-O2']
    conf.check_cxx(lib = 'udt', uselib_store = 'UDT')

def build(bld):
    bld(features = 'cxx cprogram', source = 'ffton-child.cc', target = 'ffton-child', uselib = 'UDT')
    executables = ['ffton']
    bld.install_files('${PREFIX}/bin', executables, chmod=0755)


