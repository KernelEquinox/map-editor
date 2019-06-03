Gen 1/2 Map Editor
=====
This is a map editor for Pokémon R/G/B/Y and Pokémon G/S/C.

I'll update this soon with supported formats, etc.

Too lazy tonight though.

Installation
--------

Linux:

Install `libglfw` via your distro's package manager. Debian: `libglfw-dev`; Arch: `glfw-x11` or `glfw-wayland`

Mac:
```
brew install glfw
make
```

MSYS2:
```
pacman -S --noconfirm --needed mingw-w64-x86_64-toolchain mingw-w64-x86_64-glfw
mingw32-make.exe
```

Visual Studio or whatever:
```
(just run build_win32.bat)
```
