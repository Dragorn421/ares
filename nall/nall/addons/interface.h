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

enum breakpoint_type {
    BREAKPOINT_TYPE_NONE,
    BREAKPOINT_TYPE_PI_DMA_WRITE_PLANNED
};

struct ares_addon_interface;

struct ares_addon_interface_com {
    void *addonsManager;
    void (*addListenerEventCoreLoad)(const struct ares_addon_interface*, EventCoreLoadCallback, void *udata);
    void (*setEmulationPaused)(const struct ares_addon_interface*, bool paused);
    // TODO(addons) make core-specific callbacks a pointed-to separate struct, for organizational purposes
    bool (*n64_getRAM)(const struct ares_addon_interface*, void *data, uint32_t start, uint32_t end);
    void* (*n64_addBreakpoint)(const struct ares_addon_interface*, uint32_t start, uint32_t end, enum breakpoint_type type);
    void (*n64_removeBreakpoint)(const struct ares_addon_interface*, void* breakpoint_handle);
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