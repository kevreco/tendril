#ifndef IMGUI_PLUS_EXTENSIONS_H
#define IMGUI_PLUS_EXTENSIONS_H

//
// NOTE: This file is not part of imgui.
//

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"
#include "imgui_internal.h"

#ifndef IMGUI_EXTENSION_TEXT_FONT
#define IMGUI_EXTENSION_TEXT_FONT "./dejavu-sans.book.ttf"
#endif
#ifndef IMGUI_EXTENSION_ICONS_FONT
#define IMGUI_EXTENSION_ICONS_FONT "./lucide.ttf"
#endif

#include "IconFontCppHeader/IconsLucid.h"

#include <string>

namespace ImGui {

	struct InputTextCallback_UserData
	{
		std::string* Str;
		ImGuiInputTextCallback  ChainCallback;
		void* ChainCallbackUserData;
	};

	static int InputTextCallback(ImGuiInputTextCallbackData* data)
	{
		InputTextCallback_UserData* user_data = (InputTextCallback_UserData*)data->UserData;
		if (data->EventFlag == ImGuiInputTextFlags_CallbackResize)
		{
			// Resize string callback
			// If for some reason we refuse the new length (BufTextLen) and/or capacity (BufSize) we need to set them back to what we want.
			std::string* str = user_data->Str;
			IM_ASSERT(data->Buf == str->c_str());
			str->resize(data->BufTextLen);
			data->Buf = (char*)str->c_str();
		}
		else if (user_data->ChainCallback)
		{
			// Forward to user callback, if any
			data->UserData = user_data->ChainCallbackUserData;
			return user_data->ChainCallback(data);
		}
		return 0;
	}

	static bool InputText(const char* label, std::string* str, ImGuiInputTextFlags flags = 0, ImGuiInputTextCallback callback = nullptr, void* user_data = nullptr)
	{
		IM_ASSERT((flags & ImGuiInputTextFlags_CallbackResize) == 0);
		flags |= ImGuiInputTextFlags_CallbackResize;

		InputTextCallback_UserData cb_user_data;
		cb_user_data.Str = str;
		cb_user_data.ChainCallback = callback;
		cb_user_data.ChainCallbackUserData = user_data;
		return InputText(label, (char*)str->c_str(), str->capacity() + 1, flags, InputTextCallback, &cb_user_data);
	}

	static bool U32ColorEdit3(const char* label, ImU32* color, ImGuiColorEditFlags flags = ImGuiColorEditFlags_NoAlpha)
	{
		ImVec4 v = ColorConvertU32ToFloat4(*color);
		bool res = ImGui::ColorEdit3(label, (float*)&v, flags);
		if (res)
		{
			*color = ColorConvertFloat4ToU32(v);
		}
		return res;
	}

	static bool U32ColorEdit4(const char* label, ImU32* color, ImGuiColorEditFlags flags = ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_Uint8)
	{
		ImVec4 v = ColorConvertU32ToFloat4(*color);
		bool res = ImGui::ColorEdit4(label, (float*)&v, flags);
		if (res)
		{
			*color = ColorConvertFloat4ToU32(v);
		}
		return res;
	}

	static bool U32ColorPicker3(const char* label, ImU32* color, ImGuiColorEditFlags flags = ImGuiColorEditFlags_NoAlpha)
	{
		ImVec4 v = ColorConvertU32ToFloat4(*color);
		bool res = ImGui::ColorPicker3(label, (float*)&v, flags);
		if (res)
		{
			*color = ColorConvertFloat4ToU32(v);
		}
		return res;
	}

	static void ImageWithBorder(ImTextureID user_texture_id, const ImVec2& image_size)
	{
		ImVec4 border_col = ImGui::GetStyleColorVec4(ImGuiCol_Border);
		ImGui::Image(user_texture_id, image_size, ImVec2(0, 0), ImVec2(1, 1), ImVec4(1, 1, 1, 1), border_col);
	}

	static bool InputTextWithoutLabel(const char* label_id, char* buf, size_t buf_len)
	{
		float avail_x = ImGui::GetContentRegionAvail().x;
		PushItemWidth(avail_x);
		bool b = InputText(label_id, buf, buf_len);
		PopItemWidth();
		return b;
	}

