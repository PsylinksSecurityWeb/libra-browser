// PDF fill and sign (libra://pdfsign, Ctrl+Shift+F). Included after Import.inl.
// The tools (pdf.js, pdf-lib) are NOT in the exe: on request they are downloaded once, each file checked against a fixed size and SHA-256, and kept in
// %LOCALAPPDATA%\LibraBrowser\pdftools. They are re-checked every time the page opens. The tab sees them only through a private host name (libra-pdf.local).
// The PDF you choose is copied to work.pdf for the page, deleted as soon as the page has read it. The signed copy is saved by a Windows Save-as window you answer.

struct PdfFile { const wchar_t* name; const char* path; const char* sha; size_t size; };
static const PdfFile kPdfFiles[3] = {   // pdf.js 3.11.174 (Apache-2.0) and pdf-lib 1.17.1 (MIT), from cdn.jsdelivr.net
    { L"pdf.min.js",        "/npm/pdfjs-dist@3.11.174/build/pdf.min.js",        "5b5799e6f8c680663207ac5b42ee14eed2a406fa7af48f50c154f0c0b1566946", 320004 },
    { L"pdf.worker.min.js", "/npm/pdfjs-dist@3.11.174/build/pdf.worker.min.js", "feabdf309770ed24bba31a5467836cdc8cf639c705af27d52b585b041bb8527b", 1087212 },
    { L"pdf-lib.min.js",    "/npm/pdf-lib@1.17.1/dist/pdf-lib.min.js",          "0f9a5cad07941f0826586c94e089d89b918c46e5c17cf2d5a3c6f666e3bc694f", 525099 } };
static const size_t kPdfMax = 12u << 20;   // bigger PDFs are refused (they travel through the page)
struct PdfDl { std::string err; };

