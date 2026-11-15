# Sponsored content (optional, off by default in this source)

Libra Browser can show up to 2 labelled sponsored tiles and one background picture on the new-tab page. It is built so a user can see, check and switch it off.

## What it does and does not do
- One anonymous `GET` of a signed feed per day, through Libra's own DNS-over-HTTPS proxy. No cookies, no redirects, no identifier, nothing about the user or the pages visited is sent.
- The feed is accepted only if its ECDSA P-256 / SHA-256 signature matches a public key built into the browser, it has not expired and its counter (`seq`) did not go back. Otherwise it is ignored.
- Every click link and picture must live on the feed's own host (`https://<feed host>/c/<id>` for clicks, `https://<feed host>/img/...` for pictures), so a signed feed cannot point the tile click at a third-party tracker. The feed host then redirects to the advertiser.
- Hidden in private windows, with Child Safety on, during first run, and whenever the user turns it off (Settings > Privacy). Sponsored items are always labelled.
- No ad network, no tracking pixels, no scripts from advertisers: the browser draws plain tiles itself.

## Turned off in a fresh build
`LibraBrowser/src/Sponsor.inl`, function `SpConfig()`, holds two things:
1. `c.feed`: the https address of the feed.
2. `owner[2]`: two public-key slots (`kid`, base64url `x`, base64url `y`). Both are empty in this source.

With empty keys the browser fetches nothing and shows nothing. A fork or a later release switches the feature on by filling in its own feed address and public key, then rebuilding.

## Feed format (for people running their own feed)
The envelope is `{"kid", "payload", "sig"}`. `payload` is the exact JSON text that was signed; `sig` is the raw 64-byte `r||s` signature, base64url. The payload carries `seq`, `issued`, `expires`, `disabled` and a list of items (`id`, `kind`, `title`, `advertiser`, `dest`, `click`, `img`, `start`, `end`, `weight`, `countries`). The exact checks are in `LibraBrowser/src/Sponsor.h` (`SpParse`).

The signing server and the private key are not part of this repository. Never put a private key in the source.
