#pragma once
#include "Filters.h"
#include "pch.h"
#include "resource.h"
#include "Tab.h"
#include "AdBlocker.h"
#include "Data.h"
#include "Sponsor.h"
#include "Signals.h"
#include "Update.h"
#include "Import.h"
#include "Suggest.h"
#include "AdRules.h"
#include "Vault.h"
#include <set>
#include <deque>
#include <functional>

// Forward declarations
class BrowserApp;
bool RegisterAsBrowser();   // Privacy.inl
bool IsDefaultBrowser();

// â”€â”€â”€ BrowserWindow â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
// One top-level window = one set of tabs + one WebView2 environment.
// All GDI/painting is here; each Tab owns its ICoreWebView2Controller.
class BrowserWindow {
public:
    explicit BrowserWindow(HINSTANCE hInst, bool incognito = false);
    ~BrowserWindow();

    bool Create(int nCmdShow);
    void OpenTab(const std::wstring& url = L"", bool incognito = false, bool background = false);
    void SetApp(const std::wstring& id, const std::wstring& name, const std::wstring& url, int port) { m_app = true; m_appId = id; m_appName = name; m_appUrl = url; m_appPort = port; }   // web-app window (Apps.h)
    void SetHostHidden() { m_hostHidden = true; }
    void SetStartUrl(const std::wstring& u) { m_startUrl = u; }   // private window opened for a link: that page is its first tab
    void OpenLinkPrivate(const std::wstring& url); void NavRefresh(Tab* t);
    Flt::Set m_flt; void LoadFilters(); bool FilterBlocks(const wchar_t* uri, const std::wstring& pageUrl, int ctx) const; std::wstring FilterJs() const;   // FilterRun.inl
    std::wstring m_startUrl;   // engine host for an app window while the browser is not open: no window shown
    int TabHpx() const; void AppMinimized(bool min);
    void CloseTab(int index);
    void SwitchTab(int index);
    void NavigateTo(const std::wstring& input);    // Smart URL/search
    void SetAdBlockEnabled(bool enabled);
    std::wstring GetHomepageHtml();
    void OnOmniboxReturn();
    bool HandleKey(UINT vk);   // browser shortcuts; true = handled
    bool SugKey(UINT vk);      // address-bar suggestion list keys; true = handled

    HWND Hwnd() const { return m_hWnd; }
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

private:
    // â”€â”€â”€ Init helpers â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    void RegisterClass();
    void CreateControls();
    void InitWebView2(int tabIndex);

    // â”€â”€â”€ Paint / Layout â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    void OnPaint(HDC hdc, RECT clientRect);
    void PaintTabBar(HDC hdc, const RECT& tabBarRect);
    void PaintToolbar(HDC hdc, const RECT& toolbarRect);
    void PaintProgressBar(HDC hdc, const RECT& toolbarRect);
    RECT WebViewRect() const;
    RECT OmniPillRect() const;                 // rounded address-bar pill (client coords)
    void LayoutToolbar(int cx);                // position buttons + omnibox edit
    void DrawNavButton(const DRAWITEMSTRUCT* dis);
    // Recently visited sites (host + base64 PNG favicon), most recent first, max 5
    std::vector<std::pair<std::wstring, std::string>> m_sites;
    std::map<std::wstring, std::pair<std::wstring, std::wstring>> m_tileMeta;   // host -> custom name, url (edited home tiles)
    static std::wstring SitesPath();
    void LoadSites();
    void SaveSites();
    void RecordVisit(const std::wstring& host, const std::string& icon = "");
    static std::wstring HostOf(const std::wstring& url);
    HWND m_hoverBtn = nullptr;                 // button currently under the cursor
    HBRUSH m_brOmni = nullptr;                 // cached omnibox edit background
    void ResizeWebViews();
    void UpdateOmniboxText(const std::wstring& url);
    void UpdateNavButtons();

