#include "app.hpp"

#include <string>

#include "effects/wobbler.hpp"
#include "effects/periodic.hpp"
#include "fit_curves.hpp"

#include "tendril_internal.hpp"

static td_rgba8 background_color{ 220, 210, 205, 255 };

static td_rgba8 green_tendrilis{ 10, 85, 5, 255 };
static td_rgba8 line_color{ 180, 90, 180, 255 };
static td_rgba8 point_hover_color{ 180, 90, 180, 255 };
static td_rgba8 background_grid_color{ 175, 175, 175, 255 };
static td_rgba8 bitmap_buffer[TD_CANVAS_WIDTH * TD_CANVAS_HEIGHT];

enum demo_type {
	demo_type_TEXT_ON_LINE,
	demo_type_TEXT_ON_POLYLINE,
	demo_type_TEXT_ON_CURVE,

	demo_type_TRIANGLE_ON_CURVE,
	demo_type_TENDRILIS_ON_CURVE,
	demo_type_DRAW_TENDRILIS_SPIRO,

	demo_type_REGULAR_DEMO_COUNT,
	demo_type_DEBUG_DEMO_START,
	demo_type_RECT_ON_LINE,
	demo_type_CURVE_ON_LINE,
	demo_type_VBARS_ON_CURVE,
	demo_type_HBARS_ON_CURVE,
	demo_type_DRAW_ARC,
	demo_type_ALL_DEMO_COUNT
};

enum path_bender_flags
{
	path_bender_flags_NONE = 0,
	// Strech target path along the X axis.
	path_bender_flags_STRETCH_TARGET = 1 << 0,
	// Interpolate tangent of piecewise path.
	path_bender_flags_INTERPOLATE_TANGENT = 1 << 1,
};

struct path_bender
{
	td_piecewise_path pw_directive_path;
	td_path result_path;

	void set(const td_path& target_path, const td_path& curve, int flags = 0)
	{
		result_path.clear();
		pw_directive_path.clear();
		add(target_path, curve, flags);
	}

	void add(const td_path& target_path, const td_path& directive_path, int flags = 0)
	{
		// Convert to piecwise path to transform the target path more precisely.
		td::to_piecewise_path(directive_path, &pw_directive_path);

		const td_path* target = &target_path;

		td_path stretched_target;

		if (flags & path_bender_flags_STRETCH_TARGET)
		{
			target_path.clone_to(&stretched_target);
			// @TODO make it strech also on Y axis, we should probably forward a td_vec2 for scale
			td::elements_multiply(&stretched_target, td_vec2(pw_directive_path.length(), 5.0f));

			target = &stretched_target;
		}

		td::to_fragmented_path(*target, &result_path, 2.0f);

		// Transform
		for (int i = 0; i < result_path.points.size(); i += 1)
		{
			if (flags & path_bender_flags_INTERPOLATE_TANGENT)
			{
				result_path.points[i] = td::smooth_transform_along_piecewise(pw_directive_path, result_path.points[i]);
			}
			else
			{
				result_path.points[i] = td::transform_along_piecewise(pw_directive_path, result_path.points[i]);
			}
		}
	}
};

struct td_spiro_ctx {

	bezctx base;
	td_path* path;
	td_vec2 current_point;

	td_spiro_ctx(td_path* _path)
	{
		path = _path;
		base.moveto = move_to;
		base.lineto = line_to;
		base.quadto = quad_to;
		base.curveto = curve_to;
		base.mark_knot = NULL;
	}

	static void move_to(bezctx* bc, double x, double y, int is_open)
	{
		td_spiro_ctx* ctx = (td_spiro_ctx*)bc;
		ctx->path->move_to((float)x, (float)y);
		ctx->current_point.x = (float)x;
		ctx->current_point.y = (float)y;
	}

	static void line_to(bezctx* bc, double x, double y)
	{
		td_spiro_ctx* ctx = (td_spiro_ctx*)bc;
		ctx->path->line_to((float)x, (float)y);

		ctx->current_point.x = (float)x;
		ctx->current_point.y = (float)y;
	}

	static void quad_to(bezctx* bc, double x1, double y1, double x2, double y2)
	{
		td_spiro_ctx* ctx = (td_spiro_ctx*)bc;
		ctx->path->quad_to((float)x1, (float)y1, (float)x2, (float)y2);
		ctx->current_point.x = (float)x2;
		ctx->current_point.y = (float)y2;
	}

	static void curve_to(bezctx* bc, double x1, double y1, double x2, double y2, double x3, double y3)
	{
		td_spiro_ctx* ctx = (td_spiro_ctx*)bc;
		ctx->path->cubic_to((float)x1, (float)y1, (float)x2, (float)y2, (float)x3, (float)y3);
		ctx->current_point.x = (float)x3;
		ctx->current_point.y = (float)y3;
	}
};

// Display extra drawing (directive path, normals, points)
static void display_bender_overlay(td_demo* demo, const td_point_array* directive_path_points, const td_piecewise_path* directive_pw_path, const td_vec2& point_hovered);

static wobbler wobble;
static periodic period;

static td_path* apply_various_effect(td_path* input_path);

// Remove space and replace them with the separator "/"
// As suggested from the guide: https://anomalis.gumroad.com/l/tendrilis-guide
static void normalize_tendrilis_text(std::string* str);

