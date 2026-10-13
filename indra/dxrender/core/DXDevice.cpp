#include "DXDevice.h"
#include "llerror.h"
#include "llstring.h"
#include <dxgi.h>
#include <dxgi1_2.h>   // IDXGIFactory1 / IDXGIAdapter1 / DXGI_ADAPTER_DESC1
#include <vector>
#include <string>
#include <cstdint>

DXDevice gDXDevice;

// S24 (2026-08-26, task #260): shared by both initialize() branches below -
// QI's mDevice for IDXGIDevice to find which physical adapter it landed on,
// regardless of whether the device was adopted or created here.
static LUID queryAdapterLuid(ID3D11Device* device)
{
    LUID luid = { 0, 0 };
    IDXGIDevice* dxgi_device = nullptr;
    if (SUCCEEDED(device->QueryInterface(__uuidof(IDXGIDevice), (void**)&dxgi_device)) && dxgi_device)
    {
        IDXGIAdapter* adapter = nullptr;
        if (SUCCEEDED(dxgi_device->GetAdapter(&adapter)) && adapter)
        {
            DXGI_ADAPTER_DESC desc;
            if (SUCCEEDED(adapter->GetDesc(&desc)))
            {
                luid = desc.AdapterLuid;
            }
            adapter->Release();
        }
        dxgi_device->Release();
    }
    return luid;
}

// S24 (2026-08-16): see DXDevice.h's header comment.
//
// Default is now FALSE, where it used to be true. The validation layer is a
// development aid: the file's own note above says it carries "a real per-draw-
// call performance cost", and it makes startup depend on the Windows "Graphics
// Tools" optional feature being installed. A shipped viewer should not pay that
// cost, and should not refuse to start on a machine that never installed it.
// initialize() retries without the flag if creation fails, so turning it on
// still gets you a device on such a machine - you just lose the validation
// messages. Set S24DXDebugLayerEnabled to bring it back.
bool DXDevice::sDebugLayerEnabled = false;

bool DXDevice::initialize(ID3D11Device* existing_device, ID3D11DeviceContext* existing_context)
{
    if (mDevice)
    {
        // already initialized
        return true;
    }

    if (existing_device && existing_context)
    {
        mDevice = existing_device;
        mContext = existing_context;
        mDevice->AddRef();
        mContext->AddRef();
        mFeatureLevel = mDevice->GetFeatureLevel();
        mDevice->QueryInterface(__uuidof(ID3D11InfoQueue), (void**)&mInfoQueue);
        mAdapterLuid = queryAdapterLuid(mDevice);
        return true;
    }

    // No device to adopt (e.g. a single-adapter system, where
    // selectHighPerformanceAdapter() skips device creation entirely) -
    // create our own.
    D3D_FEATURE_LEVEL requested_levels[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0 };
    // S24 (2026-08-16): now gated by S24DXDebugLayerEnabled (see
    // sDebugLayerEnabled's header comment) instead of being hardcoded on -
    // enables the runtime validation layer so state/binding hazards surface
    // as real debug-output messages instead of silent wrong pixels, at a
    // real per-draw-call performance cost. Requires the Windows "Graphics
    // Tools" optional feature installed - if missing, this call fails
    // outright with the flag on (turn the setting off if that happens and
    // this isn't the path being investigated).
    // S24 (2026-08-29, task #278): D3D11_CREATE_DEVICE_SINGLETHREADED added -
    // this codebase confirmed (source review) to never call into the D3D11
    // device/context from any thread but the main one since task #260
    // removed the one feature (DXImageThread) that ever did. This flag tells
    // the driver to skip its internal per-call thread-safety locking
    // entirely rather than just happening to go uncontended - a real,
    // measurable per-API-call CPU cost reduction across every single D3D11
    // call this app makes. Must match at every D3D11CreateDevice call site
    // (see llwindowwin32.cpp's selectHighPerformanceAdapter(), the other two
    // sites, for the multi-adapter path) - a mismatched flag between sites
    // would be meaningless since only one of them actually creates the
    // device DXDevice::initialize() ends up adopting or owning.
    const UINT base_flags = D3D11_CREATE_DEVICE_SINGLETHREADED;
    const UINT requested_flags = base_flags | (sDebugLayerEnabled ? D3D11_CREATE_DEVICE_DEBUG : 0);

    HRESULT hr = D3D11CreateDevice(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        requested_flags,
        requested_levels,
        _countof(requested_levels),
        D3D11_SDK_VERSION,
        &mDevice,
        &mFeatureLevel,
        &mContext);

    // Retry without D3D11_CREATE_DEVICE_DEBUG. The debug layer is only present
    // when the "Graphics Tools" optional feature is installed, and asking for a
    // device without it fails outright rather than silently degrading - so a
    // machine that never installed the feature would otherwise be unable to run
    // this viewer at all. Only pays the second call when the flag was set.
    if (FAILED(hr) && (requested_flags & D3D11_CREATE_DEVICE_DEBUG))
    {
        LL_INFOS("DXRender") << "D3D11CreateDevice failed with the debug layer requested "
            << "(hr=0x" << std::hex << (unsigned long)hr << std::dec
            << "); retrying without it - is the Graphics Tools optional feature installed?" << LL_ENDL;

        mDevice = nullptr;
        mContext = nullptr;

        hr = D3D11CreateDevice(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            base_flags,
            requested_levels,
            _countof(requested_levels),
            D3D11_SDK_VERSION,
            &mDevice,
            &mFeatureLevel,
            &mContext);
    }

    if (FAILED(hr))
    {
        // Name the adapter that actually answered. DXGI_ERROR_UNSUPPORTED
        // (0x887A002D) almost always means "this adapter has no Direct3D 11
        // support at all", which is what a virtual display adapter looks like -
        // a null pAdapter binds to the first adapter IDXGIFactory1 enumerates,
        // and on a machine exposing one (e.g. a "USB Mobile Monitor Virtual
        // Display") that can be the wrong one. Naming it turns the next log
        // from a guess into a diagnosis.
        LL_WARNS("DXRender") << "D3D11CreateDevice failed, hr=0x" << std::hex << (unsigned long)hr << std::dec
            << " (debug layer " << (sDebugLayerEnabled ? "requested" : "off")
            << ", default adapter)" << LL_ENDL;

        IDXGIFactory1* factory = nullptr;
        if (SUCCEEDED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)&factory)) && factory)
        {
            IDXGIAdapter1* adapter = nullptr;
            for (UINT i = 0; factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i)
            {
                DXGI_ADAPTER_DESC1 desc;
                if (SUCCEEDED(adapter->GetDesc1(&desc)))
                {
                    const bool software = (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0;
                    LL_WARNS("DXRender") << "  adapter " << i << ": "
                        << ll_convert_wide_to_string(std::wstring(desc.Description))
                        << " (vendor 0x" << std::hex << desc.VendorId << std::dec
                        << ", device 0x" << std::hex << desc.DeviceId << std::dec
                        << ", VRAM " << (desc.DedicatedVideoMemory / 1024 / 1024) << " MB"
                        << ", flags 0x" << std::hex << desc.Flags << std::dec
                        << (software ? ", SOFTWARE" : ", hardware") << ")" << LL_ENDL;
                }
                adapter->Release();
                adapter = nullptr;
            }
            factory->Release();
        }

        mDevice = nullptr;
        mContext = nullptr;
        return false;
    }

    mDevice->QueryInterface(__uuidof(ID3D11InfoQueue), (void**)&mInfoQueue);
    mAdapterLuid = queryAdapterLuid(mDevice);

    return true;
}

