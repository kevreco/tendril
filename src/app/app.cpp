#include "app.hpp"

static td_demo demo;

static int td_app_show(void* ctx);
static void td_app_preloop(void* ctx);

td_app::td_app()
{
    show_func = td_app_show;
    show_func_ctx = (void*)this;

    pre_loop_func = td_app_preloop;
    pre_loop_func_ctx = (void*)this;
}

td_app::~td_app()
{
}

static int td_app_show(void* ctx)
{
    td_app* app = (td_app*)ctx;
    (void)app;

    demo.display();

    return 0;
}

static void td_app_preloop(void* ctx)
{
    ImGui::ApplyCustomTheme();
    ImGui::LoadFonts();
}