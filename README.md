<p align="center"><img src="docs/icon.png" width="120" alt="Libra Browser icon"></p>

<h1 align="center">Libra Browser</h1>
<p align="center"><b>Secure, private and lightweight. A Windows browser that blocks ads and trackers before they ever load.</b><br>
No account. No sync. No telemetry in Libra's own code. Built by <b>Psylinks Security Private Limited</b>, an Indian company. Version 3.2.0 beta for Windows 10/11 (64-bit).</p>


<p align="center"><a href="https://github.com/PsylinksSecurityWeb/libra-browser/releases/tag/v3.2.0-beta">Download Libra Browser 3.2.0 beta</a> · <a href="https://github.com/PsylinksSecurityWeb/libra-browser">Source code</a> · <a href="https://github.com/PsylinksSecurityWeb/libra-browser/issues">Issues</a> · <a href="https://github.com/PsylinksSecurityWeb/libra-browser/security/advisories/new">Report a vulnerability</a></p>

> **License:** source available, not open source. You may read and audit the code; you may not modify, share or sell it without written permission. See [LICENSE](LICENSE).

## Official download: Libra Browser 3.2.0 beta for Windows

[Download the official Libra Browser 3.2.0 beta installer](https://github.com/PsylinksSecurityWeb/libra-browser/releases/download/v3.2.0-beta/LibraBrowser-Setup-3.2.0.exe) (`LibraBrowser-Setup-3.2.0.exe`). This is the official Psylinks Security release package for Windows 10/11 (64-bit); it installs per-user and does not require administrator rights. The beta is not code-signed yet, so Windows SmartScreen may show “Windows protected your PC”. If you downloaded it from this GitHub release, select **More info → Run anyway** only after checking the checksum below.

**Verify the original release before installing (PowerShell):**

```powershell
(Get-FileHash .\LibraBrowser-Setup-3.2.0.exe -Algorithm SHA256).Hash
```

Expected SHA-256:

```text
d8639238cb886580af2b31042564aee6bfb8e880c8abe4de29b648f8e1f1e5a3
```

The checksum, filename, release tag, and source repository together provide a practical way to confirm that you have the intended official beta installer rather than a renamed or modified copy.

## Why Libra
- **Blocks ads and trackers at the DNS layer.** Every site name is looked up through our own DNS-over-HTTPS server, so trackers are refused *before* a connection exists. Less data, less CPU, faster pages.
- **Three levels per site:** Off, Balanced, Max, one tap from the shield icon. Fix a broken site without turning protection off everywhere.
- **Built for old and small PCs.** One small `.exe` on top of the Edge WebView2 runtime that Windows already uses, tab sleeping, a low-end mode, and a first-run animation that skips itself on weak machines.
- **Private by design.** No account, no cloud sync, no analytics, no ads. Your history, bookmarks and passwords never leave the PC.
- **Security defaults that matter:** popups need a real click, downloads started by a page without a click are refused, permissions default to *ask*, motion sensors are always blocked, https upgrade, fingerprint protection, Mark-of-the-Web on downloads.
- **Clean, flat, calm UI.** White canvas, no shadows, no clutter, no promotions.

## Screenshots

A quick look at Libra's calm, lightweight interface, private search setup, everyday browsing, and live protection report.

| Welcome | Choose your search | New-tab page |
|---|---|---|
| ![Libra Browser welcome screen](docs/screenshots/welcome.png) | ![Libra Browser search-engine selection](docs/screenshots/search-engines.png) | ![Libra Browser new-tab page](docs/screenshots/new-tab.png) |

| Everyday browsing | Privacy report | Settings |
|---|---|---|
| ![Libra Browser browsing view](docs/screenshots/browsing.png) | ![Libra Browser privacy report](docs/screenshots/privacy-report.png) | ![Libra Browser settings](docs/screenshots/settings.png) |

## Features
**Protection**
- Cloud ad and tracker blocking (DoH + blocklist), per-site Off / Balanced / Max, live counters and a 7-day protection report
- Empty ad-slot cleaner (removes the blank gaps ads leave behind)
- HTTPS upgrade with a clear "no secure connection" page; certificate errors cannot be bypassed silently
- Fingerprint protection (canvas, audio, hardware hints) that changes per site and per launch
- Optional third-party cookie blocking, autoplay blocking, popup blocker, drive-by download guard
- Microsoft SmartScreen on by default (switchable); permissions default to *ask*
- Private windows that leave no history, cookies or saved passwords

**Everyday**
- Address-bar and new-tab search suggestions from your chosen engine (Google, DuckDuckGo, Bing, Brave, Startpage, Ecosia, Perplexity, Brave Ask), plus local history and bookmark matches
- Editable home shortcuts (rename, change address, remove)
- Password vault (Windows DPAPI) with a strong password generator and tap-to-fill
- Fast downloads: segmented multi-connection, resume after failure, live speed
- "Save videos" helper and right-click save for images, links and PDFs (off by default)
- History, Bookmarks, Downloads, Tabs manager, pinned tabs, Light mode, set as default browser

## How Libra is different
| | **Libra** | Chrome | Edge | Brave | Firefox | AVG Secure Browser |
|---|---|---|---|---|---|---|
| Engine | Chromium (WebView2) | Chromium | Chromium | Chromium | Gecko | Chromium |
| Ad/tracker blocking built in | Yes, DNS-level, 3 levels per site | No (extensions) | Tracking prevention | Yes (Shields) | Yes (Enhanced Tracking Protection) | Yes (ad blocker) |
| Account / sync needed | None, no sync at all | Optional | Optional | Optional | Optional | Not required |
| Telemetry in the app's own code | None | Yes, can be limited | Yes, can be limited | Minimal, can be limited | Yes, can be limited | See vendor policy |
| Source code | Source available (read-only) | Chromium open, Chrome closed | Closed | Open (MPL-2.0) | Open (MPL-2.0) | Closed |
| Extensions | No (by design) | Yes | Yes | Yes | Yes | Yes |
| Platforms | Windows 10/11 | All | All | All | All | Windows, macOS, mobile |
| Antivirus included | No | No | No | No | No | Sold alongside AVG products |

**Honest trade-offs.** Libra is young and focused. It has no extensions, no sync and no mobile or macOS version. It is not an antivirus: it relies on SmartScreen and Windows Defender for file scanning. Because it runs on Microsoft's WebView2, engine security updates come through the WebView2 runtime, and that runtime keeps a background connection to Microsoft that Libra cannot route (see [SECURITY-AUDIT.md](SECURITY-AUDIT.md)).

**Where Libra wins:** a "security browser" you can *read*; protection that works before connections are made (so it also saves bandwidth and CPU); per-site control; and a deliberately small surface: nothing to sign in to, nothing to sync, nothing to upsell.

*Competitor cells describe publicly documented features as we understand them; please check each vendor for current details.*

## What Libra stores and contacts
| Where | What |
|---|---|
| `%LOCALAPPDATA%\LibraBrowser\` | settings, history, bookmarks, pinned tabs, DNS cache, home tiles, statistics, encrypted password vault, WebView2 profile (`UserData`). Private windows write none of this |
| Network: `dns.psylinks-beta.com` | DNS-over-HTTPS lookups (the blocking brain) |
| Network: your search engine | Search suggestions while typing (can be switched off) |
| Network: Microsoft | SmartScreen (switchable) and the WebView2 runtime's own background traffic (outside Libra's control) |
| Registry (HKCU only) | Only when you press *Set as default browser* |
| Permissions | Runs as a normal user, no admin rights. The local proxy listens on `127.0.0.1` only. Camera, mic and location default to *ask* |

The full list of services and licenses is in [THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md).

## Developer guide
**Requirements:** Windows 10/11, Visual Studio Build Tools (MSVC + Windows SDK), and the Edge WebView2 Runtime. The NuGet packages (WebView2 SDK, WIL) are restored automatically.

**Build**
```bat
git clone https://github.com/PsylinksSecurityWeb/libra-browser.git
cd libra-browser/LibraBrowser
build.bat
```
Or open `LibraBrowser/LibraBrowser.vcxproj` and build **Release | x64**. Output: `LibraBrowser/x64/Release/LibraBrowser.exe` (one exe).

**Run and debug**
- Start the exe normally. For automation, launch with the environment variable `WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS=--remote-debugging-port=9333`, then drive it with the Chrome DevTools Protocol.
- Automated tests are kept out of this public copy. Write your own against the debug port (Chrome DevTools Protocol).
- Internal pages (`libra://settings`, `history`, `privacy`, ...) are HTML strings built in C++ (`Pages.h`); open them from the address bar. Buttons use `data-c` / `data-a` and post a message handled in `OnPageMessage`.
- Settings are plain `key=value` lines in `settings.ini`.

**Code map (`LibraBrowser/src`)**
| File | Role |
|---|---|
| `main.cpp` | Entry point, single instance, command line (`--register-default`), URL hand-off |
| `BrowserWindow.cpp/.h` | Window, tabs, toolbar, WebView2 events, downloads hook-up, error pages, shield, suggestions |
| `Pages.h`, `PrivPage.h`, `PwPage.h` | All `libra://` pages and the first-run flow |
| `DohProxy.h` | Loopback HTTP/CONNECT proxy; every name resolved only via DoH; blocklist; rebinding protection |
| `AdBlocker.cpp/.h` | Local ad and tracker rules |
| `Downloader.h` | Segmented, resumable downloads; Mark-of-the-Web |
| `Vault.h`, `PwJs.h`, `PwSite.h`, `Passwords.inl` | Password vault (DPAPI), generator, autofill |
| `Fp.inl` | Fingerprint protection script |
| `Video.inl` | "Save videos" and right-click save |
| `Privacy.inl` | Clear data, default-browser registration |
| `Data.h` | Settings, search engines, history and bookmark types |
| `HeroAssets.h` | Generated: the first-run animation videos (byte arrays) |

**Add a search engine:** add a row to `kEngines` in `Data.h`; optionally add its suggestion URL in `SugEndpoint` in `BrowserWindow.cpp`.

**Conventions:** keep it small and light; animate transform and opacity only; no new network contact without a visible switch in Settings; never add telemetry.

## Roadmap
Settings groups and search, sensor and permission notices, offline spelling help, offline voice search, upstream proxy support for VPN and proxy users, code signing.

## FAQ
**Is it open source?** It is *source available*. The code is public to read, but modifying, redistributing or selling it needs written permission.

**Why not an extension like uBlock Origin?** Libra blocks one level lower (DNS), needs no extension and uses less memory. Libra has no extension system by design.

**Does it work on Windows 7 or XP?** No. WebView2 requires Windows 10 or newer.

**Does it collect my data?** Libra's own code does not. See the table above for the optional services it can contact.

## Security
Report vulnerabilities privately through [GitHub security advisories](https://github.com/PsylinksSecurityWeb/libra-browser/security/advisories/new) or see [SECURITY.md](SECURITY.md). Audit notes: [SECURITY-AUDIT.md](SECURITY-AUDIT.md).

---
<sub>Built by Psylinks Security Private Limited (India). Other names are trademarks of their owners.</sub>
