#include "addons.hpp"
#include <dlfcn.h>
#include <filesystem>
#include <vector>

namespace nall::addons {
  AddonsManager addonsManager{};

  AddonsManager::AddonsManager() {
    initAAI();
  }

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
    auto addonEntryFunc = (void(*)(const ares_addon_interface*))dlsym(addon.handle, "ares_addon_entry");
    if (!addonEntryFunc) {
      fprintf(stderr, "dlsym ares_addon_entry failed: %s\n", dlerror());
      dlclose(addon.handle);
      addon.handle = NULL;
      return;
    }
    addonEntryFunc(&aai);
    addon.enabled = true;
    addonStateChangeCallback(addon);
  }

  void AddonsManager::disable(Addon& addon) {
    if (!addon.enabled)
      return;
    // TODO somehow keep track of the addon's listeners and remove them here
    dlclose(addon.handle);
    addon.handle = nullptr;
    addon.enabled = false;
    addonStateChangeCallback(addon);
  }

  void AddonsManager::addListenerEventCoreLoad(EventCoreLoadCallback callback, void *udata) {
    listenersEventCoreLoad.push_back({callback, udata});
  }

  void AddonsManager::sendEventCoreLoad(string& name) {
    EventCoreLoad event{name.data()};
    for (auto& listener : listenersEventCoreLoad) {
      listener.first(&event, listener.second);
    }
  }

  extern "C" void AAI_addListenerEventCoreLoad(const ares_addon_interface *aai, EventCoreLoadCallback callback, void *udata) {
    auto addonsManager = static_cast<AddonsManager*>(aai->addonsManager);
    addonsManager->addListenerEventCoreLoad(callback, udata);
  }

  void AddonsManager::initAAI() {
    aai.addonsManager = this;
    aai.addListenerEventCoreLoad = AAI_addListenerEventCoreLoad;
  }
}
