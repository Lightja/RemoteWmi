#pragma once
#include "RemoteWmiStdIncludes.h"
#include "wbemidl.h"
/* #include "Wincred.h" */

namespace RemoteWmi {

    struct Wmi {
        IWbemServices* pSvc = nullptr;
        wstring domain, nspace, server, user, pass, spn;
        bool HasCredentials();
        bool IsRemoteConnection();
        bool RemoteDomainIsLocal(wstring input_domain);
        void InitNamespace();
        void InitDomain();
        void InitSpn(wstring input_domain); 
        /* void SetProxySecurity(IUnknown* pProxy,wstring remote_server, wstring user, wstring password, wstring domain, wstring spn=L""); */ 
        void SetProxySecurity(IUnknown* pProxy); 
        IEnumWbemClassObject* Query(const char* query); 
        void InitWmi(); 
        Wmi(const wstring& server=L"", const wstring& user=L"", const wstring& pass=L"", const wstring& domain=L"", const wstring& nspace=L"root\\cimv2"); 
        ~Wmi(); 
    };




}
