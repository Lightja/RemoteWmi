#include <ws2tcpip.h>
#include <winsock2.h>
#include <algorithm>
#include <comdef.h>       //COM (for WMI)
#include <Wbemidl.h>      //WMI
#include <codecvt>        //std::codecvt_utf8
#include <sstream>        //std::stringstream
#include <chrono>         //std::chrono
#include <thread>         //std::this_thread
#include <strsafe.h>
#include "RemoteWmiUtils.h"
#include "RemoteWmiStdIncludes.h"
        

namespace RemoteWmi {


    void InitCom() {
        HRESULT hres = CoInitializeEx(0, COINIT_APARTMENTTHREADED);
        std::cout << "CoInitializeEx() result: 0x" << std::hex << hres << std::dec << " - " << HresultToString(hres);
        if (FAILED(hres)) { std::cout << "Failed to initialize COM library. Error code = 0x" << std::hex << hres << std::endl; return; }
        hres = CoInitializeSecurity(
                NULL,
                -1,                             // COM negotiates service if -1, otherwise size of Authentication Services array
                NULL,                           // Authentication services array
                NULL,                           // Reserved
                RPC_C_AUTHN_LEVEL_PKT_PRIVACY,  // Default Security
                RPC_C_IMP_LEVEL_IMPERSONATE,    // Default Impersonation
                NULL,                           // Authentication info
                EOAC_MUTUAL_AUTH,               // Additional capabilities
                0);                             // Reserved
        std::cout << "CoInitializeSecurity() result: 0x" << std::hex << hres << std::dec << " - " << HresultToString(hres);
        if (FAILED(hres)) { std::cout << "Failed to initialize security." << std::endl; CoUninitialize(); return;}
    }

    wstring GetClientDomain() {
        wstring domain; IWbemLocator*  pLoc = NULL; IWbemServices* pSvc = NULL; IEnumWbemClassObject* pEnumerator;
        std::wcout << L"Attempting to retrieve domain..." << std::endl;
        CoCreateInstance(CLSID_WbemLocator,0,CLSCTX_INPROC_SERVER,IID_IWbemLocator,(LPVOID*)&pLoc);
        pLoc->ConnectServer(bstr_t(L"ROOT\\CIMV2"),NULL,NULL,0,NULL,0,0,&pSvc); pLoc->Release();
        pSvc->ExecQuery(bstr_t("WQL"),bstr_t("SELECT Domain FROM Win32_ComputerSystem"),WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,NULL,&pEnumerator);
        while (pEnumerator) {
            ULONG uReturn = 0; IWbemClassObject *pclsObj; VARIANT vtProp;
            HRESULT hres = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
            if (hres != S_OK) 
                break;
            hres = pclsObj->Get(L"Domain", 0, &vtProp, 0, 0); pclsObj->Release();
            if (SUCCEEDED(hres)) { domain = wstring(vtProp.bstrVal); VariantClear(&vtProp); }
        }
        pSvc->Release();
        std::wcout << L"Retrieved Domain: " << domain << std::endl;
        return domain;
    }

    SEC_WINNT_AUTH_IDENTITY_W* GetAuthStruct(wstring user, wstring password, wstring domain) {
        SEC_WINNT_AUTH_IDENTITY_W* auth_identity = new SEC_WINNT_AUTH_IDENTITY_W;
        ZeroMemory(auth_identity, sizeof(SEC_WINNT_AUTH_IDENTITY_W));
        auth_identity->User     = new unsigned short[32];
        auth_identity->Password = new unsigned short[32];
        auth_identity->UserLength     = user.length();
        auth_identity->PasswordLength = password.length();
        StringCbCopyW((LPWSTR)auth_identity->User,     32 * sizeof(WCHAR), user.c_str());
        StringCbCopyW((LPWSTR)auth_identity->Password, 32 * sizeof(WCHAR), password.c_str());
        if (domain.length() > 0) {
            auth_identity->Domain = new unsigned short[32];
            StringCbCopyW((LPWSTR)auth_identity->Domain,   32 * sizeof(WCHAR), domain.c_str());
            auth_identity->DomainLength = domain.length();
        }
        auth_identity->Flags = SEC_WINNT_AUTH_IDENTITY_UNICODE;
        return auth_identity;
    }

