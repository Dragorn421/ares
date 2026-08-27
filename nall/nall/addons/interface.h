#ifndef NALL_ADDONS_INTERFACE_H
#define NALL_ADDONS_INTERFACE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

struct ares_addon_interface;

/* ares */

struct EventCoreLoad {
    const char *name;
};
typedef void (*EventCoreLoadCallback)(const struct EventCoreLoad*, void *udata);

struct EventEmulationPaused {
    bool paused;
};
typedef void (*EventEmulationPausedCallback)(const struct EventEmulationPaused*, void *udata);

struct EventGDBHalted {
    bool halted;
};
typedef void (*EventGDBHaltedCallback)(const struct EventGDBHalted*, void *udata);

struct ares_addon_interface_ares {
    void (*addListenerEventCoreLoad)(const struct ares_addon_interface*, EventCoreLoadCallback, void *udata);
    void (*addListenerEventEmulationPaused)(const struct ares_addon_interface*, EventEmulationPausedCallback, void *udata);
    void (*addListenerEventGDBHalted)(const struct ares_addon_interface*, EventGDBHaltedCallback, void *udata);
    void (*setEmulationPaused)(const struct ares_addon_interface*, bool paused);
};

/* N64 */

struct n64_EventPIDMAWritePlanned {
    uint32_t dramAddrReg;
    uint32_t wrLenReg;
};
typedef void (*n64_EventPIDMAWritePlannedCallback)(const struct n64_EventPIDMAWritePlanned*, void *udata);

struct ares_addon_interface_n64 {
    void (*gdbReportSignalTrap)(const struct ares_addon_interface*);
    bool (*getRAM)(const struct ares_addon_interface*, void *data, uint32_t start, uint32_t end);
    void (*addListenerEventPIDMAWritePlanned)(const struct ares_addon_interface*, n64_EventPIDMAWritePlannedCallback callback, void *udata);
};

/* interface */

struct ares_addon_interface_com {
    struct ares_addon_interface_ares *ares;
    struct ares_addon_interface_n64 *n64;
    void *addonsManager;
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