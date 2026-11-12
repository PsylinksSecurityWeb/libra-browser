# Libra Browser: security and privacy audit

## Privacy rule
Libra's own code collects nothing and has no telemetry, accounts or analytics. Optional, switchable network contacts: search suggestions go to the search engine you chose (Settings > Privacy and speed, on by default), and Microsoft SmartScreen if you leave it on.
Its main network contact is the DNS server `dns.psylinks-beta.com` (every site name is looked up there).
History, bookmarks, settings, pinned tabs and the DNS cache stay in `%LOCALAPPDATA%\LibraBrowser\` on the PC.

## Fixed in this audit
| Risk | Fix |
|---|---|
| DNS rebinding: a site name pointing at your own PC or router | Proxy refuses any public name that resolves to a private/loopback/link-local address |
| Downloads made by the download manager had no "from the internet" mark | Zone.Identifier (ZoneId=3, referrer, host) is written, so Windows SmartScreen / Office Protected View apply |
| Camera/mic/location granted silently to every site | Default is now ask. Auto-allow is opt-in, https-only, never remembered. Motion sensors always blocked |
| Start page contacted the search engine on every new tab (preconnect) | Removed |
| Browser-engine background networking / crash reports / sync | Switched off with engine flags |
| SmartScreen was switched off by a flag | Now on by default, with a Settings switch (it sends site addresses to Microsoft) |
| Injected markup in internal pages could load or send data | Content-Security-Policy on every internal page (no outside loads) |
| Download file names: path tricks, `:stream`, device names | Names are sanitized; more "never auto-open" file types |
| Server claiming a huge file to fill the disk | Disk-space check before reserving space |
| Runaway page opening endless proxy connections | Cap of 400 |
| No exploit mitigations in the exe | ASLR (high entropy), DEP, Control Flow Guard, CET |
| Pages getting native objects | Host objects disabled |

## Checked and fine
Pages cannot control the browser (messages only from Libra's own pages; a website tab was refused every command).
Titles/URLs are escaped (a history entry with a script tag stayed plain text).
Only http/https can be opened from the address bar; `javascript:`, `data:`, `file:` become searches.
Certificate errors cannot be bypassed silently (a Proceed button, per site, for this session only).

## Known limits (cannot be fixed inside Libra)
- WebView2 (Microsoft's engine) keeps one direct background connection to a Microsoft service that bypasses our proxy and DNS.
  Only Windows or enterprise policy can turn that off (not changed here: system setting).
- The proxy listens on 127.0.0.1 only, but any program of the same Windows user can use it.
- The exe is not code-signed.
- "Proceed anyway" on a bad certificate is the user's responsibility.