    // â”€â”€â”€ Event handlers â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    void OnSize(int cx, int cy);
    void OnClose(); void SaveState(); void SaveSession(); void OnDpiChanged(UINT dpi, const RECT* r);
    void OnCommand(WPARAM wParam, LPARAM lParam);
    void OnLButtonDown(int x, int y);
    bool OnSetCursor(HWND hwnd, UINT hitTest);
    void OnTimer(UINT_PTR id);
    void HoldBegin(int i); void HoldDone(void* p); bool HoldRelease(int i); bool HoldSandbox(int i); void HoldSandboxCleanup(); std::wstring m_riskNote;   // download quarantine (Hold.inl)
    UpInfo m_up; bool m_upBusy = false; void UpLoad(); void UpKick(bool force = false); void UpGot(void* r); void UpAct(const std::wstring& what); void UpReady(void* r); std::wstring UpHtml();   // update notifier (Update.inl)
    std::vector<std::pair<std::wstring, std::wstring>> m_impSet; void OnBackup(int idx, const std::vector<std::wstring>& v);   // encrypted backup (Backup.inl)
    ImList m_imp; void OnImport(int idx, const std::wstring& c, const std::wstring& a); void OnboardImport(int idx, const std::wstring& a);   // bookmark import (Import.inl)
    bool m_pdfBusy = false; void PdfMapTools(Tab* t); void OnPdf(int idx, const std::vector<std::wstring>& v); void PdfToolsGot(void* r);   // PDF fill and sign (Pdf.inl)
    std::wstring m_readerRaw; void OpenReader();   // reader mode (F9, Reader.inl)
    void OpenShot(); bool ShotKey(UINT vk); bool ShotTimer(UINT_PTR id);   // Screenshot (Shot.inl)
    void Burn();                       // hamburger: Burn (Burn.inl)
    bool BurnTimer(UINT_PTR id);
    bool m_burning = false; Tab* m_burnTab = nullptr; std::function<void(bool, std::wstring)> m_wipeDone;   // called once when WipeAll has finished
    void ShowContextMenu(int x, int y);

    // â”€â”€â”€ Tab-bar hit testing â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    int  TabIndexAt(int x, int y) const;
    bool TabCloseHit(int tabIndex, int x, int y) const;
    RECT TabRect(int index) const;
    RECT TabCloseRect(int index) const;

