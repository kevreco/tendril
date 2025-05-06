#include "app.hpp"
#include "tendril_internal.hpp"

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

    if (cfg.show_feedback_window)
    {
        display_feedback_window(&cfg.show_feedback_window);
    }

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

        if (ImGui::BeginMenu("Options"))
        {
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Help"))
        {
            if (ImGui::MenuItem("Feedback", NULL, &cfg.show_feedback_window))
            {
                // Bring about window to front
                ImGui::SetWindowFocus("Feedback");
            }

            if (ImGui::MenuItem("About", NULL, &cfg.show_about_window))
            {
                // Bring about window to front
                ImGui::SetWindowFocus("About");
            }
           
            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }
}

void td_app::display_about_window(bool* p_open)
{
    if (ImGui::Begin("About", p_open, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse))
    {
        ImGui::Text(TD_APP_NAME);
        ImGui::Text("Tendril is a C++ library dedicated to draw");
        ImGui::SameLine();
        ImGui::TextLinkOpenURL("Tendrilis", "https://www.omniglot.com/conscripts/tendrilis.htm");
        ImGui::Text("Tendrilis is script created by");
        ImGui::SameLine();
        ImGui::TextLinkOpenURL("Anomalis", "https://linktr.ee/Tendrilis");
        ImGui::Separator();
        ImGui::Text("Version: %s (%d)", TD_APP_VERSION_TEXT, TD_APP_VERSION_NUMBER);
        ImGui::Separator();
        ImGui::Text("(c) kevreco");
        ImGui::Text("Developed by kevreco.");
        ImGui::Text("%s is licensed under the MIT License", TD_APP_NAME);
        ImGui::Separator();
        {
            ImGui::TextLinkOpenURL("Homepage", "https://github.com/kevreco/tendril");
            ImGui::SameLine();
            ImGui::Text("-");
            ImGui::SameLine();
            ImGui::TextLinkOpenURL("README.md", "https://github.com/kevreco/tendril/blob/master/README.md");
            ImGui::SameLine();
            ImGui::Text("-");
            ImGui::SameLine();
            ImGui::TextLinkOpenURL("LICENSE", "https://github.com/kevreco/tendril/blob/master/LICENSE");
        }
        ImGui::Separator();
        ImGui::Text("Backend: %s", identifier());
#ifdef SOURCE_REVISION
        ImGui::Text("Source revision: %s", TD_XSTR(SOURCE_REVISION));
#endif
    }
    ImGui::End();
}

void td_app::display_feedback_window(bool* p_open)
{
    if (ImGui::Begin("Feedback", p_open, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse))
    {
        ImGui::Text("Any suggestion or feedback?");
        ImGui::Text("Feel free to send an email to:");
        // NOTE: The email address is not pre-concatenated as a poor attempt to avoid bot spam.
        static char address[] = "tendril.draw" "?" "outlook" ".com";
        for (size_t i = 0; i < sizeof(address); i += 1)
        {
            if (address[i] == '?')
            {
                address[i] = '@';
            }
        }
        ImGui::InputText("##Email", address, sizeof(address), ImGuiInputTextFlags_ReadOnly);
        ImGui::SameLine();
        if (ImGui::Button(ICON_LC_COPY))
        {
            ImGui::SetClipboardText(address);
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
    ImGuiEx::ApplyCustomTheme();
    ImGuiEx::LoadFonts();
}