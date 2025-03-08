#pragma once

#include <pch.h>
#include <Unknwn.h>
#include <Windows.h>
#include <winnt.h>
#include <filesystem>
#include <vector>
#include <mutex>

// ... [Keep existing GetDriverStore() implementation but consider adding error checking] ...

/* Improved COM-compliant implementation */
MIDL_INTERFACE("b58d6601-7401-4234-8180-6febfc0e484c")
IAmdExtFfxApi : public IUnknown
{
public:
    virtual HRESULT UpdateFfxApiProvider(void* pData, uint32_t dataSizeInBytes) = 0;
};

typedef HRESULT(STDMETHODCALLTYPE* PFN_UpdateFfxApiProvider)(void* pData, uint32_t dataSizeInBytes);

class AmdExtFfxApi : public IAmdExtFfxApi
{
private:
    std::mutex m_mutex;
    HMODULE m_hModule = nullptr;
    PFN_UpdateFfxApiProvider m_pfnUpdate = nullptr;
    ULONG m_refCount = 1;

public:
    virtual ~AmdExtFfxApi()
    {
        if (m_hModule) {
            FreeLibrary(m_hModule);
        }
    }

    HRESULT STDMETHODCALLTYPE UpdateFfxApiProvider(void* pData, uint32_t dataSizeInBytes) override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        LOG_INFO("UpdateFfxApiProvider called");

        if (!m_pfnUpdate && !InitializeFunctionPointer()) {
            return E_NOINTERFACE;
        }

        return m_pfnUpdate(pData, dataSizeInBytes);
    }

    // Proper COM implementation
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override
    {
        if (!ppvObject) return E_POINTER;
        
        if (IsEqualIID(riid, __uuidof(IAmdExtFfxApi)) {
            *ppvObject = static_cast<IAmdExtFfxApi*>(this);
        } else if (IsEqualIID(riid, IID_IUnknown)) {
            *ppvObject = static_cast<IUnknown*>(this);
        } else {
            *ppvObject = nullptr;
            return E_NOINTERFACE;
        }
        
        AddRef();
        return S_OK;
    }

    ULONG STDMETHODCALLTYPE AddRef() override
    {
        return InterlockedIncrement(&m_refCount);
    }

    ULONG STDMETHODCALLTYPE Release() override
    {
        ULONG ref = InterlockedDecrement(&m_refCount);
        if (ref == 0) {
            delete this;
        }
        return ref;
    }

private:
    bool InitializeFunctionPointer()
    {
        auto storePaths = GetDriverStore();
        
        // Try driver store paths first
        for (const auto& path : storePaths) {
            const auto dllPath = path / L"amdxcffx64.dll";
            m_hModule = LoadLibraryEx(dllPath.c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR);
            if (m_hModule) break;
        }

        // Fallback to default search
        if (!m_hModule) {
            m_hModule = LoadLibrary(L"amdxcffx64.dll");
        }

        if (!m_hModule) {
            LOG_ERROR("Failed to load amdxcffx64.dll");
            return false;
        }

        m_pfnUpdate = reinterpret_cast<PFN_UpdateFfxApiProvider>(
            GetProcAddress(m_hModule, "UpdateFfxApiProvider")
        );

        if (!m_pfnUpdate) {
            LOG_ERROR("Failed to get function pointer");
            FreeLibrary(m_hModule);
            m_hModule = nullptr;
            return false;
        }

        return true;
    }
};
