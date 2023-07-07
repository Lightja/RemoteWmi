#include "RemoteWmiStdIncludes.h"
#include "RemoteWmiUtils.h"
#include "RemoteWmi.h"
#include <utility>
#include <comdef.h>
#include <Wbemidl.h>
#include <codecvt>
#include <sstream>
#include <chrono>
#include <thread>
#include <wincred.h>
#include <strsafe.h>

namespace RemoteWmi {

    bool Wmi::HasCredentials() { return !this->user.empty() && !this->pass.empty(); }
    bool Wmi::IsRemoteConnection() { return !this->server.empty(); }
    bool Wmi::RemoteDomainIsLocal(wstring input_domain) { return input_domain == this->server || input_domain == L"localhost" || input_domain == L"." || input_domain == L"WORKGROUP" || input_domain == L"127.0.0.1"; }

    void Wmi::InitNamespace() { if (this->IsRemoteConnection()) this->nspace = L"\\\\" + this->server + L"\\" + this->nspace; }
    void Wmi::InitDomain() {
        if (this->HasCredentials()) {
            if (this->domain.empty()) this->domain = GetClientDomain();
            if (RemoteDomainIsLocal(ParseDomain(this->user)) || RemoteDomainIsLocal(this->domain)) this->domain = L""; // if the domain is local, don't use it
        }
    }

    void Wmi::InitSpn(wstring input_domain) { 
        if (RemoteDomainIsLocal(ParseDomain(input_domain)) || RemoteDomainIsLocal(this->domain)) { this->spn = L""; }
        else if (!input_domain.empty() && !strContainsStr(this->domain, input_domain)) this->spn = L"KERBEROS:" + input_domain + L"\\" + this->server; // allows for cross-domain auth
        else this->spn = L"KERBEROS:" + this->domain + L"\\" + this->server; // keep fqdn if user input isnt fqdn
    }

    void Wmi::InitWmi() {
        std::wcout << L"InitWmi - server: " << this->server << L", domain: " << this->domain << L", user: " << this->user << L", pass: ******"  << std::endl;
        HRESULT hres;
        wstring input_domain = ParseDomain(this->user);
        this->InitNamespace();
        this->InitDomain();
        BSTR user = nullptr; BSTR pass = nullptr; 
        if (this->HasCredentials()) {
            this->InitSpn(input_domain);
            this->user = ParseUser(this->user);
            user = SysAllocString(this->user.c_str());
            pass = SysAllocString(this->pass.c_str());
        }
        BSTR authority = SysAllocString(this->spn.c_str());
        BSTR nspace    = SysAllocString(this->nspace.c_str());
        IWbemLocator *pLoc = NULL;
        hres = CoCreateInstance(CLSID_WbemLocator,nullptr,CLSCTX_INPROC_SERVER,IID_IWbemLocator,(void**)&pLoc);
        if (FAILED(hres)) { std::cout << "Failed to create IWbemLocator object. Err code = 0x" << std::hex << hres << std::endl; return; }
        std::wcout << L"Connecting to " << nspace; if (!this->user.empty() && !this->pass.empty()) std::wcout << L" as " << user; if (!this->spn.empty()) std::wcout << L" using SPN " << authority; std::wcout << L"..." << std::endl;
        hres = pLoc->ConnectServer( nspace, user, pass, NULL, WBEM_FLAG_CONNECT_USE_MAX_WAIT, 0, 0, &this->pSvc);
        std::cout << "ConnectServer() result: 0x" << std::hex << hres << std::dec << " - " << HresultToString(hres);
        pLoc->Release(); SysFreeString(nspace); SysFreeString(user); SysFreeString(pass); SysFreeString(authority);
        if (FAILED(hres) || pSvc == NULL) { std::cout << "Could not connect. Error code = 0x" << std::hex << hres << std::dec << std::endl; this->pSvc = nullptr;
            return;
        }
        if (!input_domain.empty() && !strContainsStr(this->domain, input_domain) && !this->RemoteDomainIsLocal(input_domain)) this->domain = input_domain; // only update domain when successful
    }

