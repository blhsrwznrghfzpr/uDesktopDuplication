#include <d3d11.h>
#include <dxgi1_2.h>
#include <vector>
#include <string>
#include <memory>
#include <mutex>
#include <queue>

#include "IUnityInterface.h"
#include "IUnityGraphics.h"
#include "include/IUnityGraphicsD3D11.h"

#include "Common.h"
#include "Debug.h"
#include "Monitor.h"
#include "Duplicator.h"
#include "Cursor.h"
#include "MonitorManager.h"

#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "Shcore.lib")


IUnityInterfaces* g_unity = nullptr;
std::unique_ptr<MonitorManager> g_manager;
std::queue<Message> g_messages;
std::mutex g_managerMutex;
std::mutex g_messageMutex;


extern "C"
{
    UNITY_INTERFACE_EXPORT bool UNITY_INTERFACE_API IsInitialized()
    {
        std::lock_guard<std::mutex> lock(g_managerMutex);
        return g_unity && g_manager;
    }

    UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API Initialize()
    {
        std::lock_guard<std::mutex> lock(g_managerMutex);
        if (!g_unity) return;

        if (!g_manager)
        {
            Debug::Initialize();
            OutputWindowsInformation();
            g_manager = std::make_unique<MonitorManager>();
            g_manager->Initialize();
        }
    }

    UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API Finalize()
    {
        std::lock_guard<std::mutex> lock(g_managerMutex);
        if (g_manager)
        {
            g_manager->Finalize();
            g_manager.reset();
        }

        {
            std::lock_guard<std::mutex> messageLock(g_messageMutex);
            std::queue<Message> empty;
            g_messages.swap(empty);
        }

        Debug::Finalize();
    }

    void UNITY_INTERFACE_API OnGraphicsDeviceEvent(UnityGfxDeviceEventType event)
    {
        switch (event)
        {
            case kUnityGfxDeviceEventInitialize:
            {
                Initialize();
                break;
            }
            case kUnityGfxDeviceEventShutdown:
            {
                Finalize();
                break;
            }
        }
    }

    UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API UnityPluginLoad(IUnityInterfaces* unityInterfaces)
    {
        g_unity = unityInterfaces;
        auto unityGraphics = g_unity->Get<IUnityGraphics>();
        unityGraphics->RegisterDeviceEventCallback(OnGraphicsDeviceEvent);
    }

    UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API UnityPluginUnload()
    {
        auto unityGraphics = g_unity->Get<IUnityGraphics>();
        unityGraphics->UnregisterDeviceEventCallback(OnGraphicsDeviceEvent);
        g_unity = nullptr;
    }

    void UNITY_INTERFACE_API OnRenderEvent(int id)
    {
        std::lock_guard<std::mutex> lock(g_managerMutex);
        if (!g_manager) return;
        if (auto monitor = g_manager->GetMonitor(id))
        {
            monitor->Render();
        }
    }

    UNITY_INTERFACE_EXPORT UnityRenderingEvent UNITY_INTERFACE_API GetRenderEventFunc()
    {
        return OnRenderEvent;
    }

    UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API Reinitialize()
    {
        std::lock_guard<std::mutex> lock(g_managerMutex);
        if (!g_manager) return;
        return g_manager->Reinitialize();
    }

    UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API Update()
    {
        std::lock_guard<std::mutex> lock(g_managerMutex);
        if (!g_manager) return;
        g_manager->Update();
    }

    UNITY_INTERFACE_EXPORT Message UNITY_INTERFACE_API PopMessage()
    {
        std::lock_guard<std::mutex> lock(g_messageMutex);
        if (g_messages.empty()) return Message::None;

        const auto message = g_messages.front();
        g_messages.pop();
        return message;
    }

    UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API SetDebugMode(Debug::Mode mode)
    {
        Debug::SetMode(mode);
    }

    UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API SetLogFunc(Debug::DebugLogFuncPtr func)
    {
        Debug::SetLogFunc(func);
    }

    UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API SetErrorFunc(Debug::DebugLogFuncPtr func)
    {
        Debug::SetErrorFunc(func);
    }

    UNITY_INTERFACE_EXPORT size_t UNITY_INTERFACE_API GetMonitorCount()
    {
        std::lock_guard<std::mutex> lock(g_managerMutex);
        if (!g_manager) return 0;
        return g_manager->GetMonitorCount();
    }

    UNITY_INTERFACE_EXPORT bool UNITY_INTERFACE_API HasMonitorCountChanged()
    {
        std::lock_guard<std::mutex> lock(g_managerMutex);
        if (!g_manager) return false;
        return g_manager->HasMonitorCountChanged();
    }

    UNITY_INTERFACE_EXPORT int UNITY_INTERFACE_API GetCursorMonitorId()
    {
        std::lock_guard<std::mutex> lock(g_managerMutex);
        if (!g_manager) return -1;
        return g_manager->GetCursorMonitorId();
    }

    UNITY_INTERFACE_EXPORT int UNITY_INTERFACE_API GetTotalWidth()
    {
        std::lock_guard<std::mutex> lock(g_managerMutex);
        if (!g_manager) return 0;
        return g_manager->GetTotalWidth();
    }

    UNITY_INTERFACE_EXPORT int UNITY_INTERFACE_API GetTotalHeight()
    {
        std::lock_guard<std::mutex> lock(g_managerMutex);
        if (!g_manager) return 0;
        return g_manager->GetTotalHeight();
    }

    UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API GetId(int id)
    {
        std::lock_guard<std::mutex> lock(g_managerMutex);
        if (!g_manager) return;
        if (auto monitor = g_manager->GetMonitor(id))
        {
            monitor->GetId();
        }
    }

    UNITY_INTERFACE_EXPORT DuplicatorState UNITY_INTERFACE_API GetState(int id)
    {
        std::lock_guard<std::mutex> lock(g_managerMutex);
        if (!g_manager) return DuplicatorState::NotSet;
        if (auto monitor = g_manager->GetMonitor(id))
        {
            return monitor->GetDuplicatorState();
        }
        return DuplicatorState::NotSet;
    }

    UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API GetName(int id, char* buf, int len)
    {
        std::lock_guard<std::mutex> lock(g_managerMutex);
        if (!g_manager) return;
        if (auto monitor = g_manager->GetMonitor(id))
        {
            monitor->GetName(buf, len);
        }
    }

    UNITY_INTERFACE_EXPORT bool UNITY_INTERFACE_API IsPrimary(int id)
    {
        std::lock_guard<std::mutex> lock(g_managerMutex);
        if (!g_manager) return false;
        if (auto monitor = g_manager->GetMonitor(id))
        {
            return monitor->IsPrimary();
        }
        return false;
    }

    UNITY_INTERFACE_EXPORT int UNITY_INTERFACE_API GetLeft(int id)
    {
        std::lock_guard<std::mutex> lock(g_managerMutex);
        if (!g_manager) return -1;
        if (auto monitor = g_manager->GetMonitor(id))
        {
            return monitor->GetLeft();
        }
        return -1;
    }

    UNITY_INTERFACE_EXPORT int UNITY_INTERFACE_API GetRight(int id)
    {
        std::lock_guard<std::mutex> lock(g_managerMutex);
        if (!g_manager) return -1;
        if (auto monitor = g_manager->GetMonitor(id))
        {
            return monitor->GetRight();
        }
        return 0;
    }

    UNITY_INTERFACE_EXPORT int UNITY_INTERFACE_API GetTop(int id)
    {
        std::lock_guard<std::mutex> lock(g_managerMutex);
        if (!g_manager) return -1;
        if (auto monitor = g_manager->GetMonitor(id))
        {
            return monitor->GetTop();
        }
        return 0;
    }

    UNITY_INTERFACE_EXPORT int UNITY_INTERFACE_API GetBottom(int id)
    {
        std::lock_guard<std::mutex> lock(g_managerMutex);
        if (!g_manager) return -1;
        if (auto monitor = g_manager->GetMonitor(id))
        {
            return monitor->GetBottom();
        }
        return 0;
    }

    UNITY_INTERFACE_EXPORT int UNITY_INTERFACE_API GetWidth(int id)
    {
        std::lock_guard<std::mutex> lock(g_managerMutex);
        if (!g_manager) return -1;
        if (auto monitor = g_manager->GetMonitor(id))
        {
            return monitor->GetWidth();
        }
        return -1;
    }

    UNITY_INTERFACE_EXPORT int UNITY_INTERFACE_API GetHeight(int id)
    {
        std::lock_guard<std::mutex> lock(g_managerMutex);
        if (!g_manager) return -1;
        if (auto monitor = g_manager->GetMonitor(id))
        {
            return monitor->GetHeight();
        }
        return -1;
    }

    UNITY_INTERFACE_EXPORT int UNITY_INTERFACE_API GetRotation(int id)
    {
        std::lock_guard<std::mutex> lock(g_managerMutex);
        if (!g_manager) return -1;
        if (auto monitor = g_manager->GetMonitor(id))
        {
            return monitor->GetRotation();
        }
        return DXGI_MODE_ROTATION_UNSPECIFIED;
    }

    UNITY_INTERFACE_EXPORT int UNITY_INTERFACE_API GetDpiX(int id)
    {
        std::lock_guard<std::mutex> lock(g_managerMutex);
        if (!g_manager) return -1;
        if (auto monitor = g_manager->GetMonitor(id))
        {
            return monitor->GetDpiX();
        }
        return -1;
    }

    UNITY_INTERFACE_EXPORT int UNITY_INTERFACE_API GetDpiY(int id)
    {
        std::lock_guard<std::mutex> lock(g_managerMutex);
        if (!g_manager) return -1;
        if (auto monitor = g_manager->GetMonitor(id))
        {
            return monitor->GetDpiY();
        }
        return -1;
    }

    UNITY_INTERFACE_EXPORT bool UNITY_INTERFACE_API IsHDR(int id)
    {
        std::lock_guard<std::mutex> lock(g_managerMutex);
        if (!g_manager) return false;
        if (auto monitor = g_manager->GetMonitor(id))
        {
            return monitor->IsHDR();
        }
        return false;
    }

    UNITY_INTERFACE_EXPORT bool UNITY_INTERFACE_API IsCursorVisible()
    {
        std::lock_guard<std::mutex> lock(g_managerMutex);
        if (!g_manager) return false;
        return g_manager->GetCursor()->IsVisible();
    }

    UNITY_INTERFACE_EXPORT int UNITY_INTERFACE_API GetCursorX()
    {
        std::lock_guard<std::mutex> lock(g_managerMutex);
        if (!g_manager) return -1;
        return g_manager->GetCursor()->GetX();
    }

    UNITY_INTERFACE_EXPORT int UNITY_INTERFACE_API GetCursorY()
    {
        std::lock_guard<std::mutex> lock(g_managerMutex);
        if (!g_manager) return -1;
        return g_manager->GetCursor()->GetY();
    }

    UNITY_INTERFACE_EXPORT int UNITY_INTERFACE_API GetCursorShapeWidth()
    {
        std::lock_guard<std::mutex> lock(g_managerMutex);
        if (!g_manager) return -1;
        return g_manager->GetCursor()->GetWidth();
    }

    UNITY_INTERFACE_EXPORT int UNITY_INTERFACE_API GetCursorShapeHeight()
    {
        std::lock_guard<std::mutex> lock(g_managerMutex);
        if (!g_manager) return -1;
        return g_manager->GetCursor()->GetHeight();
    }

    UNITY_INTERFACE_EXPORT int UNITY_INTERFACE_API GetCursorShapePitch()
    {
        std::lock_guard<std::mutex> lock(g_managerMutex);
        if (!g_manager) return -1;
        return g_manager->GetCursor()->GetPitch();
    }

    UNITY_INTERFACE_EXPORT int UNITY_INTERFACE_API GetCursorShapeType()
    {
        std::lock_guard<std::mutex> lock(g_managerMutex);
        if (!g_manager) return -1;
        return g_manager->GetCursor()->GetType();
    }

    UNITY_INTERFACE_EXPORT int UNITY_INTERFACE_API GetCursorHotSpotX()
    {
        std::lock_guard<std::mutex> lock(g_managerMutex);
        if (!g_manager) return -1;
        return g_manager->GetCursor()->GetHotSpotX();
    }

    UNITY_INTERFACE_EXPORT int UNITY_INTERFACE_API GetCursorHotSpotY()
    {
        std::lock_guard<std::mutex> lock(g_managerMutex);
        if (!g_manager) return -1;
        return g_manager->GetCursor()->GetHotSpotY();
    }

    UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API GetCursorTexture(ID3D11Texture2D* texture)
    {
        std::lock_guard<std::mutex> lock(g_managerMutex);
        if (!g_manager) return;
        return g_manager->GetCursor()->GetTexture(texture);
    }

    UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API SetTexturePtr(int id, void* texture)
    {
        std::lock_guard<std::mutex> lock(g_managerMutex);
        if (!g_manager) return;
        if (auto monitor = g_manager->GetMonitor(id))
        {
            auto d3d11Texture = reinterpret_cast<ID3D11Texture2D*>(texture);
            monitor->SetUnityTexture(d3d11Texture);
        }
    }

    UNITY_INTERFACE_EXPORT void* UNITY_INTERFACE_API GetSharedTextureHandle(int id)
    {
        std::lock_guard<std::mutex> lock(g_managerMutex);
        if (!g_manager) return nullptr;
        if (auto monitor = g_manager->GetMonitor(id))
        {
            return monitor->GetSharedTextureHandle();
        }
        return nullptr;
    }

    UNITY_INTERFACE_EXPORT int UNITY_INTERFACE_API GetMoveRectCount(int id)
    {
        std::lock_guard<std::mutex> lock(g_managerMutex);
        if (!g_manager) return -1;
        if (auto monitor = g_manager->GetMonitor(id))
        {
            return monitor->GetMoveRectCount();
        }
        return 0;
    }

    UNITY_INTERFACE_EXPORT DXGI_OUTDUPL_MOVE_RECT* UNITY_INTERFACE_API GetMoveRects(int id)
    {
        std::lock_guard<std::mutex> lock(g_managerMutex);
        if (!g_manager) return nullptr;
        if (auto monitor = g_manager->GetMonitor(id))
        {
            return monitor->GetMoveRects();
        }
        return nullptr;
    }

    UNITY_INTERFACE_EXPORT int UNITY_INTERFACE_API GetDirtyRectCount(int id)
    {
        std::lock_guard<std::mutex> lock(g_managerMutex);
        if (!g_manager) return -1;
        if (auto monitor = g_manager->GetMonitor(id))
        {
            return monitor->GetDirtyRectCount();
        }
        return 0;
    }

    UNITY_INTERFACE_EXPORT RECT* UNITY_INTERFACE_API GetDirtyRects(int id)
    {
        std::lock_guard<std::mutex> lock(g_managerMutex);
        if (!g_manager) return nullptr;
        if (auto monitor = g_manager->GetMonitor(id))
        {
            return monitor->GetDirtyRects();
        }
        return nullptr;
    }

    UNITY_INTERFACE_EXPORT bool UNITY_INTERFACE_API GetPixels(int id, BYTE* output, int x, int y, int width, int height)
    {
        std::lock_guard<std::mutex> lock(g_managerMutex);
        if (!g_manager) return false;
        if (auto monitor = g_manager->GetMonitor(id))
        {
            return monitor->GetPixels(output, x, y, width, height);
        }
        return false;
    }

    UNITY_INTERFACE_EXPORT BYTE* UNITY_INTERFACE_API GetBuffer(int id)
    {
        std::lock_guard<std::mutex> lock(g_managerMutex);
        if (!g_manager) return nullptr;
        if (auto monitor = g_manager->GetMonitor(id))
        {
            return monitor->GetBuffer();
        }
        return nullptr;
    }

    UNITY_INTERFACE_EXPORT bool UNITY_INTERFACE_API HasBeenUpdated(int id)
    {
        std::lock_guard<std::mutex> lock(g_managerMutex);
        if (!g_manager) return nullptr;
        if (auto monitor = g_manager->GetMonitor(id))
        {
            return monitor->HasBeenUpdated();
        }
        return false;
    }

    UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API UseGetPixels(int id, bool use)
    {
        std::lock_guard<std::mutex> lock(g_managerMutex);
        if (!g_manager) return;
        if (auto monitor = g_manager->GetMonitor(id))
        {
            return monitor->UseGetPixels(use);
        }
    }

    UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API SetFrameRate(UINT frameRate)
    {
        std::lock_guard<std::mutex> lock(g_managerMutex);
        if (!g_manager) return;
        g_manager->SetFrameRate(frameRate);
    }
}
