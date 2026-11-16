# Libra Browser: security and privacy audit

## Privacy rule
Libra's own code collects nothing and has no telemetry, accounts or analytics. Optional, switchable network contacts: search suggestions go to the search engine you chose (Settings > Privacy, on by default), Microsoft SmartScreen if you leave it on, and, once a day, the signed list of sponsored items (Settings > Privacy > Show sponsored content; no cookies, no ID, no user data in the request).
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
- The proxy listens on 127.0.0.1 only and accepts connections only from Libra Browser's own process tree. Malware running as the same Windows user can still read anything that user can read.
- The exe is not code-signed.
- "Proceed anyway" on a bad certificate is the user's responsibility.

## Audit 2026-11-14 (version 3.9.2, beta)
- **Secrets:** the source, tests, docs and release files were searched for API-key patterns (OpenAI, Anthropic, Google, Groq, xAI, GitHub, bearer tokens). None found; the only match is an obviously fake test string. The user's API key lives only in an encrypted file (Windows DPAPI), is never shown, and is sent only to the provider the user chose. "Clear browsing data" never deletes it.
- **Device details:** the code never reads the computer name, user name, hardware ids, disk serials or network adapters. Sites see one generic Windows identity that changes daily. The compiler's build-folder text is removed from the release exes.
- **Test hooks:** the `LIBRA_TEST_*` environment variables used by our automated tests (auto-answering Agent questions, opening pages, picking files) now work only if a file named `libra-test.enabled` sits next to the exe. Release packages never contain it, so another program cannot use these variables to weaken a safety check.
- **Assistant closed = the AI sees nothing:** every command that reads or acts on a page sits behind one gate that only a task typed in the open panel can open; closing the panel by any route stops the task. Test: 0 model requests in 25 s of browsing, nothing added to the page.
- **Auto suggest (local, no AI):** designed from how Chrome, Firefox and Safari handle autofill: it opens only after a real click or key press (script focus, autofocus and made-up events are ignored), the popup is kept inside the page area (a forged position is refused), sensitive boxes and sign-in forms are never offered, the text is typed only after the user clicks Libra's own popup and only into the same still-empty box. The page script reports only the clicked box's label, name, type and position.
- **Clearing is verified:** every data kind is cleared with its own call whose result is read, cookies and open pages' storage are swept a second time, the numbers shown after a clear come from the engine, and a failure is reported instead of "Cleared". Tested after a hard kill, a normal close, a forced failure and each time range.
- **Ad paths are refused before the request:** a short reviewed list (no bare `/banners/` or single-word rules, so real pictures are not hit) in Max mode; checked on 10 large sites with zero false positives, and the real AdBlock Tester page scores 100/100.
- **Menu crash fixed:** an empty menu entry could crash the browser when the menu was opened.


## 3.9.2 release-scope review

The 3.9.2 additions were reviewed for encrypted backup handling, bookmark import path handling, PDF tool checksums and page isolation, reader-mode escaping, update-manifest verification, and anonymous-count behavior. The public candidate keeps the sponsored feed and count reporting disabled until a publisher-controlled endpoint and public signing keys are independently verified.
