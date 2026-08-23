#pragma once

#include <nall/string.hpp>
#include <vector>
#include "interface.h"

namespace nall::addons {
  struct Addon {
      nall::string name;
      bool enabled;
      void *handle;
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

      ares_addon_interface aai;
      void initAAI();

      std::vector<std::pair<EventCoreLoadCallback,void*>> listenersEventCoreLoad;
      void addListenerEventCoreLoad(EventCoreLoadCallback callback, void *udata);
      void sendEventCoreLoad(string& name);
  };

  extern AddonsManager addonsManager;
}
