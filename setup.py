import os
import subprocess
import sys
import sysconfig
from pathlib import Path

from setuptools import Extension, setup
from setuptools.command.build_ext import build_ext

class XMakeExtension(Extension):
    def __init__(self, name: str, xmake_target: str = ''):
        super().__init__(name, sources=[])
        if xmake_target:
            self.xmake_target = xmake_target
        else:
            self.xmake_target = name

class XMakeBuild(build_ext):
    def build_extension(self, ext: XMakeExtension):
        debug = int(os.environ.get('DEBUG',0)) if self.debug is None else self.debug
        mode = 'debug' if debug else 'release'
        if os.name=='nt':
            pylib = f'python{sysconfig.get_config_var("py_version_nodot")}'
        else:
            pylib = f'python{sysconfig.get_config_var("py_version_short")}'
        libdirs = self.library_dirs
        if conf_libdir := sysconfig.get_config_var('LIBDIR'):
            libdirs.extend(conf_libdir.split(os.path.pathsep))
        libdirs = set(libdirs)
        subprocess.run([
            'xmake', 'config',
            f'--python={sys.executable}',
            f'--mode={mode}',
            f'--pyextension_fullpath={self.get_ext_fullpath(ext.name)}',
            f'--pyincludedirs={os.path.pathsep.join(self.include_dirs)}',
            f'--pylibdirs={os.path.pathsep.join(self.library_dirs)}',
            f'--pylib={pylib}',
        ])
        subprocess.run(['xmake', 'build', ext.xmake_target])

        # generate stubs
        sodir = os.path.dirname(os.path.abspath(self.get_ext_fullpath(ext.name)))
        stubdir = os.path.abspath('build/nged-stubs')
        print(f'generate stubs into {stubdir}')
        subprocess.run([sys.executable, '-m', 'pybind11_stubgen', '-o', stubdir, '--ignore-invalid-expressions', '.*', '--ignore-unresolved-names', '.*', 'nged'], cwd=sodir)

setup(
    name = 'nged',
    version = '0.0.1',
    author = 'iiif',
    description = 'A Node Graph EDitor',
    ext_modules = [XMakeExtension('nged', 'ngpy')],
    cmdclass = {'build_ext': XMakeBuild},
    zip_safe = False,
    python_requires='>=3.0',
    setup_requires=['pybind11-stubgen'],
    packages = ['nged', 'nged-stubs'],
    package_dir= {
        'nged': 'build',
        'nged-stubs': 'build/nged-stubs/nged'
    },
    package_data = {
        'nged': ['*.pyd'],
        'nged-stubs': ['*.pyi']
    },
    include_package_data = True
)
