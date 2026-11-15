# Third-party notices

Libra Browser itself is under the Libra Source-Available License (see `LICENSE`). It builds on the following; each keeps its own license.

| Component | Used for | License |
|---|---|---|
| Microsoft Edge WebView2 SDK (`Microsoft.Web.WebView2` NuGet, 1.0.4314.35) | The web engine API (Chromium) | Microsoft license shipped inside the NuGet package |
| Microsoft Edge WebView2 Runtime | The Chromium engine that renders pages. Installed by Windows / Microsoft, **not redistributed by Libra** | Microsoft Software License Terms |
| Windows Implementation Libraries (`Microsoft.Windows.ImplementationLibrary` NuGet, 1.0.240803.1) | COM smart pointers (`wil::com_ptr`) | MIT |
| Windows system APIs (Win32, GDI+, WinHTTP, DPAPI, BCrypt, DWM) | Window, drawing, networking, vault encryption | Part of Windows |
| Segoe UI / Segoe Fluent Icons | UI text and icons, loaded from the user's Windows | Microsoft system fonts, not bundled |
| NuGet command line (`nuget.exe`) | Downloaded by `build.bat` at build time, not stored in this repo | Apache-2.0 |

## Trademarks and artwork
- Search-engine names and small logos (Google, DuckDuckGo, Bing, Brave, Startpage, Ecosia, Perplexity) belong to their owners and are used only to identify the engine in the picker.
- The Libra name, icon and the animated first-run videos were created for this project by Psylinks Security Private Limited. The videos were produced with AI video tools; they are part of this repository and covered by the project license.

## Services Libra can contact (all visible in Settings)
| Service | Why | Can it be turned off? |
|---|---|---|
| `dns.psylinks-beta.com` (DNS-over-HTTPS) | Resolves every site name and applies the ad/tracker blocklist | It is the core of "cloud ad blocking" (Settings: blocking level Off uses system DNS per site) |
| Your search engine's suggestion service | Address-bar and new-tab suggestions | Yes: Settings > Privacy and speed > Search suggestions |
| Microsoft SmartScreen | Warns about phishing/malware sites (sends site addresses to Microsoft) | Yes: Settings |
| Microsoft WebView2 runtime background connection | Engine updates and diagnostics, controlled by Windows, outside Libra's proxy | Only via Windows or enterprise policy |

- Lucide icons (https://lucide.dev), ISC licence: assistant panel icons (sparkles, square-pen, book-open, settings, x, chevron-left, arrow-up, square).

- whisper.cpp (MIT, https://github.com/ggml-org/whisper.cpp) and the OpenAI Whisper base model weights (MIT): optional offline voice input; downloaded on request, not bundled.
