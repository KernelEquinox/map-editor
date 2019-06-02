@REM Build for Visual Studio compiler. Run your copy of vcvars32.bat or vcvarsall.bat to setup command-line compiler.
mkdir Debug
cl /nologo /Zi /MD /I imgui /I imgui\libs /I imgui\libs\glfw\include /I imgui\libs\gl3w *.cpp imgui\libs\imgui_impl_glfw.cpp imgui\libs\imgui_impl_opengl3.cpp imgui\imgui*.cpp imgui\libs\gl3w\GL\gl3w.c /FeDebug/map_editor.exe /FoDebug/ /link /LIBPATH:imgui\libs\glfw\lib-vc2010-32 glfw3.lib opengl32.lib gdi32.lib shell32.lib