// Returns true if the state was changed.
static bool set_curve_edit_state(td_curve_edit_state* edit_state, td_curve_edit_state state);

static bool display_curve_toolstrip(td_curve_edit_state* edit_state);

static void display_font_combox(const char* label, float label_margin, int* selected_index);

static void points_to_spiro(const td_point_array& points, td_path* path, bool is_closed = false);
static void point_to_path(const td_point_array& points, td_path* path);

td_demo::td_demo()
{
	selected_demo = demo_type_TENDRILIS_ON_CURVE;

	td_font_store::init_from_file(&regular_font, IMGUI_EXTENSION_TEXT_FONT, 0);
	td_font_store::init_from_file(&tendrilis_font, "./TendrilisExtra-Regular.ttf", 0);
	// Initialize bitmap with static buffer of rgba.
	bitmap = td_bitmap{ bitmap_buffer, TD_CANVAS_WIDTH , TD_CANVAS_HEIGHT };
	
	// Line reused for various demo.
	td_path directive_line;
	directive_line.move_to(td_vec2{ 0.10f,        1.0f / 2.0f });
	directive_line.line_to(td_vec2{ 1.0f - 0.10f,  1.0f / 2.0f });

	td::elements_multiply(&directive_line, canvas_size);

	// Curve reused for various demo.
	td_path directive_curve;
	directive_curve.move_to(td_vec2{ 0.20f, 0.60f });
	directive_curve.cubic_to(td_vec2{ 0.35f, 0.4f }, td_vec2{ 0.65f, 0.4f }, td_vec2{ 0.8f, 0.6f });

	td::elements_multiply(&directive_curve, canvas_size);

	// Text on line
	{
		text_on_line.path = directive_line;
	}

	// Text on polyline
	{
		td_path polyline;
		polyline.move_to(td_vec2{ 0.10f,        1.0f / 2.0f });
		polyline.line_to(td_vec2{ 0.5f,         1.0f / 3.0f });
		polyline.line_to(td_vec2{ 1.0f - 0.10f,  1.0f / 2.0f });

		td::elements_multiply(&polyline, canvas_size);

		text_on_polyline.path = polyline;
	}

	// Text on curve
	{
		text_on_curve.path = directive_curve;
	}

	// Triangle on curve
	{
		td_path triangle;
		triangle.move_to(td_vec2{ 0.0f, 0.5f });
		triangle.line_to(td_vec2{ 1.0f, 0.0f });
		triangle.line_to(td_vec2{ 0.0f, -0.5f });
		triangle.close();

		triangle_on_curve.target = triangle;

		triangle_on_curve.path = directive_curve;
	}

	// Tendrilis on curve
	{
		td_path curve;
		curve.move_to(td_vec2{ 0.10f, 0.80f });
		curve.cubic_to(td_vec2{ 0.60f, 0.65f }, td_vec2{ 0.30f, 0.15f }, td_vec2{ 0.80f, 0.20f });

		td::elements_multiply(&curve, canvas_size);

		tendrilis_on_curve.path = curve;
	}

	// Draw tendrilis (Spiro)
	{
		if (draw_tendrilis_spiro.edit_state == td_curve_edit_state_EDIT)
		{
			// Add first point that will be displayed under the mouse cursor.
			draw_tendrilis_spiro.points.push_back({});
		}
	}

	//
	// DEBUG
	//

	// Rect on line
	{
		td_path rect;
		rect.add_rect(0.0f, 0.0f, 1.0f, 1.0f);

		td::elements_multiply(&rect, td_vec2(1.0f, -1.0f));
		td::elements_multiply(&rect, td_vec2(100.f, 100.0f));

		rect_on_line.target = rect;
		rect_on_line.path = directive_line;
	}

	// Curve on line
	{

		td_path curve;
		curve.move_to(td_vec2{ 0.15f, 0.50f });
		curve.cubic_to(td_vec2{ 0.15f + 0.15f, 0.35f }, td_vec2{ 1.0f - 0.15f - 0.15f, 0.35f }, td_vec2{ 1.0f - 0.15f, 0.50f });

		td::elements_multiply(&curve, td_vec2(1.0f, -1.0f));
		td::elements_multiply(&curve, td_vec2(100.f, 100.0f));


		td_path line;
		line.move_to(td_vec2{ 0.10f,        1.0f / 2.0f });
		line.line_to(td_vec2{ 1.0f - 0.10f,  1.0f / 2.0f });

		td::elements_multiply(&line, canvas_size);

		curve_on_line.path = line;

		curve_on_line.target = curve;

	}

	// Vertical bars on curve
	{

		td_path curve;
		curve.move_to(td_vec2{ 0.15f, 0.50f });
		curve.cubic_to(td_vec2{ 0.15f + 0.15f, 0.35f }, td_vec2{ 1.0f - 0.15f - 0.15f, 0.35f }, td_vec2{ 1.0f - 0.15f, 0.50f });

		td_path bars;

		for (int i = 0; i < 10; i += 1)
		{
			if (i % 2)
			{
				bars.add_rect(td_vec2{ 0.1f * i, 0.0f }, td_vec2{ 0.1f * i + 0.1f, 1.0f } );
			}
		}

		td::elements_multiply(&bars, td_vec2(1.0f, -1.0f));
		td::elements_multiply(&bars, td_vec2(100.f, 100.0f));

		directive_curve.clone_to(&vbars_on_curve.path);
		vbars_on_curve.target = bars;
	}

	// Horizontal bars on curve
	{
		td_path bars;

		for (int i = 0; i < 10; i += 1)
		{
			if (i % 2)
			{
				bars.add_rect(td_vec2{ 0.0f , 0.1f * i, }, td_vec2{ 1.0f, 0.1f * i + 0.1f, });
			}
		}

		td::elements_multiply(&bars, td_vec2(1.0f, -1.0f));
		td::elements_multiply(&bars, td_vec2(100.f, 100.0f));

		hbars_on_curve.path = directive_curve;
		hbars_on_curve.target = bars;
	}
}

