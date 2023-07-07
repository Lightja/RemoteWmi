# RemoteWmi

RemoteWmi is a lightweight library for connecting to and interacting with remote Windows Management Instrumentation (WMI) providers. I could not find good information easily available for how to do this, specifically with the credentials of another user that isn't running the client, so I'm hoping this will save someone else that headache. Feel free to use the code in its entirety or just as an example. In summary, the difficult to find information for me was setting the security on the parent IUnknown proxy as well as the com object and its IUnknown parent proxy returned by ExecQuery().

## Features

- Connect to remote WMI providers
- Execute WMI queries remotely as a different user than the client with provided credentials
- Retrieve information from remote WMI classes and instances
- Invoke methods on remote WMI objects
- Debugging tools for Wmi

## Build 

You can build RemoteWmi using Windows:

```
git clone https://github.com/exampleuser/RemoteWmi.git
cd RemoteWmi
tools\build.bat
```
Note: tools\build.bat is building the project using default locations for latest MSVC as of July 2023 and locations for clang executables as if built from LLVM into c:\llvm-project. Builds both Visual Studio and Ninja makefiles. If you want the ninja makefiles and you dont know how to build llvm yourself, you can download and build llvm accordingly using the following commands: (Warning: takes several hours, I also included optional features like lldb and the vscode plugin for lldb integration). You can also try just changing the compiler, its not a difficult program to build.
```
cd c:\
git clone https://github.com/llvm/llvm-project
cd C:\llvm-project
cmake -G "Visual Studio 17 2022" -A x64 -Thost=x64 -DLLVM_ENABLE_PROJECTS="clang;clang-tools-extra;lldb;compiler-rt" -DLLVM_ENABLE_RUNTIMES="compiler-rt" llvm -B build/
cmake --build build --config Release
cmake --build build/ --target lldb-vscode
cmake --build build/ --target lldb-server
```

## Usage

Here's a basic example of how to use RemoteWmi to connect to a remote WMI provider and retrieve information:

```
InitCom();
Wmi test_wmi;
test_wmi = Wmi(L"192.168.1.1", L".\\testuser",L"testpassword");
OutputWMIObjectValues(test_wmi.Query("SELECT DeviceID, FreeSpace, Size FROM Win32_LogicalDisk WHERE DriveType=3"));
OutputWMIObjectValues(test_wmi.Query("SELECT TotalVisibleMemorySize, FreePhysicalMemory FROM Win32_OperatingSystem"));
test_wmi = Wmi(L"192.168.1.1", L"testdomain\\testuser",L"testpassword");
OutputWMIObjectValues(test_wmi.Query("SELECT DeviceID, FreeSpace, Size FROM Win32_LogicalDisk WHERE DriveType=3"));
OutputWMIObjectValues(test_wmi.Query("SELECT TotalVisibleMemorySize, FreePhysicalMemory FROM Win32_OperatingSystem"));
test_wmi = Wmi(); // local Wmi connection
OutputWMIObjectValues(test_wmi.Query("SELECT DeviceID, FreeSpace, Size FROM Win32_LogicalDisk WHERE DriveType=3"));
OutputWMIObjectValues(test_wmi.Query("SELECT TotalVisibleMemorySize, FreePhysicalMemory FROM Win32_OperatingSystem"));
CoUninitialize();
```

## Additional useful Wmi Documentation:
[Microsoft Example: Setting Security](https://github.com/microsoft/Windows-classic-samples/blob/main/Samples/Win7Samples/sysmgmt/wmi/vc/security/security.cpp)
[Microsoft Documentation: Creating a WMI Application Using C++](https://learn.microsoft.com/en-us/windows/win32/wmisdk/creating-a-wmi-application-using-c-)

## Contribution

Contributions are welcome! If you find any issues or have suggestions for improvements, please feel free to open an issue or submit a pull request

## License

This project is licensed under the [MIT License](https://opensource.org/license/mit/).

