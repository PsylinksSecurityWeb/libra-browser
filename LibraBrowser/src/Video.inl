// Save-video helper (off by default), live download push, background-tab memory. Included at the end of BrowserWindow.cpp.
// The helper only listens to media responses while the setting is on; it injects no page script and keeps a tiny list per tab.

static std::wstring VidLabel(const std::wstring& url, long long size) {
    std::wstring u = url.substr(0, url.find_first_of(L"?#")), ext = L"Video", res;
    size_t d = u.find_last_of(L'.');
    if (d != std::wstring::npos) { std::wstring e = u.substr(d + 1); for (auto& c : e) c = (wchar_t)towupper(c); if (e == L"MP4" || e == L"WEBM" || e == L"MKV" || e == L"MOV") ext = e; }
    for (const wchar_t* r : { L"2160", L"1440", L"1080", L"720", L"480", L"360", L"240" }) {
        size_t p = url.find(r); while (p != std::wstring::npos) {
            bool l = p == 0 || !iswdigit(url[p - 1]), rr = p + wcslen(r) >= url.size() || !iswdigit(url[p + wcslen(r)]);
            if (l && rr) { res = std::wstring(r) + L"p · "; break; }
            p = url.find(r, p + 1); }
        if (!res.empty()) break;
    }
    return res + ext + (size > 0 ? L" · " + Fmt(size) : L"");
}

void BrowserWindow::AttachVideoSniff(Tab* t) {
    if (!t->webView || t->vidOn || !m_set.On(L"save_video") || m_incognito) return;
    wil::com_ptr<ICoreWebView2_2> w2; if (FAILED(t->webView->QueryInterface(IID_PPV_ARGS(&w2)))) return;
    t->webView->AddWebResourceRequestedFilter(L"*", COREWEBVIEW2_WEB_RESOURCE_CONTEXT_MEDIA);
    w2->add_WebResourceResponseReceived(Microsoft::WRL::Callback<ICoreWebView2WebResourceResponseReceivedEventHandler>(
        [this, t](ICoreWebView2*, ICoreWebView2WebResourceResponseReceivedEventArgs* a) -> HRESULT {
            if (IndexOf(t) < 0 || t->vids.size() >= 8) return S_OK;
            wil::com_ptr<ICoreWebView2WebResourceRequest> rq; wil::com_ptr<ICoreWebView2WebResourceResponseView> rs;
            a->get_Request(&rq); a->get_Response(&rs); if (!rq || !rs) return S_OK;
            int st = 0; rs->get_StatusCode(&st); if (st != 200 && st != 206) return S_OK;
            wil::com_ptr<ICoreWebView2HttpResponseHeaders> h; rs->get_Headers(&h); if (!h) return S_OK;
            wil::unique_cotaskmem_string ct, cl, cr, uri;
            rq->get_Uri(&uri); if (!uri.get() || !IsHttp(uri.get())) return S_OK;
            h->GetHeader(L"Content-Type", &ct); h->GetHeader(L"Content-Range", &cr); h->GetHeader(L"Content-Length", &cl);
            std::wstring url = uri.get(), path = url.substr(0, url.find_first_of(L"?#"));
            bool vid = ct.get() && _wcsnicmp(ct.get(), L"video/", 6) == 0 && !wcsstr(ct.get(), L"mp2t");
            if (!vid && ct.get() && (wcsstr(ct.get(), L"octet-stream"))) vid = path.size() > 4 && (path.rfind(L".mp4") == path.size() - 4 || path.rfind(L".webm") == path.size() - 5 || path.rfind(L".mkv") == path.size() - 4);
            if (!vid) return S_OK;
            long long size = 0;
            if (cr.get()) { const wchar_t* sl = wcschr(cr.get(), L'/'); if (sl) size = _wtoi64(sl + 1); }
            if (!size && cl.get()) size = _wtoi64(cl.get());
            if (size < (2ll << 20)) return S_OK;   // segments and tiny clips are not "a video to save"
            for (auto& v : t->vids) if (v.first == url) return S_OK;
            t->vids.push_back({ url, size });
            if (IndexOf(t) == m_activeTab) InvalidateRect(m_hWnd, nullptr, FALSE);
            return S_OK;
        }).Get(), &t->tokVid);
    t->vidOn = true;
}
void BrowserWindow::DetachVideoSniff(Tab* t) {
    if (!t->vidOn || !t->webView) return;
    wil::com_ptr<ICoreWebView2_2> w2;
    if (SUCCEEDED(t->webView->QueryInterface(IID_PPV_ARGS(&w2)))) w2->remove_WebResourceResponseReceived(t->tokVid);
    t->webView->RemoveWebResourceRequestedFilter(L"*", COREWEBVIEW2_WEB_RESOURCE_CONTEXT_MEDIA);
    t->vidOn = false; t->vids.clear();
    InvalidateRect(m_hWnd, nullptr, FALSE);
}
RECT BrowserWindow::VidRect() const { RECT s = StarRect(); int w = MulDiv(30, m_dpi, 96); return { s.left - w, s.top, s.left, s.bottom }; }
void BrowserWindow::ShowVideoPopup() {
    if (m_activeTab < 0) return;
    Tab* t = m_tabs[m_activeTab].get(); if (t->vids.empty()) return;
    m_pwChoice.clear(); for (int i = 0; i < (int)t->vids.size() && i < 5; ++i) m_pwChoice.push_back(i);
    ShowPw(4, t, L"");
}