td_demo::~td_demo()
{
	td_font_store::destroy(&regular_font);
	td_font_store::destroy(&tendrilis_font);
}

void td_demo::display()
{
	// Clear bitmap at beginning of each frame.

	td_uint32_memset((uint32_t*)bitmap.data, background_color.to_u32(), TD_CANVAS_WIDTH * TD_CANVAS_HEIGHT);

	display_demo();

	handle_interactions();
}

void td_demo::display_options()
{
	const float item_width = 140;

	if (ImGui::TreeNode("Colors"))
	{
		ImGui::SetNextItemWidth(item_width);
		ImGui::U32ColorEdit3("Filling Color##Drawing", green_tendrilis.to_u32_ptr());
		ImGui::SetNextItemWidth(item_width);
		ImGui::U32ColorEdit3("Line Color##Directive", line_color.to_u32_ptr());
		ImGui::SetNextItemWidth(item_width);
		ImGui::U32ColorEdit3("Point Hover Color##Directive", point_hover_color.to_u32_ptr());
		ImGui::SetNextItemWidth(item_width);
		ImGui::U32ColorEdit4("Bg Color##Directive", background_color.to_u32_ptr());
		ImGui::SetNextItemWidth(item_width);
		ImGui::U32ColorEdit3("Bg Grid Color##Directive", background_grid_color.to_u32_ptr());

		ImGui::TreePop();
	}

	ImGui::SeparatorText("Path"); // ===

	ImGui::Checkbox("Show Path", &cfg.show_path);
	ImGui::Checkbox("Edit Path Points", &cfg.edit_path_points);
	ImGui::SetNextItemWidth(item_width);
	ImGui::InputFloat("Path Offset", &cfg.path_offset, 1.0f);

	ImGui::SeparatorText("Normal"); // ===

	ImGui::Checkbox("Show Normals of Path", &cfg.show_normals);
	ImGui::SetNextItemWidth(item_width);
	ImGui::InputFloat("Normal Scale", &cfg.normal_scale, 5.0f);

	ImGui::SeparatorText("Background"); // ===

	ImGui::Checkbox("Show Grid", &cfg.show_background_grid);

	ImGui::SeparatorText("Other"); // ===

	ImGui::Checkbox("Show Debug Demos", &cfg.show_debug_demos);
	ImGui::Checkbox("Show Options On Left Panel", &cfg.show_options_on_left_panel);
	
	if (ImGui::Button("Reset demo"))
	{
		ImGui::OpenPopup("##PopUpReset");
	}

	if (ImGui::BeginPopup("##PopUpReset"))
	{
		ImGui::Text("Reset Confirmation:");
		if (ImGui::Selectable("Yes"))
		{
			td_demo new_demo;
			td_memswap_ptr(&new_demo, this);
		}
		ImGui::Selectable("No");
		ImGui::EndPopup();
	}
}

void td_demo::display_canvas_background(const td_vec2& scrolling)
{
	td_vec2i canvas_min = td_vec2i(0, 0);
	td_vec2i canvas_size = td_vec2i(TD_CANVAS_WIDTH, TD_CANVAS_HEIGHT);
	td_vec2i canvas_max{ canvas_min.x + canvas_size.x, canvas_min.y + canvas_size.y };

	if (cfg.show_background_grid)
	{
		const int grid_step = 50;
		for (int x = (int)fmodf(scrolling.x, grid_step); x < canvas_size.x; x += grid_step)
		{
			rasterizer.render_vertical_line(x, canvas_min.y, canvas_max.y, bitmap.data, canvas_size.x, canvas_size.y, background_grid_color);
		}

		for (int y = (int)fmodf(scrolling.y, grid_step); y < canvas_size.y; y += grid_step)
		{
			rasterizer.render_horizontal_line(canvas_min.x, canvas_max.x, y, bitmap.data, canvas_size.x, canvas_size.y, background_grid_color);
		}
	}
}

