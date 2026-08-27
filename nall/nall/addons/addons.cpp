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

  template<typename T> void EventManager<T>::addListener(Addon& addon, EventCallback callback, void *udata) {
    auto guard = lock();
    listeners.push_back(Listener{&addon, callback, udata});
  }

  template<typename T> void EventManager<T>::removeAllListeners(Addon& addon) {
    auto guard = lock();
    auto iter = listeners.begin();
    while (iter != listeners.end()) {
      if (iter->addon == &addon)
        iter = listeners.erase(iter);
      else
        iter++;
    }
  }

  template<typename T> void EventManager<T>::send(T& event) {
    auto guard = lock();
    for (auto& listener : listeners) {
      listener.callback(&event, listener.udata);
    }
  }

  AddonsManager::AddonsManager() {
    initAAICom();
    clearCoreSpecificHooks();
  }

  void AddonsManager::clearCoreSpecificHooks() {
    n64_gdbReportSignalTrapCallback = nullptr;
    n64_getRAMCallback = nullptr;
    // TODO(addons) clear all listeners? in this method?
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
    eventCoreLoad.removeAllListeners(addon);
    n64_eventPIDMAWritePlanned.removeAllListeners(addon);
    dlclose(addon.handle);
    addon.handle = nullptr;
    addon.enabled = false;
    if (addonStateChangeCallback != nullptr)
      addonStateChangeCallback(addon);
  }

  template<typename T> static void AAI_addListener(std::function<EventManager<T>&(AddonsManager&)> eventManagerGetter, const ares_addon_interface *aai, void (*callback)(const T*, void*), void *udata) {
    auto addon = static_cast<Addon*>(aai->addon);
    auto addonsManager = static_cast<AddonsManager*>(aai->com->addonsManager);
    eventManagerGetter(*addonsManager).addListener(*addon, callback, udata);
  }

  void AddonsManager::sendEventCoreLoad(string& name) {
    EventCoreLoad event{name.data()};
    eventCoreLoad.send(event);
  }

  static void AAI_addListenerEventCoreLoad(const ares_addon_interface *aai, EventCoreLoadCallback callback, void *udata) {
    AAI_addListener<EventCoreLoad>([](auto& addonsManager) -> auto& {return addonsManager.eventCoreLoad;}, aai, callback, udata);
  }

  void AddonsManager::sendEventEmulationPaused(bool paused) {
    EventEmulationPaused event{paused};
    eventEmulationPaused.send(event);
  }

  static void AAI_addListenerEventEmulationPaused(const ares_addon_interface *aai, EventEmulationPausedCallback callback, void *udata) {
    AAI_addListener<EventEmulationPaused>([](auto& addonsManager) -> auto& {return addonsManager.eventEmulationPaused;}, aai, callback, udata);
  }

  void AddonsManager::sendEventGDBHalted(bool halted) {
    EventGDBHalted event{halted};
    eventGDBHalted.send(event);
  }

  static void AAI_addListenerEventGDBHalted(const ares_addon_interface *aai, EventGDBHaltedCallback callback, void *udata) {
    AAI_addListener<EventGDBHalted>([](auto& addonsManager) -> auto& {return addonsManager.eventGDBHalted;}, aai, callback, udata);
  }

  void AddonsManager::setEmulationPaused(bool paused) {
    if (setEmulationPausedCallback != nullptr)
      setEmulationPausedCallback(paused);
  }

  static void AAI_setEmulationPaused(const ares_addon_interface *aai, bool paused) {
    auto addonsManager = static_cast<AddonsManager*>(aai->com->addonsManager);
    addonsManager->setEmulationPaused(paused);
  }

  void AddonsManager::n64_gdbReportSignalTrap() {
    auto guard = acquireEmulatorThreadGuardCallback();
    n64_gdbReportSignalTrapCallback();
  }

  static void AAI_n64_gdbReportSignalTrap(const ares_addon_interface *aai) {
    auto addonsManager = static_cast<AddonsManager*>(aai->com->addonsManager);
    addonsManager->n64_gdbReportSignalTrap();
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

  static bool AAI_n64_getRAM(const ares_addon_interface *aai, void *data, uint32_t start, uint32_t end) {
    auto addonsManager = static_cast<AddonsManager*>(aai->com->addonsManager);
    return addonsManager->n64_getRAM(data, start, end);
  }

  static void AAI_n64_addListenerPIDMAWritePlanned(const struct ares_addon_interface *aai, EventPIDMAWritePlannedCallback callback, void *udata) {
    AAI_addListener<EventPIDMAWritePlanned>([](auto& addonsManager) -> auto& {return addonsManager.n64_eventPIDMAWritePlanned;}, aai, callback, udata);
  }

  void AddonsManager::n64_sendEventPIDMAWritePlanned(uint32_t dramAddrReg, uint32_t wrLenReg) {
    EventPIDMAWritePlanned event{dramAddrReg, wrLenReg};
    n64_eventPIDMAWritePlanned.send(event);
  }

  void AddonsManager::initAAICom() {
    aaiCom.addonsManager = this;
    aaiCom.addListenerEventCoreLoad = AAI_addListenerEventCoreLoad;
    aaiCom.addListenerEventEmulationPaused = AAI_addListenerEventEmulationPaused;
    aaiCom.addListenerEventGDBHalted = AAI_addListenerEventGDBHalted;
    aaiCom.setEmulationPaused = AAI_setEmulationPaused;
    aaiCom.n64_gdbReportSignalTrap = AAI_n64_gdbReportSignalTrap;
    aaiCom.n64_getRAM = AAI_n64_getRAM;
    aaiCom.n64_addListenerPIDMAWritePlanned = AAI_n64_addListenerPIDMAWritePlanned;
  }
}