void DXDevice::shutdown()
{
    // S24 (DX_RENDER diagnostic, 2026-07-28): TEMPORARY - logPendingDebugMessages()
    // only logs each distinct D3D11 message ID once (see its own comment) to
    // avoid the per-frame I/O cost of logging every occurrence - this final
    // tally is the cheap way to still see how often each one actually fired
    // over the whole session, without paying for it at runtime.
    for (const auto& entry : mSeenMessageIDs)
    {
        // entry.first is now "messageID|context" (see logPendingDebugMessages()'s
        // header comment for why context was added to the dedup key).
        LL_WARNS("DXDebugLayer") << "final tally: message id|context=" << entry.first
            << " occurred " << entry.second << " times this session" << LL_ENDL;
    }

    if (mInfoQueue) { mInfoQueue->Release(); mInfoQueue = nullptr; }
    if (mContext) { mContext->Release(); mContext = nullptr; }
    if (mDevice) { mDevice->Release(); mDevice = nullptr; }
}

int DXDevice::logPendingDebugMessages(const char* context)
{
    if (!mInfoQueue)
    {
        return 0;
    }

    UINT64 num_messages = mInfoQueue->GetNumStoredMessages();
    int newly_logged = 0;
    for (UINT64 i = 0; i < num_messages; ++i)
    {
        SIZE_T msg_len = 0;
        if (FAILED(mInfoQueue->GetMessage(i, nullptr, &msg_len)) || msg_len == 0)
        {
            continue;
        }

        std::vector<uint8_t> buffer(msg_len);
        D3D11_MESSAGE* msg = (D3D11_MESSAGE*)buffer.data();
        if (SUCCEEDED(mInfoQueue->GetMessage(i, msg, &msg_len)))
        {
            // S24 (2026-08-02): keyed by (message ID, context) together, not
            // message ID alone - see this method's header comment. Without
            // this, a genuinely different shader hitting the exact same
            // D3D11 validation rule (same numeric ID) as an earlier-seen
            // one would never be reported at all.
            const std::string key = std::to_string((int)msg->ID) + "|" + (context ? context : "");
            uint64_t& seen_count = mSeenMessageIDs[key];
            ++seen_count;
            // Only the FIRST time this distinct message ID is ever seen this
            // process does it actually hit LL_WARNS (real disk I/O) - every
            // repeat just increments the counter above. See this method's
            // header comment for why logging every single occurrence was a
            // real, self-inflicted performance problem.
            if (seen_count == 1)
            {
                LL_WARNS("DXDebugLayer") << "[D3D11 sev=" << (int)msg->Severity
                    << " id=" << (int)msg->ID << "] (first occurrence, will not repeat per-instance) "
                    << (context ? (std::string("context='") + context + "' ") : std::string())
                    << std::string(msg->pDescription, msg->DescriptionByteLength > 0 ? msg->DescriptionByteLength - 1 : 0)
                    << LL_ENDL;
                ++newly_logged;
            }
        }
    }

    mInfoQueue->ClearStoredMessages();
    return newly_logged;
}