void td_demo::display_demo()
{
	// Avoid moving window by mistake: only move window from the title bar.
	ImGui::GetCurrentContext()->IO.ConfigWindowsMoveFromTitleBarOnly = true;

	cfg.parameter_label_width = ImGui::CalcTextSize("XXXXXXXXXXX").x;

	static const char* ids[demo_type_ALL_DEMO_COUNT] = {
		text_on_line.id,
		text_on_polyline.id,
		text_on_curve.id,

		triangle_on_curve.id,
		tendrilis_on_curve.id,
		draw_tendrilis_spiro.id,
		"demo_type_REGULAR_DEMO_COUNT",
		"demo_type_DEBUG_DEMO_START",
		rect_on_line.id,
		curve_on_line.id,
		vbars_on_curve.id,
		hbars_on_curve.id,
		draw_arc.id
	};

	// Display demo options in the main menu bar
	{
		if (ImGui::BeginMainMenuBar())
		{
			if (ImGui::BeginMenu("Options"))
			{
				if (ImGui::BeginMenu("Demo"))
				{
					display_options();
					ImGui::EndMenu();
				}

				ImGui::EndMenu();
			}

			ImGui::EndMainMenuBar();
		}
	}

	path_bender bender;

	const ImGuiViewport* main_viewport = ImGui::GetMainViewport();
	float vp_margin = 20.0f;
	ImGui::SetNextWindowPos(ImVec2(main_viewport->WorkPos.x + vp_margin, main_viewport->WorkPos.y + vp_margin), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(main_viewport->WorkSize.x - (vp_margin * 2), main_viewport->WorkSize.y - (vp_margin * 2)), ImGuiCond_FirstUseEver);

	if (ImGui::Begin("Tendril Demo"))
	{
		ImGui::BeginChild("##LeftPanel", ImVec2(300, 0), ImGuiChildFlags_ResizeX | ImGuiChildFlags_Borders | ImGuiChildFlags_NavFlattened);
		
		if (ImGui::CollapsingHeader("Demos", ImGuiTreeNodeFlags_DefaultOpen))
		{
			if (ImGui::BeginChild("##DemoList", ImVec2(0.0f, 0.0f), ImGuiChildFlags_AutoResizeY))
			{
				for (int i = 0; i < demo_type_REGULAR_DEMO_COUNT; i += 1)
				{
					ImGui::RadioButton(ids[i], &selected_demo, i);
				}
				ImGui::EndChild();
			}
		}

		if (cfg.show_debug_demos)
		{
			if (ImGui::CollapsingHeader("Debug demos"))
			{
				if (ImGui::BeginChild("##DebugDemoList", ImVec2(0.0f, 0.0f), ImGuiChildFlags_AutoResizeY))
				{
					for (int i = demo_type_DEBUG_DEMO_START + 1; i < demo_type_ALL_DEMO_COUNT; i += 1)
					{
						ImGui::RadioButton(ids[i], &selected_demo, i);
					}

					ImGui::EndChild();
				}
			}
		}

		if (cfg.show_options_on_left_panel)
		{
			if (ImGui::CollapsingHeader("Options", ImGuiTreeNodeFlags_DefaultOpen))
			{
				display_options();
			}
		}

		if (ImGui::CollapsingHeader("Path Effects"))
		{
			ImGui::Checkbox("Wobble", &wobble.enabled);
			ImGui::Checkbox("Periodic Wave", &period.enabled);
		}

		ImGui::EndChild(); // ##LeftPanel
		
		ImGui::SameLine();
		
		ImGui::BeginChild("##CanvasPanel", ImVec2(canvas_size.x, 0));
		bool display_curve_handle = true;
		ImGui::Text("Demo: %s", ids[selected_demo]);

		float label_margin = cfg.parameter_label_width;
		float max_font_size = 150.0f;

		switch (selected_demo)
		{
		case demo_type_TEXT_ON_LINE:
		{
			td_font_store* font = get_font(text_on_line.font_type);

			text_on_line.target.clear();
			td::insert_text_to_path(font, text_on_line.text.data(), td_vec2(), text_on_line.font_size, &text_on_line.target);

			td_path* path = apply_various_effect(&text_on_line.path);

			bender.set(text_on_line.target, *path);

			display_canvas_with_bender(&bender, &text_on_line.path.points);
			break;
		}
		case demo_type_TEXT_ON_POLYLINE:
		{
			td_font_store* font = get_font(text_on_polyline.font_type);

			text_on_polyline.target.clear();
			td::insert_text_to_path(font, text_on_polyline.text.data(), td_vec2(), text_on_polyline.font_size, &text_on_polyline.target);

			td_path* path = apply_various_effect(&text_on_polyline.path);

			bender.set(text_on_polyline.target, *path);

			display_canvas_with_bender(&bender, &text_on_polyline.path.points);

			break;
		}
		case demo_type_TEXT_ON_CURVE:
		{
			td_font_store* font = get_font(text_on_curve.font_type);

			text_on_curve.target.clear();
			td::insert_text_to_path(font, text_on_curve.text.data(), td_vec2(), text_on_curve.font_size, &text_on_curve.target);

			td_path* path = apply_various_effect(&text_on_curve.path);

			bender.set(text_on_curve.target, *path, path_bender_flags_INTERPOLATE_TANGENT);

			display_canvas_with_bender(&bender, &text_on_curve.path.points);
			break;
		}
		
		case demo_type_TRIANGLE_ON_CURVE:
		{
			td_path* path = apply_various_effect(&triangle_on_curve.path);

			int flags = path_bender_flags_INTERPOLATE_TANGENT | path_bender_flags_STRETCH_TARGET;
			bender.set(triangle_on_curve.target, *path, flags);

			display_canvas_with_bender(&bender, &triangle_on_curve.path.points);

			break;
		}
		case demo_type_TENDRILIS_ON_CURVE: {
			td_font_store* font = get_font(tendrilis_on_curve.font_type);

			// Adjust text if tendrilis font is used.
			if (font == &tendrilis_font)
			{
				normalize_tendrilis_text(&tendrilis_on_curve.text);
			}

			tendrilis_on_curve.target.clear();
			td::insert_text_to_path(font, tendrilis_on_curve.text.data(), td_vec2(), tendrilis_on_curve.font_size, &tendrilis_on_curve.target);

			td_path* path = apply_various_effect(&tendrilis_on_curve.path);

			bender.set(tendrilis_on_curve.target, *path, path_bender_flags_INTERPOLATE_TANGENT);

			display_canvas_with_bender(&bender, &tendrilis_on_curve.path.points);

			break;
		}

		case demo_type_DRAW_TENDRILIS_SPIRO: {

			auto& spiro = draw_tendrilis_spiro;

			td_font_store* font = get_font(spiro.font_type);

			// Adjust text if tendrilis font is used.
			if (font == &tendrilis_font)
			{
				normalize_tendrilis_text(&spiro.text);
			}

			spiro.target.clear();
		
			td::insert_text_to_path(font, spiro.text.data(), td_vec2(), spiro.font_size, &spiro.target);

			spiro.path.clear();
			points_to_spiro(spiro.points, &spiro.path);

			td_path* path = apply_various_effect(&spiro.path);

			bender.set(spiro.target, *path, path_bender_flags_INTERPOLATE_TANGENT);

			spiro.mouse_state = display_canvas_with_bender(&bender, &spiro.points);
			break;
		}
		case demo_type_RECT_ON_LINE: {

			td_path* path = apply_various_effect(&rect_on_line.path);

			bool smooth = true;
			bender.set(rect_on_line.target, *path, path_bender_flags_INTERPOLATE_TANGENT);

			display_canvas_with_bender(&bender, &rect_on_line.path.points);
			break;
		}
		case demo_type_CURVE_ON_LINE: {

			td_path* path = apply_various_effect(&curve_on_line.path);

			bool smooth = true;
			bender.set(curve_on_line.target, *path, path_bender_flags_INTERPOLATE_TANGENT);

			display_canvas_with_bender(&bender, &curve_on_line.path.points);
			break;
		}
		case demo_type_VBARS_ON_CURVE: {

			td_path* path = apply_various_effect(&vbars_on_curve.path);

			bool smooth = true;
			bender.set(vbars_on_curve.target, *path, path_bender_flags_INTERPOLATE_TANGENT);

			display_canvas_with_bender(&bender, &vbars_on_curve.path.points);
			break;
		}
		case demo_type_HBARS_ON_CURVE: {

			td_path* path = apply_various_effect(&hbars_on_curve.path);

			bool smooth = true;
			bender.set(hbars_on_curve.target, *path, path_bender_flags_INTERPOLATE_TANGENT);

			display_canvas_with_bender(&bender, &hbars_on_curve.path.points);
			break;
		}
		case demo_type_DRAW_ARC: {

			draw_arc.path.clear();
			draw_arc.path.move_to(draw_arc.mx, draw_arc.my);

			draw_arc.path.arc_to(draw_arc.rx, draw_arc.ry, draw_arc.xrotation, draw_arc.large_arc, draw_arc.sweep_flag, draw_arc.x, draw_arc.y);

			td_path* path = apply_various_effect(&draw_arc.path);

			display_canvas(draw_arc.path, NULL, NULL);
			break;
		}
		}

		ImGui::EndChild(); // ##Canvas

		ImGui::SameLine();
		ImGui::BeginChild("##RightPanel", ImVec2(), ImGuiChildFlags_Borders );
		
		switch (selected_demo)
		{
		case demo_type_TEXT_ON_LINE:
		{
			ImGui::SeparatorText("Parameters");
			ImGui::Spacing();
			ImGui::InputText("Text", &text_on_line.text);
			ImGui::SliderFloat("Font size", &text_on_line.font_size, 10, max_font_size);
			display_font_combox("Font ", label_margin, &text_on_line.font_type);

			break;
		}
		case demo_type_TEXT_ON_POLYLINE:
		{
			ImGui::SeparatorText("Parameters");
			ImGui::Spacing();
			ImGui::InputText("Text", &text_on_polyline.text);
			ImGui::SliderFloat("Font size", &text_on_polyline.font_size, 10, max_font_size);
			display_font_combox("Font ", label_margin, &text_on_polyline.font_type);
			break;
		}
		case demo_type_TEXT_ON_CURVE:
		{
			ImGui::SeparatorText("Parameters");
			ImGui::Spacing();
			ImGui::InputText("Text", &text_on_curve.text);
			ImGui::SliderFloat("Font size", &text_on_curve.font_size, 10, max_font_size);
			display_font_combox("Font ", label_margin, &text_on_curve.font_type);
			break;
		}

		case demo_type_TRIANGLE_ON_CURVE:
		{
			break;
		}
		case demo_type_TENDRILIS_ON_CURVE: {
			
			ImGui::SeparatorText("Parameters");
			ImGui::Spacing();
			ImGui::InputText("Text", &tendrilis_on_curve.text);
			ImGui::SliderFloat("Font size", &tendrilis_on_curve.font_size, 10, max_font_size);
			display_font_combox("Font ", label_margin, &tendrilis_on_curve.font_type);
			break;
		}

		case demo_type_DRAW_TENDRILIS_SPIRO: {

			auto& spiro = draw_tendrilis_spiro;
			spiro.edit_state_changed = display_curve_toolstrip(&spiro.edit_state);

			ImGui::SeparatorText("Parameters");
			ImGui::Spacing();
			ImGui::InputText("Text", &spiro.text);
			ImGui::SliderFloat("Font size", &spiro.font_size, 10, max_font_size);
			display_font_combox("Font ", label_margin, &spiro.font_type);
			break;
		}
		case demo_type_RECT_ON_LINE: {
			break;
		}
		case demo_type_CURVE_ON_LINE: {

			break;
		}
		case demo_type_VBARS_ON_CURVE: {

			break;
		}
		case demo_type_HBARS_ON_CURVE: {

			break;
		}
		case demo_type_DRAW_ARC: {
		
			ImGui::SeparatorText("Parameters");
			ImGui::Spacing();
			ImGui::DragFloat("mx", &draw_arc.mx);
			ImGui::DragFloat("my", &draw_arc.my);
			ImGui::DragFloat("rx", &draw_arc.rx);
			ImGui::DragFloat("ry", &draw_arc.ry);
			ImGui::DragFloat("xrotation", &draw_arc.xrotation, 0.1);
			ImGui::Checkbox("large_arc", &draw_arc.large_arc);
			ImGui::Checkbox("sweep_flag", &draw_arc.sweep_flag);
			ImGui::DragFloat("x", &draw_arc.x);
			ImGui::DragFloat("y", &draw_arc.y);

			break;
		}
		}

		ImGui::EndChild(); // ##RightPanel

	}
	ImGui::End();
}

