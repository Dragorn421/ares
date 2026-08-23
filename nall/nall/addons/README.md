Currently Linux only.

An addon is an addon.so file inside a folder inside ares' Addons folder (see ares paths settings for where to find it).

The shared object must define `ares_addon_entry` which will be called when the addon is enabled.
