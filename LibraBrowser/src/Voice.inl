// Voice.inl : included at the end of BrowserWindow.cpp (after Agent.inl). Local speech-to-text for the Assistant.
// whisper.cpp (MIT) + the multilingual "base" q5 model run on THIS PC; audio never leaves it and is never kept.
// The "voice pack" (about 70 MB) is downloaded once, only after the user asks, through Libra's own DNS proxy, and both files are checked against pinned SHA-256 hashes.
#pragma comment(lib, "winmm.lib")
#include <mmsystem.h>
#include <atomic>
#include <functional>

static std::atomic<bool> g_vStop{ false }, g_vBusy{ false };
static const char* kVoiceZipSha = "6ba69e3482d7826214f90a6a9c84ca07782aec1e1d0c6a7c30c994fd5d816ccb";     // whisper.cpp b5454 whisper-bin-x64.zip
static const char* kVoiceModelSha = "422f1ae452ade6f30a004d7e5c6a43195e4433bc370bf23fac9cc591f01a8898";   // ggml-base-q5_1.bin
static const wchar_t* kVoiceZipUrl = L"https://github.com/ggml-org/whisper.cpp/releases/download/b5454/whisper-bin-x64.zip";
static const wchar_t* kVoiceModelUrl = L"https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-base-q5_1.bin";

static std::wstring VoiceDir() { return DataDir() + L"\\voice"; }
static bool VoiceExists(const std::wstring& rel) { return GetFileAttributesW((VoiceDir() + rel).c_str()) != INVALID_FILE_ATTRIBUTES; }
static bool VoiceReady() { return VoiceExists(L"\\ready") && VoiceExists(L"\\Release\\whisper-cli.exe") && VoiceExists(L"\\model.bin"); }
static void VoicePost(HWND hw, const wchar_t* kind, const std::wstring& text) { PostMessage(hw, WM_APP + 36, 0, (LPARAM) new std::pair<std::wstring, std::wstring>(kind, text)); }

static bool VoiceGet(int port, const std::wstring& url, const std::wstring& dest, const char* want, const std::function<void(int)>& prog) {
    dl::Url u; if (!dl::Crack(url, u)) return false; bool ok = false;
    HINTERNET ses = dl::Session(port, 1); if (!ses) return false; WinHttpSetTimeouts(ses, 20000, 20000, 60000, 60000);
    BCRYPT_ALG_HANDLE alg = nullptr; BCRYPT_HASH_HANDLE h = nullptr;
    BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0); BCryptCreateHash(alg, &h, nullptr, 0, nullptr, 0, 0);
    if (HINTERNET con = WinHttpConnect(ses, u.host.c_str(), u.port, 0)) {
        if (HINTERNET rq = WinHttpOpenRequest(con, L"GET", u.path.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE)) {
            if (WinHttpSendRequest(rq, WINHTTP_NO_ADDITIONAL_HEADERS, 0, nullptr, 0, 0, 0) && WinHttpReceiveResponse(rq, nullptr)) {
                DWORD code = 0, sz = sizeof code; WinHttpQueryHeaders(rq, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, nullptr, &code, &sz, nullptr);
                wchar_t lb[32]; DWORD ls = sizeof lb; unsigned long long total = 0; if (WinHttpQueryHeaders(rq, WINHTTP_QUERY_CONTENT_LENGTH, nullptr, lb, &ls, nullptr)) total = _wcstoui64(lb, nullptr, 10);
                HANDLE f = code == 200 ? CreateFileW(dest.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr) : INVALID_HANDLE_VALUE;
                if (f != INVALID_HANDLE_VALUE) {
                    std::vector<char> buf(65536); DWORD n = 0; unsigned long long got = 0; int last = -1; bool good = true;
                    while (WinHttpReadData(rq, buf.data(), (DWORD)buf.size(), &n) && n) {
                        DWORD wr = 0; if (!WriteFile(f, buf.data(), n, &wr, nullptr) || wr != n) { good = false; break; }
                        BCryptHashData(h, (PUCHAR)buf.data(), n, 0); got += n;
                        if (total) { int p = (int)(got * 100 / total); if (p != last) { last = p; prog(p); } }
                    }
                    CloseHandle(f);
                    if (good) { BYTE d[32]; if (BCryptFinishHash(h, d, 32, 0) == 0) { char hex[65]; for (int i = 0; i < 32; ++i) sprintf_s(hex + i * 2, 3, "%02x", d[i]); ok = _stricmp(hex, want) == 0; } }
                    if (!ok) DeleteFileW(dest.c_str());   // wrong or partial file: never kept
                }
            }
            WinHttpCloseHandle(rq);
        }
        WinHttpCloseHandle(con);
    }
    if (h) BCryptDestroyHash(h); if (alg) BCryptCloseAlgorithmProvider(alg, 0); WinHttpCloseHandle(ses);
    return ok;
}