    unique_ptr<map<wstring, vector<VARIANT>>> MapResults(IEnumWbemClassObject* pEnumerator) {
        unique_ptr<map<wstring, vector<VARIANT>>> results = make_unique<map<wstring, vector<VARIANT>>>();
        ULONG uReturn = 0; HRESULT hres;
        if (pEnumerator == nullptr) 
            return results;
        while (true) {
            IWbemClassObject* result = nullptr; VARIANT vt_obj; BSTR column;                
            /* HRESULT hres = pEnumerator->Next(WBEM_INFINITE, 1, &result, &uReturn); */
            if ((hres = pEnumerator->Next(WBEM_INFINITE, 1, &result, &uReturn)) != S_OK) { if (hres != 1) std::cout << "ERROR enumerating results, Next() returned error 0x" << std::hex << hres << std::dec << " - " << HresultToString(hres);//1 means we just reached the end and is normal, only output if it an abnormal error.
                break;
            }
            result->BeginEnumeration(WBEM_FLAG_NONSYSTEM_ONLY);
            while (result->Next(0, &column, &vt_obj, nullptr, nullptr) == S_OK) {
                (*results)[wstring(column)].push_back(vt_obj);
                SysFreeString(column);
            }
            result->EndEnumeration(); result->Release();
        } 
        return results;
    }

    void OutputWMIObjectValues(IEnumWbemClassObject* pEnumerator) {PrintResults(pEnumerator);}
    void PrintResults(IEnumWbemClassObject* pEnumerator) {
        if (!pEnumerator) { std::cout << "Invalid results enumerator! (Null)" << std::endl;
            return; 
        }
        ULONG uReturn = 0; IWbemClassObject* result = nullptr;
        while (pEnumerator->Next(WBEM_INFINITE, 1, &result, &uReturn) == S_OK && uReturn > 0) {
            VARIANT vtValue; CIMTYPE propType; BSTR column;
            if (FAILED(result->BeginEnumeration(WBEM_FLAG_NONSYSTEM_ONLY))) { result->Release(); std::cout << "Failed to begin enumeration of WMI object properties!" << std::endl;
                continue; 
            }
            while (result->Next(0, &column, &vtValue, &propType, nullptr) == S_OK) {
                switch (propType) {
                    case CIM_SINT32:  std::wcout << column << ": " << vtValue.intVal  << " "; break;
                    case CIM_UINT32:  std::wcout << column << ": " << vtValue.uintVal << " "; break;
                    case CIM_STRING:  std::wcout << column << ": " << vtValue.bstrVal << " "; break;
                    case CIM_UINT64:  std::wcout << column << ": " << vtValue.uintVal << " "; break;
                    case CIM_BOOLEAN: std::wcout << column << ": " << vtValue.boolVal << " "; break;
                    default:          std::wcout << column << ": " << vtValue.bstrVal << " "; break; 
                }
            }
            result->EndEnumeration(); result->Release();
        }
    }

    void PrintResults(unique_ptr<map<wstring, vector<VARIANT>>> results) {
        for (pair<wstring, vector<VARIANT>> result : *results) {
            std::wcout << result.first << ": ";
            for (size_t i = 0; i < result.second.size(); ++i) {
                std::wcout << result.second[i].bstrVal;
                if (i < result.second.size() - 1) std::wcout << ", ";
            }
            std::wcout << std::endl;
        }
    }

    string GetAuthnSvcStr(unsigned long enum_value) {
        switch (enum_value) {
            case 0: return "RPC_C_AUTHN_NONE";
            case 1: return "RPC_C_AUTHN_DCE_PRIVATE";
            case 2: return "RPC_C_AUTHN_DCE_PUBLIC";
            case 4: return "RPC_C_AUTHN_DEC_PUBLIC";
            case 9: return "RPC_C_AUTHN_GSS_NEGOTIATE";
            case 10: return "RPC_C_AUTHN_WINNT";
            case 14: return "RPC_C_AUTHN_GSS_SCHANNEL";
            case 16: return "RPC_C_AUTHN_GSS_KERBEROS";
            case 17: return "RPC_C_AUTHN_DPA";
            case 18: return "RPC_C_AUTHN_MSN";
            case 20: return "RPC_C_AUTHN_KERNEL";
            case 21: return "RPC_C_AUTHN_DIGEST";
            case 30: return "RPC_C_AUTHN_NEGO_EXTENDER";
            case 31: return "RPC_C_AUTHN_PKU2U";
            case 100: return "RPC_C_AUTHN_MQ";
            case 0xFFFFFFFL: return "RPC_C_AUTHN_DEFAULT";
            default: return "Unknown";
        }
    }

    string GetAuthzSvcStr(unsigned long enum_value) {
        switch (enum_value) {
            case 0: return "RPC_C_AUTHZ_NONE";
            case 1: return "RPC_C_AUTHZ_NAME";
            case 2: return "RPC_C_AUTHZ_DCE";
            case 0xFFFFFFFF: return "RPC_C_AUTHZ_DEFAULT";
            default: return "Unknown";
        }
    }

