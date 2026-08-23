#ifndef NALL_ADDONS_INTERFACE_H
#define NALL_ADDONS_INTERFACE_H

#ifdef __cplusplus
extern "C" {
#endif

struct EventCoreLoad {
    const char *name;
};
typedef void (*EventCoreLoadCallback)(const struct EventCoreLoad*, void *udata);

struct ares_addon_interface {
    void *addonsManager;
    void (*addListenerEventCoreLoad)(const struct ares_addon_interface*, EventCoreLoadCallback, void *udata);
};

void ares_addon_entry(const struct ares_addon_interface* aai);

#ifdef __cplusplus
}
#endif

#endif