    // â”€â”€â”€ URL helpers â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    std::wstring SmartUrl(const std::wstring& input) const;
    std::wstring EngineUrl() const;
    std::wstring PageHtml(const std::wstring& url);
    void ShowInternal(const std::wstring& url, bool activate = true);
    void RenderTab(int idx);
    void OnPageMessage(int tabIdx, const std::wstring& msg);
    void StartupTabs();
    void LoadData();
    void RecordHistory(const std::wstring& url, const std::wstring& title);
    void SaveHistory();
    void SaveBookmarks(); void ReloadBookmarks();   // reload = take the file as the truth first (a private window runs as a second process with its own copy of the list)
    void ToggleBookmark();
    bool IsBookmarked(const std::wstring& url) const;
    RECT StarRect() const;
    void AttachDownloads(ICoreWebView2* wv);
    void SyncDownload(ICoreWebView2DownloadOperation* op, bool render);
    struct PendingDl { wil::com_ptr<ICoreWebView2DownloadStartingEventArgs> args; wil::com_ptr<ICoreWebView2Deferral> def;
                       wil::com_ptr<ICoreWebView2DownloadOperation> op; std::wstring name, path; };
    std::map<void*, PendingDl> m_pending;       // downloads waiting for the range probe (UI thread only)
    void AdoptDownload(ICoreWebView2DownloadStartingEventArgs* a, ICoreWebView2DownloadOperation* op, const std::wstring& name, const std::wstring& chosen);
    // ask-before-download gate (SaveAsk.inl): nothing starts until the user presses Save
    void SaveGate(const std::wstring& name, const std::wstring& url, Tab* tab, std::function<void(std::wstring)> cb); void SaveNext(); bool SaveTimer(UINT_PTR id); std::wstring SaveAskPath(const std::wstring& name);
    void DlContinue(wil::com_ptr<ICoreWebView2DownloadStartingEventArgs> a, wil::com_ptr<ICoreWebView2Deferral> def, wil::com_ptr<ICoreWebView2DownloadOperation> op,
                    wil::com_ptr<ICoreWebView2> snd, const std::wstring& name, const std::wstring& url, const std::wstring& chosen);
    void OnProbeDone(std::shared_ptr<DlJob> job, bool ok);
    void SyncJobs();
    void ClearData();
    void AgWipe();   // assistant memory, permissions, sandbox files, counter; NEVER the API key
    void WipeAll(int idx);
    // --- shield: blocked counter + per-site switch ---
    static std::wstring SiteOf(const std::wstring& host);
    int  ModeFor(const std::wstring& host) const; int ModeForRaw(const std::wstring& host) const;                       // 0 Max, 1 Balanced, 2 Off
    bool ShieldCtx(std::wstring& host, int& mode, int& req, int& nh, int& lt) const;   // returns true for http(s) pages
    void SetSiteMode(int mode);
    void OnLetThrough(const std::string& host);
    void SaveAllow();
    void UpdateBypass();
    ULONGLONG m_pwUntil = 0; std::vector<std::wstring> m_pwPend; Tab* m_pwPendTab = nullptr;   // Windows sign-in before showing or copying a saved password (5 min)
    bool m_child = false; std::wstring m_authHost; void ChildLoad(); void ChildApply(bool on, bool noLock); void AskWindows(int purpose); void OnWinAuth(int purpose, int r);   // Child Safety
    void InstallAdScript(Tab* t);
    void OnBlockedHit(const std::string& host);
    void ToggleShieldPopup();
    void HideShield();
    RECT ShieldSegRect(int i) const; RECT ShieldKeepRect() const; std::set<std::wstring> m_keepId; void ToggleKeepId(); void AgTrackFrames(Tab* t); struct AgFr { wil::com_ptr<ICoreWebView2Frame2> f; int base; int n; }; std::vector<AgFr> m_agFr; int m_agMainN = 0;
    void AgSnapFrames(Tab* t, unsigned gen, std::wstring acc, std::vector<wil::com_ptr<ICoreWebView2Frame2>> fr, size_t i, int total, std::vector<AgFr> got);
    bool AgBtnOn() const { return m_set.Get(L"agent_btn", L"1") != L"0"; } bool KeepIdHere();
    int ShieldRows() const;   // top-blocked rows shown in the popup (0 = compact layout)
    int ShieldSelY() const;   // 96-dpi y of the Off/Balanced/Max selector
    void PaintShield(HDC hdc);
    void DrawShieldIcon(HDC hdc, RECT rc, bool hot); void DrawAgentIcon(HDC hdc, RECT rc, bool open); void DrawShotIcon(HDC hdc, RECT rc);
    static LRESULT CALLBACK ShieldProc(HWND, UINT, WPARAM, LPARAM);
    std::map<std::wstring, int> m_modes;       // site -> 0 Max, 1 Balanced, 2 Off
    unsigned long long m_totalBlocked = 0;
    bool  m_totalDirty = false;
    HWND  m_hShield = nullptr;
    DWORD m_shieldHiddenTick = 0;
    // --- privacy page, default browser (Privacy.inl) ---
    void AttachVideoSniff(Tab* t); void DetachVideoSniff(Tab* t); void ShowVideoPopup(); RECT VidRect() const;
    void StartVideoDownload(Tab* t, int idx); void AttachContextSave(Tab* t);
    void StartSave(Tab* t, const std::wstring& url, const std::wstring& nameBase, const std::wstring& extHint, bool useTitle); void StartSaveTo(Tab* t, const std::wstring& url, const std::wstring& path); void OnVideoProbe(std::shared_ptr<DlJob> job, bool ok);
    std::map<DlJob*, std::shared_ptr<DlJob>> m_vidJobs;
    void DlPush(); void ApplyMemoryLevels();
    void InstallFpScript(Tab* t); unsigned m_fpSeed = 0; int m_fpDay = 0; std::set<std::wstring> m_httpOk; std::set<std::wstring> m_pdfAllow;   // exact file: addresses of downloaded PDFs the user asked to read (lowercase)
    void PrivSendStats(Tab* t);
    void StatLoad(); void StatSave(); void PrivSendReport(Tab* t);
    std::map<std::pair<int, std::string>, int> m_stat; bool m_statDirty = false, m_statLoaded = false;   // (day yyyymmdd, host) -> blocked requests, kept 90 days in stats.txt
    void FirstBlock();
    int m_fbAds = 0, m_fbTrk = 0;
    void PrivClear(Tab* t, const std::set<std::wstring>& keys, const std::wstring& range);
    void OnPrivPage(int idx, const std::vector<std::wstring>& v);
    void OnAgentPage(int idx, const std::vector<std::wstring>& v); void AgentOnLlm(unsigned gen, const std::wstring& text); void AgSend(const std::wstring& js); void AgStop(const wchar_t* why); int AgTarget() const; void AgRunAct(int ti, const std::wstring& op, const std::wstring& id, const std::wstring& tx, bool ok);
    bool AppOffer() const; std::wstring AppOfferName() const; RECT AppRect() const; void AppRelayout(); void AppDetectSoon(); void AppDetectRun(); void AppInstallAsk(bool manual = false); void AppLaunch(const std::wstring& id); void AppWindowProps(); bool AppInScope(const std::wstring& u) const; void AppToBrowser(const std::wstring& u); void AppRemove(const std::wstring& id); std::wstring AppsHtml(); bool m_appDesk = false; void PaintAppIcon(HDC hdc); void AppInstalled(void* p); void AppLoadInstalled(); std::set<std::wstring> m_appInst; bool m_appInstLoaded = false, m_appInstalling = false; std::map<std::wstring, std::wstring> m_appCache; bool m_appBusy = false, m_appShown = false;
    bool SiteOn() const; int SiteW() const; std::vector<std::wstring> SiteList() const; RECT SiteRect(int i) const; void PaintSites(HDC hdc); bool SiteClick(int x, int y); void OpenSite(const std::wstring& u); void ApplySite(); Tab* m_sitePanel = nullptr; std::wstring m_siteUrl; std::map<std::wstring, std::shared_ptr<void>> m_siteFav; std::set<std::wstring> m_siteTry; void SiteIconGot(void* p); void SiteSaveIcon(const std::string& bytes, const std::shared_ptr<void>& bm);
    bool TabHid(int i) const; int LastVisTab() const; bool SiteRight(int x, int y); void SiteAdd(const std::wstring& url); void SiteSetList(const std::vector<std::wstring>& v); void SwitchEngineTabs(const std::wstring& oldEu);
    bool DockOn() const { return m_agDock || m_sitePanel; }
    void ToggleAgent(); void ApplyDock(); bool AgAllowedHere() const; int AgDockIdx() const; bool m_agDock = false;
    void AgFileHook(bool on); void AgFileChosen(void* p); void AgGoNow(); bool m_agPaused = false; bool AgAsk(const std::wstring& cat, const std::wstring& dom, const std::wstring& title, const std::wstring& detail, bool once); void AgClickCheck(); std::wstring m_agClickId, m_agFp; int m_agRetried = 0; std::wstring m_agAttach; std::wstring m_agTaskRaw; std::set<std::wstring> m_agTypeOk; int m_agFcN = 0; void VoiceCmd(const std::wstring& a, const std::wstring& b); void OnVoiceMsg(void* p); bool m_agConv = false; Tab* m_agHome = nullptr; Tab* m_agRes = nullptr; int m_agTabs = 0; void AgKeyChecked(const std::wstring& r); void AgClickNow(); int m_agClickX = 0, m_agClickY = 0; Tab* m_agClickTab = nullptr; int m_agTry = 0, m_agClickRid = 0, m_agStab = 0; bool m_agNewWin = false; wil::com_ptr<ICoreWebView2Frame2> m_agClickFrame; void AgNextStrategy(); void AgExecT(const wchar_t* js, ICoreWebView2ExecuteScriptCompletedHandler* h); EventRegistrationToken m_agFcTok{}; Tab* m_agFcTab = nullptr; Tab* m_agGoTab = nullptr; std::wstring m_agTaskKey, m_agGoUrl;
    void AgCleanLbx(Tab* t); void AgSendSnap(Tab* t, unsigned gen, const std::wstring& s); void AgRunPick(int ti, int rid, const std::wstring& tx, unsigned gen); void AgPickCheck(); void AgRunDrag(int ti, int id1, int id2, unsigned gen); void AgDragCheck(); Tab* m_agPickTab = nullptr; int m_agPickRid = 0; std::wstring m_agPickTx;
    bool m_agRun = false; unsigned m_agGen = 0; int m_agSteps = 0; DWORD m_agStart = 0; Tab* m_agTarget = nullptr; std::wstring m_agScope;   // Agent.inl
    void SetAsDefault();
    // --- passwords (Passwords.inl) ---
    std::vector<Cred> m_vault; bool m_vaultLoaded = false; std::set<std::wstring> m_pwNever;
    void PwLoad();
    void InstallPwScript(Tab* t);
    void OnSitePwMessage(Tab* t, const std::wstring& srcUrl, const std::wstring& msg);
    void OnPwPage(int idx, const std::vector<std::wstring>& v);
    void ShowPw(int kind, Tab* t, const std::wstring& origin);
    void HidePw();
    void PwClick(int what);
    void PaintPw(HDC hdc);
    void PwCopy(const std::wstring& s);
    void PwClipClear();
    int  PwHit(POINT pt) const;                     // save: 0 Save,1 Not now,2 Never; fill: 10+row
    RECT PwRect(int i) const;
    static LRESULT CALLBACK PwProc(HWND, UINT, WPARAM, LPARAM);
    std::wstring m_toastT, m_toastN, m_riskPath; int m_pwAnim = 0, m_pwSl = 0, m_pwW = 0, m_pwH = 0; /* toast slide: 1 in, 2 out, 3 hide now */ HWND m_hPw = nullptr; int m_pwKind = 0, m_pwHot = -1; Tab* m_pwTab = nullptr; std::wstring m_pwOrigin;
    Cred m_pwPending; long long m_pwUpdate = 0; std::vector<long long> m_pwChoice;
    std::wstring m_clipVal; DWORD m_pwRate[2] = { 0, 0 }, m_pwShown = 0;
    void RefreshTabsPage();
    RECT FavRect(int i) const;
    int  IndexOf(const Tab* t) const;
    void TogglePin(int idx);
    void SavePinned();
    void ShowTabMenu(int idx, POINT screen);
    void SetFullscreen(bool on);
    void UpdateSuggestions();
    void FetchWebSug(const std::wstring& q);
    void HomeSug(int idx, const std::wstring& q);
    bool WebSugOn(const std::wstring& q) const;
    unsigned m_sugGen = 0; std::wstring m_sugQ;   // web suggestions: newest request id + typed text
    void ShowSug();
    void HideSug();
    void PaintSug(HDC hdc);
    static LRESULT CALLBACK SugProc(HWND, UINT, WPARAM, LPARAM);
    HWND m_hSug = nullptr;
    std::vector<std::pair<std::wstring, std::wstring>> m_sugItems;   // url, title
    int  m_sugSel = -1;
    bool m_settingText = false;
    bool m_full = false;
    DWORD m_typedTick = 0;   // last time the user typed an address (app-started navigations count as user actions)
    bool m_app = false, m_hostHidden = false; std::wstring m_appId, m_appName, m_appUrl; int m_appPort = 0;
    bool m_setup = false;   // first-run: page only, no tab strip / address bar
    bool m_lowEnd = false;   // light mode for older PCs
    WINDOWPLACEMENT m_wp{};
    LONG m_style = 0;
    void ShowError(Tab* t, int status);
    void SweepBlocked(const std::string& host);
    std::set<std::wstring> m_certAllow, m_certBad;   // hosts: user said proceed / certificate was refused
    struct ErrPend { Tab* t; UINT64 nav; int st; }; std::vector<ErrPend> m_errPend;   // aborted loads wait a moment: a page that keeps redirecting is not an error
    Settings m_set;
    std::vector<HistoryItem> m_hist;
    std::vector<BookmarkItem> m_bm;
    std::vector<DownloadItem> m_dl;
    std::vector<std::wstring> m_closed;
    bool m_cleared = false;
    std::set<std::wstring> m_ckSeen, m_ckLegacy; bool m_ckInit = false, m_ckBusy = false, m_ckAgain = false, m_closing = false; int m_ckPending = 0;   // cookie sweeper (CookieSweep.inl)
    bool CkOn() const; void CkSeen(const std::wstring& url); void CkInit(ICoreWebView2* wv); void CkSweep(ICoreWebView2* wv, const std::wstring& next, bool closing); void CkSweepActive(bool closing); void CkWait(DWORD ms);
    // Auto suggest (Suggest.inl)
    bool SgOn() const; void InstallSgScript(Tab* t); void SgSetting(); const std::vector<SgPair>& SgPairs(); void OnSiteSgMessage(Tab* t, const std::wstring& src, const std::wstring& msg); void ShowSg(); void HideSg(); void SgTick(); void SgClick(int what); void SgInsert(int i); void PaintSg(HDC hdc); int SgRowAt(POINT p) const; static LRESULT CALLBACK SgProc(HWND h, UINT msg, WPARAM wp, LPARAM lp);
    HWND m_hSg = nullptr; std::vector<SgHit> m_sgHits; SgField m_sgField; Tab* m_sgTab = nullptr; std::wstring m_sgOrigin; int m_sgHot = -1, m_sgTestClick = 0; DWORD m_sgShown = 0, m_sgRate[2] = { 0, 0 }; RECT m_sgBox{}, m_sgWv{}; std::vector<SgPair> m_sgPairs; ULONGLONG m_sgStamp = 0; bool m_sgLoaded = false;
    SpFeed m_sp; bool m_spBusy = false; bool SponsorOn() const; void SponsorLoad(); void SponsorKick(); void SponsorGot(void* r); void SponsorHide(const std::wstring& id); void SponsorHtml(std::wstring& tiles, std::wstring& bg);
    SnLedger m_sn; std::vector<SnLogLine> m_snLog; bool m_snBusy = false, m_snDirty = false;   // anonymous counts (Signals.inl)
    bool SnOn() const; void SnLoad(); void SnSave(); void SnLogAdd(const char* kind, int bytes, bool ok); void SnBlockedHit(const std::string& host); void SnEvent(const std::wstring& kind, const std::wstring& id); void SnKick(bool force = false); void SnGot(void* r); void SnClear(); void SnWipe(); void SnPush();   // sponsored content (Sponsor.inl)
    static bool         IsUrl(const std::wstring& s);