    string GetAuthnLevelStr(unsigned long enum_value) {
        switch (enum_value) {
            case 0: return "RPC_C_AUTHN_LEVEL_DEFAULT";
            case 1: return "RPC_C_AUTHN_LEVEL_NONE";
            case 2: return "RPC_C_AUTHN_LEVEL_CONNECT";
            case 3: return "RPC_C_AUTHN_LEVEL_CALL";
            case 4: return "RPC_C_AUTHN_LEVEL_PKT";
            case 5: return "RPC_C_AUTHN_LEVEL_PKT_INTEGRITY";
            case 6: return "RPC_C_AUTHN_LEVEL_PKT_PRIVACY";
            default: return "Unknown";
        }
    }

    string GetImpLevelStr(unsigned long enum_value) {
        switch (enum_value) {
            case 0: return "RPC_C_IMP_LEVEL_DEFAULT";
            case 1: return "RPC_C_IMP_LEVEL_ANONYMOUS";
            case 2: return "RPC_C_IMP_LEVEL_IDENTIFY";
            case 3: return "RPC_C_IMP_LEVEL_IMPERSONATE";
            case 4: return "RPC_C_IMP_LEVEL_DELEGATE";
            default: return "Unknown";
        }
    }

    string GetCapabilitiesStr(unsigned long enum_value) {
        switch (enum_value) {
            case 0: return "EOAC_NONE";
            case 1: return "EOAC_MUTUAL_AUTH";
            case 0x2: return "EOAC_SECURE_REFS";
            case 0x4: return "EOAC_ACCESS_CONTROL";
            case 0x8: return "EOAC_APPID";
            case 0x10: return "EOAC_DYNAMIC";
            case 0x20: return "EOAC_STATIC_CLOAKING";
            case 0x40: return "EOAC_DYNAMIC_CLOAKING";
            case 0x80: return "EOAC_ANY_AUTHORITY";
            case 0x100: return "EOAC_MAKE_FULLSIC";
            case 0x800: return "EOAC_DEFAULT";
            case 0x200: return "EOAC_REQUIRE_FULLSIC";
            case 0x400: return "EOAC_AUTO_IMPERSONATE";
            case 0x1000: return "EOAC_DISABLE_AAA";
            case 0x2000: return "EOAC_NO_CUSTOM_MARSHAL";
            case 0x4000: return "EOAC_RESERVED1";
            default: return "Unknown";
        }
    }

    void PrintAuthIdentity(const SEC_WINNT_AUTH_IDENTITY_W& auth_identity) {
        std::wstring_convert<std::codecvt_utf16<wchar_t>, wchar_t> converter;
        std::wstringstream wss;
        wss << L"--------------Printing SEC_WINNT_AUTH_IDENTITY_W--------------" << std::endl;
        wss << L"Domain: "          << (auth_identity.Domain   ? wstring(reinterpret_cast<const WCHAR*>(auth_identity.Domain)) : wstring(L"(null)")) << std::endl;
        wss << L"User: "            << (auth_identity.User     ? wstring(reinterpret_cast<const WCHAR*>(auth_identity.User))   : wstring(L"(null)")) << std::endl;
        wss << L"Password: "        << (auth_identity.Password ? L"******" : L"(null)") << std::endl;
        wss << L"Domain length: "   <<  auth_identity.DomainLength   << std::endl;
        wss << L"User length: "     <<  auth_identity.UserLength     << std::endl;
        wss << L"Password length: " <<  auth_identity.PasswordLength << std::endl;
        std::wcout << wss.str() << std::endl;
    }