    Wmi::Wmi(const wstring& server, const wstring& user, const wstring& pass, const wstring& domain, const wstring& nspace) {
        this->server = isValidIpv4(server) ? GetHostnameByIp(server) : server;
        this->user = user; this->pass = pass; this->domain = domain; this->nspace = nspace; this->spn = L"";
        this->InitWmi();
        SetProxySecurity(this->pSvc);
    }
    Wmi::~Wmi() { if (pSvc != nullptr) pSvc->Release(); }

    /* void Wmi::SetProxySecurity(IUnknown* pProxy,wstring remote_server, wstring user, wstring password, wstring domain, wstring spn) { */
    void Wmi::SetProxySecurity(IUnknown* pProxy) {
        std::wcout << L"Setting proxy security for " << this->server << L" with user " << user << L" and domain " << domain << L" and spn " << spn << std::endl;
        SEC_WINNT_AUTH_IDENTITY_W* auth_identity = NULL;
        if (!this->user.empty() && !this->pass.empty()) {
            auth_identity = GetAuth(this->user, this->pass, domain);
            /* PrintAuthIdentity(*auth_identity); */
        }
        HRESULT hres = CoSetProxyBlanket(
                pProxy,
                RPC_C_AUTHN_GSS_NEGOTIATE,
                RPC_C_AUTHZ_NONE,
                this->spn.empty() ? 0 : reinterpret_cast<OLECHAR*>(const_cast<wchar_t*>(this->spn.c_str())),
                RPC_C_AUTHN_LEVEL_PKT_PRIVACY,
                RPC_C_IMP_LEVEL_IMPERSONATE,
                auth_identity,
                EOAC_MUTUAL_AUTH);
        std::cout << "CoSetProxyBlanket() result: 0x" << std::hex << hres << std::dec << " - " << HresultToString(hres);
        IUnknown* pUnk = NULL;
        hres = pProxy->QueryInterface(IID_IUnknown, (void**)&pUnk);
        hres = CoSetProxyBlanket(
                pUnk,
                RPC_C_AUTHN_GSS_NEGOTIATE,
                RPC_C_AUTHZ_NONE,
                this->spn.empty() ? 0 : reinterpret_cast<OLECHAR*>(const_cast<wchar_t*>(this->spn.c_str())),
                RPC_C_AUTHN_LEVEL_PKT_PRIVACY,
                RPC_C_IMP_LEVEL_IMPERSONATE,
                auth_identity,
                EOAC_MUTUAL_AUTH);
        std::cout << "IUnknown CoSetProxyBlanket() result: 0x" << std::hex << hres << std::dec << " - " << HresultToString(hres);
        pUnk->Release();
        if (auth_identity != NULL) { if (auth_identity->Domain != NULL) delete[] auth_identity->Domain; delete[] auth_identity->Password; delete[] auth_identity->User; delete auth_identity; }
        PrintProxyBlanket(pProxy);
    }

    IEnumWbemClassObject* Wmi::Query(const char* query) {
        if (this->pSvc == nullptr) { std::cout << "QueryAndPrint() pSvc is null" << std::endl; return nullptr; }
        HRESULT hres; IEnumWbemClassObject* resultEnumerator = nullptr;
        hres = this->pSvc->ExecQuery(bstr_t("WQL"), bstr_t(query), WBEM_FLAG_RETURN_WBEM_COMPLETE,nullptr, &resultEnumerator);
        std::cout << "ExecQuery() result: 0x" << std::hex << hres << std::dec << " - " << HresultToString(hres);
        if(SUCCEEDED(hres)) {
            this->SetProxySecurity(resultEnumerator);
            return resultEnumerator;
        }
        return nullptr;
    }

}
