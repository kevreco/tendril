#define CB_IMPLEMENTATION
#include "cb/cb.h"
#include "cb/cb_add_files.h"
#include "cb/cb_assert.h"

#define XSTR(x) STR(x)
#define STR(x) #x
     
/* Forward declarations */

static const char* build_with(const char* config);
static void my_project(const char* project_name, const char* toolchain, const char* config);

int main()
{
    cb_init();

    build_with("Debug");
    
#ifdef CI_BUILD
    cb_clear(); /* Clear all values of cb. */

    build_with("Release");
#endif

    cb_destroy();

    return 0;
}

static const char* build_with(const char* config)
{
    cb_toolchain_set(cb_toolchain_default_cpp());

    cb_toolchain_t tool_chain = cb_toolchain_get();
    const char* toolchain_name = tool_chain.family;
    
    /* Library */
    {
        my_project("tendril", toolchain_name, config);

        cb_set(cb_BINARY_TYPE, cb_STATIC_LIBRARY);
        
        cb_add(cb_FILES, "./src/tendril/build.cpp");
       
        cb_bake();
    }
    
    /* App  */
    {
        my_project("app", toolchain_name, config);

        cb_add(cb_LINK_PROJECTS, "tendril");

        cb_set(cb_BINARY_TYPE, cb_EXE);

        cb_add(cb_FILES, "./src/app/build.cpp");
        
        cb_add(cb_INCLUDE_DIRECTORIES, "./src/tendril/");
        cb_add(cb_INCLUDE_DIRECTORIES, "./src/app/");
        cb_add(cb_INCLUDE_DIRECTORIES, "./src/app/3rdparty");
        cb_add(cb_INCLUDE_DIRECTORIES, "./src/app/3rdparty/imgui");
        cb_add(cb_INCLUDE_DIRECTORIES, "./src/app/3rdparty/imgui/backends");
#if _WIN32
        cb_add(cb_LIBRARIES, "opengl32");
#else
        cb_add(cb_LIBRARIES, "GL");
        cb_add(cb_LIBRARIES, "glfw");
        cb_add(cb_LIBRARIES, "rt");
        cb_add(cb_LIBRARIES, "m");
#endif
    }

    // Create .exe
    const char* ac_exe = cb_bake();

    if (!ac_exe)
    {
        exit(1);
    }

    /* Copy resources necessary at runtime. */
    const char* out = cb_get_output_directory(cb_current_project(), &tool_chain);
    cb_try_copy_file_to_dir("./resources/dejavu-sans.book.ttf", out);
    cb_try_copy_file_to_dir("./resources/lucide.ttf", out);
    cb_try_copy_file_to_dir("./resources/tendril.ico", out);
    cb_try_copy_file_to_dir("./resources/TendrilisExtra-Regular.ttf", out);
    cb_try_copy_file_to_dir("./resources/Tendrilis-v2-Regular.ttf", out);


    /* Build and run examples. */
    {
        struct example_item {
            const char* name;
            const char* filepath;
        } examples[] =
        {
            "example_01_stroke",          "./src/examples/01_stroke.cpp",
            "example_02_fill",            "./src/examples/02_fill.cpp",
            "example_03_text",            "./src/examples/03_text.cpp",
            "example_04_tendrilis",       "./src/examples/04_tendrilis.cpp",
            "example_05_svg_export",      "./src/examples/05_svg_export.cpp",
            "example_06_svg_path_import", "./src/examples/06_svg_path_import.cpp",
        };

        int example_count = sizeof(examples) / sizeof(examples[0]);

        for (int i = 0; i < example_count; i += 1)
        {
            my_project(examples[i].name, toolchain_name, config);

            cb_add(cb_LINK_PROJECTS, "tendril");

            cb_set(cb_BINARY_TYPE, cb_EXE);

            cb_add(cb_FILES, examples[i].filepath);

            cb_add(cb_INCLUDE_DIRECTORIES, "./src/tendril/");
            
            // Run example
            const char* example_exe = cb_bake();
            const char* example_out_dir = cb_get_output_directory(cb_current_project(), &tool_chain);
            cb_process_in_directory(example_exe, example_out_dir);
        }
    }

#ifndef CI

    /* Launch .exe */
    cb_process_in_directory(ac_exe, out);

#endif

    return ac_exe;
}

/* Shortcut to create a project with default config flags. */
static void my_project(const char* project_name, const char* toolchain, const char* config)
{
    cb_project(project_name);

    cb_set_f(cb_OUTPUT_DIR, ".build/%s_%s/%s/", toolchain, config, project_name);

#ifdef SOURCE_REVISION
    cb_add_f(cb_DEFINES, "SOURCE_REVISION=%d", XSTR(SOURCE_REVISION));
#endif

    cb_bool is_debug = cb_str_equals(config, "Debug");

    if (is_debug && (cb_str_equals(toolchain, "gcc")))
    {
        cb_add(cb_CXFLAGS, "-fsanitize=address"); /* Address sanitizer, same flag for gcc and msvc. */
    }

    if (cb_str_equals(toolchain, "msvc"))
    {
        cb_add(cb_CXFLAGS, "/Zi");   /* Produce debugging information (.pdb) */
        cb_add(cb_CXFLAGS, "/EHsc"); /* Allow exception catching. */
        
        /* Use alternate location for the .pdb.
           In this case it will be next to the .exe */
        cb_add(cb_LFLAGS, "/pdbaltpath:%_PDB%"); 

        cb_add(cb_DEFINES, "UNICODE");
        cb_add(cb_DEFINES, "_UNICODE");
            
        if (is_debug)
        {
            cb_add(cb_LFLAGS, "/MANIFEST:EMBED");
            cb_add(cb_LFLAGS, "/INCREMENTAL:NO"); /* No incremental linking */

            cb_add(cb_CXFLAGS, "-Od");   /* Disable optimization */
            cb_add(cb_DEFINES, "DEBUG"); /* Add DEBUG constant define */

        }
        else
        {
            cb_add(cb_CXFLAGS, "-O1");   /* Optimization level 1 */
        }
    }
    else if (cb_str_equals(toolchain, "gcc"))
    {
        if (is_debug)
        {
            cb_add(cb_CXFLAGS, "-g");    /* Produce debugging information  */
            cb_add(cb_CXFLAGS, "-p");    /* Profile compilation (in case of performance analysis)  */
            cb_add(cb_CXFLAGS, "-O0");   /* Disable optimization */
            cb_add(cb_DEFINES, "DEBUG"); /* Add DEBUG constant define */
        }
        else
        {
            cb_add(cb_CXFLAGS, "-O1");   /* Optimization level 1 */
        }
    }
}