#include "addons.hpp"
#include <dlfcn.h>
#include <filesystem>
#include <vector>

namespace nall::addons {
  AddonsManager addonsManager{};

  void AddonsManager::refreshAddons() {
    if (!addonsFolder)
      return;
    // list all addon names by iterating the addons folder
    std::vector<string> addonNames;
    for (auto& entry : std::filesystem::directory_iterator(std::string{addonsFolder})) {
      string addonName = entry.path().filename().c_str();
      addonNames.push_back(addonName);
    }
    // remove disabled addons that are no longer in the addons folder
    // (keep enabled addons)
    int i = 0;
    while (i < addons.size()) {
      auto& addon = addons[i];
      if (addon.enabled) {
        i++;
        continue;
      }
      bool addonStillAvailable = false;
      for (auto& addonName : addonNames) {
        if (addon.name == addonName) {
          addonStillAvailable = true;
          break;
        }
      }
      if (addonStillAvailable) {
        i++;
      } else {
        addons.erase(addons.begin() + i);
      }
    }
    // add new addons
    for (auto& addonName : addonNames) {
      bool newAddonName = true;
      for (auto& addon : addons) {
        if (addonName == addon.name) {
          newAddonName = false;
          break;
        }
      }
      if (newAddonName) {
        addons.push_back(Addon{.name=addonName, .enabled=false});
      }
    }
  }

  void AddonsManager::enable(Addon& addon) {
    if (addon.enabled)
      return;
    auto file = std::filesystem::path(std::string(addonsFolder)) / std::string(addon.name) / "addon.so";
    addon.handle = dlopen(file.c_str(), RTLD_NOW);
    if (addon.handle == nullptr) {
      fprintf(stderr, "dlopen on %s failed: %s\n", file.c_str(), dlerror());
      return;
    }
    auto addonEntryFunc = (void(*)(void))dlsym(addon.handle, "ares_addon_entry");
    if (!addonEntryFunc) {
      fprintf(stderr, "dlsym ares_addon_entry failed: %s\n", dlerror());
      dlclose(addon.handle);
      addon.handle = NULL;
      return;
    }
    addonEntryFunc();
    addon.enabled = true;
    addonStateChangeCallback(addon);
  }

  void AddonsManager::disable(Addon& addon) {
    if (!addon.enabled)
      return;
    dlclose(addon.handle);
    addon.handle = nullptr;
    addon.enabled = false;
    addonStateChangeCallback(addon);
  }
}
