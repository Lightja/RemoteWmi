#include "RemoteWmi.h"
#include "RemoteWmiUtils.h"
#include "RemoteWmiStdIncludes.h"


using namespace RemoteWmi;
int main(){
    try {
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
    }
    catch(std::exception e){
        std::cerr << "Exception: " << e.what() << std::endl;
    }
    return 42;
}
