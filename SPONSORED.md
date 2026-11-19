# Sponsored content

The sponsored-content implementation is present but disabled in the public 3.9.8 build. It shows no sponsored tiles or background picture and has no network activity until a publisher-controlled feed address and public signing key are verified. Each item can be hidden for 30 days with its Hide button. It is never shown in private windows, with Child Safety on, or during first run. Every item carries the word "Sponsored".

## What the browser does and does not do
- In a build with a verified feed configured, it can fetch one signed feed with a plain `GET`, through Libra's own DNS-over-HTTPS proxy. No cookies, no redirects, no identifier, nothing about the user or the pages visited is sent.
- The feed is accepted only if its ECDSA P-256 / SHA-256 signature matches a public key built into the browser, it has not expired and its counter (`seq`) did not go back. Otherwise it is ignored.
- The browser chooses what to show on the PC (region comes from the Windows region setting and is never sent). The feed is the same file for everybody, so nothing is sent to pick an ad.
- A tile opens the advertiser's `https` address directly, plus one constant tag `utm_source=libra` that is the same for every user. Nothing goes through our server when you click. The address must be a plain `https` host name: no login part, no port, no odd characters (`SpGoOk` in `Sponsor.h`).
- No ad network, no tracking pixels, no advertiser scripts: the browser draws plain tiles itself.

## Anonymous counts (what would be sent back if enabled)
See [SENDS.md](SENDS.md). In short: how often each item was shown and clicked, and two rounded totals of blocked ads and trackers, in one fixed-size report once a day. No identifier. The user can read the next report on the Privacy page and switch the counts off (the ads stay).

## Turned off in this source
`LibraBrowser/src/Sponsor.inl`, function `SpConfig()`, holds the feed address and two public-key slots (`kid`, base64url `x`, `y`). Both keys are empty in this source, so the browser fetches nothing, shows nothing and sends nothing. A fork or a later release switches the feature on by filling in its own feed address and public key, then rebuilding.

## Feed format (for people running their own feed)
The envelope is `{"kid", "payload", "sig"}`. `payload` is the exact JSON text that was signed; `sig` is the raw 64-byte `r||s` signature, base64url. The payload carries `seq`, `issued`, `expires`, `disabled` and a list of items (`id`, `kind`, `title`, `advertiser`, `dest`, `go`, `img`, `start`, `end`, `weight`, `countries`). `go` is the advertiser's https address. Pictures must live on the feed's own host. The exact checks are in `LibraBrowser/src/Sponsor.h` (`SpParse`).

The signing server and the private key are not part of this repository. Never put a private key in the source.
