#pragma once
#include "RemoteWmiStdIncludes.h"
#include "wbemidl.h"

namespace RemoteWmi {

    void InitCom();
    wstring GetClientDomain();
    SEC_WINNT_AUTH_IDENTITY_W* GetAuth(wstring user, wstring password, wstring domain);
    void OutputWMIObjectValues(IEnumWbemClassObject* pEnumerator);
    string GetAuthnSvcStr(unsigned long enum_value);
    string GetAuthzSvcStr(unsigned long enum_value);
    string GetAuthnLevelStr(unsigned long enum_value);
    string GetImpLevelStr(unsigned long enum_value);
    string GetCapabilitiesStr(unsigned long enum_value);
    void PrintAuthIdentity(const SEC_WINNT_AUTH_IDENTITY_W& auth_identity);
    void PrintProxyBlanket(IUnknown* pSvc);
    bool isValidIpv4(const wstring& ipAddress);
    wstring GetHostnameByIp(const wstring& ip);
    wstring ParseUser(wstring& input);
    wstring ParseDomain(wstring& input);
    string HresultToString(HRESULT hr);
    bool strContainsStr(const wstring& str1, const wstring& str2);
    wstring lowercase(const wstring& str);
}
