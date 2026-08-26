#include "addons.hpp"
#include <cstdio>
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
    n64_addBreakpointPIDMAWritePlannedCallback = nullptr;
    n64_removeBreakpointPIDMAWritePlannedCallback = nullptr;
    // TODO(addons) add a callback to clear breakpoints and call it here, which may also warrant renaming this method
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
    // TODO(addons) somehow keep track of the addon's listeners and remove them here
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

  void* AddonsManager::n64_addBreakpoint(uint32_t start, uint32_t end, enum breakpoint_type type) {
    auto guard = acquireEmulatorThreadGuardCallback();
    switch(type) {
      case BREAKPOINT_TYPE_NONE:
        return NULL;
      case BREAKPOINT_TYPE_PI_DMA_WRITE_PLANNED:
        auto id = n64_addBreakpointPIDMAWritePlannedCallback(start, end, []() {
          // TODO(addons) actually do something here. addon callback?
          fprintf(stderr, "n64_addBreakpointPIDMAWritePlannedCallback callback called\n");
        });
        auto bp_handle = new Breakpoint([this, id]() {
          n64_removeBreakpointPIDMAWritePlannedCallback(id);
        });
        // TODO(addons) store breakpoints for later removal, on addon disable
        return bp_handle;
    }
    return NULL;
  }

  extern "C" void* AAI_n64_addBreakpoint(const struct ares_addon_interface *aai, uint32_t start, uint32_t end, enum breakpoint_type type) {
    auto addonsManager = static_cast<AddonsManager*>(aai->com->addonsManager);
    return addonsManager->n64_addBreakpoint(start, end, type);
  }

  void AddonsManager::n64_removeBreakpoint(void *bp_handle) {
    auto bp = static_cast<Breakpoint*>(bp_handle);
    bp->remove();
  }

  extern "C" void AAI_n64_removeBreakpoint(const struct ares_addon_interface *aai, void *bp_handle) {
    auto addonsManager = static_cast<AddonsManager*>(aai->com->addonsManager);
    addonsManager->n64_removeBreakpoint(bp_handle);
  }

  void AddonsManager::initAAICom() {
    aaiCom.addonsManager = this;
    aaiCom.addListenerEventCoreLoad = AAI_addListenerEventCoreLoad;
    aaiCom.setEmulationPaused = AAI_setEmulationPaused;
    aaiCom.n64_getRAM = AAI_n64_getRAM;
    aaiCom.n64_addBreakpoint = AAI_n64_addBreakpoint;
    aaiCom.n64_removeBreakpoint = AAI_n64_removeBreakpoint;
  }
}
