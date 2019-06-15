// I got lazy and stopped commenting after the tileset window
// I'll finish the rest later

// Still too lazy to comment, I'll do it next time
// Also, the "c" in "0.1c" stands for "color" ;)

// Man, I really need to stop being so lazy
// This time, the "d" in "0.1d" stands for "data"

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "imguifs.h"
#include <stdio.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include <string.h>
#include <math.h>

// DMG pixel modes
#define WHITE 0xFF
#define LGREY 0x80
#define DGREY 0x40
#define BLACK 0x00

// Palette modes
#define PAL_GREY    0
#define PAL_RED     1
#define PAL_GREEN   2
#define PAL_BLUE    3
#define PAL_YELLOW  4
#define PAL_BROWN   5
#define PAL_ROOF    6
#define PAL_MSG     7

// Palette types
#define PAL_MORNING    0
#define PAL_DAY        1
#define PAL_NIGHT      2
#define PAL_DUNGEON    3
#define PAL_DARK_CAVE  4
#define PAL_BUILDING   5

// DMG colors
#define COL_WHITE   255,  255,  255
#define COL_LGREY   128,  128,  128
#define COL_DGREY   64,   64,   64
#define COL_BLACK   0,    0,    0

// Palette colors
#define COL_GREY    172.0f/255.0f,  172.0f/255.0f,  172.0f/255.0f
#define COL_RED     255.0f/255.0f,  156.0f/255.0f,  197.0f/255.0f
#define COL_GREEN   98.0f/255.0f,   205.0f/255.0f,  8.0f/255.0f
#define COL_BLUE    65.0f/255.0f,   98.0f/255.0f,   255.0f/255.0f
#define COL_YELLOW  255.0f/255.0f,  255.0f/255.0f,  57.0f/255.0f
#define COL_BROWN   197.0f/255.0f,  148.0f/255.0f,  57.0f/255.0f
#define COL_ROOF    255.0f/255.0f,  41.0f/255.0f,   173.0f/255.0f
#define COL_MSG     255.0f/255.0f,  255.0f/255.0f,  255.0f/255.0f

// Editing modes
#define TILE_EDIT 0
#define CELL_EDIT 1
#define MAP_EDIT  2

// Output formats
#define FMT_INTSYS 0    //  db  069h
#define FMT_REDNEX 1    //  db  $69
#define FMT_BINARY 2    //  0x69

// Tileset drawing modes
#define TOOL_PIXEL 0
#define TOOL_FILL  1

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


// Undo and redo history buffers
ImVector<unsigned char[16][16][64]> undo_history;
ImVector<unsigned char[16][16][64]> redo_history;


/**
 * Replaces instances of the contiguous target color with the chosen color.
 * Functions similarly to the "Fill" tool in MS Paint, Photoshop, etc.
 *
 * NOTE:
 *   This is only a per-tile fill.
 *   Tiles other than the one clicked will NOT be affected.
 *
 * @param tile - Tile currently being edited
 * @param x - X coordinate of the pixel to fill
 * @param y - Y coordinate of the pixel to fill
 * @param target_color - Color to be replaced
 * @param new_color - Color to replace with
 *
 * @return void - Just fills pixels, no return needed
 */
static void fill_color(unsigned char* tile, int x, int y, unsigned int target_color, unsigned int new_color)
{
    unsigned int cur_pixel = *(tile + (y * 8) + x);

    // Don't replace the same color or non-targeted colors
    if (cur_pixel == new_color) return;
    else if (cur_pixel != target_color) return;
    else *(tile + (y * 8) + x) = new_color;

    // Recursively replace pixels in each direction relative to the current pixel
    if (y - 1 >= 0) fill_color(tile, x, y - 1, target_color, new_color);
    if (y + 1 <= 7) fill_color(tile, x, y + 1, target_color, new_color);
    if (x - 1 >= 0) fill_color(tile, x - 1, y, target_color, new_color);
    if (x + 1 <= 7) fill_color(tile, x + 1, y, target_color, new_color);
}


/**
 * Fetches the tile at the specified tileset coordinates.
 * Places the tile at the specified output data coordinates.
 *
 * @param in_data - Tileset data
 * @param in_width - Width of the tileset
 * @param in_x - X coordinate of the tile
 * @param in_y - Y coordinate of the tile
 * @param out_data - Output data to write to
 * @param out_width - Width of the output data
 * @param out_x - X coordinate to place the tile
 * @param out_y - Y coordinate to place the tile
 * @param bpp - The bit-per-pixel count (Default: 0)
 *
 * @return void - Nope, nothin'
 */
static void get_tile(unsigned char* in_data, int in_width, int in_x, int in_y, unsigned char* out_data, int out_width, int out_x, int out_y, int bpp = 1)
{
    int pixel_x, pixel_y;
    int i = 0;
    int k = 0;
    for (pixel_y = 0; pixel_y < 8; pixel_y++)
    {
        for (pixel_x = 0; pixel_x < 8 * bpp; pixel_x += bpp)
        {
            k = 0;
            int base_x = (out_x * 8 * bpp) + pixel_x;
            int base_y = ((out_y * 8) + pixel_y) * out_width * 8 * bpp;
            int pix_idx = (in_y * in_width * 64 * bpp) + (in_x * 64 * bpp);
            for (int z = 0; z < bpp; z++)
                *(out_data + base_y + base_x + k++) = *(in_data + pix_idx + i++);
        }
    }
}


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
    char* out_data = (char*)IM_ALLOC(out_filesize);
    memset(out_data, BLACK, out_filesize);
    char* begin = out_data;

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
    char* out_data = (char*)IM_ALLOC(out_filesize);
    memset(out_data, BLACK, out_filesize);
    char* begin = out_data;

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
 *
 * @return char* - A pointer to the parsed data
 */
