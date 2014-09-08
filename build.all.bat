@echo off

:: GCC X86
build.py gcc x86 py2 debug
build.py gcc x86 py2 release
build.py gcc x86 py3 debug
build.py gcc x86 py3 release

:: GCC X64
:: build.py gcc x64 py2 debug
:: build.py gcc x64 py2 release
:: build.py gcc x64 py3 debug
:: build.py gcc x64 py3 release

:: VC9 X86
build.py vc9 x86 py2 debug
build.py vc9 x86 py2 release
build.py vc9 x86 py3 debug
build.py vc9 x86 py3 release

:: VC9 X64
build.py vc9 x64 py2 debug
build.py vc9 x64 py2 release
build.py vc9 x64 py3 debug
build.py vc9 x64 py3 release

:: VC10 X86
build.py vc10 x86 py2 debug
build.py vc10 x86 py2 release
build.py vc10 x86 py3 debug
build.py vc10 x86 py3 release

:: VC10 X64
build.py vc10 x64 py2 debug
build.py vc10 x64 py2 release
build.py vc10 x64 py3 debug
build.py vc10 x64 py3 release


