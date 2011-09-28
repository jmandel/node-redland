import Options
from os import unlink, symlink, popen
from os.path import exists 

srcdir = "."
blddir = "build"
VERSION = "0.0.1"

def set_options(opt):
  opt.tool_options("compiler_cxx")

def configure(conf):
  conf.check_tool("compiler_cxx")
  conf.check_tool("node_addon")

  conf.check_cfg(package='raptor2', args='--cflags --libs', uselib_store='LIBRAPTOR2')
  conf.check_cfg(package='rdf', args='--cflags --libs', uselib_store='LIBRDF')
  conf.check_cfg(package='redland', args='--cflags --libs', uselib_store='LIBREDLAND')
  conf.check_cfg(package='rasqal', args='--cflags --libs', uselib_store='LIBRASQAL')

def build(bld):
  obj = bld.new_task_gen("cxx", "shlib", "node_addon")
  obj.target = "rdf_binding"
  obj.source = "rdf_binding.cpp"
  obj.cxxflags = ["-g", "-D_FILE_OFFSET_BITS=64", "-D_LARGEFILE_SOURCE", "-Wall"]
  obj.uselib = ['LIBRAPTOR2', 'LIBRASQAL', 'LIBRDF', 'LIBREDLAND']

def shutdown():
  if Options.commands['clean']:
    if exists('rdf_binding.node'): unlink('rdf_binding.node')
  else:
    if exists('build/default/rdf_binding.node') and not exists('rdf_binding.node'):
      symlink('build/default/rdf_binding.node', 'rdf_binding.node')