// I got lazy and stopped commenting after the tileset window
// I'll finish the rest later

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "imguifs.h"
#include <stdio.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <string.h>
#include <math.h>

#define WHITE 0xFF
#define LGREY 0x80
#define DGREY 0x40
#define BLACK 0x00

#define TILE_EDIT 0
#define CELL_EDIT 1
#define MAP_EDIT  2

#if defined(IMGUI_IMPL_OPENGL_LOADER_GL3W)
#include <GL/gl3w.h>    // Initialize with gl3wInit()
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLEW)
#include <GL/glew.h>    // Initialize with glewInit()
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLAD)
#include <glad/glad.h>  // Initialize with gladLoadGL()
#else
#include IMGUI_IMPL_OPENGL_LOADER_CUSTOM
#endif

#include <GLFW/glfw3.h>

#if defined(_MSC_VER) && (_MSC_VER >= 1900) && !defined(IMGUI_DISABLE_WIN32_FUNCTIONS)
#pragma comment(lib, "legacy_stdio_definitions")
#endif


/**
 * Processes compressed tile data and returns the decompressed output.
 *
 * @param data - Compressed tile data
 * @param filesize - Size of the compressed data
 *
 * @return char* - A pointer to the decompressed data
 */
char* decompress_tiles(char* data, int filesize)
{
    int out_filesize = 128 * ceil(filesize / 128) * 8;
    char *out_data = (char*)IM_ALLOC(out_filesize);
    memset(out_data, BLACK, out_filesize);
    char *begin = out_data;

    int i = 0;
    // Loop through the entire file
    while (i < out_filesize)
    {
        // Loop through each vertical byte pair
        for (int y = 0; y < 8; y++)
        {
            unsigned char low_byte = data[y * 2];
            unsigned char high_byte = data[(y * 2) + 1];

            // Loop through each bit
            for (int x = 0; x < 8; x++)
            {
                char bit = 7 - x;
                char index = (((high_byte >> bit) & 1) << 1) | ((low_byte >> bit) & 1);
                if (index == 0) out_data[i++] = WHITE;
                else if (index == 1) out_data[i++] = LGREY;
                else if (index == 2) out_data[i++] = DGREY;
                else if (index == 3) out_data[i++] = BLACK;
            }
        }
        data += 16;
    }
    out_data = begin;

    return out_data;
}


/**
 * Compresses raw tile data.
 *
 * @param data - Raw tile data
 * @param filesize - Size of the raw tile data
 *
 * @return char* - A pointer to the compressed data
 */
char* compress_tiles(unsigned char* data, int filesize)
{
    int out_filesize = filesize / 4;
    char *out_data = (char*)IM_ALLOC(out_filesize);
    memset(out_data, BLACK, out_filesize);
    char *begin = out_data;

    int i = 0;
    // Loop through the entire file
    while (i < out_filesize)
    {
        // Loop through each vertical byte pair
        for (int y = 0; y < 8; y++)
        {
            unsigned char low_byte = 0;
            unsigned char high_byte = 0;

            for (int x = 0; x < 8; x++)
            {
                char bit = 7 - x;
                unsigned char cur_pixel = ~(data[x]);

                low_byte |= (cur_pixel >> 7) << bit;
                high_byte |= ((cur_pixel >> 6) & 1) << bit;
            }
            out_data[i++] = high_byte;
            out_data[i++] = low_byte;
            data += 8;
        }
    }
    out_data = begin;

    return out_data;
}


/**
 * Parses an assembly file or a binary file.
 *
 * @param filename - Full path of the file to be parsed
 * @param filesize - Any int* value since it will be overwritten
 * @param tex_offset - Offset for cell correction, otherwise 0
 *
 * @return char* - A pointer to the parsed data
 */