static std::wstring PdfDir() { return DataDir() + L"\\pdftools"; }
static bool PdfToolsOk() {
    for (auto& f : kPdfFiles) { std::string d = ImReadFile(PdfDir() + L"\\" + f.name); if (d.size() != f.size || UpSha256(d) != f.sha) return false; }
    return true;
}
void BrowserWindow::PdfMapTools(Tab* t) {   // only the pdfsign tab can see the tools
    std::wstring d = PdfDir(); CreateDirectoryW(d.c_str(), nullptr);
    wil::com_ptr<ICoreWebView2_3> w3; if (t->webView && SUCCEEDED(t->webView->QueryInterface(IID_PPV_ARGS(&w3)))) w3->SetVirtualHostNameToFolderMapping(L"libra-pdf.local", d.c_str(), COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_ALLOW);
}
static std::wstring PdfJs(const std::wstring& s) { std::wstring o; for (wchar_t c : s) { if (c == L'"' || c == L'\\') { o += L'\\'; o += c; } else if (c < 0x20 || c == L'<' || c == L'>' || c == 0x2028 || c == 0x2029) o += L' '; else o += c; } return o; }
static std::wstring PdfBase(const std::wstring& n) {   // file name without folder and extension, letters/digits/-_ . only
    size_t s = n.find_last_of(L"\\/"); std::wstring b = s == std::wstring::npos ? n : n.substr(s + 1); size_t d = b.rfind(L'.'); if (d != std::wstring::npos) b.resize(d);
    std::wstring o; for (wchar_t c : b) if (iswalnum(c) || c == L'-' || c == L'_' || c == L' ' || c == L'.') o += c; if (o.empty()) o = L"document"; if (o.size() > 80) o.resize(80); return o;
}
void BrowserWindow::OnPdf(int idx, const std::vector<std::wstring>& v) {
    Tab* t = m_tabs[idx].get(); if (!t->webView) return; const std::wstring& c = v[0];
    auto run = [&](const std::wstring& js) { t->webView->ExecuteScript(js.c_str(), nullptr); };
    if (c == L"pdf.status") { run(PdfToolsOk() ? L"lbTools(1)" : L"lbTools(0)"); return; }
    if (c == L"pdf.loaded") { DeleteFileW((PdfDir() + L"\\work.pdf").c_str()); return; }
    if (c == L"pdf.tools") {
        if (m_pdfBusy) return; SpCfg sc = SpConfig(); int port = DohProxy::Get().Port(); wchar_t tb[300]; std::wstring base = L"https://cdn.jsdelivr.net"; bool test = false;
        if (TestEnv(L"LIBRA_TEST_PDFBASE", tb, 300)) { base = tb; test = true; }
        if (!port && !test) { run(L"lbToolsErr('No connection yet. Please try again in a moment.')"); return; }
        m_pdfBusy = true; HWND hw = m_hWnd; std::wstring dir = PdfDir(); CreateDirectoryW(dir.c_str(), nullptr);
        std::thread([hw, port, base, test, dir] {
            std::unique_ptr<PdfDl> r(new PdfDl); std::string bodies[3];
            for (int i = 0; i < 3 && r->err.empty(); ++i) {
                if (!SpGet(port, test, base + FromUtf8(kPdfFiles[i].path), kPdfFiles[i].size + 4096, bodies[i], true)) r->err = "The download failed. Please try again.";
                else if (bodies[i].size() != kPdfFiles[i].size || UpSha256(bodies[i]) != kPdfFiles[i].sha) r->err = "A downloaded file did not match its fixed checksum, so it was thrown away.";
            }
            if (r->err.empty()) for (int i = 0; i < 3; ++i) WriteAllRaw(dir + L"\\" + kPdfFiles[i].name, bodies[i], false);
            PostMessage(hw, WM_APP + 43, 0, (LPARAM)r.release()); }).detach();
        return;
    }
    if (c == L"pdf.open") {
        if (!PdfToolsOk()) { run(L"lbOpenErr('The PDF tools are missing.')"); return; }
        std::wstring path; wchar_t tf[MAX_PATH];
        if (TestEnv(L"LIBRA_TEST_PDFFILE", tf, MAX_PATH)) path = tf;
        else { wil::com_ptr<IFileOpenDialog> d;
            if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&d)))) {
                COMDLG_FILTERSPEC fs[] = { { L"PDF files", L"*.pdf" } }; d->SetFileTypes(1, fs); DWORD o = 0; d->GetOptions(&o); d->SetOptions(o | FOS_FORCEFILESYSTEM | FOS_FILEMUSTEXIST); d->SetTitle(L"Choose a PDF to fill in or sign");
                if (SUCCEEDED(d->Show(m_hWnd))) { wil::com_ptr<IShellItem> it; PWSTR ps = nullptr; if (SUCCEEDED(d->GetResult(&it)) && SUCCEEDED(it->GetDisplayName(SIGDN_FILESYSPATH, &ps))) { path = ps; CoTaskMemFree(ps); } } }
            if (path.empty()) return; }
        std::string d = ImReadFile(path);
        if (d.empty() || d.size() > kPdfMax) { run(L"lbOpenErr('That file could not be read, or it is bigger than 12 MB.')"); return; }
        if (d.compare(0, 5, "%PDF-") != 0) { run(L"lbOpenErr('That file is not a PDF.')"); return; }
        WriteAllRaw(PdfDir() + L"\\work.pdf", d, false); run(L"lbOpen(\"" + PdfJs(PdfBase(path)) + L".pdf\")"); return;
    }
    if (c == L"pdf.save" && v.size() >= 3) {
        const std::wstring& b64w = v[2]; if (b64w.size() > kPdfMax * 2) { run(L"lbSaved(0,'The signed copy is too big.')"); return; }
        std::string b64 = ToUtf8(b64w), bin; DWORD n = 0;
        if (!CryptStringToBinaryA(b64.c_str(), (DWORD)b64.size(), CRYPT_STRING_BASE64, nullptr, &n, nullptr, nullptr) || n < 100 || n > kPdfMax * 2) { run(L"lbSaved(0,'The signed copy could not be read.')"); return; }
        bin.resize(n); if (!CryptStringToBinaryA(b64.c_str(), (DWORD)b64.size(), CRYPT_STRING_BASE64, (BYTE*)bin.data(), &n, nullptr, nullptr)) { run(L"lbSaved(0,'The signed copy could not be read.')"); return; } bin.resize(n);
        if (bin.compare(0, 5, "%PDF-") != 0) { run(L"lbSaved(0,'The signed copy is not a PDF.')"); return; }
        std::wstring name = PdfBase(v[1]) + L"-signed.pdf", path; wchar_t sp[MAX_PATH];
        if (TestEnv(L"LIBRA_TEST_SAVEPATH", sp, MAX_PATH)) { if (wcscmp(sp, L"CANCEL") == 0) { run(L"lbSaved(0,'Not saved.')"); return; } path = std::wstring(sp) + L"\\" + name; }
        else { wil::com_ptr<IFileSaveDialog> d;
            if (SUCCEEDED(CoCreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&d)))) {
                COMDLG_FILTERSPEC fs[] = { { L"PDF files", L"*.pdf" } }; d->SetFileTypes(1, fs); d->SetDefaultExtension(L"pdf"); d->SetFileName(name.c_str()); d->SetTitle(L"Save the signed copy");
                PWSTR dl = nullptr; if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Downloads, 0, nullptr, &dl))) { wil::com_ptr<IShellItem> fi; if (SUCCEEDED(SHCreateItemFromParsingName(dl, nullptr, IID_PPV_ARGS(&fi)))) d->SetDefaultFolder(fi.get()); CoTaskMemFree(dl); }
                if (SUCCEEDED(d->Show(m_hWnd))) { wil::com_ptr<IShellItem> it; PWSTR ps = nullptr; if (SUCCEEDED(d->GetResult(&it)) && SUCCEEDED(it->GetDisplayName(SIGDN_FILESYSPATH, &ps))) { path = ps; CoTaskMemFree(ps); } } }
            if (path.empty()) { run(L"lbSaved(0,'Not saved.')"); return; } }
        WriteAllRaw(path, bin, false); run(L"lbSaved(1,\"" + PdfJs(path.substr(path.find_last_of(L"\\/") + 1)) + L"\")"); return;
    }
}
void BrowserWindow::PdfToolsGot(void* p) {
    m_pdfBusy = false; std::unique_ptr<PdfDl> r((PdfDl*)p); if (!r) return;
    for (auto& tb : m_tabs) if (tb->url == L"libra://pdfsign" && tb->webView)
        tb->webView->ExecuteScript(r->err.empty() ? L"lbTools(1)" : (L"lbToolsErr(\"" + PdfJs(FromUtf8(r->err)) + L"\")").c_str(), nullptr);
}
