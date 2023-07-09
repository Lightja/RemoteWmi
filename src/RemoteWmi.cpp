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

    void Wmi::InitSpn(wstring input_domain) { // probably unnecessary, but it was one of the things I tried to get this working. Keeping it in case I find a need.
        if (RemoteDomainIsLocal(ParseDomain(input_domain)) || RemoteDomainIsLocal(this->domain)) { this->spn = L""; }
        else if (!input_domain.empty() && !strContainsStr(this->domain, input_domain)) this->spn = L"KERBEROS:" + input_domain + L"\\" + this->server; // allows for cross-domain auth
        else this->spn = L"KERBEROS:" + this->domain + L"\\" + this->server;
    }

    void Wmi::InitWmi() {
        if (this->HasCredentials() || this->IsRemoteConnection()) std::wcout << L"InitWmi - server: " << this->server << L", domain: " << this->domain << L", user: " << this->user << L", pass: ******"  << std::endl;
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
        this->status = CoCreateInstance(CLSID_WbemLocator,nullptr,CLSCTX_INPROC_SERVER,IID_IWbemLocator,(void**)&pLoc);
        if (FAILED(this->status)) { std::cout << "Failed to create IWbemLocator object. Err code = 0x" << std::hex << this->status << std::endl; return; }
        std::wcout << L"Connecting to " << nspace; if (!this->user.empty() && !this->pass.empty()) std::wcout << L" as " << user; if (!this->spn.empty()) std::wcout << L" using SPN " << authority; std::wcout << L"..." << std::endl;
        this->status = pLoc->ConnectServer( nspace, user, pass, NULL, WBEM_FLAG_CONNECT_USE_MAX_WAIT, 0, 0, &this->pSvc);
        std::cout << "ConnectServer() result: 0x" << std::hex << this->status << std::dec << " - " << HresultToString(this->status);
        pLoc->Release(); SysFreeString(nspace); SysFreeString(user); SysFreeString(pass); SysFreeString(authority);
        if (FAILED(this->status) || pSvc == NULL) { this->pSvc = nullptr; std::cout << "Could not connect. Error code = 0x" << std::hex << this->status << std::dec << std::endl; 
            return;
        }
        if (!input_domain.empty() && !strContainsStr(this->domain, input_domain) && !this->RemoteDomainIsLocal(input_domain)) this->domain = input_domain; // only update domain when user input credentials are for a different domain and connect successfully. Maintains fqdn from wmi.
    }

    void Wmi::Reinitialize(const wstring& user, const wstring& pass) {
        if (this->pSvc != nullptr) { this->pSvc->Release(); this->pSvc = nullptr; this->status = S_OK; }
        this->user = user; this->pass = pass; this->InitWmi();
    }

    Wmi::Wmi(const wstring& server, const wstring& user, const wstring& pass, const wstring& domain, const wstring& nspace) {
        this->server = isValidIpv4(server) ? GetHostnameByIp(server) : server;
        this->user = user; this->pass = pass; this->domain = domain; this->nspace = nspace; this->spn = L"";
        this->InitWmi();
        SetProxySecurity(this->pSvc);
    }
    Wmi::~Wmi() { if (this->pSvc) this->pSvc->Release(); }

    void Wmi::SetProxySecurity(IUnknown* pProxy) {
        if (!this->HasCredentials()) 
            return;
        std::wcout << L"Setting proxy security for " << this->server << L" with user " << user << L" and domain " << domain << L" and spn " << spn << std::endl;
        SEC_WINNT_AUTH_IDENTITY_W* auth_identity = NULL;
        if (!this->user.empty() && !this->pass.empty()) {
            auth_identity = GetAuthStruct(this->user, this->pass, domain);
            /* PrintAuthIdentity(*auth_identity); */
        }
        this->status = CoSetProxyBlanket(
                pProxy,
                RPC_C_AUTHN_GSS_NEGOTIATE,
                RPC_C_AUTHZ_NONE,
                this->spn.empty() ? NULL : reinterpret_cast<OLECHAR*>(const_cast<wchar_t*>(this->spn.c_str())), //may run into issues with non-kerberos domains
                RPC_C_AUTHN_LEVEL_PKT_PRIVACY,
                RPC_C_IMP_LEVEL_IMPERSONATE,
                auth_identity,
                EOAC_MUTUAL_AUTH);
        std::cout << "CoSetProxyBlanket() result: 0x" << std::hex << this->status << std::dec << " - " << HresultToString(this->status);
        IUnknown* pUnk = NULL;
        this->status = pProxy->QueryInterface(IID_IUnknown, (void**)&pUnk);
        this->status = CoSetProxyBlanket(
                pUnk,
                RPC_C_AUTHN_GSS_NEGOTIATE,
                RPC_C_AUTHZ_NONE,
                this->spn.empty() ? NULL : reinterpret_cast<OLECHAR*>(const_cast<wchar_t*>(this->spn.c_str())), //may run into issues with non-kerberos domains
                RPC_C_AUTHN_LEVEL_PKT_PRIVACY,
                RPC_C_IMP_LEVEL_IMPERSONATE,
                auth_identity,
                EOAC_MUTUAL_AUTH);
        std::cout << "IUnknown CoSetProxyBlanket() result: 0x" << std::hex << this->status << std::dec << " - " << HresultToString(this->status);
        pUnk->Release();
        if (auth_identity != NULL) { if (auth_identity->Domain) delete[] auth_identity->Domain; delete[] auth_identity->Password; delete[] auth_identity->User; delete auth_identity; }
        /* PrintProxyBlanket(pProxy); */
    }

    unique_ptr<map<wstring, vector<VARIANT>>> Wmi::Query(const string& query){
        IEnumWbemClassObject* results_enumerator = this->QueryOS(query);
        if (results_enumerator == nullptr) return nullptr;
        else { unique_ptr<map<wstring, vector<VARIANT>>> results = MapResults(results_enumerator); results_enumerator->Release(); return results; }
    }

    IEnumWbemClassObject* Wmi::QueryOS(string query) {
        std::cout << "Executing Query: " << query << std::endl;
        if (this->pSvc == nullptr) { std::cout << "pSvc is null" << std::endl; return nullptr; }
        IEnumWbemClassObject* results_enumerator = nullptr;
        this->status = this->pSvc->ExecQuery(bstr_t("WQL"), bstr_t(query.c_str()), WBEM_FLAG_RETURN_WBEM_COMPLETE,nullptr, &results_enumerator);
        std::cout << "ExecQuery() result: 0x" << std::hex << this->status << std::dec << " - " << HresultToString(this->status);
        if(SUCCEEDED(this->status)) 
            this->SetProxySecurity(results_enumerator);
        else 
            results_enumerator = nullptr;
        return results_enumerator;
    }

}