void td_demo::display_svg_widget(const td_path& path, float button_width)
{
	if (ImGui::Button("Save to SVG"))
	{
		td::to_svg_file(path, svg_file_name, TD_CANVAS_WIDTH, TD_CANVAS_HEIGHT, "darkgreen");

		app_backend::save_file_client_side(svg_file_name, svg_file_name);

		ImGuiContext* ctx = ImGui::GetCurrentContext();
		if (ctx->PlatformIO.Platform_OpenInShellFn != NULL)
		{
			ctx->PlatformIO.Platform_OpenInShellFn(ctx, svg_file_name);
		}
	}
	ImGui::SameLine(button_width);
	ImGui::InputText("##SVG Export", svg_file_name, sizeof(svg_file_name));
}

td_demo::canvas_mouse_state td_demo::display_canvas(const td_path& target_path, const td_piecewise_path* pw_directive_path, td_point_array* points)
{
	ImDrawList* draw_list = ImGui::GetWindowDrawList();
	ImVec2 canvas_min = ImGui::GetCursorScreenPos();

	td_demo::canvas_mouse_state state;

	ImVec2 canvas_size = ImVec2(TD_CANVAS_WIDTH, TD_CANVAS_HEIGHT);

	ImVec2 no_hovered_coord{ -9999.0f,-9999.0f };
	ImVec2 hovered_coord = no_hovered_coord;
	state.pos = no_hovered_coord;

	ImVec2 imgui_cursor_pos = ImGui::GetCursorPos();
	ImGui::PushID(texture);
	ImGui::ImageWithBorder((ImTextureID)(intptr_t)texture, canvas_size);

	if (ImGui::IsItemHovered())
	{
		state.pos = ImGui::GetMousePos() - ImGui::GetItemRectMin();
		hovered_coord = state.pos;

		if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
		{
			state.clicked = true;
		}

		state.hovered = true;
	}

	ImGui::PopID();

	// Save current imgui cursor before drawing invisible buttons.
	ImVec2 cursor_after_image = ImGui::GetCursorPos();

	int point_hovered_index = -1;

	if (cfg.edit_path_points && points)
	{
		ImVec2 grab_size = ImVec2(cfg.grab_point_size.x, cfg.grab_point_size.y);
		ImVec2 half_grab_size = grab_size / 2.0f;
		ImDrawList* draw_list = ImGui::GetWindowDrawList();
		for (int i = 0; i < points->size(); i += 1)
		{
			td_vec2& p = points->at(i);
			ImGui::SetCursorPos(imgui_cursor_pos + p - half_grab_size);
			ImGui::PushID(&p);

			ImGui::InvisibleButton("##", grab_size);

			ImGui::PopID();
			if (ImGui::IsItemHovered())
			{
				point_hovered_index = i;
			}
			if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
			{
				point_hovered_index = i;

				p = ImGui::GetIO().MousePos - canvas_min;
				p.x = td_clamp(p.x, 0.0f, canvas_size.x);
				p.y = td_clamp(p.y, 0.0f, canvas_size.y);

				// Update hovered_coor if point are moved.
				hovered_coord = p;
			}
		}
	}

	// Restore imgui cursor.
	ImGui::SetCursorPos(cursor_after_image);

	ImGui::Text("x:");
	
	if (hovered_coord != no_hovered_coord)
	{
		ImGui::SameLine();
		ImGui::Text("%3.0f (%1.2f)", hovered_coord.x, hovered_coord.x / canvas_size.x);
	}
	
	ImGui::Text("y:");
	
	if (hovered_coord != no_hovered_coord)
	{
		ImGui::SameLine();
		ImGui::Text("%3.0f (%1.2f)", hovered_coord.y, hovered_coord.y / canvas_size.y);
	}
	
	ImGui::Spacing();

	display_svg_widget(target_path, cfg.parameter_label_width);
	ImGui::Spacing();
	// No scrolling yet.
	const td_vec2 scrolling{ 0.0f, 0.0f };

	// Display background color and the lines for the grid.
	display_canvas_background(scrolling);

	rasterizer.render_fill_path(target_path, bitmap.data, bitmap.width, bitmap.height, green_tendrilis);
	td_vec2 point_hovered{ -9999.0f,-9999.0f };
	if (point_hovered_index >= 0)
	{
		point_hovered = points->at(point_hovered_index);
	}

	display_bender_overlay(this, points, pw_directive_path, point_hovered);

	bool loaded_or_updated = app_backend::load_or_update_texture(bitmap.data, bitmap.width, bitmap.height, &texture);
	TD_ASSERT(loaded_or_updated);
	return state;
}