char* parse_file(const char* filename, int* filesize, const int tex_offset = 0)
{
    unsigned int arr_pos = 0;           // Current position in conversion array
    long cur_byte = 0;                  // Temporary conversion variable
    char *cur_pos;                      // Temporary strtol marker
    bool plaintext = false;             // Test for the type of file

    size_t file_size = 0;
    char* file_data = (char*)ImFileLoadToMemory(filename, "rb", &file_size);
    if (!file_data)
        return NULL;

    // Set passed filesize value to 0
    *filesize = 0;

    // Set buffer and buffer end marker
    char* buf = (char*)IM_ALLOC(file_size + 1);
    char* buf_end = buf + file_size;
    memcpy(buf, file_data, file_size);

    buf[file_size] = 0;

    // Data conversion array
    char *output_data = (char*)IM_ALLOC(file_size + 1);
    memset(output_data, BLACK, file_size);

    char* line_end = NULL;


    for (char* line = buf; line < buf_end; line = line_end + 1)
    {
        // Skip new lines markers, then find end of the line
        while (*line == '\n' || *line == '\r')
            line++;
        line_end = line;
        while (line_end < buf_end && *line_end != '\n' && *line_end != '\r')
            line_end++;
        line_end[0] = 0;


        /*
        Match the following data patterns:
            db  001h,023h,045h
            db  $01,$23,$45
        */

        // Skip spaces and tabs
        //
        // [    ]db    001h,023h,045h
        //
        while (line[0] == '\t' || line[0] == ' ')
            line++;

        // Skip line if line starts with a comment
        if (line[0] == ';')
            continue;

        // Check if line starts with "db" and a space/tab
        //
        //     [db    ]001h,023h,045h
        //
        if ((line[0] == 'd' || line[0] == 'D') && (line[1] == 'b' || line[1] == 'B') && (line[2] == ' ' || line[2] == '\t'))
        {

            // Skip past the "db" part
            line += 2;

            // Loop through each byte until the end of line
            while (line_end > line)
            {
                // Skip spaces and tabs
                //
                //     db[    ]001h,023h,045h
                //
                while (line[0] == '\t' || line[0] == ' ') line++;

                // Skip the '$' char if present
                //
                //     db    001h,023h,045h
                //     db    [$]01,$23,$45
                //
                if (line[0] == '$') line++;

                // Get the current byte
                //
                //     db    [001]h,023h,045h
                //     db    $[01],$23,$45
                //
                cur_pos = line_end;
                cur_byte = strtol(line, &cur_pos, 16);
                cur_byte += tex_offset;

                // Add it to the converted array and increase byte counter
                if (line != cur_pos)
                {
                    output_data[arr_pos++] = (unsigned char)cur_byte;
                    *filesize = *filesize + 1;
                }

                // Seek forward to next byte or end of line
                while (line[0] != ',' && line[0] != '\n' && line[0] != '\r')
                    line++;

                // Skip ',' char if present
                //
                //     db    001h[,]023h,045h
                //     db    $01[,]$23,$45
                //
                if (line[0] == ',') line++;

                // this is probably a plaintext file
                plaintext = true;
            }
        }
    }
    IM_FREE(buf);

    // The file was probably a binary file
    if (!plaintext)
    {
        *filesize = file_size;
        // Correct any texture offsets
        if (tex_offset)
            for (size_t i = 0; i < file_size; i++)
                file_data[i] += tex_offset;
        IM_FREE(output_data);
        return file_data;
    }

    // Return the processed plaintext data
    else
    {
        IM_FREE(file_data);
        return output_data;
    }
}


/**
 * Saves data to a file.
 *
 * @param data - Data to be saved
 * @param filename - Full path of the file to be saved
 * @param filesize - Size of the data to be written
 * @param tex_offset - Offset for cell correction, otherwise 0
 *
 * @return void - It'll work guys, trust me
 */
static void save_file(unsigned char *data, const char* filename, int filesize, int tex_offset = 0)
{
    int i = 0;
    int x = 0;
    FILE* f = ImFileOpen(filename, "w");

    // Loop through all data
    while (i < filesize)
    {
        // Define each line
        fprintf(f, "\tdb\t");

        for (x = 0; x < 7; x++)
        {
            fprintf(f, "0%02xh,", data[i++] - tex_offset);
            if (i == filesize) break;
        }

        fprintf(f, "0%02xh\n", data[i++] - tex_offset);
    }

    fclose(f);
}



static void glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "Glfw Error %d: %s\n", error, description);
}

int main(int, char**)
{
    // Setup window
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return 1;

    // Limit framerate
    glfwWindowHint(GLFW_REFRESH_RATE, 60);

    // Decide GL+GLSL versions
#if __APPLE__
    // GL 3.2 + GLSL 150
    const char* glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // Required on Mac
#else
    // GL 3.0 + GLSL 130
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    //glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    //glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // 3.0+ only
#endif

    // Create window with graphics context
    GLFWwindow* window = glfwCreateWindow(1280, 720, "Test Gen I/II Map Editing Program", NULL, NULL);
    if (window == NULL)
        return 1;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    // Initialize OpenGL loader
#if defined(IMGUI_IMPL_OPENGL_LOADER_GL3W)
    bool err = gl3wInit() != 0;
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLEW)
    bool err = glewInit() != GLEW_OK;
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLAD)
    bool err = gladLoadGL() == 0;
#else
    bool err = false; // If you use IMGUI_IMPL_OPENGL_LOADER_CUSTOM, your loader is likely to requires some form of initialization.
