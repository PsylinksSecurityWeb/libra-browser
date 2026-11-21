// Libra Browser's own filter list for ads, interstitials and "disable your ad blocker" walls that live on a site's own server or page.
// Short on purpose: each line is specific (a file name or a class name that only ads use), never a bare common word. Max mode only.
// Syntax: see Filters.h. A user's own list file (setting filter_list) is read in addition.
#pragma once
static const char kOwnFilters[] = R"flt(
! ad frames and pop-ups served from the site itself
/adframe.
/ad_frame.
/ad-frame.
/adiframe.
/ad_iframe.
/ad-iframe.
/ads_iframe.
/ads-iframe.
/ads/iframe.
/ad-popup.
/ad_popup.
/adpopup.
/popunder.
/pop-under.
/popads.
! interstitials
/interstitial-ad
/interstitial_ad
/interstitialad
/ad-interstitial
/ad_interstitial
/adinterstitial
/google_vignette
! ad scripts and their settings
/ad-loader.
/adloader.
/ad_loader.
/adsconfig.
/ads-config.
/adscript.
/ad-script.
! scripts that only exist to detect an ad blocker and show a wall
/adblock-detect
/adblockdetect
/adblock_detect
/blockadblock
/fuckadblock
/adblock-notify
/adblock-notice
/adblocker-detect
/anti-adblock
/antiadblock
! sponsored banners
/sponsored-banner.
/sponsor-banner.
! boxes that only ads and ad-block walls use
##.interstitial-ad
##.interstitial_ad
##.ad-interstitial
##.ad_interstitial
##.adblock-overlay
##.adblock-modal
##.adblock-notice
##.adblock-warning
##.adblock-detected
##.adblock-wall
##.anti-adblock
##.antiadblock
##.ab-detected
##.adb-overlay
##.adb-modal
##.adblocker-overlay
##.adblocker-notice
##.adblocker-warning
##.adblocker-modal
##[id^="google_vignette"]
##[id*="adblock-overlay"]
##[id*="adblock-notice"]
##[id*="adblock-modal"]
##[id*="adblocker-overlay"]
##[id*="adblocker-notice"]
)flt";
