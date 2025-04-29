#include "app.hpp"

static td_demo demo;

static int td_app_show_core(void* ctx)
{
    td_app* app = (td_app*)ctx;
    return app->show_core();
}

td_app::td_app()
{
    show_func = td_app_show_core;
    show_func_ctx = (void*)this;
}

td_app::~td_app()
{
}

int td_app::show_core()
{
    demo.display();

    return 0;
}
