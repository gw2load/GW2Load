# GW2Load

GW2Load is a minimalist addon loader for Guild Wars 2, the popular MMORPG by ArenaNet. It is not endorsed nor supported by ArenaNet or NCSoft. Its goal is to enable other third-party plugins (in common parlance, "addons") to be injected into the game and *add onto* the game. As such, it is fundamentally useless without addons to use it with and is more of a building block for the ecosystem.

## Why yet another addon loader?

GW2Load is intended as a definitive replacement for the old de facto standard, the [GW2 Addon Loader](https://github.com/gw2-addon-loader/loader-core). The old loader has become a hindrance, frequently incompatible with various common applications (overlays like NVIDIA's drivers, third-party customization software, etc.), with a clunky and overgrown API, all on top of being left largely unmaintained.

As such, GW2Load is a continuation of the old addon loader's philosophy: it loads addons, period. Additional features will only be considered if there is overwhelming support and the maintenance burden remains minimal. Addon management is left to external tools and each addon handles its own features like GUI, networking, updating, etc.

## For developers

GW2Load describes its API in [api.h](api.h). The header file is designed to be self-contained C++17 code. There is no need to link with this library directly, exporting the correct functions is sufficient.