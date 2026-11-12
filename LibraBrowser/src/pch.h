#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <uxtheme.h>

#include <wrl.h>
#include <wrl/client.h>
#include <wrl/event.h>
#include <wil/com.h>
#include <wil/resource.h>
#include "WebView2.h"

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <unordered_set>
#include <unordered_map>
#include <map>
#include <algorithm>
#include <cassert>
#include <sstream>

using Microsoft::WRL::ComPtr;
using Microsoft::WRL::Callback;