static std::wstring UrlName(const std::wstring& url) {   // last path segment, %xx decoded, safe characters only
    std::wstring p = url.substr(0, url.find_first_of(L"?#")); size_t s = p.find_last_of(L'/'); if (s != std::wstring::npos) p = p.substr(s + 1);
    std::string raw = ToUtf8(p), o; for (size_t i = 0; i < raw.size(); ++i) { if (raw[i] == '%' && i + 2 < raw.size()) { o += (char)strtol(raw.substr(i + 1, 2).c_str(), nullptr, 16); i += 2; } else o += raw[i]; }
    return SafeName(FromUtf8(o));
}
void BrowserWindow::StartVideoDownload(Tab* t, int idx) {
    if (idx < 0 || idx >= (int)t->vids.size()) return;
    StartSave(t, t->vids[idx].first, t->title.empty() ? L"video" : t->title, L".mp4", true);
}
void BrowserWindow::StartSave(Tab* t, const std::wstring& url, const std::wstring& nameBase, const std::wstring& extHint, bool useTitle) {
    if (!t->webView || !IsHttp(url)) return;
    int port = DohProxy::Get().Port(); if (!port) return;
    std::wstring fname = UrlName(url);
    size_t dot = fname.find_last_of(L'.');
    bool hasExt = dot != std::wstring::npos && fname.size() - dot <= 6 && dot > 0;
    std::wstring base = useTitle || fname.empty() ? nameBase : fname; if (base.empty()) base = L"download"; if (base.size() > 90) base.resize(90);
    std::wstring ext = L"";
    if (useTitle || !hasExt) { ext = extHint; if (hasExt) ext = fname.substr(dot); }
    std::wstring path = UniquePath(DownloadsDir(m_set) + L"\\" + SafeName(base) + ext);
    auto job = std::make_shared<DlJob>(); job->url = url; job->port = port; job->path = path; job->part = path + L".libra-part";
    m_vidJobs[job.get()] = job;
    std::wstring ref = IsHttp(t->url) ? t->url : L"", ua;
    { wil::com_ptr<ICoreWebView2Settings> st; wil::com_ptr<ICoreWebView2Settings2> st2;
      if (SUCCEEDED(t->webView->get_Settings(&st)) && SUCCEEDED(st->QueryInterface(IID_PPV_ARGS(&st2)))) { wil::unique_cotaskmem_string x; st2->get_UserAgent(&x); if (x.get()) ua = x.get(); } }
    auto clean = [](std::wstring s) { for (auto& c : s) if (c == L'\r' || c == L'\n') c = L' '; return s; };
    auto begin = [this, job, ref, ua, clean](std::wstring cookie) {
        job->ref = ref;
        job->hdr = (ref.empty() ? L"" : L"Referer: " + clean(ref) + L"\r\n") + (ua.empty() ? L"" : L"User-Agent: " + clean(ua) + L"\r\n") + (cookie.empty() ? L"" : L"Cookie: " + clean(cookie) + L"\r\n");
        HWND hw = m_hWnd; int pr = job->port;
        std::thread([hw, job, pr] { long long total = 0; bool ok = dl::Probe(pr, job->url, job->hdr, total); job->total = total;
                                    PostMessage(hw, WM_APP + 6, ok ? 1 : 0, (LPARAM) new std::shared_ptr<DlJob>(job)); }).detach();
    };
    wil::com_ptr<ICoreWebView2_2> w2; wil::com_ptr<ICoreWebView2CookieManager> cm;
    if (SUCCEEDED(t->webView->QueryInterface(IID_PPV_ARGS(&w2))) && SUCCEEDED(w2->get_CookieManager(&cm)))
        cm->GetCookies(url.c_str(), Microsoft::WRL::Callback<ICoreWebView2GetCookiesCompletedHandler>(
            [begin](HRESULT, ICoreWebView2CookieList* list) -> HRESULT {
                std::wstring c; UINT n = 0;
                if (list) { list->get_Count(&n); for (UINT i = 0; i < n; ++i) { wil::com_ptr<ICoreWebView2Cookie> ck; if (FAILED(list->GetValueAtIndex(i, &ck))) continue;
                    wil::unique_cotaskmem_string nm, vl; ck->get_Name(&nm); ck->get_Value(&vl); if (nm.get() && vl.get()) c += std::wstring(nm.get()) + L"=" + vl.get() + L"; "; } }
                begin(c); return S_OK; }).Get());
    else begin(L"");
}
void BrowserWindow::OnVideoProbe(std::shared_ptr<DlJob> job, bool ok) {
    if (!m_vidJobs.erase(job.get())) return;
    DownloadItem d; d.job = job; d.path = job->path; d.total = job->total;
    size_t sl = job->path.find_last_of(L"\\/"); d.name = sl == std::wstring::npos ? job->path : job->path.substr(sl + 1);
    if (ok) {
        ULARGE_INTEGER fb{}; if (GetDiskFreeSpaceExW(DownloadsDir(m_set).c_str(), &fb, nullptr, nullptr) && (ULONGLONG)job->total + (64ull << 20) > fb.QuadPart) ok = false;
    }
    if (!ok || job->total < (4ll << 20)) {   // small file or no byte ranges: one plain connection
        job->total = ok ? job->total : 0; d.total = job->total; m_dl.push_back(d);
        dl::StartPlain(job); SetTimer(m_hWnd, 2005, 250, nullptr); ShowInternal(L"libra://downloads", false); return;
    }
    m_dl.push_back(d);
    int conns = job->total > (40ll << 20) ? 8 : 4; if (m_lowEnd) conns = std::min(conns, 6);
    dl::Start(job, conns);
    SetTimer(m_hWnd, 2005, 250, nullptr);
    ShowInternal(L"libra://downloads", false);
}