char* parse_file(const char* filename, int* filesize)
{
    unsigned int arr_pos = 0;           // Current position in conversion array
    long cur_byte = 0;                  // Temporary conversion variable
    char* cur_pos;                      // Temporary strtol marker
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
    char* output_data = (char*)IM_ALLOC(file_size + 1);
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
 * @param format - Format to write the data in
 *
 * @return void - It'll work guys, trust me
 */
static void save_file(unsigned char* data, const char* filename, int filesize, int format)
{
    int i = 0;
    int x = 0;
    FILE* f = ImFileOpen(filename, "w");

    // Not sure if the compiler takes care of this, but I'll optimize the loop anyway
    switch(format)
    {
        /*
         * Intelligent Systems format:
         *         db      069h
         */
        case FMT_INTSYS:
            // Loop through all data
            while (i < filesize)
            {
                // Define each 8-byte line
                fprintf(f, "\tdb\t");
                for (x = 0; x < 7; x++)
                {
                    fprintf(f, "0%02xh,", data[i++]);
                    if (i == filesize) break;
                }
                fprintf(f, "0%02xh\n", data[i++]);
            }
            break;

        /*
         * Rednex GB Dev System format:
         *         db      $69
         */
        case FMT_REDNEX:
            // Loop through all data
            while (i < filesize)
            {
                // Define each 8-byte line
                fprintf(f, "\tdb\t");
                for (x = 0; x < 7; x++)
                {
                    fprintf(f, "$%02x,", data[i++]);
                    if (i == filesize) break;
                }
                fprintf(f, "$%02x\n", data[i++]);
            }
            break;

        /*
         * Binary format:
         *   Literally just binary data
         */
        case FMT_BINARY:
            fwrite(data, 1, filesize, f);
            break;

        default:
            break;
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
    bool show_palette_window = false;
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

    bool auto_resize = true;
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_HorizontalScrollbar;


    /*----------------------  texture  data   ---------------------------*/
    GLuint color_textures[4];
    GLuint tile_textures[8192];
    GLuint palette_textures[8192];


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
    bool first_tileset = true;
    bool regen_tileset = true;

    int tileset_width = 16;
    int tileset_height = 8;

    unsigned char* tileset_data;
    unsigned char tiles[16][16][64];

    int current_tool = TOOL_PIXEL;


    /*----------------------  palette  data   ---------------------------*/
    bool first_palette = true;
    bool regen_palette = true;
    bool use_palette = false;

    ImVec4 pal_colors[] = { 
        ImVec4(COL_GREY, 1.0f), 
        ImVec4(COL_RED, 1.0f), 
        ImVec4(COL_GREEN, 1.0f), 
        ImVec4(COL_BLUE, 1.0f), 
        ImVec4(COL_YELLOW, 1.0f), 
        ImVec4(COL_BROWN, 1.0f), 
        ImVec4(COL_ROOF, 1.0f), 
        ImVec4(COL_MSG, 1.0f)
    };

    unsigned char palette_morning[] = {
        // Grey
        230, 255, 131,
        172, 172, 172,
        106, 106, 106,
        57, 57, 57,

        // Red
        230, 255, 131,
        255, 156, 197,
        246, 82, 49,
        57, 57, 57,

        // Green
        180, 255, 82,
        98, 205, 8,
        41, 115, 0,
        57, 57, 57,

        // Blue
        255, 255, 255,
        65, 98, 255,
        8, 32, 255,
        57, 57, 57,

        // Yellow
        230, 255, 131,
        255, 255, 57,
        255, 131, 8,
        57, 57, 57,

        // Brown
        230, 255, 131,
        197, 148, 57,
        164, 123, 24,
        57, 57, 57,

        // Roof
        230, 255, 131,
        123, 255, 255,
        41, 139, 255,
        57, 57, 57,

        // Msg
        255, 255, 131,
        255, 255, 131,
        115, 74, 0,
        0, 0, 0
    };

    unsigned char palette_day[] = {
        // Grey
        222, 255, 222,
        172, 172, 172,
        106, 106, 106,
        57, 57, 57,

        // Red
        222, 255, 222,
        255, 156, 197,
        246, 82, 49,
        57, 57, 57,

        // Green
        180, 255, 82,
        98, 205, 8,
        41, 115, 0,
        57, 57, 57,

        // Blue
        255, 255, 255,
        65, 98, 255,
        8, 32, 255,
        57, 57, 57,

        // Yellow
        222, 255, 222,
        255, 255, 57,
        255, 131, 8,
        57, 57, 57,

        // Brown
        222, 255, 222,
        197, 148, 57,
        164, 123, 24,
        57, 57, 57,

        // Roof
        222, 255, 222,
        123, 255, 255,
        41, 139, 255,
        57, 57, 57,

        // Msg
        255, 255, 131,
        255, 255, 131,
        115, 74, 0,
        0, 0, 0
    };

    unsigned char palette_night[] = {
        // Grey
        123, 115, 197,
        90, 90, 156,
        57, 57, 98,
        0, 0, 0,

        // Red
        123, 115, 197,
        115, 57, 139,
        106, 0, 65,
        0, 0, 0,

        // Green
        123, 115, 197,
        65, 106, 156,
        0, 90, 106,
        0, 0, 0,

        // Blue
        123, 115, 197,
        41, 41, 139,
        24, 24, 82,
        0, 0, 0,

        // Yellow
        246, 246, 90,
        131, 115, 148,
        131, 115, 82,
        0, 0, 0,

        // Brown
        123, 115, 197,
        98, 74, 123,
        65, 32, 41,
        0, 0, 0,

        // Roof
        123, 115, 197,
        106, 98, 189,
        90, 74, 164,
        0, 0, 0,

        // Msg
        255, 255, 131,
        255, 255, 131,
        115, 74, 0,
        0, 0, 0
    };

    unsigned char palette_dark_cave[] = {
        // Grey
        8, 8, 16,
        0, 0, 0,
        0, 0, 0,
        0, 0, 0,

        // Red
        8, 8, 16,
        0, 0, 0,
        0, 0, 0,
        0, 0, 0,

        // Green
        8, 8, 16,
        0, 0, 0,
        0, 0, 0,
        0, 0, 0,

        // Blue
        8, 8, 16,
        0, 0, 0,
        0, 0, 0,
        0, 0, 0,

        // Yellow
        246, 246, 90,
        0, 0, 0,
        0, 0, 0,
        0, 0, 0,

        // Brown
        8, 8, 16,
        0, 0, 0,
        0, 0, 0,
        0, 0, 0,

        // Roof
        8, 8, 16,
        0, 0, 0,
        0, 0, 0,
        0, 0, 0,

        // Msg
        255, 255, 131,
        255, 255, 131,
        115, 74, 0,
        0, 0, 0
    };

    unsigned char palette_building[] = {
        // Grey
        246, 230, 213,
        156, 156, 156,
        106, 106, 106,
        57, 57, 57,

        // Red
        246, 230, 213,
        255, 156, 197,
        246, 82, 49,
        57, 57, 57,

        // Green
        148, 197, 74,
        123, 164, 8,
        74, 106, 0,
        57, 57, 57,

        // Blue
        246, 230, 213,
        123, 131, 255,
        74, 74, 255,
        57, 57, 57,

        // Yellow
        246, 230, 213,
        255, 255, 57,
        255, 131, 8,
        57, 57, 57,

        // Brown
        213, 197, 139,
        172, 139, 57,
        131, 106, 24,
        57, 57, 57,

        // Roof
        246, 230, 213,
        139, 156, 255,
        115, 131, 255,
        57, 57, 57,

        // Msg
        255, 255, 131,
        255, 255, 131,
        115, 74, 0,
        0, 0, 0
    };

    unsigned char palette_water[] = {
        189, 189, 255,
        148, 156, 255,
        106, 98, 255,
        57, 57, 57,

        123, 106, 222,
        82, 74, 164,
        32, 24, 148,
        0, 0, 0
    };

    unsigned char palette_roof[] = {
        // Unused
        172, 172, 172,
        90, 90, 90,
        172, 172, 172,
        90, 90, 90,

        // Olivine City
        115, 139, 255,
        57, 90, 123,
        74, 74, 139,
        41, 57, 106,

        // Mahogany Town
        98, 156, 0,
        49, 82, 0,
        49, 74, 57,
        32, 41, 49,

        // Dungeon
        172, 172, 172,
        90, 90, 90,
        172, 172, 172,
        139, 65, 57,

        // Ecruteak City
        255, 156, 0,
        222, 82, 41,
        123, 57, 16,
        90, 32, 16,

        // Blackthorn City
        90, 82, 131,
        41, 49, 57,
        24, 32, 65,
        0, 0, 0,

        // Cinnabar Island
        255, 82, 0,
        148, 49, 0,
        148, 41, 74,
        139, 65, 57,

        // Cerulean City
        139, 222, 255,
        41, 123, 255,
        57, 65, 180,
        57, 57, 131,

        // Azalea Town
        180, 164, 82,
        139, 115, 24,
        90, 90, 41,
        82, 74, 57,

        // Lake of Rage
        255, 65, 32,
        74, 74, 65,
        148, 41, 74,
        74, 74, 65,

        // Violet City
        197, 115, 255,
        106, 57, 172,
        98, 24, 148,
        74, 24, 123,

        // Goldenrod City
        205, 205, 0,
        164, 139, 65,
        98, 98, 0,
        82, 74, 41,

        // Vermilion City
        222, 189, 8,
        189, 90, 0,
        123, 90, 8,
        90, 82, 8,

        // Pallet Town
        222, 230, 255,
        139, 156, 180,
        115, 115, 148,
        82, 74, 106,

        // Pewter City
        156, 156, 131,
        82, 98, 123,
        74, 74, 90,
        32, 41, 57,

        // Route 4
        115, 139, 255,
        57, 90, 123,
        74, 106, 156,
        57, 57, 131,

        // Indigo Plateau
        172, 172, 172,
        106, 106, 106,
        90, 90, 156,
        57, 57, 98,

        // Fuchsia City
        255, 148, 238,
        139, 106, 164,
        115, 49, 98,
        90, 24, 82,

        // Lavender Town
        189, 123, 255,
        131, 41, 255,
        98, 57, 139,
        65, 49, 82,

        // Route 28
        172, 172, 205,
        131, 131, 131,
        106, 106, 106,
        57, 57, 57,

        // Communication
        172, 172, 172,
        90, 90, 90,
        172, 172, 172,
        90, 90, 90,

        // Celadon City
        156, 255, 123,
        255, 180, 16,
        98, 106, 74,
        74, 98, 24,

        // Cianwood City
        123, 82, 255,
        57, 41, 123,
        49, 41, 139,
        16, 16, 65,

        // Viridian City
        172, 255, 57,
        106, 205, 32,
        74, 115, 65,
        49, 82, 32,

        // New Bark Town
        164, 255, 115,
        90, 189, 41,
        74, 106, 65,
        49, 74, 32,

        // Saffron City
        255, 213, 0,
        255, 123, 0,
        106, 106, 8,
        65, 65, 8,

        // Cherrygrove City
        255, 115, 230,
        255, 41, 172,
        115, 57, 139,
        106, 0, 65
    };

    unsigned char* cur_palette = (unsigned char*)&palette_day;
    unsigned char* cur_roof = (unsigned char*)&palette_roof;
    bool daytime_pal = true;
    int palette_type = PAL_DAY;
    int roof_id = 0;
    
    int tileset_x, tileset_y = 0;

    unsigned char color_tiles[16][16][192];
    unsigned char palettes[256];


    /*----------------------  cell  data   ------------------------------*/
    unsigned char cell_tiles[8192][16];
    unsigned char current_tile = 0x00;
    unsigned char current_tile_id = 0x00;

    int cell_width = 8;
    int cell_height = 8;


    /*----------------------  map  data   -------------------------------*/
    unsigned char map_cells[8192];
    unsigned char current_cell = 0x00;

    int map_width = 4;
    int map_height = 4;


    /*----------------------  file  data   ------------------------------*/
    const char* filename;
    static int filesize;

    const char* tileset_path = "./";
    const char* palette_path = "./";
    const char* cell_path = "./";
    const char* map_path = "./";

    // Default to IntSys output format
    static int output_format = FMT_INTSYS;



    // Allocate defaults
    memset(tiles, BLACK, sizeof(tiles));
    memset(palettes, BLACK, sizeof(palettes));
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
        //    Main Menu
        //================================================================================

        // Top main menu bar
        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("File"))
            {
                if (ImGui::MenuItem("Quit", "Alt+F4")) break;
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Edit"))
            {
                if (ImGui::MenuItem("Undo", "Ctrl+Z", false, undo_history.size()))
                {
                    glfwPollEvents(); frame_count = 0;
                    if (undo_history.size())
                    {
                        redo_history.push_back(tiles);
                        memcpy(tiles, undo_history.back(), sizeof(tiles));
                        undo_history.pop_back();
                        regen_tileset = true;
                        regen_palette = true;
                    }
                }
                if (ImGui::MenuItem("Redo", "Ctrl+Y", false, redo_history.size()))
                {
                    glfwPollEvents(); frame_count = 0;
                    if (redo_history.size())
                    {
                        undo_history.push_back(tiles);
                        memcpy(tiles, redo_history.back(), sizeof(tiles));
                        redo_history.pop_back();
                        regen_tileset = true;
                        regen_palette = true;
                    }
                }
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }

        // Main menu shortcuts
        if (io.KeyCtrl || io.KeySuper)
        {
            // Undo
            if (ImGui::IsKeyPressedMap(ImGuiKey_Z))
            {
                if (undo_history.size())
                {
                    redo_history.push_back(tiles);
                    memcpy(tiles, undo_history.back(), sizeof(tiles));
                    undo_history.pop_back();
                    regen_tileset = true;
                    regen_palette = true;
                }
            }
            // Redo
            if (ImGui::IsKeyPressedMap(ImGuiKey_Y))
            {
                if (redo_history.size())
                {
                    undo_history.push_back(tiles);
                    memcpy(tiles, redo_history.back(), sizeof(tiles));
                    redo_history.pop_back();
                    regen_tileset = true;
                    regen_palette = true;
                }
            }
        }



        //================================================================================
        //    Main Window
        //================================================================================
        {
            ImGui::Begin("Map Editor Test", NULL, window_flags);

            ImGui::Checkbox("Show tileset", &show_tileset_window);
            ImGui::Checkbox("Show palette", &show_palette_window);
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

            ImGui::Text("Output Format:");
            const char* formats[] = {
                "IntSys (069h)",
                "Rednex ($69)",
                "Binary (0x69)"
            };
            ImGui::SetNextItemWidth(120);
            ImGui::Combo("##Output Formats", &output_format, formats, IM_ARRAYSIZE(formats));

            ImGui::NewLine();

            // Determine whether to use auto-sizing or not
            if (ImGui::Checkbox("Auto-Resize Windows", &auto_resize)) window_flags ^= ImGuiWindowFlags_AlwaysAutoResize;

            ImGui::NewLine();

            // Version info, might update this sometimes??
            ImGui::Text("Map Test Thingy v0.1d");

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
                        if (first_tileset) glGenTextures(1, &tile_textures[i]);
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

                first_tileset = false;
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
                    ImGui::Image((void*)(intptr_t)(use_palette ? palette_textures[i++] : tile_textures[i++]), ImVec2(tile_size, tile_size));
                    ImGui::SetCursorPos(ImVec2(cur_pos.x + tile_size, cur_pos.y));
                }
                ImGui::SetCursorPos(ImVec2(orig_x, (cur_pos.y + tile_size)));
            }

            // Make tileset area draggable
            ImVec2 drag_area = ImVec2(tileset_width * 8 * tile_zoom, tileset_height * 8 * tile_zoom);
            ImGui::SetCursorPos(ImVec2(orig_x, orig_y));
            ImGui::InvisibleButton("canvas", drag_area);

            // Set coordinates for palette usage
            tileset_x = orig_x;
            tileset_y = orig_y;


            /*
             * PIXEL ALTERATION
             *
             * Change the pixel that was clicked.
             */

            static bool first_click = true;

            if (ImGui::IsMouseDown(0) && ImGui::IsWindowFocused() && !ImGui::GetMouseCursor())
            {
                mouse_pos = ImGui::GetMousePos();
                win_pos = ImGui::GetWindowPos();

                // Check where the mouse was clicked relative to the current window
                int pos_x = mouse_pos.x - win_pos.x - style.WindowPadding.x + ImGui::GetScrollX();
                int pos_y = mouse_pos.y - win_pos.y - style.WindowPadding.y + ImGui::GetScrollY() - ImGui::GetCurrentWindow()->TitleBarHeight();

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
                        // Undo/Redo functionality
                        if (first_click)
                        {
                            first_click = false;
                            redo_history.clear();
                            if (undo_history.Size == 10)
                                undo_history.erase(undo_history.begin());
                            undo_history.push_back(tiles);
                        }

                        if (current_tool == TOOL_PIXEL)
                        {
                            // Set pixel color (default: BLACK)
                            tiles[tile_y][tile_x][(pixel_y * 8) + pixel_x] = current_color;
                        }
                        else if (current_tool == TOOL_FILL)
                        {
                            unsigned char* cur_tile = (unsigned char*)tiles[tile_y][tile_x];
                            unsigned int cur_pixel = *(cur_tile + (pixel_y * 8) + pixel_x);
                            fill_color(cur_tile, pixel_x, pixel_y, cur_pixel, current_color);
                        }

                        // Regenerate the texture
                        glBindTexture(GL_TEXTURE_2D, tile_textures[tex_id]);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                        glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, rgb_mask);
                        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
                        glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, 8, 8, 0, GL_RED, GL_UNSIGNED_BYTE, tiles[tile_y][tile_x]);
                        regen_palette = true;
                    }
                    // Select the tile if in cell editing mode
                    else if (edit_mode == CELL_EDIT && clicked_inside)
                    {
                        current_tile = tile_textures[tex_id];
                        current_tile_id = tex_id;
                    }
                }
            }

            if (!ImGui::IsMouseDown(0) && !first_click)
                first_click = true;

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



            // TILE HEIGHT
            ImGui::AlignTextToFramePadding();
            ImGui::Text("Height: ");
            ImGui::SameLine(64);

            ImGui::SetNextItemWidth(75);
            ImGui::InputScalar("## Tile Height", ImGuiDataType_U8, &tileset_height, &u8_one, NULL, "%u");

            if (!tileset_height) tileset_height = 1;

            ImGui::NewLine();


            ImGui::Text("Drawing Tool:");
            const char* tools[] = {
                "Pixel",
                "Fill"
            };
            ImGui::SetNextItemWidth(120);
            ImGui::Combo("##Drawing Tools", &current_tool, tools, IM_ARRAYSIZE(tools));

            ImGui::NewLine();


            // Pick 1 of 4 DMG colors for drawing
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
            ImGui::Image((void*)(intptr_t)(use_palette ? palette_textures[current_tile_id] : tile_textures[current_tile_id]), ImVec2(tile_size, tile_size));

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
                    /*
                     * BL         DG         LG         WH
                     *       |          |          |      
                     * 00         55         AA         FF
                     *       |          |          |
                     * 00-2A    2B--7F     80--D4    D5-FF
                     */
                    for (int a = 0; a < (tileset_width * tileset_height); a++)
                    {
                        if (tileset_data[a] > 0xD5) tileset_data[a] = WHITE;
                        else if (tileset_data[a] >= 0x80 && tileset_data[a] <= 0xD4) tileset_data[a] = LGREY;
                        else if (tileset_data[a] >= 0x2B && tileset_data[a] <= 0x7F) tileset_data[a] = DGREY;
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
                    char* file_data = parse_file(filename, &filesize);
                    tileset_data = (unsigned char*)decompress_tiles(file_data, filesize);
                    tileset_width = 16;
                    tileset_height = ceil((float)filesize / (float)(tileset_width * 8)) / 2;
                    if (filesize > sizeof(tiles))
                        ImGui::OpenPopup("ERROR##Tileset Dimensions");
                    else memcpy(tiles, tileset_data, sizeof(tiles));
                }

                regen_tileset = true;
                first_palette = true;
                regen_palette = true;
                tileset_path = tileset_open.getLastDirectory();
                current_tile = tile_textures[0];
            }

            if (ImGui::BeginPopupModal("ERROR##Tileset Dimensions", NULL, ImGuiWindowFlags_AlwaysAutoResize))
            {
                ImGui::Text("\nFile exceeds the maximum dimensions!\n\n");
                tileset_width = 16;
                tileset_height = 8;
                ImGui::Separator();

                ImGui::SetCursorPosX((ImGui::GetWindowContentRegionWidth() - 120) / 2 + style.WindowPadding.x);
                if (ImGui::Button("OK", ImVec2(120, 0))) { ImGui::CloseCurrentPopup(); }
                ImGui::SetItemDefaultFocus();
                ImGui::EndPopup();
            }

            // Show "Save" dialog
            const bool tileset_save_btn = ImGui::Button("Save File...##Tileset");
            static ImGuiFs::Dialog tileset_save;
            const char* save_path = tileset_save.saveFileDialog(tileset_save_btn, tileset_path, "tileset.dat", "");
            if (strlen(save_path) > 0)
            {
                char* file_data = compress_tiles((unsigned char*)tiles, tileset_width * 8 * tileset_height * 8);
                save_file((unsigned char*)file_data, tileset_save.getChosenPath(), tileset_width * 8 * ((tileset_height * 8) / 4), output_format);
                tileset_path = tileset_open.getLastDirectory();
            }

            // Show "Save Image" dialog
            const bool tileset_img_save_btn = ImGui::Button("Save PNG...##Tileset");
            static ImGuiFs::Dialog tileset_img_save;
            const char* img_save_path = tileset_img_save.saveFileDialog(tileset_img_save_btn, tileset_path, "tileset.png", "");
            if (strlen(img_save_path) > 0)
            {
                unsigned char* png_data = (unsigned char*)IM_ALLOC(tileset_width * 8 * tileset_height * 8);

                // Convert blocky image to PNG data
                int tile_x, tile_y;
                for (tile_y = 0; tile_y < tileset_height; tile_y++)
                {
                    for (tile_x = 0; tile_x < tileset_width; tile_x++)
                    {
                        get_tile((unsigned char*)tiles, tileset_width, tile_x, tile_y, png_data, tileset_width, tile_x, tile_y);
                    }
                }

                stbi_write_png(tileset_img_save.getChosenPath(), tileset_width * 8, tileset_height * 8, 1, (const void*)png_data, tileset_width * 8);

                IM_FREE(png_data);

                tileset_path = tileset_open.getLastDirectory();
            }

            ImGui::End();
        }



        //================================================================================
        //    Palette Window
        //================================================================================
        if (show_palette_window)
        {
            ImGui::Begin("Palette", &show_palette_window, window_flags);


            // Only run this code if the palette needs regenerating
            if (regen_palette)
            {
                int tile_x, tile_y;
                int i = 0;

                // Generate the initial palette
                if (first_palette)
                {
                    for (tile_y = 0; tile_y < tileset_height; tile_y++)
                    {
                        for (tile_x = 0; tile_x < tileset_width; tile_x++)
                        {
                            for (i = 0; i < 64; i++)
                            {
                                // Check the current pixel
                                unsigned char cur_pixel = tiles[tile_y][tile_x][i];

                                // Process each color
                                unsigned char a, b, c;
                                switch (cur_pixel)
                                {
                                    case WHITE:
                                        a = cur_palette[0];
                                        b = cur_palette[1];
                                        c = cur_palette[2];
                                        break;
                                    case LGREY:
                                        a = cur_palette[3];
                                        b = cur_palette[4];
                                        c = cur_palette[5];
                                        break;
                                    case DGREY:
                                        a = cur_palette[6];
                                        b = cur_palette[7];
                                        c = cur_palette[8];
                                        break;
                                    case BLACK:
                                        a = cur_palette[9];
                                        b = cur_palette[10];
                                        c = cur_palette[11];
                                        break;
                                    default:
                                        a = 0; b = 0; c = 0; break;
                                }
                                color_tiles[tile_y][tile_x][i * 3] = a;
                                color_tiles[tile_y][tile_x][(i * 3) + 1] = b;
                                color_tiles[tile_y][tile_x][(i * 3) + 2] = c;
                            }
                        }
                    }
                }


                // Re-color if needed
                for (i = 0; i < tileset_width * tileset_height; i++)
                {
                    int tile_y = i / tileset_width;
                    int tile_x = i % tileset_width;
                    unsigned char pal = palettes[i];

                    for (int z = 0; z < 64; z++)
                    {
                        // Check the current pixel
                        unsigned char cur_pixel = tiles[tile_y][tile_x][z];

                        // Process each color
                        unsigned char a, b, c;
                        switch (cur_pixel)
                        {
                            case WHITE:
                                if (pal == PAL_BLUE && palette_type < PAL_DUNGEON)
                                {
                                    a = (daytime_pal ? palette_water[0] : palette_water[12]);
                                    b = (daytime_pal ? palette_water[1] : palette_water[13]);
                                    c = (daytime_pal ? palette_water[2] : palette_water[14]);
                                    break;
                                }
                                a = (pal == PAL_ROOF ? cur_palette[0] : cur_palette[pal * 12]);
                                b = (pal == PAL_ROOF ? cur_palette[1] : cur_palette[(pal * 12) + 1]);
                                c = (pal == PAL_ROOF ? cur_palette[2] : cur_palette[(pal * 12) + 2]);
                                break;
                            case LGREY:
                                if (pal == PAL_BLUE && palette_type < PAL_DUNGEON)
                                {
                                    a = (daytime_pal ? palette_water[3] : palette_water[15]);
                                    b = (daytime_pal ? palette_water[4] : palette_water[16]);
                                    c = (daytime_pal ? palette_water[5] : palette_water[17]);
                                    break;
                                }
                                a = (pal == PAL_ROOF ? cur_roof[0] : cur_palette[(pal * 12) + 3]);
                                b = (pal == PAL_ROOF ? cur_roof[1] : cur_palette[(pal * 12) + 4]);
                                c = (pal == PAL_ROOF ? cur_roof[2] : cur_palette[(pal * 12) + 5]);
                                break;
                            case DGREY:
                                if (pal == PAL_BLUE && palette_type < PAL_DUNGEON)
                                {
                                    a = (daytime_pal ? palette_water[6] : palette_water[18]);
                                    b = (daytime_pal ? palette_water[7] : palette_water[19]);
                                    c = (daytime_pal ? palette_water[8] : palette_water[20]);
                                    break;
                                }
                                a = (pal == PAL_ROOF ? cur_roof[3] : cur_palette[(pal * 12) + 6]);
                                b = (pal == PAL_ROOF ? cur_roof[4] : cur_palette[(pal * 12) + 7]);
                                c = (pal == PAL_ROOF ? cur_roof[5] : cur_palette[(pal * 12) + 8]);
                                break;
                            case BLACK:
                                if (pal == PAL_BLUE && palette_type < PAL_DUNGEON)
                                {
                                    a = (daytime_pal ? palette_water[9] : palette_water[21]);
                                    b = (daytime_pal ? palette_water[10] : palette_water[22]);
                                    c = (daytime_pal ? palette_water[11] : palette_water[23]);
                                    break;
                                }
                                a = (pal == PAL_ROOF ? cur_palette[9] : cur_palette[(pal * 12) + 9]);
                                b = (pal == PAL_ROOF ? cur_palette[10] : cur_palette[(pal * 12) + 10]);
                                c = (pal == PAL_ROOF ? cur_palette[11] : cur_palette[(pal * 12) + 11]);
                                break;
                            default:
                                a = 0; b = 0; c = 0; break;
                        }
                        color_tiles[tile_y][tile_x][z * 3] = a;
                        color_tiles[tile_y][tile_x][(z * 3) + 1] = b;
                        color_tiles[tile_y][tile_x][(z * 3) + 2] = c;
                    }
                }


                // Generate the palette textures
                i = 0;
                for (tile_y = 0; tile_y < tileset_height; tile_y++)
                {
                    for (tile_x = 0; tile_x < tileset_width; tile_x++)
                    {
                        if (first_palette) glGenTextures(1, &palette_textures[i]);
                        glBindTexture(GL_TEXTURE_2D, palette_textures[i]);

                        // GL_NEAREST = Nearest Neighbor = disable anti-aliasing
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

                        // Get them pixels bruh
                        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
                        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 8, 8, 0, GL_RGB, GL_UNSIGNED_BYTE, color_tiles[tile_y][tile_x]);
                        i++;
                    }
                }

                first_palette = false;
                regen_palette = false;
            }


            ImGuiColorEditFlags pal_flags = ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_NoPicker | ImGuiColorEditFlags_NoTooltip;


            // Set an individual invisible button for each tile
            int orig_x = ImGui::GetCursorPos().x;
            for (int i = 0; i < tileset_width * tileset_height; i++)
            {
                ImGui::PushID(i);


                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.00f, 0.0f));

                if ((i % tileset_width) != 0) ImGui::SameLine();
                else if (i) ImGui::SetCursorPos(ImVec2(orig_x, ImGui::GetCursorPos().y + (8 * tile_zoom)));

                ImGui::PopStyleVar();


                ImVec2 click_area = ImVec2(8 * tile_zoom, 8 * tile_zoom);
                if (ImGui::InvisibleButton("##color-tile", click_area))
                    ImGui::OpenPopup("Palette List");


                /*
                 * PALETTE POPUP
                 *
                 * Choose the color of individual tiles.
                 */
                const char* palette_colors[] = {
                    "Grey",
                    "Red",
                    "Green",
                    "Blue",
                    "Yellow",
                    "Brown",
                    "Roof",
                    "Message"
                };
                if (ImGui::BeginPopup("Palette List"))
                {
                    ImGui::BeginGroup();
                    ImGui::Text("Palettes");

                    for (int n = 0; n < 8; n++)
                    {
                        ImGui::PushID(n);

                        if (n % 8)
                            ImGui::SameLine(0.0f, ImGui::GetStyle().ItemSpacing.y);

                        bool btn_clicked = ImGui::ColorButton("##palette", pal_colors[n], pal_flags, ImVec2(20, 20));
                        if (ImGui::IsItemHovered())
                            ImGui::SetTooltip("%s", palette_colors[n]);
                        if (btn_clicked)
                        {
                            int tile_y = i / tileset_width;
                            int tile_x = i % tileset_width;

                            palettes[i] = n;

                            for (int z = 0; z < 64; z++)
                            {
                                // Check the current pixel
                                unsigned char cur_pixel = tiles[tile_y][tile_x][z];

                                // Process each color
                                unsigned char a, b, c;
                                switch (cur_pixel)
                                {
                                    case WHITE:
                                        if (n == PAL_BLUE && palette_type < PAL_DUNGEON)
                                        {
                                            a = (daytime_pal ? palette_water[0] : palette_water[12]);
                                            b = (daytime_pal ? palette_water[1] : palette_water[13]);
                                            c = (daytime_pal ? palette_water[2] : palette_water[14]);
                                            break;
                                        }
                                        a = (n == PAL_ROOF ? cur_palette[0] : cur_palette[n * 12]);
                                        b = (n == PAL_ROOF ? cur_palette[1] : cur_palette[(n * 12) + 1]);
                                        c = (n == PAL_ROOF ? cur_palette[2] : cur_palette[(n * 12) + 2]);
                                        break;
                                    case LGREY:
                                        if (n == PAL_BLUE && palette_type < PAL_DUNGEON)
                                        {
                                            a = (daytime_pal ? palette_water[3] : palette_water[15]);
                                            b = (daytime_pal ? palette_water[4] : palette_water[16]);
                                            c = (daytime_pal ? palette_water[5] : palette_water[17]);
                                            break;
                                        }
                                        a = (n == PAL_ROOF ? cur_roof[0] : cur_palette[(n * 12) + 3]);
                                        b = (n == PAL_ROOF ? cur_roof[1] : cur_palette[(n * 12) + 4]);
                                        c = (n == PAL_ROOF ? cur_roof[2] : cur_palette[(n * 12) + 5]);
                                        break;
                                    case DGREY:
                                        if (n == PAL_BLUE && palette_type < PAL_DUNGEON)
                                        {
                                            a = (daytime_pal ? palette_water[6] : palette_water[18]);
                                            b = (daytime_pal ? palette_water[7] : palette_water[19]);
                                            c = (daytime_pal ? palette_water[8] : palette_water[20]);
                                            break;
                                        }
                                        a = (n == PAL_ROOF ? cur_roof[3] : cur_palette[(n * 12) + 6]);
                                        b = (n == PAL_ROOF ? cur_roof[4] : cur_palette[(n * 12) + 7]);
                                        c = (n == PAL_ROOF ? cur_roof[5] : cur_palette[(n * 12) + 8]);
                                        break;
                                    case BLACK:
                                        if (n == PAL_BLUE && palette_type < PAL_DUNGEON)
                                        {
                                            a = (daytime_pal ? palette_water[9] : palette_water[21]);
                                            b = (daytime_pal ? palette_water[10] : palette_water[22]);
                                            c = (daytime_pal ? palette_water[11] : palette_water[23]);
                                            break;
                                        }
                                        a = (n == PAL_ROOF ? cur_palette[9] : cur_palette[(n * 12) + 9]);
                                        b = (n == PAL_ROOF ? cur_palette[10] : cur_palette[(n * 12) + 10]);
                                        c = (n == PAL_ROOF ? cur_palette[11] : cur_palette[(n * 12) + 11]);
                                        break;
                                    default:
                                        a = 0; b = 0; c = 0; break;
                                }
                                color_tiles[tile_y][tile_x][z * 3] = a;
                                color_tiles[tile_y][tile_x][(z * 3) + 1] = b;
                                color_tiles[tile_y][tile_x][(z * 3) + 2] = c;
                            }

                            regen_palette = true;
                            ImGui::CloseCurrentPopup();
                        }

                        ImGui::PopID();
                    }
                    ImGui::EndGroup();
                    ImGui::EndPopup();
                }

                ImGui::PopID();
                ImGui::SameLine();
            }


            
            /*
             * PALETTE DISPLAY
             *
             * Display a color palette overlaid on top of the tileset.
             */

            if (!first_tileset)
            {
                ImGui::SetCursorPos(ImVec2(tileset_x, tileset_y));
                int orig_x = tileset_x;
                int tile_size = 8 * tile_zoom;

                int i = 0;
                int pal_x, pal_y;
                for (pal_y = 0; pal_y < tileset_height; pal_y++)
                {
                    for (pal_x = 0; pal_x < tileset_width; pal_x++)
                    {
                        cur_pos = ImGui::GetCursorPos();
                        ImGui::Image((void*)(intptr_t)palette_textures[i++], ImVec2(tile_size, tile_size));
                        ImGui::SetCursorPos(ImVec2(cur_pos.x + tile_size, cur_pos.y));
                    }
                    ImGui::SetCursorPos(ImVec2(orig_x, (cur_pos.y + tile_size)));
                }
            }

            ImGui::End();



            //================================================================================
            //    Palette Controls Window
            //================================================================================
            ImGui::Begin("Palette Ctrl", &show_palette_window, window_flags);


            ImGui::Text("Palette Type:");
            const char* palette_options[] = {
                "Morning",
                "Day",
                "Night",
                "Dungeon",
                "Dark Cave",
                "Building"
            };
            ImGui::SetNextItemWidth(120);
            if (ImGui::Combo("##Pallete Types", &palette_type, palette_options, IM_ARRAYSIZE(palette_options)))
            {
                switch(palette_type)
                {
                    case 0: cur_palette = (unsigned char*)palette_morning;    daytime_pal = true;   cur_roof = (unsigned char*)(palette_roof + (roof_id * 12));      break;
                    case 1: cur_palette = (unsigned char*)palette_day;        daytime_pal = true;   cur_roof = (unsigned char*)(palette_roof + (roof_id * 12));      break;
                    case 2: cur_palette = (unsigned char*)palette_night;      daytime_pal = false;  cur_roof = (unsigned char*)(palette_roof + (roof_id * 12) + 6);  break;
                    case 3: cur_palette = (unsigned char*)palette_day;        daytime_pal = false;  cur_roof = (unsigned char*)(palette_roof + (roof_id * 12) + 6);  break;
                    case 4: cur_palette = (unsigned char*)palette_dark_cave;  daytime_pal = false;  cur_roof = (unsigned char*)(palette_roof + (roof_id * 12) + 6);  break;
                    case 5: cur_palette = (unsigned char*)palette_building;   daytime_pal = true;   cur_roof = (unsigned char*)(palette_roof + (roof_id * 12));      break;
                    default: break;
                }
                regen_palette = true;
            }


            ImGui::NewLine();


            ImGui::Text("Roof Type:");
            const char* roof_options[] = {
                "Unused",
                "Olivine City",
                "Mahogany Town",
                "Dungeon",
                "Ecruteak City",
                "Blackthorn City",
                "Cinnabar Island",
                "Cerulean City",
                "Azalea Town",
                "Lake of Rage",
                "Violet City",
                "Goldenrod City",
                "Vermilion City",
                "Pallet Town",
                "Pewter City",
                "Route 4",
                "Indigo Plateau",
                "Fuchsia City",
                "Lavender Town",
                "Route 28",
                "Communication",
                "Celadon City",
                "Cianwood City",
                "Viridian City",
                "New Bark Town",
                "Saffron City",
                "Cherrygrove City"
            };
            const ImVec4 roof_colors[] = {
                ImVec4(172.0f/255.0f, 172.0f/255.0f, 172.0f/255.0f, 1.0f),
                ImVec4(115.0f/255.0f, 139.0f/255.0f, 255.0f/255.0f, 1.0f),
                ImVec4(98.0f/255.0f,  156.0f/255.0f, 0.0f/255.0f,   1.0f),
                ImVec4(172.0f/255.0f, 172.0f/255.0f, 172.0f/255.0f, 1.0f),
                ImVec4(255.0f/255.0f, 156.0f/255.0f, 0.0f/255.0f,   1.0f),
                ImVec4(90.0f/255.0f,  82.0f/255.0f,  131.0f/255.0f, 1.0f),
                ImVec4(255.0f/255.0f, 82.0f/255.0f,  0.0f/255.0f,   1.0f),
                ImVec4(139.0f/255.0f, 222.0f/255.0f, 255.0f/255.0f, 1.0f),
                ImVec4(180.0f/255.0f, 164.0f/255.0f, 82.0f/255.0f,  1.0f),
                ImVec4(255.0f/255.0f, 65.0f/255.0f,  32.0f/255.0f,  1.0f),
                ImVec4(197.0f/255.0f, 115.0f/255.0f, 255.0f/255.0f, 1.0f),
                ImVec4(205.0f/255.0f, 205.0f/255.0f, 0.0f/255.0f,   1.0f),
                ImVec4(222.0f/255.0f, 189.0f/255.0f, 8.0f/255.0f,   1.0f),
                ImVec4(222.0f/255.0f, 230.0f/255.0f, 255.0f/255.0f, 1.0f),
                ImVec4(156.0f/255.0f, 156.0f/255.0f, 131.0f/255.0f, 1.0f),
                ImVec4(115.0f/255.0f, 139.0f/255.0f, 255.0f/255.0f, 1.0f),
                ImVec4(172.0f/255.0f, 172.0f/255.0f, 172.0f/255.0f, 1.0f),
                ImVec4(255.0f/255.0f, 148.0f/255.0f, 238.0f/255.0f, 1.0f),
                ImVec4(189.0f/255.0f, 123.0f/255.0f, 255.0f/255.0f, 1.0f),
                ImVec4(172.0f/255.0f, 172.0f/255.0f, 205.0f/255.0f, 1.0f),
                ImVec4(172.0f/255.0f, 172.0f/255.0f, 172.0f/255.0f, 1.0f),
                ImVec4(156.0f/255.0f, 255.0f/255.0f, 123.0f/255.0f, 1.0f),
                ImVec4(123.0f/255.0f, 82.0f/255.0f,  255.0f/255.0f, 1.0f),
                ImVec4(172.0f/255.0f, 255.0f/255.0f, 57.0f/255.0f,  1.0f),
                ImVec4(164.0f/255.0f, 255.0f/255.0f, 115.0f/255.0f, 1.0f),
                ImVec4(255.0f/255.0f, 213.0f/255.0f, 0.0f/255.0f,   1.0f),
                ImVec4(255.0f/255.0f, 115.0f/255.0f, 230.0f/255.0f, 1.0f),
                ImVec4(255.0f/255.0f, 41.0f/255.0f,  172.0f/255.0f, 1.0f)
            };
            static const char* roof_current = roof_options[0];
            ImGui::SetNextItemWidth(150);
            if (ImGui::BeginCombo("##Roof Colors", roof_current, 0))
            {
                for (int x = 0; x < IM_ARRAYSIZE(roof_options); x++)
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, roof_colors[x]);
                    bool selected = (roof_current == roof_options[x]);
                    if (ImGui::Selectable(roof_options[x], selected))
                    {
                        roof_current = roof_options[x];
                        roof_id = x;
                        cur_roof = (unsigned char*)(palette_roof + (x * 12));
                        // Move onto nighttime palette if it's a darker palette
                        if (!daytime_pal) cur_roof += 6;
                        regen_palette = true;
                    }
                    if (selected)
                        ImGui::SetItemDefaultFocus();
                    ImGui::PopStyleColor();
                }
                ImGui::EndCombo();
            }

            
            ImGui::NewLine();


            ImGui::Checkbox("Use palette", &use_palette);
            ImGui::NewLine();

            

            // Show "Open" dialog
            const bool pal_open_btn = ImGui::Button("Open File...##Palette");
            static ImGuiFs::Dialog pal_open;
            filename = pal_open.chooseFileDialog(pal_open_btn, palette_path, "");
            if (strlen(filename) > 0)
            {
                unsigned char* file_data = (unsigned char*)parse_file(filename, &filesize);
                memcpy(palettes, file_data, sizeof(palettes));
                palette_path = pal_open.getLastDirectory();
                regen_palette = true;
            }

            // Show "Save" dialog
            const bool pal_save_btn = ImGui::Button("Save File...##Palette");
            static ImGuiFs::Dialog pal_save;
            const char* save_path = pal_save.saveFileDialog(pal_save_btn, palette_path, "palette.pal", "");
            if (strlen(save_path) > 0)
            {
                save_file((unsigned char*)palettes, pal_save.getChosenPath(), tileset_width * tileset_height, output_format);
                palette_path = pal_open.getLastDirectory();
            }

            // Show "Save Image" dialog
            const bool pal_img_save_btn = ImGui::Button("Save PNG...##Palette");
            static ImGuiFs::Dialog pal_img_save;
            const char* img_save_path = pal_img_save.saveFileDialog(pal_img_save_btn, palette_path, "palette.png", "");
            if (strlen(img_save_path) > 0)
            {
                unsigned char* png_data = (unsigned char*)IM_ALLOC(tileset_width * 8 * 3 * tileset_height * 8);

                // Convert blocky image to PNG data
                int tile_x, tile_y;
                for (tile_y = 0; tile_y < tileset_height; tile_y++)
                {
                    for (tile_x = 0; tile_x < tileset_width; tile_x++)
                    {
                        get_tile((unsigned char*)color_tiles, tileset_width, tile_x, tile_y, png_data, tileset_width, tile_x, tile_y, 3);
                    }
                }

                stbi_write_png(pal_img_save.getChosenPath(), tileset_width * 8, tileset_height * 8, 3, (const void*)png_data, tileset_width * 8 * 3);

                IM_FREE(png_data);

                palette_path = pal_img_save.getLastDirectory();
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
                            int tile_index = (tile_y * 4) + tile_x;
                            int tile_num = cell_tiles[(cell_y * cell_width) + cell_x][tile_index];
                            int tile_tex = (use_palette ? palette_textures[tile_num] : tile_textures[tile_num]);
                            ImGui::Image((void*)(intptr_t)tile_tex, ImVec2(tile_size, tile_size));
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

            // Make cell area draggable
            ImVec2 drag_area = ImVec2(cell_width * 4 * 8 * cell_zoom, cell_height * 4 * 8 * cell_zoom);
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

                int pos_x = mouse_pos.x - win_pos.x - style.WindowPadding.x + ImGui::GetScrollX();
                int pos_y = mouse_pos.y - win_pos.y - style.WindowPadding.y + ImGui::GetScrollY() - ImGui::GetCurrentWindow()->TitleBarHeight();

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
                        int tex_offset = (use_palette ? palette_textures[0] : tile_textures[0]);
                        cell_tiles[(cell_y * cell_width) + cell_x][tile_index] = current_tile - tex_offset;
                    }
                    else if (edit_mode == MAP_EDIT && clicked_inside)
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



            // Display current cell
            ImGui::Text("Current Cell: ");
            ImGui::SameLine();

            cur_pos = ImGui::GetCursorPos();
            cur_cell_x = cur_pos.x;
            cur_cell_y = cur_pos.y;
            for (tile_y = 0; tile_y < 4; tile_y++)
            {
                for (tile_x = 0; tile_x < 4; tile_x++)
                {
                    cur_pos = ImGui::GetCursorPos();
                    int tile_idx = (tile_y * 4) + tile_x;
                    int tex_offset = (use_palette ? palette_textures[0] : tile_textures[0]);
                    int tile_tex = cell_tiles[current_cell][tile_idx] + tex_offset;
                    ImGui::Image((void*)(intptr_t)tile_tex, ImVec2(tile_size, tile_size));
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
                unsigned char* file_data = (unsigned char*)parse_file(filename, &filesize);
                // Determine initial cell height
                if (filesize > 1024) cell_width = 16;
                else cell_width = 8;
                if (filesize > 2048) cell_height = 16;
                else cell_height = 8;

                if (filesize > sizeof(cell_tiles))
                    ImGui::OpenPopup("ERROR##Cell Dimensions");
                else memcpy(cell_tiles, file_data, sizeof(cell_tiles));

                cell_path = cell_open.getLastDirectory();
            }

            if (ImGui::BeginPopupModal("ERROR##Cell Dimensions", NULL, ImGuiWindowFlags_AlwaysAutoResize))
            {
                ImGui::Text("\nFile exceeds the maximum dimensions!\n\n");
                cell_width = 8;
                cell_height = 8;
                ImGui::Separator();

                ImGui::SetCursorPosX((ImGui::GetWindowContentRegionWidth() - 120) / 2 + style.WindowPadding.x);
                if (ImGui::Button("OK", ImVec2(120, 0))) { ImGui::CloseCurrentPopup(); }
                ImGui::SetItemDefaultFocus();
                ImGui::EndPopup();
            }

            // Show "Save" dialog
            const bool cell_save_btn = ImGui::Button("Save File...##Cell");
            static ImGuiFs::Dialog cell_save;
            const char* save_path = cell_save.saveFileDialog(cell_save_btn, cell_path, "cell.cel", "");
            if (strlen(save_path) > 0)
            {
                cell_path = cell_save.getLastDirectory();
                save_file((unsigned char*)cell_tiles, cell_save.getChosenPath(), cell_width * 4 * cell_height * 4, output_format);
            }

            // Show "Save Image" dialog
            const bool cell_img_save_btn = ImGui::Button("Save PNG...##Cell");
            static ImGuiFs::Dialog cell_img_save;
            const char* img_save_path = cell_img_save.saveFileDialog(cell_img_save_btn, cell_path, "cell.png", "");
            if (strlen(img_save_path) > 0)
            {
                unsigned char* png_data = (unsigned char*)IM_ALLOC((cell_width * 4 * 8 * 3) * (cell_height * 4 * 8));

                // Convert blocky image to PNG data
                int cell_x, cell_y;
                int tile_x, tile_y;
                int bpp = (use_palette ? 3 : 1);
                unsigned char* bpp_tiles = (use_palette ? (unsigned char*)color_tiles : (unsigned char*)tiles);
                for (cell_y = 0; cell_y < cell_height; cell_y++)
                {
                    for (cell_x = 0; cell_x < cell_width; cell_x++)
                    {
                        for (tile_y = 0; tile_y < 4; tile_y++)
                        {
                            for (tile_x = 0; tile_x < 4; tile_x++)
                            {
                                int cell_idx = (cell_y * cell_width) + cell_x;
                                int tile_idx = (tile_y * 4) + tile_x;
                                int tile_num = cell_tiles[cell_idx][tile_idx];
                                int tile_ypos = tile_num / tileset_width;
                                int tile_xpos = tile_num % tileset_width;
                                int cell_ypos = (cell_y * 4) + tile_y;
                                int cell_xpos = (cell_x * 4) + tile_x;
                                get_tile(bpp_tiles, tileset_width, tile_xpos, tile_ypos, png_data, cell_width * 4, cell_xpos, cell_ypos, bpp);
                            }
                        }
                    }
                }

                stbi_write_png(cell_img_save.getChosenPath(), cell_width * 4 * 8, cell_height * 4 * 8, bpp, (const void*)png_data, cell_width * 4 * 8 * bpp);

                IM_FREE(png_data);

                cell_path = cell_img_save.getLastDirectory();
            }

            ImGui::End();
        }



        //================================================================================
        //    Map Window
        //================================================================================
        if (show_map_window)
        {
            ImGui::SetNextWindowSizeConstraints(ImVec2(-1, -1),  ImVec2(500, 500));
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
                            int tile_num = cell_tiles[cell_index][tile_index];
                            int tile_tex = (use_palette ? palette_textures[tile_num] : tile_textures[tile_num]);
                            ImGui::Image((void*)(intptr_t)tile_tex, ImVec2(tile_size, tile_size));
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



            // Make map area draggable
            ImVec2 drag_area = ImVec2(map_width * 4 * 8 * map_zoom, map_height * 4 * 8 * map_zoom);
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

                int pos_x = mouse_pos.x - win_pos.x - style.WindowPadding.x + ImGui::GetScrollX();
                int pos_y = mouse_pos.y - win_pos.y - style.WindowPadding.y + ImGui::GetScrollY() - ImGui::GetCurrentWindow()->TitleBarHeight();

                int max_width = map_width * 4 * 8 * map_zoom;
                int max_height = map_height * 4 * 8 * map_zoom;

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
                        // Set cell (default: 0)
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
                unsigned char* file_data = (unsigned char*)parse_file(filename, &filesize);
                if (filesize > sizeof(map_cells))
                    ImGui::OpenPopup("ERROR##Map Dimensions");
                else memcpy(map_cells, file_data, sizeof(map_cells));

                map_path = map_open.getLastDirectory();
            }

            if (ImGui::BeginPopupModal("ERROR##Map Dimensions", NULL, ImGuiWindowFlags_AlwaysAutoResize))
            {
                ImGui::Text("\nFile exceeds the maximum dimensions!\n\n");
                ImGui::Separator();

                ImGui::SetCursorPosX((ImGui::GetWindowContentRegionWidth() - 120) / 2 + style.WindowPadding.x);
                if (ImGui::Button("OK", ImVec2(120, 0))) { ImGui::CloseCurrentPopup(); }
                ImGui::SetItemDefaultFocus();
                ImGui::EndPopup();
            }

            // Show "Save" dialog
            const bool map_save_btn = ImGui::Button("Save File...##Map");
            static ImGuiFs::Dialog map_save;
            const char* save_path = map_save.saveFileDialog(map_save_btn, map_path, "map.map", "");
            if (strlen(save_path) > 0)
            {
                map_path = map_save.getLastDirectory();
                save_file((unsigned char*)map_cells, map_save.getChosenPath(), map_width * map_height, output_format);
            }

            // Show "Save Image" dialog
            const bool map_img_save_btn = ImGui::Button("Save PNG...##Map");
            static ImGuiFs::Dialog map_img_save;
            const char* img_save_path = map_img_save.saveFileDialog(map_img_save_btn, map_path, "map.png", "");
            if (strlen(img_save_path) > 0)
            {
                unsigned char* png_data = (unsigned char*)IM_ALLOC((map_width * 4 * 8 * 3) * (map_height * 4 * 8));

                // Convert blocky image to PNG data
                int map_x, map_y;
                int tile_x, tile_y;
                int bpp = (use_palette ? 3 : 1);
                unsigned char* bpp_tiles = (use_palette ? (unsigned char*)color_tiles : (unsigned char*)tiles);
                for (map_y = 0; map_y < map_height; map_y++)
                {
                    for (map_x = 0; map_x < map_width; map_x++)
                    {
                        int cell_idx = map_cells[(map_y * map_width) + map_x];
                        for (tile_y = 0; tile_y < 4; tile_y++)
                        {
                            for (tile_x = 0; tile_x < 4; tile_x++)
                            {
                                int tile_idx = (tile_y * 4) + tile_x;
                                int tile_num = cell_tiles[cell_idx][tile_idx];
                                int tile_ypos = tile_num / tileset_width;
                                int tile_xpos = tile_num % tileset_width;
                                int map_ypos = (map_y * 4) + tile_y;
                                int map_xpos = (map_x * 4) + tile_x;
                                get_tile(bpp_tiles, tileset_width, tile_xpos, tile_ypos, png_data, map_width * 4, map_xpos, map_ypos, bpp);
                            }
                        }
                    }
                }
                
                stbi_write_png(map_img_save.getChosenPath(), map_width * 4 * 8, map_height * 4 * 8, bpp, (const void*)png_data, map_width * 4 * 8 * bpp);

                IM_FREE(png_data);

                map_path = map_img_save.getLastDirectory();
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
