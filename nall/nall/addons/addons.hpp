#pragma once

#include <nall/string.hpp>
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

  class AddonsManager {
    public:
      AddonsManager();

      void refreshAddons();
      void enable(Addon& addon);
      void disable(Addon& addon);

      string addonsFolder;
      std::function<void(Addon& addon)> addonStateChangeCallback;
      std::vector<Addon> addons;

      ares_addon_interface_com aaiCom;
      void initAAICom();

      std::vector<std::pair<EventCoreLoadCallback,void*>> listenersEventCoreLoad;
      void addListenerEventCoreLoad(EventCoreLoadCallback callback, void *udata);
      void sendEventCoreLoad(string& name);

      std::function<void(bool paused)> setEmulationPausedCallback;
      void setEmulationPaused(bool paused);
  };

  extern AddonsManager addonsManager;
}
