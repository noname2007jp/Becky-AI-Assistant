#include "ProofreadDialog.h"
#include "resource.h"
#include "ProgressIndicator.h"
#include "PiiMaskPreviewDialog.h"
#include "gemini/GeminiProofread.h"
#include "gemini/GeminiAsync.h"
#include "gemini/GeminiModels.h"
#include "gemini/GeminiFallback.h"
#include "gemini/PiiMasker.h"
#include "settings/ApiKeyStore.h"
#include <string>
#include <vector>

namespace {
struct Context {
    HWND compose = nullptr; HINSTANCE instance = nullptr; CBeckyAPI* api = nullptr;
    std::wstring original; std::vector<GeminiModelOption> models;
    bool busy = false; bool closeAfterCancel = false; GeminiAsync::WorkItem* work = nullptr;
};
std::wstring GetText(HWND dlg, int id) {
    HWND h = GetDlgItem(dlg,id); int n=GetWindowTextLengthW(h);
    std::wstring s((size_t)n+1,L'\0'); GetWindowTextW(h,s.data(),n+1); s.resize((size_t)n); return s;
}
std::wstring NormalizeWindowsNewlines(const std::wstring& value) {
    std::wstring out;
    out.reserve(value.size() + 16);
    for (size_t i = 0; i < value.size(); ++i) {
        if (value[i] == L'\r') {
            out += L"\r\n";
            if (i + 1 < value.size() && value[i + 1] == L'\n') ++i;
        } else if (value[i] == L'\n') {
            out += L"\r\n";
        } else {
            out += value[i];
        }
    }
    return out;
}
std::wstring FromAnsi(const char* s) {
    if(!s||!*s)return {};
    int n=MultiByteToWideChar(CP_ACP,0,s,-1,nullptr,0); if(n<=1)return {};
    std::wstring w((size_t)n,L'\0'); MultiByteToWideChar(CP_ACP,0,s,-1,w.data(),n); w.resize((size_t)n-1); return w;
}
std::string ToAnsi(const std::wstring& s) {
    if(s.empty())return {};
    int n=WideCharToMultiByte(CP_ACP,0,s.c_str(),-1,nullptr,0,nullptr,nullptr); if(n<=1)return {};
    std::string a((size_t)n,'\0'); WideCharToMultiByte(CP_ACP,0,s.c_str(),-1,a.data(),n,nullptr,nullptr); a.resize((size_t)n-1); return a;
}
void SetBusy(HWND d, bool busy) {
    EnableWindow(GetDlgItem(d,IDC_PROOFREAD_RUN),!busy);
    EnableWindow(GetDlgItem(d,IDC_PROOFREAD_MODEL),!busy);
    EnableWindow(GetDlgItem(d,IDC_PROOFREAD_APPLY),!busy);
    ShowWindow(GetDlgItem(d,IDC_PROOFREAD_CANCEL_REQUEST),busy?SW_SHOW:SW_HIDE);
}
INT_PTR CALLBACK Proc(HWND d, UINT m, WPARAM w, LPARAM l) {
    if(m==WM_INITDIALOG) {
        auto* c=reinterpret_cast<Context*>(l); SetWindowLongPtrW(d,DWLP_USER,(LONG_PTR)c);
        char mime[256]{}; LPSTR body=c->api->CompGetText(c->compose,mime,sizeof(mime));
        if(body){c->original=NormalizeWindowsNewlines(FromAnsi(body));c->api->Free(body);}
        if(c->original.empty()) {
            MessageBoxW(d,L"校正する本文がありません。メール本文を入力してから実行してください。",L"メール校正",MB_OK|MB_ICONINFORMATION);
            EndDialog(d,IDCANCEL); return TRUE;
        }
        SendDlgItemMessageW(d,IDC_PROOFREAD_ORIGINAL,EM_SETLIMITTEXT,100000,0);
        SendDlgItemMessageW(d,IDC_PROOFREAD_RESULT,EM_SETLIMITTEXT,100000,0);
        SetDlgItemTextW(d,IDC_PROOFREAD_ORIGINAL,c->original.c_str());
        std::wstring err;
        if(!LoadGeminiModelCatalog(c->models,err)||c->models.empty())c->models=DefaultGeminiModels();
        int def=0;
        for(size_t i=0;i<c->models.size();++i){
            SendDlgItemMessageW(d,IDC_PROOFREAD_MODEL,CB_ADDSTRING,0,(LPARAM)c->models[i].label.c_str());
            if(c->models[i].id==L"gemini-3.5-flash-lite")def=(int)i;
        }
        SendDlgItemMessageW(d,IDC_PROOFREAD_MODEL,CB_SETCURSEL,def,0);
        SetDlgItemTextW(d,IDC_PROOFREAD_STATUS,L"本文を確認し「校正する」を押してください。");
        ShowWindow(GetDlgItem(d,IDC_PROOFREAD_CANCEL_REQUEST),SW_HIDE);
        return TRUE;
    }
    if(m==GeminiAsync::WM_GENERATION_COMPLETE) {
        auto* c=reinterpret_cast<Context*>(GetWindowLongPtrW(d,DWLP_USER));
        auto* work=reinterpret_cast<GeminiAsync::WorkItem*>(l); if(!work)return TRUE;
        if(c){
            c->busy=false;c->work=nullptr;SetBusy(d,false);
            const auto& r=work->result;
            if(r.cancelled){
                SetDlgItemTextW(d,IDC_PROOFREAD_STATUS,L"校正をキャンセルしました。");
                bool close=c->closeAfterCancel;c->closeAfterCancel=false;
                GeminiAsync::Destroy(work);if(close)EndDialog(d,IDCANCEL);return TRUE;
            }
            c->closeAfterCancel=false;
            if(!r.success){
                SetDlgItemTextW(d,IDC_PROOFREAD_STATUS,L"校正に失敗しました。");
                MessageBoxW(d,r.error.c_str(),L"Gemini APIエラー",MB_OK|MB_ICONWARNING);
            } else {
                const std::wstring displayResult=NormalizeWindowsNewlines(r.text);
                SetDlgItemTextW(d,IDC_PROOFREAD_RESULT,displayResult.c_str());
                SetDlgItemTextW(d,IDC_PROOFREAD_STATUS,r.notice.empty()?L"校正結果を確認してください。":r.notice.c_str());
            }
        }
        GeminiAsync::Destroy(work);return TRUE;
    }
    if(m==WM_COMMAND) {
        switch(LOWORD(w)){
        case IDC_PROOFREAD_RUN:{
            auto* c=reinterpret_cast<Context*>(GetWindowLongPtrW(d,DWLP_USER));if(!c||c->busy)return TRUE;
            LRESULT ix=SendDlgItemMessageW(d,IDC_PROOFREAD_MODEL,CB_GETCURSEL,0,0);
            if(ix<0||ix>=(LRESULT)c->models.size())return TRUE;
            PiiMasker masker;if(!masker.IsReady()){MessageBoxW(d,masker.Error().c_str(),L"マスキング設定エラー",MB_OK|MB_ICONWARNING);return TRUE;}
            // A second pass starts from the previous result (including user edits).
            // The original compose body is never changed until the user explicitly applies.
            std::wstring source = GetText(d, IDC_PROOFREAD_RESULT);
            if (source.empty()) source = c->original;
            source = NormalizeWindowsNewlines(source);
            c->original = source;
            SetDlgItemTextW(d, IDC_PROOFREAD_ORIGINAL, source.c_str());
            SetDlgItemTextW(d, IDC_PROOFREAD_RESULT, L"");
            const std::wstring masked = masker.Mask(source);
            if(masker.IsEnabled()&&!ShowPiiMaskPreviewDialog(d,c->instance,L"メール校正：Gemini送信内容の確認",masked,masker))return TRUE;
            std::wstring key,error;
            if(!LoadGeminiApiKey(key,error)||key.empty()){MessageBoxW(d,error.empty()?L"先に設定画面でGemini APIキーを保存してください。":error.c_str(),L"Becky AI Assistant",MB_OK|MB_ICONWARNING);return TRUE;}
            GeminiReliabilityOptions rel;std::wstring relError;
            if(!LoadGeminiReliabilityOptions(rel,relError)){MessageBoxW(d,relError.c_str(),L"設定エラー",MB_OK|MB_ICONWARNING);return TRUE;}
            auto model=c->models[(size_t)ix];auto models=c->models;bool fallback=rel.automaticModelFallback;auto original=source;
            c->busy=true;SetBusy(d,true);ShowApiProcessingStatus(d,IDC_PROOFREAD_STATUS,L"Geminiでメール本文を校正しています...");
            bool started=GeminiAsync::Start(d,std::move(key),
                [model,models,fallback,original,masker](const std::wstring& k,const std::atomic<int>& cancel) mutable {
                    return GenerateWithModelFallback(model.id,model.thinking,models,fallback,
                        [&](const std::wstring& id,const std::wstring& thinking){return GenerateEmailProofread(k,id,thinking,original,&masker);},&cancel);
                },0,&c->work);
            if(!started){c->busy=false;c->work=nullptr;SetBusy(d,false);SetDlgItemTextW(d,IDC_PROOFREAD_STATUS,L"バックグラウンド処理を開始できませんでした。");}
            return TRUE;
        }
        case IDC_PROOFREAD_CANCEL_REQUEST:{
            auto* c=reinterpret_cast<Context*>(GetWindowLongPtrW(d,DWLP_USER));
            if(c&&c->busy&&c->work){EnableWindow(GetDlgItem(d,IDC_PROOFREAD_CANCEL_REQUEST),FALSE);SetDlgItemTextW(d,IDC_PROOFREAD_STATUS,L"キャンセルしています...");GeminiAsync::RequestCancel(c->work);}return TRUE;
        }
        case IDC_PROOFREAD_APPLY:{
            auto* c=reinterpret_cast<Context*>(GetWindowLongPtrW(d,DWLP_USER));if(!c)return TRUE;
            const std::wstring corrected=GetText(d,IDC_PROOFREAD_RESULT);
            if(corrected.empty()){MessageBoxW(d,L"校正結果がありません。先に校正を実行してください。",L"メール校正",MB_OK|MB_ICONINFORMATION);return TRUE;}
            if(MessageBoxW(d,L"校正結果で現在のメール本文を置き換えます。\n\n元の本文は自動保存されません。適用しますか？",L"校正結果の適用確認",MB_YESNO|MB_ICONWARNING|MB_DEFBUTTON2)!=IDYES)return TRUE;
            const std::wstring normalized=NormalizeWindowsNewlines(corrected);
            const std::string a=ToAnsi(normalized);
            if(a.empty()){MessageBoxW(d,L"校正結果をBecky!の文字コードに変換できませんでした。",L"メール校正",MB_OK|MB_ICONWARNING);return TRUE;}
            c->api->CompSetText(c->compose,0,a.c_str());EndDialog(d,IDOK);return TRUE;
        }
        case IDCANCEL:
        case IDC_PROOFREAD_CLOSE:{
            auto* c=reinterpret_cast<Context*>(GetWindowLongPtrW(d,DWLP_USER));
            if(c&&c->busy){c->closeAfterCancel=true;if(c->work)GeminiAsync::RequestCancel(c->work);SetDlgItemTextW(d,IDC_PROOFREAD_STATUS,L"キャンセルして終了しています...");return TRUE;}
            EndDialog(d,IDCANCEL);return TRUE;
        }
        }
    }
    if(m==WM_CLOSE){auto* c=reinterpret_cast<Context*>(GetWindowLongPtrW(d,DWLP_USER));
        if(c&&c->busy){c->closeAfterCancel=true;if(c->work)GeminiAsync::RequestCancel(c->work);SetDlgItemTextW(d,IDC_PROOFREAD_STATUS,L"キャンセルして終了しています...");return TRUE;}
        EndDialog(d,IDCANCEL);return TRUE;}
    return FALSE;
}
}
INT_PTR ShowEmailProofreadDialog(HWND composeWindow,HINSTANCE instance,CBeckyAPI& api){
    Context c;c.compose=composeWindow;c.instance=instance;c.api=&api;
    return DialogBoxParamW(instance,MAKEINTRESOURCEW(IDD_EMAIL_PROOFREAD),composeWindow,Proc,(LPARAM)&c);
}
