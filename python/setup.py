# DataReel 7.5 beta: builds the datareel Python package. The _datareel extension is built with CMake
# (python/CMakeLists.txt), which builds the DataReel C++ library from the repository top (or DATAREEL_SOURCE)
# and links it in statically. Needs a C++17 compiler, CMake 3.16+, the Python headers and the OpenSSL headers.
#
#   pip wheel python/ --no-build-isolation -w dist/     (from the repository top)
#   DATAREEL_JOBS=N sets the build's parallel jobs (default 2)
import os
import shutil
import subprocess
import sys
from setuptools import setup, Extension
from setuptools.command.build_ext import build_ext

HERE = os.path.dirname(os.path.abspath(__file__))


class CMakeBuild(build_ext):
    def build_extension(self, ext):
        src = os.environ.get("DATAREEL_SOURCE", os.path.dirname(HERE))
        if not os.path.exists(os.path.join(src, "include", "gxobjrepl.h")):
            raise RuntimeError("the DataReel source is not at %s (set DATAREEL_SOURCE)" % src)
        bdir = os.path.join(os.path.abspath(self.build_temp), "cmake")
        os.makedirs(bdir, exist_ok=True)
        jobs = os.environ.get("DATAREEL_JOBS", "2")
        subprocess.check_call(["cmake", "-S", HERE, "-B", bdir, "-DCMAKE_BUILD_TYPE=Release",
                               "-DDATAREEL_SOURCE=" + src, "-DPython3_EXECUTABLE=" + sys.executable])
        subprocess.check_call(["cmake", "--build", bdir, "--config", "Release", "--target", "_datareel",
                               "-j", jobs])
        built = None
        for root, _, files in os.walk(bdir):
            for f in files:
                if f.startswith("_datareel") and (f.endswith(".so") or f.endswith(".pyd")):
                    built = os.path.join(root, f)
        if not built:
            raise RuntimeError("the _datareel extension was not built")
        out = self.get_ext_fullpath(ext.name)
        os.makedirs(os.path.dirname(out), exist_ok=True)
        shutil.copyfile(built, out)


setup(
    ext_modules=[Extension("datareel._datareel", sources=[])],
    cmdclass={"build_ext": CMakeBuild},
)