td_demo::canvas_mouse_state td_demo::display_canvas_with_bender(path_bender* bender, td_point_array* points)
{
	return display_canvas(bender->result_path, &bender->pw_directive_path, points);
}

td_font_store* td_demo::get_font(int font_type)
{
	return font_type == td_font_type_REGULAR ? &regular_font : &tendrilis_font;
}

void td_demo::handle_interactions()
{
	switch (selected_demo)
	{
	case demo_type_DRAW_TENDRILIS_SPIRO:
	{
		auto& spiro = draw_tendrilis_spiro;

		// On right click go back to view mode
		if (ImGui::IsMouseClicked(ImGuiMouseButton_Right))
		{
			spiro.edit_state_changed = set_curve_edit_state(&spiro.edit_state, td_curve_edit_state_VIEW) || spiro.edit_state_changed;
		}

		// Add or remove point depending on curve edit state.
		if (spiro.edit_state_changed)
		{
			if (spiro.edit_state == td_curve_edit_state_EDIT)
			{
				spiro.points.push_back({});
			}
			else
			{
				spiro.points.erase(spiro.points.end() - 1);
			}
		}

		// Update point from mouse position depending on curve edit state.
		if (spiro.edit_state == td_curve_edit_state_EDIT)
		{
			if (spiro.mouse_state.hovered)
			{
				spiro.points[spiro.points.size() - 1] = spiro.mouse_state.pos;
			}

			if (spiro.mouse_state.hovered && spiro.mouse_state.clicked)
			{
				spiro.points.push_back(spiro.mouse_state.pos);
			}
		}
	}
	}
}