static void VoiceDownloadThread(HWND hw, int port) {
    const std::wstring dir = VoiceDir(); SHCreateDirectoryExW(nullptr, dir.c_str(), nullptr);
    if (!VoiceGet(port, kVoiceZipUrl, dir + L"\\bin.zip", kVoiceZipSha, [&](int p) { VoicePost(hw, L"dl", std::to_wstring(p * 15 / 100)); })) { VoicePost(hw, L"err", L"The voice download failed or did not match its checksum. Nothing was installed."); g_vBusy = false; return; }
    if (!VoiceGet(port, kVoiceModelUrl, dir + L"\\model.bin", kVoiceModelSha, [&](int p) { VoicePost(hw, L"dl", std::to_wstring(15 + p * 83 / 100)); })) { VoicePost(hw, L"err", L"The voice model download failed or did not match its checksum. Nothing was installed."); g_vBusy = false; return; }
    wchar_t sys[MAX_PATH]; GetSystemDirectoryW(sys, MAX_PATH);
    std::wstring cmd = std::wstring(L"\"") + sys + L"\\tar.exe\" -xf \"" + dir + L"\\bin.zip\" -C \"" + dir + L"\"";
    STARTUPINFOW st{ sizeof st }; PROCESS_INFORMATION pi{};
    if (CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, dir.c_str(), &st, &pi)) { WaitForSingleObject(pi.hProcess, 90000); CloseHandle(pi.hThread); CloseHandle(pi.hProcess); }
    DeleteFileW((dir + L"\\bin.zip").c_str());
    if (!VoiceExists(L"\\Release\\whisper-cli.exe")) { VoicePost(hw, L"err", L"Could not unpack the voice files."); g_vBusy = false; return; }
    WriteAllRaw(dir + L"\\ready", "b5454 base-q5_1", false);
    g_vBusy = false; VoicePost(hw, L"ready", L"");
}

static std::wstring VoiceTranscribe(const std::wstring& wav, const std::wstring& lang) {
    const std::wstring dir = VoiceDir(), cli = dir + L"\\Release\\whisper-cli.exe";
    SYSTEM_INFO si; GetSystemInfo(&si); const int th = (int)std::min<DWORD>(4, std::max<DWORD>(2, si.dwNumberOfProcessors / 2));
    std::wstring cmd = L"\"" + cli + L"\" -m \"" + dir + L"\\model.bin\" -f \"" + wav + L"\" -l " + lang + L" -nt -np -nth 0.5 -t " + std::to_wstring(th) + L" --prompt \"Flipkart, LinkedIn, Typeform, Amazon, Google, Libra Browser.\"";
    SECURITY_ATTRIBUTES sa{ sizeof sa, nullptr, TRUE }; HANDLE rd = nullptr, wr = nullptr; if (!CreatePipe(&rd, &wr, &sa, 0)) return L"";
    SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);
    HANDLE nul = CreateFileW(L"NUL", GENERIC_WRITE, FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, nullptr);
    STARTUPINFOW st{ sizeof st }; st.dwFlags = STARTF_USESTDHANDLES; st.hStdOutput = wr; st.hStdError = nul; st.hStdInput = nullptr;
    PROCESS_INFORMATION pi{}; std::string out;
    if (CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW | BELOW_NORMAL_PRIORITY_CLASS, nullptr, (dir + L"\\Release").c_str(), &st, &pi)) {
        CloseHandle(wr); wr = nullptr; DWORD t0 = GetTickCount(); char buf[4096];
        for (;;) {
            DWORD avail = 0; PeekNamedPipe(rd, nullptr, 0, nullptr, &avail, nullptr);
            if (avail) { DWORD n = 0; if (ReadFile(rd, buf, sizeof buf, &n, nullptr) && n && out.size() < 20000) out.append(buf, n); continue; }
            if (WaitForSingleObject(pi.hProcess, 40) == WAIT_OBJECT_0) { PeekNamedPipe(rd, nullptr, 0, nullptr, &avail, nullptr); if (!avail) break; }
            if (GetTickCount() - t0 > 90000) { TerminateProcess(pi.hProcess, 1); break; }
        }
        CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
    }
    if (wr) CloseHandle(wr); CloseHandle(rd); if (nul != INVALID_HANDLE_VALUE) CloseHandle(nul);
    std::wstring t = FromUtf8(out), r;   // tidy: drop [BLANK_AUDIO]-style tags, collapse spaces
    for (size_t i = 0; i < t.size(); ++i) {
        if (t[i] == L'[' || t[i] == L'(') { size_t e = t.find(t[i] == L'[' ? L']' : L')', i); if (e != std::wstring::npos && e - i < 30) { i = e; continue; } }
        r += (t[i] == L'\r' || t[i] == L'\n' || t[i] == L'\t') ? L' ' : t[i];
    }
    std::wstring o; bool sp = true; for (wchar_t c : r) { if (c == L' ') { if (!sp) o += c; sp = true; } else { o += c; sp = false; } }
    while (!o.empty() && o.back() == L' ') o.pop_back();
    if (o.size() > 500) o.resize(500);
    { std::map<std::wstring, int> cnt; int words = 0, top = 0; size_t i = 0;   // a noisy microphone makes the recognizer repeat one word: that is not speech
      while (i < o.size()) { size_t e = o.find(L' ', i); if (e == std::wstring::npos) e = o.size(); int c = ++cnt[o.substr(i, e - i)]; ++words; top = std::max(top, c); i = e + 1; }
      if (words >= 8 && top * 10 > words * 6) return L""; }
    return o;
}