    // â”€â”€â”€ Members â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    HINSTANCE  m_hInst       = nullptr;
    HWND       m_hWnd        = nullptr;
    HWND       m_hOmnibox    = nullptr;
    HWND       m_hBtnBack    = nullptr;
    HWND       m_hBtnFwd     = nullptr;
    HWND       m_hBtnRefresh = nullptr;
    HWND       m_hBtnMenu    = nullptr;
    HWND       m_hBtnShield  = nullptr;
    HWND       m_hBtnAgent   = nullptr;
    HWND       m_hBtnShot    = nullptr;   // toolbar camera: screenshot
    WNDPROC    m_origOmniboxProc = nullptr;

    std::vector<std::unique_ptr<Tab>> m_tabs;
    int        m_activeTab   = -1;
    int        m_hoveredTab  = -1;
    bool       m_incognito   = false;
    std::wstring m_privDir;          // private window: throw-away profile folder
public:
    void NewPrivateWindow();
    void NewNormalWindow();
    void ApplyTheme();
    void HideFromRecall(HWND w);
    void PruneHistory();
private:
    bool       m_adBlock     = false;   // local blocking OFF by default; tracker blocking is done by the DoH server
    bool       m_imagesOff   = false;
    UINT       m_dpi         = 96;

    // WebView2 environment (shared across tabs in this window)
    wil::com_ptr<ICoreWebView2Environment> m_env;
    bool       m_envReady    = false;

    AdBlocker  m_adBlocker;

    // GDI resources
    HFONT      m_fontUI      = nullptr;
    HFONT      m_fontSmall   = nullptr;
    HBRUSH     m_brToolbar   = nullptr;
    HBRUSH     m_brTabActive = nullptr;
    HBRUSH     m_brTabInact  = nullptr;
    HBRUSH     m_brSurface   = nullptr;
    HPEN       m_penBorder   = nullptr;
    HPEN       m_penAccent   = nullptr;

    // Progress animation
    float      m_displayProgress = 0.f;
    bool       m_progressVisible  = false;

    // Per-DPI layout values (computed on first paint / resize)
    int        m_tabH = 38;
    int        m_toolH = 52;
};
