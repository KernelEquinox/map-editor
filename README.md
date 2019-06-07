Gen 1/2 Map Editor
=====
This is a map editor for Pokémon R/G/B/Y and Pokémon G/S/C.

I needed a solution that would open arbitrary tileset, cell, and map files, so I went and made one myself.

Building
--------
This program requies `libglfw`. Below are building instructions for various operating systems.

### Linux
Debian/Ubuntu:
```
apt-get install libglfw-dev
make
```

Fedora/CentOS/Red Hat:
```
yum -y install mesa-libGL-devel glfw-devel
make
```

**Note:** CentOS may require the Fedora EPEL repository, which you can add by running the following command:
```
yum -y install https://dl.fedoraproject.org/pub/epel/epel-release-latest-7.noarch.rpm
```

Arch Linux (X11):
```
pacman -S glfw-x11
make
```

**Note:** If your version of Arch Linux uses Wayland instead of X11, run the following command instead:
```
pacman -S glfw-wayland
make
```

### Mac
```
brew install glfw
make
```

### MSYS2
```
pacman -S --noconfirm --needed mingw-w64-x86_64-toolchain mingw-w64-x86_64-glfw
mingw32-make.exe
```

### Visual Studio or whatever
```
(just run build_win32.bat)
```

Usage
--------
When you first start the program, you'll be presented with this screen:
<br>![main window](https://user-images.githubusercontent.com/15955749/58767119-937d9380-854c-11e9-99f3-09eef54530b4.png)

Clicking on each checkbox shows the associated window, and clicking the button switches what editing mode you're currently in, all of which are covered below.

![change mode](https://user-images.githubusercontent.com/15955749/58827258-321afa80-8608-11e9-8f24-da35d99f67a4.gif)

Windows
--------
### Tileset Window
The Tileset window is for modifying the current tileset, which controls all of the available tiles.
<br>![tileset_window](https://user-images.githubusercontent.com/15955749/58767178-4fd75980-854d-11e9-8d83-37964f958a6d.png)

You can load existing tilesets via image file, assembly file, or binary file.

When saving the tileset, the compressed tile data will be saved as an assembly file.

When in **Tile Editing Mode**, you can draw on the tileset using the 4 available colors.
<br>![edit_tiles](https://user-images.githubusercontent.com/15955749/58767245-1d7a2c00-854e-11e9-9b79-c4a6f594f02f.gif)

**Note:** When editing the tileset, the cells and map update with the new tile(s) automatically.

### Palette Window
The Palette window is for modifying the current palette for Gen II games.
<br>![palette](https://user-images.githubusercontent.com/15955749/59137941-dcb85380-894f-11e9-8b4f-7b9f4c3aa0e9.png)

You can load existing tilesets via assembly file or binary file.

When saving the palette, the palette indices (two per byte) will be saved as an assembly file.

Palette editing works in **any mode**, and the available palette colors are as follows:
- Grey
- Red
- Green
- Blue
- Yellow
- Brown
- Roof color (dynamic)
- Message color (text)

![edit_palette](https://user-images.githubusercontent.com/15955749/59138098-a6c79f00-8950-11e9-847a-5e6076c71803.gif)

**Note:** When editing the palette, the tileset, cells, and map all update with the new color(s) automatically.

### Cells Window
The Cells window is for modifying the map cells, which are the building blocks of the map.
<br>![cells_window](https://user-images.githubusercontent.com/15955749/58767278-6fbb4d00-854e-11e9-9faf-b9102cad7fb9.png)

You can load existing cells via assembly file or binary file.

When saving the cells, the tiles indices that make up the cell will be saved as an assembly file.

When in **Cell Editing Mode**, you can select a tile by clicking in the Tileset window, then place the selected tile in a cell by clicking in the Cells window.
<br>![edit_cells](https://user-images.githubusercontent.com/15955749/58767297-d3de1100-854e-11e9-998c-1aebc73a458c.gif)

**Note:** When editing the cells, the map updates with the new cells automatically.

### Map Window
The map window is for modifying, you guessed it, _the map_.
<br>![map_window](https://user-images.githubusercontent.com/15955749/58767329-37683e80-854f-11e9-8a8d-35c2364870e3.png)

You can load an existing map via assembly file or binary file.

When saving the map, the cell indices that make up the map will be saved as an assembly file.

When in **Map Editing Mode**, you can select a cell by clicking in the Cells window, then place the selected cell in the map by clicking in the Map window.
<br>![edit_map](https://user-images.githubusercontent.com/15955749/58767338-55ce3a00-854f-11e9-87bb-17c01220ea8b.gif)

Supported Formats
--------
While only the **Tileset** window supports opening image formats, all of the windows support opening both assembly and binary files.

Assembly Format ($xx syntax):
```
        db      $01,$23,$45,$67,$89,$AB,$CD,$EF
        db      $13,$37,$FA,$CA,$DE,$C0,$DE,$69
        ...
```

Assembly Format (0xxh syntax):
```
        db      001h,023h,045h,067h,089h,0abh,0cdh,0efh
        db      013h,037h,0fah,0cah,0deh,0c0h,0deh,069h
        ...
```

Check out [FORMATS.md](https://github.com/KernelEquinox/map-editor/blob/master/FORMATS.md) if you want a better understanding of how each file works.
