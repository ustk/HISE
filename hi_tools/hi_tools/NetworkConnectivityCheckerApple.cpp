/*  ===========================================================================
*
*   Network connectivity detection for macOS / iOS.
*   Based on code from SquarePine Modules (https://gitlab.com/squarepine/squarepinemodules)
*   Used under the MIT licence.
*
*   Uses the SystemConfiguration C API directly -- no Objective-C required.
*
*   ===========================================================================
*/

#if JUCE_MAC || JUCE_IOS

// SystemConfiguration drags in CoreServices/MacTypes.h, which defines a
// global 'Point' (and friends) that collides with juce::Point inside the
// hi_tools unity build. Use the same #define guard trick as the Accelerate
// include in hi_tools_01.cpp.
#define Point DummyPoint
#define Component DummyComponent
#define MemoryBlock DummyMB
#include <SystemConfiguration/SystemConfiguration.h>
#undef Point
#undef Component
#undef MemoryBlock

#include <netinet/in.h>

namespace hise {

NetworkConnectivityChecker::NetworkType getCurrentSystemNetworkType()
{
    struct sockaddr_in zeroAddress;
    zerostruct (zeroAddress);
    zeroAddress.sin_len    = sizeof (zeroAddress);
    zeroAddress.sin_family = AF_INET;

    auto* reachability = SCNetworkReachabilityCreateWithAddress (
        kCFAllocatorDefault, (const struct sockaddr*) &zeroAddress);

    if (reachability == nullptr)
        return NetworkConnectivityChecker::NetworkType::none;

    SCNetworkReachabilityFlags flags = 0;
    const bool gotFlags = SCNetworkReachabilityGetFlags (reachability, &flags);
    CFRelease (reachability);

    if (!gotFlags || !(flags & kSCNetworkReachabilityFlagsReachable))
        return NetworkConnectivityChecker::NetworkType::none;

    if (flags & kSCNetworkReachabilityFlagsConnectionRequired)
        return NetworkConnectivityChecker::NetworkType::none;

   #if JUCE_IOS
    if (flags & kSCNetworkReachabilityFlagsIsWWAN)
        return NetworkConnectivityChecker::NetworkType::mobile;
   #endif

    return NetworkConnectivityChecker::NetworkType::wifi;
}

double getCurrentSystemRSSI()
{
    return 1.0;
}

} // namespace hise

#endif // JUCE_MAC || JUCE_IOS