    // This SplitterBehavior is an alternative version of the one posted on Dear ImGui github:
    //    https://github.com/ocornut/imgui/issues/319#issuecomment-345795629
    // This new version fits my need more.
    static bool SplitterBehavior(const ImRect& bb, ImGuiID id, ImGuiAxis axis, float* pos, float min_position, float max_position, float hover_extend = 0.0f, float hover_visibility_delay = 0.0f, ImU32 bg_col = 0)
    {
        IM_ASSERT(min_position <= max_position);

		// [kevreco] Not sure why but when the splitter it at zero it's as if there was no splitter at all.
		if (min_position <= 0)
			min_position = 0.1f;

		// Add margin equal to the size of the splitter bar.
		max_position -= axis == ImGuiAxis_X ? bb .GetWidth() : bb.GetHeight();

        ImGuiContext& g = *GImGui;
        ImGuiWindow* window = g.CurrentWindow;

        if (!ItemAdd(bb, id, NULL, ImGuiItemFlags_NoNav))
            return false;

        // FIXME: AFAIK the only leftover reason for passing ImGuiButtonFlags_AllowOverlap here is
        // to allow caller of SplitterBehavior() to call SetItemAllowOverlap() after the item.
        // Nowadays we would instead want to use SetNextItemAllowOverlap() before the item.
        ImGuiButtonFlags button_flags = ImGuiButtonFlags_FlattenChildren;

        bool hovered, held;
        ImRect bb_interact = bb;
        bb_interact.Expand(axis == ImGuiAxis_Y ? ImVec2(0.0f, hover_extend) : ImVec2(hover_extend, 0.0f));
        ButtonBehavior(bb_interact, id, &hovered, &held, button_flags);
        if (hovered)
            g.LastItemData.StatusFlags |= ImGuiItemStatusFlags_HoveredRect; // for IsItemHovered(), because bb_interact is larger than bb

        if (held || (hovered && g.HoveredIdPreviousFrame == id && g.HoveredIdTimer >= hover_visibility_delay))
            SetMouseCursor(axis == ImGuiAxis_Y ? ImGuiMouseCursor_ResizeNS : ImGuiMouseCursor_ResizeEW);

        ImRect bb_render = bb;
        if (held)
        {
            float mouse_delta = (g.IO.MousePos - g.ActiveIdClickOffset - bb_interact.Min)[axis];

            if (mouse_delta < 0)
            {
                if (*pos + mouse_delta < min_position)
                {
                    mouse_delta = min_position - *pos;
                }
            }
            if (mouse_delta > 0)
            {
                if (*pos + mouse_delta > max_position)
                {
                    mouse_delta = max_position - *pos;
                }
            }

            // Apply resize
            if (mouse_delta != 0.0f)
            {
                *pos += mouse_delta;
                bb_render.Translate((axis == ImGuiAxis_X) ? ImVec2(mouse_delta, 0.0f) : ImVec2(0.0f, mouse_delta));

                MarkItemEdited(id);
            }
        }

        // Render at new position
        if (bg_col & IM_COL32_A_MASK)
            window->DrawList->AddRectFilled(bb_render.Min, bb_render.Max, bg_col, 0.0f);
        const ImU32 col = GetColorU32(held ? ImGuiCol_SeparatorActive : (hovered && g.HoveredIdTimer >= hover_visibility_delay) ? ImGuiCol_SeparatorHovered : ImGuiCol_Separator);
        window->DrawList->AddRectFilled(bb_render.Min, bb_render.Max, col, 0.0f);

        return held;
    }

	inline bool SplitterVertical(float thickness, float* pos, float min_pos, float max_pos, float splitter_long_axis_size = -1.0f)
    {
        using namespace ImGui;
        ImGuiContext* g = GetCurrentContext();
        ImGuiWindow* window = g->CurrentWindow;
        ImGuiID id = window->GetID("##SplitterV");
        ImRect bb;
        bb.Min = window->DC.CursorPos + ImVec2(*pos, 0.0f);
        bb.Max = bb.Min + CalcItemSize(ImVec2(thickness, splitter_long_axis_size), 0.0f, 0.0f);
        return SplitterBehavior(bb, id, ImGuiAxis_X, pos, min_pos, max_pos, 0.0f);
    }

