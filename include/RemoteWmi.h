#pragma once
#include "RemoteWmiStdIncludes.h"
#include "wbemidl.h"
/* #include "Wincred.h" */

namespace RemoteWmi {

    class Wmi {
        public:
            Wmi(const wstring& server=L"", const wstring& user=L"", const wstring& pass=L"", const wstring& domain=L"", const wstring& nspace=L"root\\cimv2"); 
            ~Wmi(); 
            unique_ptr<map<wstring, vector<VARIANT>>> Query(const string& query);
            HRESULT status;
        private:
            IEnumWbemClassObject* QueryOS(string query); 
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
            void InitWmi(); 
            void Reinitialize(const wstring& user, const wstring& pass);
    };




}
