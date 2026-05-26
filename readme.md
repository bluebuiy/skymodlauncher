
# Mod Manager for Linux
This is an experimental, prototype mod manager that runs natively in Linux.  I made this to evaluate using overlayfs to inject mod files into the game data, acomplishing the same kind of environment as MO2 does with the usvfs.  If people like it and I dont get bored, I might rewrite this using a more professional gui library.

Right now, the only officially supported distribution is the GOG Offline version.  It can likely be made to work with other distributions with changes to paths.

# How to Use
## Building

Required packages:
    libcurl4-dev

Download the source, configure with cmake, and build.
For example, using the supplied presets:

    cmake --preset ninja-release
    cmake --build out/ninja-release

then the executable will be at `out/ninja-release/skymodlaunch`.  For less technical users, you can copy the executable to the directory you want the mod setup in and running from there.  It will use the current directory.  Otherwise, you can pass it parameters to select the directory to use.

## Manually Installing Mods
In the UI, create a mod.  The program will create a directory;  Copy the mod contents to the directory it created.  Unlike MO2, where mod folders are attached to `/Data`, mod folders are attached to the game's install root (ie the same directory that SkyrimSE.exe is located).  This means most mods need a Data directory in them, and things like SKSE can be added as a mod, instead of directly added to the game root.

## Todo:
 * import loadorder.txt (from loot)
 * Overlay ~/Documents/My Games/Skyrim for ini and saves
 * Directly invoke unshare and mount, instead of cli

## Possible features
 * Per-file overrides
 * multi-game support, ie plugins
 * improved api for mod and file references
 * Automatic installation (ie fomod)


## Long term goals
These are things I am personally interested in, that I plan to implement eventually:
 * Nexus account linking
 * Nexus file download
 * Support Fomod
 * Nexus collections
 * Plugins and support other games

