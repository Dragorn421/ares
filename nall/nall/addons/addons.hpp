#pragma once

#include <nall/string.hpp>
#include <vector>

namespace nall::addons {
  struct Addon {
      nall::string name;
      bool enabled;
      void *handle;
  };

  class AddonsManager {
    public:
      void refreshAddons();
      void enable(Addon& addon);
      void disable(Addon& addon);

      string addonsFolder;
      std::function<void(Addon& addon)> addonStateChangeCallback;
      std::vector<Addon> addons;
  };

  extern AddonsManager addonsManager;
}
