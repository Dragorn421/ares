#include "../desktop-ui.hpp"
#include "platform.cpp"
#include "load.cpp"
#include "states.cpp"
#include "rewind.cpp"
#include "status.cpp"
#include "utility.cpp"
#include "drivers.cpp"
#include <nall/addons/addons.hpp>
#include <vector>

Program program;
thread worker;

auto Program::create() -> void {
  ares::platform = this;

  videoDriverUpdate();
  audioDriverUpdate();
  inputDriverUpdate();

  if(kiosk) {
    if(startFullScreen) videoFullScreenToggle();
    if(startPseudoFullScreen) videoPseudoFullScreenToggle();
  }

  addons::addonsManager.acquireEmulatorThreadGuardCallback = []() -> std::unique_ptr<addons::Guard> {
    class WrappedGuard : public addons::Guard {
      Program::Guard guard;
    };
    return std::make_unique<WrappedGuard>();
  };
  addons::addonsManager.addonsFolder = settings.paths.addons ? settings.paths.addons : locate("Addons/");
  addons::addonsManager.addonStateChangeCallback = [](addons::Addon& addon){
    int i = 0;
    while (i < settings.addons.enabledAddonsList.size()) {
      if (settings.addons.enabledAddonsList[i] == addon.name) {
        if (addon.enabled) {
          // enabled addon already in the enabled addons list
          return;
        } else {
          // remove disabled addon from the enabled addons list
          settings.addons.enabledAddonsList.erase(settings.addons.enabledAddonsList.begin() + i);
        }
      } else {
        i++;
      }
    }
    if (addon.enabled) {
      // add enabled addon to the enabled addons list
      settings.addons.enabledAddonsList.push_back(addon.name);
    }
  };
  addons::addonsManager.setEmulationPausedCallback = [](bool paused) {
    program.pause(paused);
  };
  addons::addonsManager.refreshAddons();
  for (auto& addon : addons::addonsManager.addons) {
    bool enable = false;
    for (auto& n : settings.addons.enabledAddonsList) {
      if (addon.name == n) {
        enable = true;
        break;
      }
    }
    if (enable) {
      addons::addonsManager.enable(addon);
    }
  }

  _isRunning = true;
  worker = thread::create(std::bind_front(&Program::emulatorRunLoop, this));
  program.rewindReset();

  if(!startGameLoad.empty()) {
    Program::Guard guard;
    auto gameToLoad = startGameLoad.front();
    startGameLoad.erase(startGameLoad.begin());
    if(startSystem) {
      for(auto &emulator: emulators) {
        if(emulator->name == startSystem) {
          if(load(emulator, gameToLoad)) {
            if(!kiosk) {
              if(startFullScreen) videoFullScreenToggle();
              if(startPseudoFullScreen) videoPseudoFullScreenToggle();
            }
          }
          return;
        }
      }
      return;
    }

    if(auto emulator = identify(gameToLoad)) {
      if(load(emulator, gameToLoad)) {
        if(!kiosk) {
          if(startFullScreen) videoFullScreenToggle();
          if(startPseudoFullScreen) videoPseudoFullScreenToggle();
        }
      }
    }
  }
}

auto Program::waitForInterrupts() -> void {
  std::unique_lock<std::mutex> lock(_programMutex);
  _interruptWorking = true;
  _programConditionVariable.notify_one();
  _programConditionVariable.wait(lock, [this] { return !_interruptWorking || _quitting; });
}

auto Program::emulatorRunLoop(uintptr_t) -> void {
  thread::setName("dev.ares.worker");
  _programThread = true;
  while(!_quitting) {
    // Allow other threads to carry out tasks between emulator run loop iterations
    if(_interruptWaiting) {
      waitForInterrupts();
      continue;
    }
    if(!emulator) {
      usleep(20 * 1000);
      continue;
    }

    if(emulator && nall::GDB::server.isHalted()) {
      ruby::audio.clear();
      nall::GDB::server.updateLoop(); // sleeps internally
      continue;
    }

    bool defocused = settings.input.defocus == "Pause" && !ruby::video.fullScreen() && !presentation.focused();

    if(!emulator || (paused && !program.requestFrameAdvance) || defocused) {
      ruby::audio.clear();
      nall::GDB::server.updateLoop();
      usleep(20 * 1000);
      continue;
    }

    rewindRun();

    nall::GDB::server.updateLoop();

    program.requestFrameAdvance = false;
    if(!runAhead || fastForwarding || rewinding) {
      emulator->root->run();
    } else {
      ares::setRunAhead(true);
      emulator->root->run();
      auto state = emulator->root->serialize(false);
      ares::setRunAhead(false);
      emulator->root->run();
      state.setReading();
      emulator->root->unserialize(state);
    }

    nall::GDB::server.updateLoop();

    if(settings.general.autoSaveMemory) {
      static u64 previousTime = chrono::timestamp();
      u64 currentTime = chrono::timestamp();
      if(currentTime - previousTime >= 30) {
        previousTime = currentTime;
        emulator->save();
      }
    }

    if(emulator->latch.changed) {
      emulator->latch.changed = false;
      _needsResize = true;
    }
  }
}

auto Program::main() -> void {
  if(Application::modal()) {
    ruby::audio.clear();
    return;
  }

  if(pendingKioskExit) {
    pendingKioskExit = false;
    quit();
    return;
  }
  
  inputManager.poll();
  inputManager.pollHotkeys();

  updateMessage();

  //If Platform::video() changed the screen resolution, resize the presentation window here.
  //Window operations must be performed from the main thread.
  
  if(_needsResize) {
    if(settings.video.adaptiveSizing && !startPseudoFullScreen) presentation.resizeWindow();
    _needsResize = false;
  }

  if(toolsWindowConstructed) {
    memoryEditor.liveRefresh();
    graphicsViewer.liveRefresh();
    propertiesViewer.liveRefresh();
    tapeViewer.liveRefresh();
  }
  if (_quitRequested) {
    quit();
  }
}

auto Program::quit() -> void {
  if (_programThread) {
    _quitRequested = true;
    return;
  }
  Program::Guard guard;
  _quitRequested = false;
  _quitting = true;
  if(lock.owns_lock()) {
    lock.unlock();
  }
  _programConditionVariable.notify_all();
  worker.join();
  program._isRunning = false;
  unload();
  Application::processEvents();
  Application::quit();

  ruby::video.reset();
  ruby::audio.reset();
  ruby::input.reset();
}
