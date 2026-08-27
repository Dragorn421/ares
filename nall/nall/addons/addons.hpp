#pragma once

#include <nall/string.hpp>
#include <memory>
#include <vector>
#include "interface.h"

namespace nall::addons {
  class Addon {
    public:
      Addon(string directory, string name);

      string directory;
      string name;
      bool enabled;
      void *handle;
      ares_addon_interface aai;
  };

  class Guard {
    public:
      virtual ~Guard() = default;
  };

  struct Breakpoint {
    std::function<void()> remove;
  };

  template<typename T> class EventManager {
    using EventCallback = void (*)(const T*, void*);
    struct Listener {
      Addon *addon;
      EventCallback callback;
      void *udata;
    };
    std::vector<Listener> listeners;

    std::mutex mutex;
    auto lock() { return std::lock_guard<std::mutex>(mutex); }
   public:
    void addListener(Addon *addon, EventCallback callback, void *udata);
    void removeAllListeners(Addon *addon);
    void send(T& event);
  };

  class AddonsManager {
    public:
      AddonsManager();

      void clearCoreSpecificHooks();

      void refreshAddons();
      void enable(Addon& addon);
      void disable(Addon& addon);

      string addonsFolder;
      std::function<void(Addon& addon)> addonStateChangeCallback;
      std::vector<Addon> addons;

      std::function<std::unique_ptr<Guard>()> acquireEmulatorThreadGuardCallback;

      ares_addon_interface_com aaiCom;
      void initAAICom();

      EventManager<EventCoreLoad> eventCoreLoad;
      void sendEventCoreLoad(string& name);

      std::function<void(bool paused)> setEmulationPausedCallback;
      void setEmulationPaused(bool paused);

      std::function<void()> n64_gdbReportSignalTrapCallback;
      void n64_gdbReportSignalTrap();

      std::function<void(void *data, uint32_t start, uint32_t end)> n64_getRAMCallback;
      bool n64_getRAM(void *data, uint32_t start, uint32_t end);

      EventManager<EventPIDMAWritePlanned> n64_eventPIDMAWritePlanned;
      void n64_sendEventPIDMAWritePlanned(uint32_t dramAddrReg, uint32_t wrLenReg);
  };

  extern AddonsManager addonsManager;
}