#endif
    if (err)
    {
        fprintf(stderr, "Failed to initialize OpenGL loader!\n");
        return 1;
    }

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer bindings
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);


    /*----------------------  display  data   ---------------------------*/
    bool show_tileset_window = false;
    bool show_cell_window = false;
    bool show_map_window = false;

    unsigned char tile_zoom = 1;
    unsigned char cell_zoom = 1;
    unsigned char map_zoom = 1;

    unsigned int frame_count = 0;

    const ImU8 u8_one = 1;

    float zoom_btn_size = 0.0f;
    ImGuiButtonFlags zoom_btn_flags = ImGuiButtonFlags_Repeat | ImGuiButtonFlags_DontClosePopups;



    /*----------------------  window  data   ----------------------------*/
    ImVec2 mouse_pos;
    ImVec2 win_pos;
    ImVec2 cur_pos;

    ImGuiStyle& style = ImGui::GetStyle();

    GLint rgb_mask[] = {GL_RED, GL_RED, GL_RED, GL_ONE};

    unsigned int edit_mode = TILE_EDIT;

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_HorizontalScrollbar;


    /*----------------------  texture  data   ---------------------------*/
    GLuint tile_textures[8192];
    GLuint color_textures[4];


    /*----------------------  color  data   -----------------------------*/
    unsigned int current_color = BLACK;
    unsigned char colors[] = { WHITE, LGREY, DGREY, BLACK };

    // Create the initial 4 textures for the buttons
    for (int i = 0; i < 4; i++)
    {
        glGenTextures(1, &color_textures[i]);
        glBindTexture(GL_TEXTURE_2D, color_textures[i]);

        // GL_NEAREST = Nearest Neighbor = disable anti-aliasing
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        // Convert GL_RED mask to a greyscale image
        glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, rgb_mask);

        // Get them pixels bruh
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, 1, 1, 0, GL_RED, GL_UNSIGNED_BYTE, colors+i);
    }


    /*----------------------  tileset  data   ---------------------------*/
    bool first_run = true;
    bool regen_tileset = true;

    int tileset_width = 16;
    int tileset_height = 8;

    unsigned char* tileset_data;
    unsigned char tiles[16][16][64];


    /*----------------------  cell  data   ------------------------------*/
    unsigned char cell_tiles[8192][16];    // shhh
    unsigned char current_tile = 0x01;

    int cell_width = 8;
    int cell_height = 8;


    /*----------------------  map  data   -------------------------------*/
    unsigned char map_cells[8192];         // don't question it
    unsigned char current_cell = 0x01;

    unsigned int map_width = 4;
    unsigned int map_height = 4;


    /*----------------------  file  data   ------------------------------*/
    const char *filename;
    static int filesize;

    const char *tileset_path = "./";
    const char *cell_path = "./";
    const char *map_path = "./";



    // Allocate defaults
    memset(tiles, BLACK, sizeof(tiles));
    memset(cell_tiles, BLACK, sizeof(cell_tiles));
    memset(map_cells, BLACK, sizeof(map_cells));

    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);


    // Main loop
    while (!glfwWindowShouldClose(window))
    {
        // Always increase frame count bby ;)
        frame_count++;

        // Check if any action was performed
        if (ImGui::IsAnyItemActive())
        {
            // Actively poll all events and reset frame count
            glfwPollEvents();
            frame_count = 0;
        }

        // Wait 3 extra frames for any action, then go idle
        else if (frame_count > 3)
        {
            glfwWaitEvents();
        }



        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();



        //================================================================================
        //    Main Window
        //================================================================================
        {
            ImGui::Begin("Map Editor Test", NULL, window_flags);

            ImGui::Checkbox("Show tileset", &show_tileset_window);
            ImGui::Checkbox("Show cells", &show_cell_window);
            ImGui::Checkbox("Show map", &show_map_window);

            ImGui::NewLine();

            /*
             * Tile Editing Mode:
             *     Clicking a pixel in the tileset changes that pixel.
             *     Basically MS Paint for tilesets.
             *
             * Cell Editing Mode:
             *     Clicking a tile in the tileset window selects that tile.
             *     Clicking a tile in the cell window places the selected tile.
             *
             * Map Editing Mode:
             *     Clicking a cell in the cell window selects that cell.
             *     Clicking a cell in the map window places the selected cell.
             */
            switch (edit_mode)
            {
                case TILE_EDIT:
                    if (ImGui::Button("Tile Editing Mode")) edit_mode = CELL_EDIT;
                    break;
                case CELL_EDIT:
                    if (ImGui::Button("Cell Editing Mode")) edit_mode = MAP_EDIT;
                    break;
                case MAP_EDIT:
                    if (ImGui::Button("Map Editing Mode")) edit_mode = TILE_EDIT;
                    break;
                default:
                    break;
            }

            ImGui::NewLine();

            // Version info, might update this sometimes??
            ImGui::Text("Map Test Thingy v0.1");

            // Only set zoom button size on initial render
            if (zoom_btn_size == 0.0f) zoom_btn_size = ImGui::GetFrameHeight();

            ImGui::End();
        }
        


        //================================================================================
        //    Tileset Window
        //================================================================================
        if (show_tileset_window)
        {
            ImGui::Begin("Tileset", &show_tileset_window, window_flags);

            // Only run this code if the tileset needs regenerating
            if (regen_tileset)
            {
                /*
                 * TILE TEXTURE CONVERSION
                 *
                 * Converts each tile into a texture.
                 */

                int tile_x, tile_y;
                int i = 0;
                for (tile_y = 0; tile_y < tileset_height; tile_y++)
                {
                    for (tile_x = 0; tile_x < tileset_width; tile_x++)
                    {
                        printf("%d: y=%d, x=%d, tile=%02x, wxh=%dx%d\n", i, tile_y, tile_x, *tiles[tile_y][tile_x], tileset_width, tileset_height);
                        if (first_run) glGenTextures(1, &tile_textures[i]);
                        glBindTexture(GL_TEXTURE_2D, tile_textures[i]);

                        // GL_NEAREST = Nearest Neighbor = disable anti-aliasing
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

                        // Convert GL_RED mask to a greyscale image
                        glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, rgb_mask);

                        // Get them pixels bruh
                        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
                        glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, 8, 8, 0, GL_RED, GL_UNSIGNED_BYTE, tiles[tile_y][tile_x]);
                        i++;
                    }
                }

                first_run = false;
                regen_tileset = false;
            }


            /*
             * IMAGE DISPLAY
             *
             * Prints each texture to the window.
             */

            cur_pos = ImGui::GetCursorPos();
            int orig_x = cur_pos.x;
            int orig_y = cur_pos.y;

            int i = 0;
            int tile_x, tile_y;
            int tile_size = 8 * tile_zoom;
            for (tile_y = 0; tile_y < tileset_height; tile_y++)
            {
                for (tile_x = 0; tile_x < tileset_width; tile_x++)
                {
                    cur_pos = ImGui::GetCursorPos();
                    ImGui::Image((void*)(intptr_t)tile_textures[i++], ImVec2(tile_size, tile_size));
                    ImGui::SetCursorPos(ImVec2(cur_pos.x + tile_size, cur_pos.y));
                }
                ImGui::SetCursorPos(ImVec2(orig_x, (cur_pos.y + tile_size)));
            }

            // Make tileset area draggable
            ImVec2 drag_area = ImVec2(tileset_width * 8 * tile_zoom, tileset_height * 8 * tile_zoom);
            ImGui::SetCursorPos(ImVec2(orig_x, orig_y));
            ImGui::InvisibleButton("canvas", drag_area);


            /*
             * PIXEL ALTERATION
             *
             * Change the pixel that was clicked.
             */

            if (ImGui::IsMouseDown(0) && ImGui::IsWindowFocused() && !ImGui::GetMouseCursor())
            {
                mouse_pos = ImGui::GetMousePos();
                win_pos = ImGui::GetWindowPos();

                // Check where the mouse was clicked relative to the current window
                int pos_x = mouse_pos.x - win_pos.x - style.WindowPadding.x;
                int pos_y = mouse_pos.y - win_pos.y - style.WindowPadding.y - ImGui::GetCurrentWindow()->TitleBarHeight();

                // Check if the mouse is currently within the bounds of the tileset area
                int max_width = tileset_width * 8 * tile_zoom;
                int max_height = tileset_height * 8 * tile_zoom;
                bool within_bounds = (pos_x < max_width && pos_y < max_height);


                // Check if the mouse was actually clicked within the tileset area
                static bool clicked_inside = true;
                if (ImGui::IsMouseClicked(0))
                    clicked_inside = (within_bounds) ? true : false;

                if (pos_x > 0 && pos_y > 0 && within_bounds)
                {
                    // Derive target tile
                    int tile_y = (int)pos_y / (8 * tile_zoom);
                    int tile_x = (int)pos_x / (8 * tile_zoom);

                    // Derive target pixel
                    int pixel_y = ((int)pos_y / tile_zoom) % 8;
                    int pixel_x = ((int)pos_x / tile_zoom) % 8;

                    // Derive target texture
                    int tex_id = (tile_y * 16) + tile_x;


                    /*
                     * MODE CHECK
                     *
                     * Check if we're in tile editing mode.
                     */

                    if (edit_mode == TILE_EDIT && clicked_inside)
                    {
                        // Set pixel color (default: BLACK)
                        tiles[tile_y][tile_x][(pixel_y*8) + pixel_x] = current_color;

                        // Regenerate the texture
                        glBindTexture(GL_TEXTURE_2D, tile_textures[tex_id]);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                        glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, rgb_mask);
                        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
                        glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, 8, 8, 0, GL_RED, GL_UNSIGNED_BYTE, tiles[tile_y][tile_x]);
                    }
                    // Select the tile if in cell editing mode
                    else if (edit_mode == CELL_EDIT)
                        current_tile = tile_textures[tex_id];
                }
            }

            ImGui::End();



            //================================================================================
            //    Tileset Controls Window
            //================================================================================
            ImGui::Begin("Tileset Ctrl", &show_tileset_window, window_flags);

            ImGui::AlignTextToFramePadding();
            ImGui::Text("Zoom:");
            ImGui::SameLine();
            if (ImGui::ButtonEx("-", ImVec2(zoom_btn_size, zoom_btn_size), zoom_btn_flags) && tile_zoom > 1) tile_zoom /= 2;
            ImGui::SameLine();
            if (ImGui::ButtonEx("+", ImVec2(zoom_btn_size, zoom_btn_size), zoom_btn_flags) && tile_zoom < 4) tile_zoom *= 2;

            ImGui::NewLine();


            // TILE WIDTH
            ImGui::AlignTextToFramePadding();
            ImGui::Text("Width: ");
            ImGui::SameLine(64);

            ImGui::SetNextItemWidth(75);
            ImGui::InputScalar("## Tile Width", ImGuiDataType_U8, &tileset_width, &u8_one, NULL, "%u");

            if (!tileset_width) tileset_width = 1;

            // Hell yeah, no limits \o/
            //if (tileset_width * tileset_height > 256) tileset_height--;



            // TILE HEIGHT
            ImGui::AlignTextToFramePadding();
            ImGui::Text("Height: ");
            ImGui::SameLine(64);

            ImGui::SetNextItemWidth(75);
            ImGui::InputScalar("## Tile Height", ImGuiDataType_U8, &tileset_height, &u8_one, NULL, "%u");

            if (!tileset_height) tileset_height = 1;

            // Hell yeah, no limits \o/
            //if (tileset_height * tileset_width > 256) tileset_width--;

            ImGui::NewLine();

            ImGui::Text("Colors:");
            static GLuint cur_color_tex = color_textures[3];
            static bool cur_color = false;
            for (int i = 0; i < 4; i++)
            {
                if (cur_color_tex == color_textures[i]) cur_color = true;

                ImGui::PushID(i);

                if (cur_color)
                {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.06f, 0.53f, 0.98f, 1.00f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.26f, 0.59f, 0.98f, 1.00f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.06f, 0.53f, 0.98f, 1.00f));
                }

                if (ImGui::ImageButton((void*)(intptr_t)color_textures[i], ImVec2(16, 16)))
                {
                    current_color = colors[i];
                    cur_color_tex = color_textures[i];
                }

                if (cur_color)
                {
                    ImGui::PopStyleColor(3);
                    cur_color = false;
                }

                ImGui::PopID();
                ImGui::SameLine();
            }

            ImGui::NewLine();
            ImGui::NewLine();

            ImGui::Text("Current Tile: ");
            ImGui::SameLine();
            ImGui::Image((void*)(intptr_t)current_tile, ImVec2(tile_size, tile_size));

            ImGui::NewLine();

            // Show "Open" dialog
            const bool tileset_open_btn = ImGui::Button("Open File...##Tileset");
            static ImGuiFs::Dialog tileset_open;
            filename = tileset_open.chooseFileDialog(tileset_open_btn, tileset_path, "");
            if (strlen(filename) > 0)
            {
                // Try to load as an image
                tileset_data = stbi_load(filename, &tileset_width, &tileset_height, NULL, 1);

                if (tileset_data)
                {
                    // Correct the pixels and stuff
                    for (int a = 0; a < (tileset_width * tileset_height); a++)
                    {
                        if (tileset_data[a] > 0x90) tileset_data[a] = WHITE;
                        else if (tileset_data[a] > 0x40) tileset_data[a] = LGREY;
                        else if (tileset_data[a] > 0x00) tileset_data[a] = DGREY;
                        else tileset_data[a] = BLACK;
                    }

                    tileset_width /= 8;
                    tileset_height /= 8;

                    // Convert image to blocky image
                    int tile_x, tile_y;
                    int pixel_x, pixel_y;
                    int i = 0;
                    for (tile_y = 0; tile_y < tileset_height; tile_y++)
                    {
                        for (tile_x = 0; tile_x < tileset_width; tile_x++)
                        {
                            i = 0;
                            for (pixel_y = 0; pixel_y < 8; pixel_y++)
                            {
                                for (pixel_x = 0; pixel_x < 8; pixel_x++)
                                {
                                    int base_x = (tile_x * 8) + pixel_x;
                                    int base_y = ((tile_y * 8) + pixel_y) * tileset_width * 8;
                                    tiles[tile_y][tile_x][i++] = tileset_data[base_y + base_x];
                                }
                            }
                        }
                    }
                }

                // Image load failed, fall back
                else
                {
                    char *file_data = parse_file(filename, &filesize);
                    tileset_data = (unsigned char*)decompress_tiles(file_data, filesize);
                    tileset_width = 16;
                    tileset_height = ceil((float)filesize / (float)(tileset_width * 8)) / 2;
                    memcpy(tiles, tileset_data, sizeof(tiles));
                }

                regen_tileset = true;
                tileset_path = tileset_open.getLastDirectory();
            }

            // Show "Save" dialog
            const bool tileset_save_btn = ImGui::Button("Save File...##Tileset");
            static ImGuiFs::Dialog tileset_save;
            const char* save_path = tileset_save.saveFileDialog(tileset_save_btn, tileset_path, "tileset.dat", "");
            if (strlen(save_path) > 0)
            {
                char *test_data = compress_tiles((unsigned char*)tiles, tileset_width * 8 * tileset_height * 8);
                save_file((unsigned char*)test_data, tileset_save.getChosenPath(), tileset_width * 8 * ((tileset_height * 8) / 4));

            }

            ImGui::End();
        }



        //================================================================================
        //    Cells Window
        //================================================================================
        if (show_cell_window)
        {
            ImGui::Begin("Cells", &show_cell_window, window_flags);


            cur_pos = ImGui::GetCursorPos();
            int orig_x = cur_pos.x;
            int orig_y = cur_pos.y;
            int cur_cell_x = cur_pos.x;
            int cur_cell_y = cur_pos.y;

            int cell_x, cell_y;
            int tile_x, tile_y;
            int tile_size = 8 * cell_zoom;

            for (cell_y = 0; cell_y < cell_height; cell_y++)
            {
                for (cell_x = 0; cell_x < cell_width; cell_x++)
                {
                    cur_pos = ImGui::GetCursorPos();

                    for (tile_y = 0; tile_y < 4; tile_y++)
                    {
                        for (tile_x = 0; tile_x < 4; tile_x++)
                        {
                            cur_pos = ImGui::GetCursorPos();
                            int test_index = (tile_y * 4) + tile_x;
                            int tile_test = cell_tiles[(cell_y * cell_width) + cell_x][test_index];
                            ImGui::Image((void*)(intptr_t)tile_test, ImVec2(tile_size, tile_size));
                            ImGui::SetCursorPos(ImVec2(cur_pos.x + tile_size, cur_pos.y));
                        }
                        ImGui::SetCursorPos(ImVec2(cur_cell_x, (cur_pos.y + tile_size)));
                    }
                    cur_cell_x += tile_size * 4;
                    ImGui::SetCursorPos(ImVec2(cur_cell_x, cur_cell_y));
                }
                cur_cell_y += tile_size * 4;
                cur_cell_x = orig_x;
                ImGui::SetCursorPos(ImVec2(cur_cell_x, cur_cell_y));
            }

            // Make tileset area draggable
            ImVec2 drag_area = ImVec2(8*4*8 * cell_zoom, 8*4*8 * cell_zoom);
            ImGui::SetCursorPos(ImVec2(orig_x, orig_y));
            ImGui::InvisibleButton("canvas", drag_area);


            /*
             * CELL ALTERATION
             *
             * Change the cell that was clicked.
             */

            if (ImGui::IsMouseDown(0) && ImGui::IsWindowFocused() && !ImGui::GetMouseCursor())
            {
                mouse_pos = ImGui::GetMousePos();
                win_pos = ImGui::GetWindowPos();

                int pos_x = mouse_pos.x - win_pos.x - style.WindowPadding.x;
                int pos_y = mouse_pos.y - win_pos.y - style.WindowPadding.y - ImGui::GetCurrentWindow()->TitleBarHeight();

                int max_width = cell_width*4*8*cell_zoom;
                int max_height = cell_height*4*8*cell_zoom;

                bool within_bounds = (pos_x < max_width && pos_y < max_height);
                static bool clicked_inside = true;

                if (ImGui::IsMouseClicked(0))
                    clicked_inside = (within_bounds) ? true : false;

                if (pos_x > 0 && pos_y > 0 && within_bounds)
                {
                    // Derive target cell
                    cell_y = (int)pos_y / (32*cell_zoom);
                    cell_x = (int)pos_x / (32*cell_zoom);

                    // Derive target tile
                    tile_y = ((int)pos_y / (8*cell_zoom)) % 4;
                    tile_x = ((int)pos_x / (8*cell_zoom)) % 4;

                    // Derive target texture


                    /*
                     * MODE SWITCH
                     *
                     * Check if we're in cell editing mode.
                     */

                    if (edit_mode == CELL_EDIT && clicked_inside)
                    {
                        // Set tile (default: 0)
                        int tile_index = (tile_y * 4) + tile_x;
                        cell_tiles[(cell_y * cell_width) + cell_x][tile_index] = current_tile;
                    }
                    else
                        current_cell = (cell_y * cell_width) + cell_x;
                }
            }

            ImGui::End();


            //================================================================================
            //    Cell Controls Window
            //================================================================================
            ImGui::Begin("Cells Ctrl", &show_cell_window, window_flags);

            ImGui::AlignTextToFramePadding();
            ImGui::Text("Zoom:");
            ImGui::SameLine(64);
            if (ImGui::ButtonEx("-", ImVec2(zoom_btn_size, zoom_btn_size), zoom_btn_flags) && cell_zoom > 1) cell_zoom /= 2;
            ImGui::SameLine();
            if (ImGui::ButtonEx("+", ImVec2(zoom_btn_size, zoom_btn_size), zoom_btn_flags) && cell_zoom < 4) cell_zoom *= 2;

            ImGui::NewLine();


            // CELL WIDTH
            ImGui::AlignTextToFramePadding();
            ImGui::Text("Width: ");
            ImGui::SameLine(64);

            ImGui::SetNextItemWidth(75);
            ImGui::InputScalar("## Cell Width", ImGuiDataType_U8, &cell_width, &u8_one, NULL, "%u");

            if (!cell_width) cell_width = 1;

            // Hell yeah, no limits \o/
            //if (cell_width * cell_height > 256) cell_height--;



            // CELL HEIGHT
            ImGui::AlignTextToFramePadding();
            ImGui::Text("Height: ");
            ImGui::SameLine(64);

            ImGui::SetNextItemWidth(75);
            ImGui::InputScalar("## Cell Height", ImGuiDataType_U8, &cell_height, &u8_one, NULL, "%u");

            if (!cell_height) cell_height = 1;

            // Hell yeah, no limits \o/
            //if (cell_height * cell_width > 256) cell_width--;

            ImGui::NewLine();



            // Display current tile
            ImGui::Text("Current Tile: ");
            ImGui::SameLine();

            cur_pos = ImGui::GetCursorPos();
            cur_cell_x = cur_pos.x;
            cur_cell_y = cur_pos.y;
            for (tile_y = 0; tile_y < 4; tile_y++)
            {
                for (tile_x = 0; tile_x < 4; tile_x++)
                {
                    cur_pos = ImGui::GetCursorPos();
                    int test_index = (tile_y * 4) + tile_x;
                    int tile_test = cell_tiles[current_cell][test_index];
                    ImGui::Image((void*)(intptr_t)tile_test, ImVec2(tile_size, tile_size));
                    ImGui::SetCursorPos(ImVec2(cur_pos.x + tile_size, cur_pos.y));
                }
                ImGui::SetCursorPos(ImVec2(cur_cell_x, (cur_pos.y + tile_size)));
            }

            ImGui::NewLine();

            // Show "Open" dialog
            const bool cell_open_btn = ImGui::Button("Open File...##Cell");
            static ImGuiFs::Dialog cell_open;
            filename = cell_open.chooseFileDialog(cell_open_btn, cell_path, "");
            if (strlen(filename) > 0)
            {
                // Find texture offset
                int tex_offset = tile_textures[0];
                unsigned char *file_data = (unsigned char*)parse_file(filename, &filesize, tex_offset);
                memcpy(cell_tiles, file_data, sizeof(cell_tiles));
                // Determine cell height
                if (filesize > 1024) cell_width = 16;
                else cell_width = 8;
                if (filesize > 2048) cell_height = 16;
                else cell_height = 8;
                cell_path = cell_open.getLastDirectory();
            }

            // Show "Save" dialog
            const bool cell_save_btn = ImGui::Button("Save File...##Cell");
            static ImGuiFs::Dialog cell_save;
            const char* save_path = cell_save.saveFileDialog(cell_save_btn, cell_path, "cell.cel", "");
            if (strlen(save_path) > 0)
            {
                cell_path = cell_save.getLastDirectory();
                int tex_offset = tile_textures[0];
                save_file((unsigned char*)cell_tiles, cell_save.getChosenPath(), cell_width * 4 * cell_height * 4, tex_offset);
            }

            ImGui::End();
        }



        //================================================================================
        //    Map Window
        //================================================================================
        if (show_map_window)
        {
            ImGui::Begin("Map", &show_map_window, window_flags);


            cur_pos = ImGui::GetCursorPos();
            int orig_x = cur_pos.x;
            int orig_y = cur_pos.y;
            int cur_map_x = cur_pos.x;
            int cur_map_y = cur_pos.y;

            unsigned int map_x, map_y;
            unsigned int tile_x, tile_y;
            int tile_size = 8 * map_zoom;



            for (map_y = 0; map_y < map_height; map_y++)
            {
                for (map_x = 0; map_x < map_width; map_x++)
                {
                    cur_pos = ImGui::GetCursorPos();

                    for (tile_y = 0; tile_y < 4; tile_y++)
                    {
                        for (tile_x = 0; tile_x < 4; tile_x++)
                        {
                            cur_pos = ImGui::GetCursorPos();
                            int tile_index = (tile_y * 4) + tile_x;
                            int cell_index = map_cells[(map_y * map_width) + map_x];
                            int tile_test = cell_tiles[cell_index][tile_index];
                            ImGui::Image((void*)(intptr_t)tile_test, ImVec2(tile_size, tile_size));
                            ImGui::SetCursorPos(ImVec2(cur_pos.x + tile_size, cur_pos.y));
                        }
                        ImGui::SetCursorPos(ImVec2(cur_map_x, (cur_pos.y + tile_size)));
                    }
                    cur_map_x += tile_size * 4;
                    ImGui::SetCursorPos(ImVec2(cur_map_x, cur_map_y));
                }
                cur_map_y += tile_size * 4;
                cur_map_x = orig_x;
                ImGui::SetCursorPos(ImVec2(cur_map_x, cur_map_y));
            }



            // Make tileset area draggable
            ImVec2 drag_area = ImVec2(map_width*4*8 * map_zoom, map_height*4*8 * map_zoom);
            ImGui::SetCursorPos(ImVec2(orig_x, orig_y));
            ImGui::InvisibleButton("canvas", drag_area);





            /*
             * MAP ALTERATION
             *
             * Change the map block that was clicked.
             */

            if (ImGui::IsMouseDown(0) && ImGui::IsWindowFocused() && !ImGui::GetMouseCursor())
            {
                mouse_pos = ImGui::GetMousePos();
                win_pos = ImGui::GetWindowPos();

                int pos_x = mouse_pos.x - win_pos.x - style.WindowPadding.x;
                int pos_y = mouse_pos.y - win_pos.y - style.WindowPadding.y - ImGui::GetCurrentWindow()->TitleBarHeight();

                int max_width = map_width*4*8*map_zoom;
                int max_height = map_height*4*8*map_zoom;

                bool within_bounds = (pos_x < max_width && pos_y < max_height);
                static bool clicked_inside = true;

                if (ImGui::IsMouseClicked(0))
                    clicked_inside = (within_bounds) ? true : false;

                if (pos_x > 0 && pos_y > 0 && within_bounds)
                {
                    // Derive target cell
                    map_y = (int)pos_y / (32*map_zoom);
                    map_x = (int)pos_x / (32*map_zoom);

                    /*
                     * MODE SWITCH
                     *
                     * Check if we're in map editing mode.
                     */

                    if (edit_mode == MAP_EDIT && clicked_inside)
                    {
                        // Set tile (default: 0)
                        int cell_index = (map_y * map_width) + map_x;
                        map_cells[cell_index] = current_cell;
                    }
                }
            }

            ImGui::End();

            //================================================================================
            //    Map Controls Window
            //================================================================================
            ImGui::Begin("Map Ctrl", &show_map_window, window_flags);

            ImGui::AlignTextToFramePadding();
            ImGui::Text("Zoom:");
            ImGui::SameLine(64);
            if (ImGui::ButtonEx("-", ImVec2(zoom_btn_size, zoom_btn_size), zoom_btn_flags) && map_zoom > 1) map_zoom /= 2;
            ImGui::SameLine();
            if (ImGui::ButtonEx("+", ImVec2(zoom_btn_size, zoom_btn_size), zoom_btn_flags) && map_zoom < 4) map_zoom *= 2;


            ImGui::NewLine();


            // MAP WIDTH
            ImGui::AlignTextToFramePadding();
            ImGui::Text("Width: ");
            ImGui::SameLine(64);

            ImGui::SetNextItemWidth(75);
            ImGui::InputScalar("## Map Width", ImGuiDataType_U8, &map_width, &u8_one, NULL, "%u");

            if (!map_width) map_width = 1;

            // Hell yeah, no limits \o/
            //if (map_width * map_height > 256) map_height--;



            // MAP HEIGHT
            ImGui::AlignTextToFramePadding();
            ImGui::Text("Height: ");
            ImGui::SameLine(64);

            ImGui::SetNextItemWidth(75);
            ImGui::InputScalar("## Map Height", ImGuiDataType_U8, &map_height, &u8_one, NULL, "%u");

            if (!map_height) map_height = 1;

            // Hell yeah, no limits \o/
            //if (map_height * map_width > 256) map_width--;

            ImGui::NewLine();




            // Show "Open" dialog
            const bool map_open_btn = ImGui::Button("Open File...##Map");
            static ImGuiFs::Dialog map_open;
            filename = map_open.chooseFileDialog(map_open_btn, map_path, "");
            if (strlen(filename) > 0)
            {
                // Find texture offset
                unsigned char *file_data = (unsigned char*)parse_file(filename, &filesize);
                memcpy(map_cells, file_data, sizeof(map_cells));
                map_path = map_open.getLastDirectory();
            }

            // Show "Save" dialog
            const bool map_save_btn = ImGui::Button("Save File...##Map");
            static ImGuiFs::Dialog map_save;
            const char* save_path = map_save.saveFileDialog(map_save_btn, map_path, "map.map", "");
            if (strlen(save_path) > 0)
            {
                map_path = map_save.getLastDirectory();
                save_file((unsigned char*)map_cells, map_save.getChosenPath(), map_width * map_height);
            }

            ImGui::End();
        }


        // Rendering
        ImGui::Render();
        int display_w, display_h;
        glfwMakeContextCurrent(window);
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwMakeContextCurrent(window);
        glfwSwapBuffers(window);
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