static void VoiceWriteWav(const std::wstring& path, const std::vector<short>& pcm) {
    std::string h(44, '\0'); const unsigned dl = (unsigned)(pcm.size() * 2), rl = 36 + dl, sr = 16000, br = 32000; const unsigned short fmt = 1, ch = 1, ba = 2, bps = 16; const unsigned fl = 16;
    memcpy(&h[0], "RIFF", 4); memcpy(&h[4], &rl, 4); memcpy(&h[8], "WAVEfmt ", 8); memcpy(&h[16], &fl, 4); memcpy(&h[20], &fmt, 2); memcpy(&h[22], &ch, 2); memcpy(&h[24], &sr, 4); memcpy(&h[28], &br, 4); memcpy(&h[32], &ba, 2); memcpy(&h[34], &bps, 2); memcpy(&h[36], "data", 4); memcpy(&h[40], &dl, 4);
    HANDLE f = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY, nullptr); if (f == INVALID_HANDLE_VALUE) return;
    DWORD w = 0; WriteFile(f, h.data(), 44, &w, nullptr); WriteFile(f, pcm.data(), dl, &w, nullptr); CloseHandle(f);
}

static void VoiceListenThread(HWND hw, std::wstring lang, std::wstring testWav) {
    const std::wstring wav = VoiceDir() + L"\\rec.wav";
    if (testWav.empty()) {
        WAVEFORMATEX f{}; f.wFormatTag = WAVE_FORMAT_PCM; f.nChannels = 1; f.nSamplesPerSec = 16000; f.wBitsPerSample = 16; f.nBlockAlign = 2; f.nAvgBytesPerSec = 32000;
        HWAVEIN hin = nullptr;
        if (waveInOpen(&hin, WAVE_MAPPER, &f, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) { g_vBusy = false; VoicePost(hw, L"err", L"I cannot use the microphone. Check Windows Settings > Privacy > Microphone (allow desktop apps)."); return; }
        const int N = 6, SAMPLES = 4000;   // 0.25 s buffers
        std::vector<std::vector<short>> bufs(N, std::vector<short>(SAMPLES)); WAVEHDR hd[N]{};
        for (int i = 0; i < N; ++i) { hd[i].lpData = (LPSTR)bufs[i].data(); hd[i].dwBufferLength = SAMPLES * 2; waveInPrepareHeader(hin, &hd[i], sizeof(WAVEHDR)); waveInAddBuffer(hin, &hd[i], sizeof(WAVEHDR)); }
        waveInStart(hin);
        std::vector<short> pcm; int i = 0, calib = 0; double floorSum = 0, thr = 120, maxRms = 0; bool spoke = false; int quiet = 0; const DWORD t0 = GetTickCount();
        while (!g_vStop && GetTickCount() - t0 < 30000) {
            if (!(hd[i].dwFlags & WHDR_DONE)) { Sleep(15); continue; }
            const short* p = (const short*)hd[i].lpData; const int n = (int)(hd[i].dwBytesRecorded / 2); double e = 0; for (int k = 0; k < n; ++k) e += (double)p[k] * p[k];
            const double rms = n ? sqrt(e / n) : 0; pcm.insert(pcm.end(), p, p + n); if (rms > maxRms) maxRms = rms;
            if (calib < 2) { ++calib; if (rms < floorSum || floorSum == 0) floorSum = rms; thr = std::min(2000.0, std::max(120.0, floorSum * 3)); }   // room noise = the quietest buffer so far, so speaking right away cannot raise the bar
            else { if (rms < floorSum) { floorSum = rms; thr = std::min(2000.0, std::max(120.0, floorSum * 3)); } if (rms > thr) { spoke = true; quiet = 0; } else if (spoke) ++quiet; }
            if (spoke && quiet >= 6) break;   // 1.5 s of silence after speech (slow speakers can pause)
            hd[i].dwFlags &= ~WHDR_DONE; hd[i].dwBytesRecorded = 0; waveInAddBuffer(hin, &hd[i], sizeof(WAVEHDR)); i = (i + 1) % N;
        }
        waveInStop(hin); waveInReset(hin); for (int k = 0; k < N; ++k) waveInUnprepareHeader(hin, &hd[k], sizeof(WAVEHDR)); waveInClose(hin);
        if (!spoke) { g_vBusy = false; if (!pcm.empty()) SecureZeroMemory(pcm.data(), pcm.size() * 2); VoicePost(hw, L"err", maxRms < 30 ? L"Your microphone seems muted or silent. Check Windows Settings > System > Sound > Input, pick your microphone and raise its volume." : L"I did not hear anything. Speak a little louder and try again."); return; }
        { int pk = 1; for (short v : pcm) pk = std::max(pk, (int)abs((int)v)); if (pk < 12000) { double g = std::min(20.0, 16000.0 / pk); for (auto& v : pcm) v = (short)std::max(-32767.0, std::min(32767.0, v * g)); } }   // quiet microphones: lift the level for the recognizer
        VoiceWriteWav(wav, pcm); SecureZeroMemory(pcm.data(), pcm.size() * 2);
    }
    VoicePost(hw, L"proc", L"");
    std::wstring text = VoiceTranscribe(testWav.empty() ? wav : testWav, lang);
     if (testWav.empty()) DeleteFileW(wav.c_str());   // the recording is never kept
    g_vBusy = false; VoicePost(hw, L"text", text);
}

void BrowserWindow::VoiceCmd(const std::wstring& a, const std::wstring& b) {
    if (a == L"state") { AgSend(VoiceReady() ? L"lbVoice('ready','')" : L"lbVoice('nopack','')"); return; }
    if (a == L"stop") { g_vStop = true; return; }
    if (a == L"dl") {
        if (VoiceReady()) { AgSend(L"lbVoice('ready','')"); return; }
        if (g_vBusy.exchange(true)) return;
        int port = DohProxy::Get().Port(); HWND hw = m_hWnd; std::thread([hw, port] { VoiceDownloadThread(hw, port); }).detach(); return;
    }
    const bool test = a == L"test"; wchar_t tw[MAX_PATH]; std::wstring testWav;
    if (test) {   // automated tests only: LIBRA_TEST_WAV is a folder, b is a plain file name inside it
        if (TestEnv(L"LIBRA_TEST_WAV", tw, MAX_PATH) == 0 || b.empty() || b.find_first_of(L"\\/:") != std::wstring::npos) return;
        testWav = std::wstring(tw) + L"\\" + b;
    } else if (a != L"start") return;
    if (!VoiceReady()) { AgSend(L"lbVoice('nopack','')"); return; }
    if (g_vBusy.exchange(true)) return;
    g_vStop = false; std::wstring lang = m_set.Get(L"agent_vlang", L"auto"); HWND hw = m_hWnd;
    VoicePost(hw, L"rec", L"");
    std::thread([hw, lang, testWav] { VoiceListenThread(hw, lang, testWav); }).detach();
}
void BrowserWindow::OnVoiceMsg(void* p) {
    std::unique_ptr<std::pair<std::wstring, std::wstring>> m((std::pair<std::wstring, std::wstring>*)p);
    AgSend(L"lbVoice(" + AgJs(m->first) + L"," + AgJs(m->second) + L")");
}