    void PrintProxyBlanket(IUnknown* pSvc) {
        HRESULT hres; DWORD authentication_svc, authorization_svc, authentication_level, impersonation_level, capabilities; LPOLESTR p_server_princ_name; RPC_AUTH_IDENTITY_HANDLE p_auth_identity;
        hres = CoQueryProxyBlanket(pSvc, &authentication_svc, &authorization_svc, &p_server_princ_name, &authentication_level, &impersonation_level, &p_auth_identity, &capabilities);
        std::cout << "Service CoQueryProxyBlanket() result: 0x" << std::hex << hres << std::dec << " - " << HresultToString(hres);
        if (FAILED(hres)) 
            return;
        std::cout << "---------------- Read Proxy Blanket ----------------" << std::endl;
        if (p_server_princ_name && wcslen(p_server_princ_name) > 0) std::wcout << L"p_server_princ_name: " << wstring(p_server_princ_name) << std::endl; //HexToASCII(wstringToString(p_server_princ_name)) << std::endl;
        std::cout << "authentication_svc: "   << authentication_svc   << " - " << GetAuthnSvcStr(authentication_svc) << std::endl;
        std::cout << "authorization_svc: "    << authorization_svc    << " - " << GetAuthzSvcStr(authorization_svc) << std::endl;
        std::cout << "authentication_level: " << authentication_level << " - " << GetAuthnLevelStr(authentication_level) << std::endl;
        std::cout << "impersonation_level: "  << impersonation_level  << " - " << GetImpLevelStr(impersonation_level) << std::endl;
        std::cout << "capabilities: "         << capabilities         << " - " << GetCapabilitiesStr(capabilities) << std::endl;
        IUnknown* pUnk = nullptr;
        hres = pSvc->QueryInterface(IID_IUnknown, (void**)&pUnk);
        std::cout << "QueryInterface() result: 0x" << std::hex << hres << std::dec << " - " << HresultToString(hres);
        if (FAILED(hres)) { std::cerr << "Could not get IUnknown interface above proxy." << std::endl; pUnk->Release();
            return;
        }
        hres = CoQueryProxyBlanket(pUnk, &authentication_svc, &authorization_svc, &p_server_princ_name, &authentication_level, &impersonation_level, &p_auth_identity, &capabilities);
        std::cout << "IUnknown CoQueryProxyBlanket() result: 0x" << std::hex << hres << std::dec << " - " << HresultToString(hres);
        if (FAILED(hres)) 
            return;
        std::cout << "--------- Read IUnknown (Parent) Proxy Blanket ---------" << std::endl;
        if (p_server_princ_name && wcslen(p_server_princ_name) > 0) std::wcout << L"p_server_princ_name: " << wstring(p_server_princ_name) << std::endl; //HexToASCII(wstringToString(p_server_princ_name)) << std::endl;
        std::cout << "authentication_svc: "   << authentication_svc   << " - " << GetAuthnSvcStr(authentication_svc) << std::endl;
        std::cout << "authorization_svc: "    << authorization_svc    << " - " << GetAuthzSvcStr(authorization_svc) << std::endl;
        std::cout << "authentication_level: " << authentication_level << " - " << GetAuthnLevelStr(authentication_level) << std::endl;
        std::cout << "impersonation_level: "  << impersonation_level  << " - " << GetImpLevelStr(impersonation_level) << std::endl;
        std::cout << "capabilities: "         << capabilities         << " - " << GetCapabilitiesStr(capabilities) << std::endl;
    }

    bool isValidIpv4(const wstring& ipAddress) {
        vector<int> octets; int num; WCHAR dot;
        std::wistringstream iss(ipAddress);
        while (iss >> num && octets.size() < 4) {
            if (num < 0 || num > 255)
                return false;
            octets.push_back(num);
            iss >> dot;
        }
        if (octets.size() != 4) 
            return false;
        return true;
    }

    wstring GetHostnameByIp(const wstring& w_ip) {
        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
        string ip = converter.to_bytes(w_ip);
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) { std::cout << "WSAStartup failed." << std::endl;
            return L"";
        }
        sockaddr_in saGNI;
        WCHAR hostname[NI_MAXHOST];
        ZeroMemory(&saGNI, sizeof(sockaddr_in));
        saGNI.sin_family = AF_INET;
        inet_pton(AF_INET, ip.c_str(), &(saGNI.sin_addr));
        if (GetNameInfoW((sockaddr*)&saGNI, sizeof(sockaddr_in), hostname, NI_MAXHOST, NULL, 0, 0) != 0) { std::cout << "GetNameInfoW failed." << std::endl;
            return L"";
        }
        WSACleanup();
        return wstring(hostname);
    }

    wstring ParseUser(wstring& input) {
        size_t pos = input.find('\\');
        if (pos == string::npos)
            return input;
        return input.substr(pos + 1);
    }

    wstring ParseDomain(wstring& input) {
        size_t pos = input.find('\\');
        if (pos == string::npos)
            return L"";
        return input.substr(0, pos);
    }

    string HresultToString(HRESULT hr) {
        char buf[256];
        if (FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, nullptr, hr, 0, buf, 256, nullptr) != 0) return string(buf);
        else return string(_com_error(hr).ErrorMessage());
    }

    wstring lowercase(const wstring& str) {
        wstring result = str;
        std::transform(result.begin(), result.end(), result.begin(), ::tolower);
        return result;
    }

    bool strContainsStr(const wstring& str1, const wstring& str2){
        if (lowercase(str1).find (lowercase(str2)) != wstring::npos) 
            return true;
        return false;
    }
}