static bool IsRiskyFile(const std::wstring& n) {   // files that can run programs
    size_t dot = n.find_last_of(L'.'); if (dot == std::wstring::npos) return false;
    std::wstring e = n.substr(dot + 1); for (auto& c : e) c = (wchar_t)towlower(c);
    for (const wchar_t* r : { L"exe", L"msi", L"bat", L"cmd", L"ps1", L"vbs", L"vbe", L"js", L"jse", L"wsf", L"jar", L"scr", L"lnk", L"reg", L"dll", L"hta", L"com", L"cpl", L"msix", L"appx" }) if (e == r) return true;
    return false;
}
void BrowserWindow::DlPush() {   // live numbers into the open Downloads page: no re-render, so bars glide
    SyncJobs(); DWORD now = GetTickCount(); bool active = false; std::wstring js = L"lbDl([";
    for (int i = 0; i < (int)m_dl.size(); ++i) {
        auto& d = m_dl[i];
        if (d.state == 0) {
            active = true;
            if (!d.lastTick) { d.lastTick = now; d.lastGot = d.got; }
            else if (now - d.lastTick >= 200) { double inst = (d.got - d.lastGot) * 1000.0 / (now - d.lastTick); d.speed = d.speed > 0 ? d.speed * 0.55 + inst * 0.45 : inst; d.lastGot = d.got; d.lastTick = now; }
        } else d.speed = 0;
        if (d.state == 1 && !(d.toast & 4) && !d.path.empty() && IsRiskyFile(d.name.empty() ? d.path : d.name)) {   // a finished file that can run programs: ask first
            if (GetForegroundWindow() == m_hWnd && !(m_pwKind == 1 && m_hPw && IsWindowVisible(m_hPw))) { d.toast |= 7; m_riskPath = d.path; m_toastN = d.name.empty() ? d.path : d.name; ShowPw(7, nullptr, L""); }
        }
        int ev = !(d.toast & 1) ? 1 : (d.state == 1 && !(d.toast & 2)) ? 2 : 0;   // toast: started / finished
        if (ev && d.state != 2 && !(m_pwKind == 1 && m_hPw && IsWindowVisible(m_hPw)) && GetForegroundWindow() == m_hWnd) {
            d.toast |= ev == 1 ? 1 : 3; m_toastT = ev == 1 ? L"Download started" : L"Download complete";
            m_toastN = d.name.empty() ? L"File" : d.name; ShowPw(6, nullptr, L"");
        } else if (d.state == 2) d.toast = 3;
        js += L"{i:" + std::to_wstring(i) + L",g:" + std::to_wstring(d.got) + L",t:" + std::to_wstring(d.total) + L",s:" + std::to_wstring(d.state) + L",v:" + std::to_wstring((long long)d.speed) + L"},";
    }
    js += L"])";
    for (auto& tb : m_tabs) if (tb->webView && tb->url == L"libra://downloads") tb->webView->ExecuteScript(js.c_str(), nullptr);
    if (!active) KillTimer(m_hWnd, 2005);
}