	inline bool SplitterHorizontal(float thickness, float* pos, float min_pos, float max_pos, float splitter_long_axis_size = -1.0f)
    {
        using namespace ImGui;
        ImGuiContext* g = GetCurrentContext();
        ImGuiWindow* window = g->CurrentWindow;
        ImGuiID id = window->GetID("##SplitterH");
        ImRect bb;
        bb.Min = window->DC.CursorPos + ImVec2(0.0f, *pos);
        bb.Max = bb.Min + CalcItemSize(ImVec2(splitter_long_axis_size, thickness), 0.0f, 0.0f);
        return SplitterBehavior(bb, id, ImGuiAxis_Y, pos, min_pos, max_pos, 0.0f);
    }

	static inline bool NoPaddingButton(const char* label)
	{
		ImGuiContext* g = GetCurrentContext();
		ImVec2 backup_padding = g->Style.FramePadding;
		g->Style.FramePadding = ImVec2(0.0f, 0.0f);
		bool pressed = ButtonEx(label, ImVec2(0, 0), ImGuiButtonFlags_AlignTextBaseLine);
		g->Style.FramePadding = backup_padding;
		return pressed;
	}

	static inline bool IconButton(const char* label)
	{
		return NoPaddingButton(label);
	}

	struct vec3 {
		float x, y, z;
		vec3(float x, float y, float z)
		{
			this->x = (x); this->y = (y); this->z = (z);
		}
	};

	static vec3 vec3_lerp(vec3 a, vec3 b, float t)
	{
		return vec3(
			(a.x + (b.x - a.x) * t),
			(a.y + (b.y - a.y) * t),
			(a.z + (b.z - a.z) * t)
		);
	}

	static vec3 to_vec3(float x, float y, float z)
	{
		return vec3(x, y, z);
	}

	static ImVec4 to_vec4(vec3 v)
	{
		return ImVec4(v.x, v.y, v.z, 1.0f);
	}
	static ImVec4 to_vec4(vec3 v, float w)
	{
		return ImVec4(v.x, v.y, v.z, w);
	}

	static ImVec4 to_vec4(float x, float y, float z)
	{
		return ImVec4(x, y, z, 1.0f);
	}

	static ImVec4 to_vec4(float x, float y, float z, float w)
	{
		return ImVec4(x, y, z, w);
	}

	static vec3 vec3_plus_vec3(vec3 a, vec3 b)
	{
		return  vec3(a.x + b.x, a.y + b.y, a.z + b.z);
	}

	ImVec4 rgba8_to_vec4(unsigned char r, unsigned char g, unsigned char b, unsigned char a)
	{
		return ColorConvertU32ToFloat4(IM_COL32(r, g, b, a));
	}

	ImVec4 vec4_with_alpha(ImVec4 v, float alpha)
	{
		return ImVec4{ v.x, v.y, v.z, alpha};
	}

