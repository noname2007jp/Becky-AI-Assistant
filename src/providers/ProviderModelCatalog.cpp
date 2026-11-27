#include "ProviderModelCatalog.h"
#include "WorkersAIModelId.h"
#include "ProviderModelEligibility.h"

#include <windows.h>
#include <winhttp.h>
#include <algorithm>
#include <cctype>
#include <string>

#pragma comment(lib, "winhttp.lib")

namespace {
std::string Utf8(const std::wstring& s) {
    if (s.empty()) return {};
    int n=WideCharToMultiByte(CP_UTF8,0,s.data(),(int)s.size(),nullptr,0,nullptr,nullptr);
    if(n<=0)return {};
    std::string r((size_t)n,'\0');
    WideCharToMultiByte(CP_UTF8,0,s.data(),(int)s.size(),r.data(),n,nullptr,nullptr);
    return r;
}
std::wstring Wide(const std::string& s) {
    if(s.empty())return {};
    int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),(int)s.size(),nullptr,0);
    if(n<=0)return {};
    std::wstring r((size_t)n,L'\0');
    MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),(int)s.size(),r.data(),n);
    return r;
}
bool ReadString(const std::string& s,size_t& p,std::string& out) {
    if(p>=s.size()||s[p++]!='"')return false;
    out.clear();
    while(p<s.size()) {
        unsigned char c=(unsigned char)s[p++];
        if(c=='"')return true;
        if(c<0x20)return false;
        if(c!='\\'){out.push_back((char)c);continue;}
        if(p>=s.size())return false;
        char e=s[p++];
        switch(e){case '"':out+='"';break;case '\\':out+='\\';break;case '/':out+='/';break;
        case 'n':out+=' ';break;case 'r':out+=' ';break;case 't':out+=' ';break;
        default:return false;}
    }
    return false;
}
std::string Field(const std::string& object,const char* key) {
    const std::string k=std::string("\"")+key+"\"";
    size_t p=object.find(k); if(p==std::string::npos)return {};
    p=object.find(':',p+k.size()); if(p==std::string::npos)return {};
    ++p; while(p<object.size()&&(object[p]==' '||object[p]=='\r'||object[p]=='\n'||object[p]=='\t'))++p;
    std::string v; return ReadString(object,p,v)?v:std::string{};
}
std::vector<std::string> Objects(const std::string& json,const char* arrayName) {
    std::vector<std::string> out;
    const std::string key=std::string("\"")+arrayName+"\"";
    size_t p=json.find(key); if(p==std::string::npos)return out;
    p=json.find('[',p+key.size()); if(p==std::string::npos)return out; ++p;
    while(p<json.size()) {
        while(p<json.size()&&(json[p]==' '||json[p]=='\r'||json[p]=='\n'||json[p]=='\t'||json[p]==','))++p;
        if(p>=json.size()||json[p]==']')break;
        if(json[p]!='{'){++p;continue;}
        size_t begin=p; int depth=0; bool str=false,esc=false;
        for(;p<json.size();++p) {
            char c=json[p];
            if(str){if(!esc&&c=='"')str=false;if(!esc&&c=='\\')esc=true;else esc=false;continue;}
            if(c=='"'){str=true;continue;} if(c=='{')++depth;
            else if(c=='}'&&--depth==0){++p;break;}
        }
        if(depth==0)out.push_back(json.substr(begin,p-begin)); else break;
    }
    return out;
}
bool Download(const wchar_t* host,const std::wstring& path,const std::wstring& token,
    const wchar_t* extraHeader,std::string& body,std::wstring& error) {
    HINTERNET session=WinHttpOpen(L"Becky-AI-Assistant/0.89.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0);
    if(!session){error=L"モデル一覧の通信を開始できません。";return false;}
    WinHttpSetTimeouts(session,10000,10000,15000,30000);
    HINTERNET conn=WinHttpConnect(session,host,INTERNET_DEFAULT_HTTPS_PORT,0);
    HINTERNET req=conn?WinHttpOpenRequest(conn,L"GET",path.c_str(),nullptr,WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE):nullptr;
    if(!req){if(conn)WinHttpCloseHandle(conn);WinHttpCloseHandle(session);error=L"モデル一覧APIへ接続できません。";return false;}
    std::wstring headers=L"Authorization: Bearer "+token+L"\r\nAccept: application/json\r\n";
    if(extraHeader&&*extraHeader){headers+=extraHeader;headers+=L"\r\n";}
    BOOL ok=WinHttpSendRequest(req,headers.c_str(),(DWORD)headers.size(),WINHTTP_NO_REQUEST_DATA,0,0,0);
    if(!headers.empty())SecureZeroMemory(headers.data(),headers.size()*sizeof(wchar_t));
    ok=ok&&WinHttpReceiveResponse(req,nullptr);
    DWORD status=0,sz=sizeof(status);
    if(ok)WinHttpQueryHeaders(req,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX,&status,&sz,WINHTTP_NO_HEADER_INDEX);
    DWORD avail=0; bool readOk=true;
    while(ok&&WinHttpQueryDataAvailable(req,&avail)&&avail) {
        if(body.size()+avail>4*1024*1024){readOk=false;break;}
        size_t old=body.size();body.resize(old+avail);DWORD got=0;
        if(!WinHttpReadData(req,body.data()+old,avail,&got)){body.resize(old);readOk=false;break;}
        body.resize(old+got);if(!got)break;
    }
    DWORD last=GetLastError();
    WinHttpCloseHandle(req);WinHttpCloseHandle(conn);WinHttpCloseHandle(session);
    if(!ok||!readOk||status!=200) {
        error=L"モデル一覧APIの取得に失敗しました";
        if(status)error+=L"（HTTP "+std::to_wstring(status)+L"）";
        else error+=L"（通信エラー "+std::to_wstring(last)+L"）";
        if(status==401||status==403)error+=L"。API Token／APIキーと権限を確認してください。";
        return false;
    }
    return true;
}
}

