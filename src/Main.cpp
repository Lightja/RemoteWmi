#include "RemoteWmi.h"
#include "RemoteWmiUtils.h"
#include "RemoteWmiStdIncludes.h"


using namespace RemoteWmi;
int main(){
    try {
        InitCom();
        IEnumWbemClassObject* pEnumerator;
        unique_ptr<map<wstring, vector<VARIANT>>> results;

        Wmi* test_wmi1 = new Wmi(); //local WMI when no creds are provided
        results = test_wmi1->Query("SELECT DeviceID, FreeSpace, Size FROM Win32_LogicalDisk WHERE DriveType=3");
        PrintResults(std::move(results));
        delete test_wmi1;

        Wmi* test_wmi2 = new Wmi(L"192.168.1.1", L".\\testuser",L"testpassword1");
        results = test_wmi2->Query("SELECT DeviceID, FreeSpace, Size FROM Win32_LogicalDisk WHERE DriveType=3");
        PrintResults(std::move(results));
        delete test_wmi2;

        Wmi* test_wmi3 = new Wmi(L"192.168.1.1", L"testuser",L"testpassword1");
        results = test_wmi3->Query("SELECT DeviceID, FreeSpace, Size FROM Win32_LogicalDisk WHERE DriveType=3");
        PrintResults(std::move(results));
        delete test_wmi3;       
    }
    catch(std::exception e){
        std::cerr << "Exception: " << e.what() << std::endl;
    }
    CoUninitialize();
    return 42;
}
