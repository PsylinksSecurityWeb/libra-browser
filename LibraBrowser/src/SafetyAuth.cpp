// Windows sign-in check (password, PIN or Windows Hello) through the system's own window.
// Libra never sees the password: Windows only answers verified / not.
#include <windows.h>
#include <unknwn.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Security.Credentials.UI.h>
#include <UserConsentVerifierInterop.h>
using namespace winrt::Windows::Security::Credentials::UI;

// 0 verified, 1 no Windows lock set up on this account (nothing to ask), 2 wrong / cancelled / blocked
int WinAuthBlocking(HWND owner, const wchar_t* msg) {
    try {
        winrt::init_apartment(winrt::apartment_type::multi_threaded);
        auto interop = winrt::get_activation_factory<UserConsentVerifier, IUserConsentVerifierInterop>();
        winrt::Windows::Foundation::IAsyncOperation<UserConsentVerificationResult> op{ nullptr };
        winrt::hstring m = msg;
        winrt::check_hresult(interop->RequestVerificationForWindowAsync(owner, (HSTRING)winrt::get_abi(m),
            winrt::guid_of<winrt::Windows::Foundation::IAsyncOperation<UserConsentVerificationResult>>(), winrt::put_abi(op)));
        auto r = op.get();
        if (r == UserConsentVerificationResult::Verified) return 0;
        if (r == UserConsentVerificationResult::DeviceNotPresent || r == UserConsentVerificationResult::NotConfiguredForUser) return 1;
        return 2;
    } catch (...) { return 2; }   // any trouble: stay locked (fail safe)
}
