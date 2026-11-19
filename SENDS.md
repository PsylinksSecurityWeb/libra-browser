# What Libra Browser sends to Psylinks servers

Libra Browser's own code sends nothing about you and keeps no account, id or cookie. The only requests to servers we run are listed here. In the public 3.9.8 build, sponsored-content requests and reports are disabled. The sponsored-content requests below only happen in builds that have a feed address and public key set (this source has none, see [SPONSORED.md](SPONSORED.md)).

| Request | When | What it contains |
|---|---|---|
| DNS lookups (`dns.psylinks-beta.com`, DNS-over-HTTPS) | when you open a site | the host name being looked up, padded to a multiple of 128 bytes, DNS id 0, no client-subnet option. No cookie, no account. |
| Signed sponsored list (`GET /v1/feed`) | disabled in the public 3.9.8 build | no request is made; a future verified build would send nothing identifying you. |
| Update check (`GET /v1/update`) | once a week | nothing from you. A small signed file that names the newest version. Nothing is installed unless you click Update and confirm; the installer must match the signed SHA-256. |
| PDF tools (`cdn.jsdelivr.net`) | once, only if you press Download in "Sign a PDF" | a plain download of three open-source files (pdf.js, pdf-lib). Nothing from you is sent. Your PDFs never leave your PC. |
| Count report (`POST /r`) | once a day, at a random moment, only if there is something to report | see below |

## The count report
One JSON body of exactly 2048 bytes (padded), for example:
```
{"v":1,"c":[{"id":"campaign-a","s":2,"k":1},{"id":"campaign-b","s":0,"k":0}],"b":{"a":120,"t":340},"pad":"xxxx..."}
```
- `c`: every active sponsored item in the feed, zeros included, so the report never shows which ones you saw. `s` = shown (a visible, foreground new-tab page for 1 second, at most 20), `k` = clicked by a real mouse click (never more than shown).
- `b`: blocked ads and blocked trackers since the last report, rounded to the nearest 10. No host names, ever.
- No date, version, language, region, operating system, id or cookie. Headers are fixed (generic user agent, no cookies, no referrer). A fresh connection is used for each report. The server stores only totals per day and campaign, never the report itself, and logging is switched off.
- Nothing is sent from private windows or Child Safety. Old counts are dropped after 3 days. A failed send is retried once, about an hour later.

## Check it yourself
- Privacy page > "What Libra sends" shows the exact next report and a log of the requests above. You can discard the counts or switch counting off.

## Honest limits
- Until a privacy relay is added, the network between you and our servers can see your IP address, as for any connection. We do not store it in the report.
- Advertisers see their own visitors after you click an ad, like any website visit.
- Counts are anonymous and therefore approximate; a script could send fake totals.

## Installing a site as an app (only when you click Install)
Libra Browser downloads that site's own icon once, from the site itself, through its DNS proxy and without cookies. Nothing is sent to Psylinks. The app list stays on your PC (encrypted).