static void display_bender_overlay(td_demo* demo, const td_point_array* directive_path_points, const td_piecewise_path* directive_pw_path, const td_vec2& point_hovered)
{
	const td_point_array* points = directive_path_points;
	const td_piecewise_path* pw = directive_pw_path;

	td_path stroke_path;
	td_path fill_path;

	// Display path
	if (pw && pw->points.size())
	{
		if (demo->cfg.show_path)
		{
			td_vec2 point = pw->points[0];
			point += pw->unit_normal_at(0) * demo->cfg.path_offset;
			stroke_path.move_to(point);

			for (int i = 1; i < pw->points.size(); i += 1)
			{
				point = pw->points[i];
				point += pw->unit_normal_at(i) * demo->cfg.path_offset;
				stroke_path.line_to(point);
			}
		}

		if (demo->cfg.show_normals)
		{
			for (int i = 0; i < pw->points.size(); i += 1)
			{
				td_vec2 p = pw->points[i];
				td_vec2 n = pw->unit_normal_at(i) * demo->cfg.normal_scale;

				td_vec2 normal_start = p - n;
				td_vec2 normal_end = p + n;
				stroke_path.move_to(normal_start);
				stroke_path.line_to(normal_end);
			}
		}
	}

	// Draw directive path points.
	if (demo->cfg.edit_path_points && points)
	{
		for (int i = 0; i < points->size(); i += 1)
		{
			td_vec2 p = points->at(i);
			// Ignore the point that is hovered, it will be displayed later.
			if (point_hovered == p)
			{
				continue;
			}

			td_vec2 margin = demo->cfg.display_point_size * 0.5f;
			td_rect r{ p - margin , p + margin };
			stroke_path.add_rect(r.min, r.max);
			r.reduce(1.0f);
			fill_path.add_rect(r.min, r.max);
		}
	}

	demo->rasterizer.render_stroke_path(stroke_path, demo->bitmap.data, demo->bitmap.width, demo->bitmap.height, line_color);
	demo->rasterizer.render_fill_path(fill_path, demo->bitmap.data, demo->bitmap.width, demo->bitmap.height, background_color);

	// Draw hovered_point
	td_path hovered_point_rect;

	td_vec2 margin = demo->cfg.display_point_size * 0.5f;
	hovered_point_rect.add_rect(point_hovered - margin, point_hovered + margin);
	demo->rasterizer.render_fill_path(hovered_point_rect, demo->bitmap.data, demo->bitmap.width, demo->bitmap.height, point_hover_color);
}

static td_path* apply_various_effect(td_path* input_path)
{
	td_path* path = input_path;

	path = td::apply_effect(&wobble, path);
	path = td::apply_effect(&period, path);

	return path;
}