void BrowserWindow::ApplyMemoryLevels() {   // background tabs ask Chromium to keep a small footprint
    for (int i = 0; i < (int)m_tabs.size(); ++i) {
        wil::com_ptr<ICoreWebView2_19> w; if (!m_tabs[i]->webView || FAILED(m_tabs[i]->webView->QueryInterface(IID_PPV_ARGS(&w)))) continue;
        w->put_MemoryUsageTargetLevel(i == m_activeTab ? COREWEBVIEW2_MEMORY_USAGE_TARGET_LEVEL_NORMAL : COREWEBVIEW2_MEMORY_USAGE_TARGET_LEVEL_LOW);
    }
}

void BrowserWindow::AttachContextSave(Tab* t) {   // right-click: one clear "Save ..." entry on top for images, links, PDFs and videos
    wil::com_ptr<ICoreWebView2_11> w11; wil::com_ptr<ICoreWebView2Environment9> e9;
    if (!t->webView || !m_env || FAILED(t->webView->QueryInterface(IID_PPV_ARGS(&w11))) || FAILED(m_env->QueryInterface(IID_PPV_ARGS(&e9)))) return;
    EventRegistrationToken tk;
    w11->add_ContextMenuRequested(Microsoft::WRL::Callback<ICoreWebView2ContextMenuRequestedEventHandler>(
        [this, t, e9](ICoreWebView2*, ICoreWebView2ContextMenuRequestedEventArgs* a) -> HRESULT {
            if (IndexOf(t) < 0) return S_OK;
            wil::com_ptr<ICoreWebView2ContextMenuTarget> tg; a->get_ContextMenuTarget(&tg); if (!tg) return S_OK;
            COREWEBVIEW2_CONTEXT_MENU_TARGET_KIND kind; tg->get_Kind(&kind);
            BOOL hasLink = FALSE, hasSrc = FALSE; tg->get_HasLinkUri(&hasLink); tg->get_HasSourceUri(&hasSrc);
            wil::unique_cotaskmem_string link, src; if (hasLink) tg->get_LinkUri(&link); if (hasSrc) tg->get_SourceUri(&src);
            std::wstring url, label, ext = L".bin"; bool video = false;
            if (kind == COREWEBVIEW2_CONTEXT_MENU_TARGET_KIND_IMAGE && src.get()) { url = src.get(); label = L"Save image"; ext = L".png"; }
            else if ((kind == COREWEBVIEW2_CONTEXT_MENU_TARGET_KIND_VIDEO || kind == COREWEBVIEW2_CONTEXT_MENU_TARGET_KIND_AUDIO) && (IsHttp(src.get() ? src.get() : L"") || !t->vids.empty())) {
                url = src.get() ? src.get() : L""; label = kind == COREWEBVIEW2_CONTEXT_MENU_TARGET_KIND_VIDEO ? L"Save video..." : L"Save audio"; ext = L".mp4"; video = true; }
            else if (hasLink && link.get()) { url = link.get(); label = L"Save link as file"; }
            else if (kind == COREWEBVIEW2_CONTEXT_MENU_TARGET_KIND_PAGE && IsHttp(t->url) && t->url.substr(0, t->url.find_first_of(L"?#")).size() > 4 &&
                     _wcsicmp(t->url.substr(0, t->url.find_first_of(L"?#")).c_str() + t->url.substr(0, t->url.find_first_of(L"?#")).size() - 4, L".pdf") == 0) { url = t->url; label = L"Save PDF"; ext = L".pdf"; }
            if (label.empty() || (!video && !IsHttp(url))) return S_OK;   // blob:/data: sources keep the default menu
            wil::com_ptr<ICoreWebView2ContextMenuItem> it;
            if (FAILED(e9->CreateContextMenuItem(label.c_str(), nullptr, COREWEBVIEW2_CONTEXT_MENU_ITEM_KIND_COMMAND, &it)) || !it) return S_OK;
            EventRegistrationToken tk2;
            it->add_CustomItemSelected(Microsoft::WRL::Callback<ICoreWebView2CustomItemSelectedEventHandler>(
                [this, t, url, ext, video](ICoreWebView2ContextMenuItem*, IUnknown*) -> HRESULT {
                    if (IndexOf(t) < 0) return S_OK;
                    if (video) {
                        if (IsHttp(url)) { bool have = false; for (auto& v : t->vids) if (v.first == url) have = true; if (!have && t->vids.size() < 8) t->vids.push_back({ url, 0 }); }
                        ShowVideoPopup();
                    } else StartSave(t, url, L"download", ext, false);
                    return S_OK;
                }).Get(), &tk2);
            wil::com_ptr<ICoreWebView2ContextMenuItemCollection> items; a->get_MenuItems(&items);
            if (items) items->InsertValueAtIndex(0, it.get());
            return S_OK;
        }).Get(), &tk);
}
