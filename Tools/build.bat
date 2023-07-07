:: Init x64 so cmd we're independent of cmd architecture ::
setlocal enabledelayedexpansion
set "originalPath=!PATH!"
call "C:/Program Files/Microsoft Visual Studio/2022/Professional/VC/Auxiliary/Build/vcvars64.bat"
set CLANG="C:/Program Files/LLVM/bin/clang.exe"
set CLANGCL="C:/Program Files/LLVM/bin/clang-cl.exe"
set CLANGPP="C:/Program Files/LLVM/bin/clang++.exe"
set CFLAGSD="/Zi /EHsc /Od"
set CFLAGSCLANGCL=/O2 /DNDEBUG
set CFLAGSCLANGCLD=/Od /Zi /RTC1
set CFLAGSCLANGPP=-O2 -DNDEBUG
set CFLAGSCLANGPPD=-OO -g -D_DEBUG
set MSVC="C:/Program Files/Microsoft Visual Studio/2022/Professional/VC/Tools/MSVC/14.35.32215/bin/Hostx64/x64/cl.exe"
set CFLAGS=/MTd /Zi /Ob0 /Od /RTC1
@echo on
cmake -S . -B build/ -G "Ninja" -DCMAKE_CXX_COMPILER=%CLANGCL% -DCMAKE_C_COMPILER=%CLANGCL% -DCMAKE_C_FLAGS_RELEASE="%CFLAGSCLANGCL%" -DCMAKE_CXX_FLAGS_RELEASE="%CFLAGSCLANGCL%" -DCMAKE_BUILD_TYPE=Release 
cmake -S . -B build/debug/ -G "Ninja" -DCMAKE_CXX_COMPILER=%CLANGCL% -DCMAKE_C_COMPILER=%CLANGCL% -DCMAKE_C_FLAGS_DEBUG="%CFLAGSCLANGCLD%" -DCMAKE_CXX_FLAGS_DEBUG="%CFLAGSCLANGCLD%" -DCMAKE_BUILD_TYPE=DEBUG 

cmake -S . -B buildVS/ -G "Visual Studio 17 2022" -DCMAKE_CXX_COMPILER=%MSVC% -DCMAKE_C_COMPILER=%MSVC% -DCMAKE_C_FLAGS_RELEASE="%CFLAGS%" -DCMAKE_CXX_FLAGS_RELEASE="%CFLAGS%" -DCMAKE_BUILD_TYPE=Release 
cmake -S . -B buildVS/debug/ -G "Visual Studio 17 2022" -DCMAKE_CXX_COMPILER=%MSVC% -DCMAKE_C_COMPILER=%MSVC% -DCMAKE_C_FLAGS_DEBUG="%CFLAGS%" -DCMAKE_CXX_FLAGS_DEBUG="%CFLAGS%" -DCMAKE_BUILD_TYPE=DEBUG 

ninja -C build/
ninja -C build/debug/
REM ninja -C buildVS/
REM ninja -C buildVS/debug/
copy build\compile_commands.json .
set "PATH=!originalPath!"



