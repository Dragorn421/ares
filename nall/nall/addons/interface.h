#ifndef NALL_ADDONS_INTERFACE_H
#define NALL_ADDONS_INTERFACE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

struct EventCoreLoad {
    const char *name;
};
typedef void (*EventCoreLoadCallback)(const struct EventCoreLoad*, void *udata);

struct EventPIDMAWritePlanned {
    uint32_t dramAddrReg;
    uint32_t wrLenReg;
};
typedef void (*EventPIDMAWritePlannedCallback)(const struct EventPIDMAWritePlanned*, void *udata);

struct ares_addon_interface;

struct ares_addon_interface_com {
    void *addonsManager;
    void (*addListenerEventCoreLoad)(const struct ares_addon_interface*, EventCoreLoadCallback, void *udata);
    void (*setEmulationPaused)(const struct ares_addon_interface*, bool paused);
    // TODO(addons) make core-specific callbacks a pointed-to separate struct, for organizational purposes
    void (*n64_gdbReportSignalTrap)(const struct ares_addon_interface*);
    bool (*n64_getRAM)(const struct ares_addon_interface*, void *data, uint32_t start, uint32_t end);
    void (*n64_addListenerPIDMAWritePlanned)(const struct ares_addon_interface*, EventPIDMAWritePlannedCallback callback, void *udata);
};

struct ares_addon_interface {
    void *addon;
    const char *addon_directory;
    struct ares_addon_interface_com *com;
};

void ares_addon_entry(const struct ares_addon_interface* aai);

#ifdef __cplusplus
}
#endif

#endif