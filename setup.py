# setup.py

from setuptools import setup, Extension
import platform
import sys

extra_compile_args = []
extra_link_args = []
define_macros = []

if platform.system() == "Windows":
    extra_compile_args = ["/O2", "/W3"]
    define_macros = [("_CRT_SECURE_NO_WARNINGS", "1")]
else:
    extra_compile_args = [
        "-O3",
        "-Wall",
        "-Wextra",
        "-Wno-unused-parameter",
        "-std=c11",
    ]

sources = [
    "src/flexcache_python.c",
    "src/flexcache.c",
    "src/flexcache_policy_lru.c",
    "src/flexcache_policy_fifo.c",
    "src/flexcache_policy_random.c",
]

flexcache_ext = Extension(
    name="flexcache._flexcache",
    sources=sources,
    include_dirs=["src", "src/lib"],
    extra_compile_args=extra_compile_args,
    extra_link_args=extra_link_args,
    define_macros=define_macros,
)

setup(
    ext_modules=[flexcache_ext],
)