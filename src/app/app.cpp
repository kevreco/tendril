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

void td_app::render()
{
    display_main_menu_bar();

    if (cfg.show_about_window)
    {
        display_about_window(&cfg.show_about_window);
    }

    if (cfg.show_demo_window)
    {
        demo.display();
    }
}

void td_app::display_main_menu_bar()
{
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("View"))
        {
            ImGui::MenuItem("Show Demo", NULL, &cfg.show_demo_window);
            ImGui::EndMenu();
        }

        if (ImGui::MenuItem("About"))
        {
            cfg.show_about_window = true;
            // Bring about window to front
            ImGui::SetWindowFocus("About");
        }

        ImGui::EndMainMenuBar();
    }
}

void td_app::display_about_window(bool* p_open)
{
    if (ImGui::Begin("About", p_open, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("%s %s - (%d)", TD_APP_NAME, TD_APP_VERSION_TEXT, TD_APP_VERSION_NUMBER);
        ImGui::Text("Backend: %s", identifier());

        ImGui::TextLinkOpenURL("Homepage", "https://github.com/kevreco/tendril");
        ImGui::SameLine();
        ImGui::Separator();
        ImGui::Text("(c) kevreco");
        ImGui::Text("Developed by kevreco.");
        ImGui::Text("%s is licensed under the MIT License", TD_APP_NAME);
        {
            ImGui::Text("See");
            ImGui::SameLine();
            ImGui::TextLinkOpenURL("LICENSE", "https://github.com/kevreco/tendril/LICENSE");
            ImGui::SameLine();
            ImGui::Text("and");
            ImGui::SameLine();
            ImGui::TextLinkOpenURL("README.md", "https://github.com/kevreco/tendril/README.md");
            ImGui::SameLine();
            ImGui::Text("for more information.");
        }
    }
    ImGui::End();
}

static int td_app_show(void* ctx)
{
    td_app* app = (td_app*)ctx;
    
    app->render();

    return 0;
}

static void td_app_preloop(void* ctx)
{
    ImGui::ApplyCustomTheme();
    ImGui::LoadFonts();
}