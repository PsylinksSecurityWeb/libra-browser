// Reader mode: F9 reads the article of the active web page and shows it in libra://reader (text only). Included at the end of BrowserWindow.cpp.
void BrowserWindow::OpenReader() {
    if (m_activeTab < 0 || m_activeTab >= (int)m_tabs.size()) return;
    Tab* t = m_tabs[m_activeTab].get();
    if (!t->webView || !IsHttp(t->url)) return;   // only real web pages
    t->webView->ExecuteScript(kReaderJs, Microsoft::WRL::Callback<ICoreWebView2ExecuteScriptCompletedHandler>(
        [this](HRESULT hr, LPCWSTR r) -> HRESULT {
            std::wstring s = (SUCCEEDED(hr) && r) ? r : L"";
            if (s.size() < 4 || s.size() > 3u << 20 || s[0] != L'"') s = L"\"{\\\"err\\\":1}\"";   // refused: show the "not an article" message
            m_readerRaw = s; ShowInternal(L"libra://reader"); return S_OK; }).Get());
}