bool FetchGroqModelCatalog(const std::wstring& apiKey,std::vector<std::wstring>& ids,std::wstring& error) {
    ids.clear();error.clear();if(apiKey.empty()){error=L"Groq APIキーを入力してから取得してください。";return false;}
    std::string json;
    if(!Download(L"api.groq.com",L"/openai/v1/models",apiKey,nullptr,json,error))return false;
    for(const auto& obj:Objects(json,"data")) {
        std::string id=Field(obj,"id");
        if(id.empty()||!provider_model_eligibility::IsGroqChatCompletionModelEligible(id))continue;
        if(obj.find("\"active\"")!=std::string::npos&&obj.find("\"active\": false")!=std::string::npos)continue;
        ids.push_back(Wide(id));
    }
    std::sort(ids.begin(),ids.end());
    if(ids.empty()){error=L"利用可能なテキスト生成モデルが見つかりませんでした。";return false;}
    return true;
}
bool FetchWorkersAIModelCatalog(const std::wstring& accountId,const std::wstring& apiToken,
    std::vector<std::wstring>& ids,std::wstring& error) {
    ids.clear();error.clear();
    if(accountId.empty()||apiToken.empty()){error=L"Workers AIのAccount IDとAPI Tokenを入力してから取得してください。";return false;}
    const std::string account=Utf8(accountId);
    std::wstring path=L"/client/v4/accounts/"+Wide(account)+L"/ai/models/search?task=Text%20Generation&hide_experimental=true&include_deprecated=false&per_page=100&page=1";
    std::string json;
    if(!Download(L"api.cloudflare.com",path,apiToken,nullptr,json,error))return false;
    for(const auto& obj:Objects(json,"result")) {
        const std::string name = Field(obj,"name");
        const std::string recordId = Field(obj,"id");
        const std::string id = SelectWorkersAIModelIdentifier(name, recordId);
        if(id.empty()||!provider_model_eligibility::IsWorkersAIChatCompletionModelEligible(id))continue;
        std::string task=Field(obj,"task");
        std::string lower=task;std::transform(lower.begin(),lower.end(),lower.begin(),[](unsigned char c){return (char)std::tolower(c);});
        // The API's task query is authoritative; when task metadata is present, verify it.
        if(!lower.empty()&&lower.find("text")==std::string::npos)continue;
        ids.push_back(Wide(id));
    }
    std::sort(ids.begin(),ids.end());
    ids.erase(std::unique(ids.begin(),ids.end()),ids.end());
    if(ids.empty()){error=L"Workers AIのテキスト生成モデルが見つかりませんでした。API TokenのWorkers AI Read権限とAccount IDを確認してください。";return false;}
    return true;
}