    inline void ApplyCustomTheme()
    {
		ImGuiStyle& style = ImGui::GetStyle();
		auto colors = style.Colors;

		auto lightgray = to_vec3(0.847f, 0.871f, 0.914f);
		auto offWhite = to_vec3(.925f, 0.937f, 0.957f);
		auto yellowish = to_vec3(0.91f, 0.824f, 0.647f);

		auto yellowishV2 = to_vec3(0.784f, 0.686f, 0.529f);
		auto main_yellow = to_vec3(0.922f, 0.796f, 0.545f);
		auto redish = to_vec3(0.816f, 0.529f, 0.439f);
		auto purple = to_vec3(0.706f, 0.557f, 0.678f);
		auto foreground = vec3_lerp(offWhite, yellowish, 0.30f); ;
		auto main_blue = to_vec3(0.506f, 0.631f, 0.757f);

		auto main_green = to_vec3(0.561f, 0.737f, 0.733f);
		auto main = main_green;
		auto main_alpha_20 = 0.20f;
		(void)main_alpha_20;
		auto main_alpha_35 = 0.35f;
		auto main_alpha_50 = 0.50f;
		(void)main_alpha_50;
		auto main_alpha_70 = 0.70f;
		
		auto window_background = rgba8_to_vec4(44, 47, 58, 240);
		auto check_mark_and_slider_grab = rgba8_to_vec4(121, 158, 200, 255);
		auto background = to_vec3(0.263f, 0.298f, 0.369f);  // "Polar Night" (grayish, slightly blue)
		auto background2 = to_vec3(0.369f, 0.506f, 0.675f);
		auto gray = to_vec3(0.263f, 0.298f, 0.369f);
		auto gray_05 = vec3_plus_vec3(gray, to_vec3(0.05f, 0.05f, 0.05f));
		auto gray_10 = vec3_plus_vec3(gray, to_vec3(0.10f, 0.10f, 0.10f));
		auto gray_15 = vec3_plus_vec3(gray, to_vec3(0.15f, 0.15f, 0.15f));

		colors[ImGuiCol_Text] = to_vec4(foreground, 1.00f);
		colors[ImGuiCol_TextDisabled] = to_vec4(0.50f, 0.50f, 0.50f);
		colors[ImGuiCol_WindowBg] = window_background;
		colors[ImGuiCol_ChildBg] = to_vec4(gray_05, 0.00f);
		colors[ImGuiCol_PopupBg] = to_vec4(background, 0.94f);
		colors[ImGuiCol_Border] = to_vec4(vec3_lerp(main_green, gray, 0.40f));
		colors[ImGuiCol_BorderShadow] = to_vec4(vec3_lerp(main_green, gray, 0.10f));
		colors[ImGuiCol_Button] = rgba8_to_vec4(113, 149, 158, 102);
		colors[ImGuiCol_ButtonHovered] = rgba8_to_vec4(103, 129, 135, 255);
		colors[ImGuiCol_ButtonActive] = rgba8_to_vec4(88, 109, 120, 255);
		colors[ImGuiCol_FrameBg] = rgba8_to_vec4(70, 85, 110, 138);
		colors[ImGuiCol_FrameBgHovered] = rgba8_to_vec4(122, 172, 228, 109);
		colors[ImGuiCol_FrameBgActive] = rgba8_to_vec4(116, 162, 215, 171);
		colors[ImGuiCol_TitleBg] = to_vec4(gray_05, 1.00f);
		colors[ImGuiCol_TitleBgActive] = to_vec4(background2, 1.00f);
		colors[ImGuiCol_TitleBgCollapsed] = to_vec4(gray_05, 0.51f);
		colors[ImGuiCol_MenuBarBg] = to_vec4(gray_10, 1.0f);
		colors[ImGuiCol_ScrollbarBg] = to_vec4(vec3_lerp(background, offWhite, 0.08f), 0.53f);
		colors[ImGuiCol_ScrollbarGrab] = rgba8_to_vec4(109, 142, 180, 255);
		colors[ImGuiCol_ScrollbarGrabHovered] = rgba8_to_vec4(133, 159, 190, 255);
		colors[ImGuiCol_ScrollbarGrabActive] = rgba8_to_vec4(101, 127, 160, 255);
		colors[ImGuiCol_CheckMark] = check_mark_and_slider_grab;
		colors[ImGuiCol_SliderGrab] = check_mark_and_slider_grab;
		colors[ImGuiCol_SliderGrabActive] = rgba8_to_vec4(140, 167, 200, 255);

		colors[ImGuiCol_Header] = vec4_with_alpha(colors[ImGuiCol_Button], 85.0f / 255.0f);
		colors[ImGuiCol_HeaderHovered] = vec4_with_alpha(colors[ImGuiCol_ButtonHovered], 200.0f / 255.0f);
		colors[ImGuiCol_HeaderActive] = vec4_with_alpha(colors[ImGuiCol_ButtonActive], 1.0f);
		colors[ImGuiCol_Separator] = colors[ImGuiCol_Button];
		colors[ImGuiCol_SeparatorHovered] = colors[ImGuiCol_ButtonHovered];
		colors[ImGuiCol_SeparatorActive] = colors[ImGuiCol_ButtonActive];
		colors[ImGuiCol_ResizeGrip] = colors[ImGuiCol_ScrollbarGrab];
		colors[ImGuiCol_ResizeGripHovered] = colors[ImGuiCol_ScrollbarGrabHovered];
		colors[ImGuiCol_ResizeGripActive] = colors[ImGuiCol_ScrollbarGrabActive];

		// Tab
		colors[ImGuiCol_Tab] = rgba8_to_vec4(76, 95, 105, 255);
		colors[ImGuiCol_TabHovered] = rgba8_to_vec4(125, 161, 166, 220);
		colors[ImGuiCol_TabActive] = rgba8_to_vec4(91, 117, 126, 255);
		//colors[ImGuiCol_TabUnfocused] = vec4_lerp(colors[ImGuiCol_Tab], colors[ImGuiCol_TitleBg], 0.80f);
		//colors[ImGuiCol_TabUnfocusedActive] = vec4_lerp(colors[ImGuiCol_TabActive], colors[ImGuiCol_TitleBg], 0.40f);

		// Plot
		colors[ImGuiCol_PlotLines] = to_vec4(0.61f, 0.61f, 0.61f, 1.00f);
		colors[ImGuiCol_PlotLinesHovered] = to_vec4(1.00f, 0.43f, 0.35f, 1.00f);
		colors[ImGuiCol_PlotHistogram] = to_vec4(0.90f, 0.70f, 0.00f, 1.00f);
		colors[ImGuiCol_PlotHistogramHovered] = to_vec4(1.00f, 0.60f, 0.00f, 1.00f);
		colors[ImGuiCol_TableHeaderBg] = colors[ImGuiCol_MenuBarBg];

		colors[ImGuiCol_TableBorderStrong] = to_vec4(colors[ImGuiCol_BorderShadow].x, colors[ImGuiCol_BorderShadow].y, colors[ImGuiCol_BorderShadow].z, 1.00f);   // Prefer using Alpha=1.0
		colors[ImGuiCol_TableBorderLight] = to_vec4(colors[ImGuiCol_Border].x, colors[ImGuiCol_Border].y, colors[ImGuiCol_Border].z, 1.00f);   // Prefer using Alpha=1.0 here
		colors[ImGuiCol_TableRowBg] = to_vec4(0.00f, 0.00f, 0.00f, 0.00f);
		colors[ImGuiCol_TableRowBgAlt] = to_vec4(1.00f, 1.00f, 1.00f, 0.06f);
		colors[ImGuiCol_TextSelectedBg] = to_vec4(main, 0.35f); // Main
		colors[ImGuiCol_DragDropTarget] = to_vec4(1.00f, 1.00f, 0.00f, 0.90f);
		colors[ImGuiCol_NavHighlight] = to_vec4(main, main_alpha_35); // Main
		colors[ImGuiCol_NavWindowingHighlight] = to_vec4(1.00f, 1.00f, 1.00f, 0.70f);
		colors[ImGuiCol_NavWindowingDimBg] = to_vec4(background, 0.50f);
		colors[ImGuiCol_ModalWindowDimBg] = to_vec4(background, 0.60f);
        
        style.DisabledAlpha               = 0.70f;
    }

	static inline void LoadFonts()
	{
		ImGuiIO& io = ImGui::GetIO();
		ImFont* font = io.Fonts->AddFontFromFileTTF(IMGUI_EXTENSION_TEXT_FONT, 18.0f);
		IM_ASSERT(font != nullptr);
		// Merge in icons from Font Awesome
		static const ImWchar icons_ranges[] = { ICON_MIN_LC, ICON_MAX_16_LC, 0 };
		ImFontConfig icons_config;
		//icons_config.OversampleV = 1;
		//icons_config.OversampleH = 1;
		icons_config.MergeMode = true;
		icons_config.GlyphOffset.y = 2.0f;
		io.Fonts->AddFontFromFileTTF(IMGUI_EXTENSION_ICONS_FONT, 18.0f, &icons_config, icons_ranges);
		io.Fonts->Build();
	}
}

#endif // IMGUI_PLUS_EXTENSIONS_H