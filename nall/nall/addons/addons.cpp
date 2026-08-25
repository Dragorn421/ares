#include "addons.hpp"
#include <dlfcn.h>
#include <filesystem>
#include <vector>

namespace nall::addons {
  AddonsManager addonsManager{};

  Addon::Addon(string directory, string name) {
    this->directory = directory;
    this->name = name;
    enabled = false;
    handle = NULL;
    aai = ares_addon_interface{
      .addon=this,
      .addon_directory=this->directory.data(),
      .com=&addonsManager.aaiCom
    };
  }

  AddonsManager::AddonsManager() {
    initAAICom();
    clearCoreSpecificHooks();
  }

  void AddonsManager::clearCoreSpecificHooks() {
    n64_getRAMCallback = nullptr;
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
        addons.push_back(Addon(string(addonsFolder, "/", addonName), addonName));
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
    addonEntryFunc(&addon.aai);
    addon.enabled = true;
    if (addonStateChangeCallback != nullptr)
      addonStateChangeCallback(addon);
  }

  void AddonsManager::disable(Addon& addon) {
    if (!addon.enabled)
      return;
    // TODO somehow keep track of the addon's listeners and remove them here
    dlclose(addon.handle);
    addon.handle = nullptr;
    addon.enabled = false;
    if (addonStateChangeCallback != nullptr)
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
    auto addonsManager = static_cast<AddonsManager*>(aai->com->addonsManager);
    addonsManager->addListenerEventCoreLoad(callback, udata);
  }

  void AddonsManager::setEmulationPaused(bool paused) {
    if (setEmulationPausedCallback != nullptr)
      setEmulationPausedCallback(paused);
  }

  extern "C" void AAI_setEmulationPaused(const ares_addon_interface *aai, bool paused) {
    auto addonsManager = static_cast<AddonsManager*>(aai->com->addonsManager);
    addonsManager->setEmulationPaused(paused);
  }

  bool AddonsManager::n64_getRAM(void *data, uint32_t start, uint32_t end) {
    auto guardCB = acquireEmulatorThreadGuardCallback;
    if (guardCB == nullptr)
      return false;
    auto guard = guardCB();
    if (n64_getRAMCallback != nullptr) {
      n64_getRAMCallback(data, start, end);
      return true;
    }
    return false;
  }

  extern "C" bool AAI_n64_getRAM(const ares_addon_interface *aai, void *data, uint32_t start, uint32_t end) {
    auto addonsManager = static_cast<AddonsManager*>(aai->com->addonsManager);
    return addonsManager->n64_getRAM(data, start, end);
  }

  void AddonsManager::initAAICom() {
    aaiCom.addonsManager = this;
    aaiCom.addListenerEventCoreLoad = AAI_addListenerEventCoreLoad;
    aaiCom.setEmulationPaused = AAI_setEmulationPaused;
    aaiCom.n64_getRAM = AAI_n64_getRAM;
  }
}