static void normalize_tendrilis_text(std::string* str)
{
	for (int i = 0; i < str->size(); i += 1)
	{
		if (str->at(i) == ' ')
		{
			str->at(i) = '/';
		}
	}
}

static bool set_curve_edit_state(td_curve_edit_state* edit_state, td_curve_edit_state state)
{
	if (*edit_state != state)
	{
		*edit_state = state;
		return true;
	}

	return false;
}

static bool display_curve_toolstrip(td_curve_edit_state* edit_state)
{
	ImGuiStyle& style = ImGui::GetCurrentContext()->Style;
	ImDrawList* draw_list = ImGui::GetWindowDrawList();

	bool state_changed = false;

	ImGui::SeparatorText("Toolstrip");

	struct item_type {
		td_curve_edit_state state;
		const char* icon;
		const char* id;
		const char* text;
	} items[] = {
		{td_curve_edit_state_VIEW, ICON_LC_SPLINE, "##1", "Move curve point"},
		{td_curve_edit_state_EDIT, ICON_LC_GIT_BRANCH_PLUS, "##2", "Add point to curve" }
	};

	item_type selected_item = items[*edit_state];

	for (size_t i = 0; i < td_size(items); i += 1)
	{
		item_type item = items[i];
		
		ImVec2 screen_pos = ImGui::GetCursorScreenPos();
		float available_width = ImGui::GetContentRegionAvail().x;
		float item_height = (ImGui::CalcTextSize(item.text) + style.FramePadding * 2).y;

		// Add invisible button taking the whole space to have a convenient big box to trigger the "hover" state.
		if (ImGui::InvisibleButton(item.id, ImVec2(available_width, item_height)))
		{
			state_changed = set_curve_edit_state(edit_state, item.state);
		}

		bool is_hovered = ImGui::IsItemHovered();
		bool is_selected = selected_item.state == item.state;

		// Add left mark when mouse is hovering the item
		ImVec2 left_box_min = ImGui::GetItemRectMin();
		ImVec2 left_box_max(left_box_min.x + style.FramePadding.x + 1, left_box_min.y + item_height);

		int color = is_hovered ? ImGuiCol_CheckMark : ImGuiCol_FrameBg;
		draw_list->AddRectFilled(left_box_min, left_box_max, ImGui::GetColorU32(color));

		// Advance cursor after the left mark
		ImVec2 left_box_size(left_box_max - left_box_min);
		screen_pos.x += left_box_size.x;

		// Restore imgui cursor where it was before the invisible button.
		ImGui::SetCursorScreenPos(screen_pos);

		// Display icon in a non-interactive button with custom color
		{
			ImGui::AlignTextToFramePadding();

			color = is_selected
				? ImGuiCol_ButtonHovered
				: ImGuiCol_Button;

			ImVec4 color_vec4 = ImGui::GetStyle().Colors[color];
			ImGui::PushStyleColor(ImGuiCol_Button, color_vec4);
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, color_vec4);
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, color_vec4);
			ImGui::Button(item.icon);
			ImGui::PopStyleColor(3);
		}

		// Display text
		ImGui::SameLine();
		ImGui::Text(item.text);
	}

	return state_changed;
}

static void display_font_combox(const char* label, float label_margin, int* selected_index)
{
	int index = *selected_index;
	const char* items[] = { "Regular", "Tendrilis" };

	const char* combo_preview_value = items[index];
	if (ImGui::BeginCombo(label, combo_preview_value))
	{
		for (int i = 0; i < IM_ARRAYSIZE(items); i++)
		{
			const bool is_selected = (index == i);
			if (ImGui::Selectable(items[i], is_selected))
				*selected_index = i;

			// Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
			if (is_selected)
				ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}
}

static void points_to_spiro(const td_point_array& points, td_path* path, bool is_closed)
{
	size_t count = points.size();

	if (!count)
	{
		return;
	}

	td_vector<spiro_cp> spiro;

	for (size_t i = 0; i < count; i += 1)
	{
		const td_vec2& p = points[i];

		// Skip if point overlap the previous one to avoid strange thing happening in the spiro algorithm.
		if (spiro.size()
			&& spiro.back().x == p.x
			&& spiro.back().y == p.y)
		{
			continue;
		}

		spiro.push_back({ p.x, p.y, 'c' });
	}

	if (spiro.size() && !is_closed)
	{
		spiro.front().ty = '{';
		spiro.back().ty = '}';
	}

	spiro_seg* spiro_segs = run_spiro(spiro.data(), (int)spiro.size());

	if (spiro_segs)
	{
		td_spiro_ctx spiro_ctx(path);

		spiro_to_bpath(spiro_segs, (int)spiro.size(), &spiro_ctx.base);

		free_spiro(spiro_segs);
	}
}

static void point_to_path(const td_point_array& points, td_path* path)
{
	if (!points.empty())
	{
		td::fit_curve_op f;

		float squared_error = 4.0f;

		f.fit_curve((td_vec2*)points.data(), (int)points.size(), squared_error);

		if (f.results.size())
		{
			path->move_to(f.results[0][0]);

			for (const td_bezier& b : f.results)
			{
				path->cubic_to(b.p[1], b.p[2], b.p[3]);
			}
		}
	}
}


