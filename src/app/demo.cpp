#include "app.hpp"

#include <string>

#include "effects/wobbler.hpp"
#include "effects/periodic.hpp"
#include "fit_curves.hpp"

#include "tendril_internal.hpp"

static td_rgba8 background_color{ 220, 210, 205, 255 };

static td_rgba8 shape_color{ 10, 85, 5, 255 };
// color for guiding path and control point.
static td_rgba8 extra_drawing_color{ 180, 90, 180, 255 };
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
		td::path_to_piecewise_path(directive_path, &pw_directive_path);

		const td_path* target = &target_path;

		td_path stretched_target;

		if (flags & path_bender_flags_STRETCH_TARGET)
		{
			target_path.clone_to(&stretched_target);
			// @TODO make it strech also on Y axis, we should probably forward a td_vec2 for scale
			td::elements_multiply(&stretched_target, td_vec2(pw_directive_path.length(), 5.0f));

			target = &stretched_target;
		}

		td::path_to_fragmented_path(*target, &result_path, 2.0f);

		// Transform
		bool interpolate_tangent = flags & path_bender_flags_INTERPOLATE_TANGENT;
		td::transform_along_piecewise(pw_directive_path, &result_path.points, interpolate_tangent);
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

static wobbler wobble;
static periodic period;

static td_path* apply_various_effect(td_path* input_path);

// Remove new line and replace them with the separator "/".
// Remove space and replace them with the separator "/"
// as suggested from the guide: https://anomalis.gumroad.com/l/tendrilis-guide
static void normalize_tendrilis_text(std::string* str);

static std::string normalized_tendrilis_text(std::string* str);

// Returns true if the state was changed.
static bool set_curve_edit_state(td_curve_edit_state* edit_state, td_curve_edit_state state);

static bool display_curve_tools_panel(td_curve_edit_state* edit_state);

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
		text_on_line.control_path = directive_line;
	}

	// Text on polyline
	{
		td_path polyline;
		polyline.move_to(td_vec2{ 0.10f,        1.0f / 2.0f });
		polyline.line_to(td_vec2{ 0.5f,         1.0f / 3.0f });
		polyline.line_to(td_vec2{ 1.0f - 0.10f,  1.0f / 2.0f });

		td::elements_multiply(&polyline, canvas_size);

		text_on_polyline.control_path = polyline;
	}

	// Text on curve
	{
		text_on_curve.control_path = directive_curve;
	}

	// Triangle on curve
	{
		td_path triangle;
		triangle.move_to(td_vec2{ 0.0f, 2.5f });
		triangle.line_to(td_vec2{ 1.0f, 0.0f });
		triangle.line_to(td_vec2{ 0.0f, -2.5f });
		triangle.close();

		triangle_on_curve.shape = triangle;

		triangle_on_curve.control_path = directive_curve;
	}

	// Tendrilis on curve
	{
		td_path curve;
		curve.move_to(td_vec2{ 0.10f, 0.80f });
		curve.cubic_to(td_vec2{ 0.60f, 0.65f }, td_vec2{ 0.30f, 0.15f }, td_vec2{ 0.80f, 0.20f });

		td::elements_multiply(&curve, canvas_size);

		tendrilis_on_curve.control_path = curve;
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

		rect_on_line.shape = rect;
		rect_on_line.control_path = directive_line;
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

		curve_on_line.control_path = line;

		curve_on_line.shape = curve;

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

		directive_curve.clone_to(&vbars_on_curve.control_path);
		vbars_on_curve.shape = bars;
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

		hbars_on_curve.control_path = directive_curve;
		hbars_on_curve.shape = bars;
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

	// Upload texture to GPU.
	bool loaded_or_updated = app_backend::load_or_update_texture(bitmap.data, bitmap.width, bitmap.height, &texture);
	TD_ASSERT(loaded_or_updated);
}

void td_demo::display_options()
{
	const float item_width = 140;

	if (ImGui::TreeNode("Colors"))
	{
		ImGui::SetNextItemWidth(item_width);
		ImGuiEx::U32ColorEdit3("Filling Color##Drawing", shape_color.to_u32_ptr());
		ImGui::SetNextItemWidth(item_width);
		ImGuiEx::U32ColorEdit3("Control Path Color##Directive", extra_drawing_color.to_u32_ptr());
		ImGui::SetNextItemWidth(item_width);
		ImGuiEx::U32ColorEdit3("Point Hover Color##Directive", point_hover_color.to_u32_ptr());
		ImGui::SetNextItemWidth(item_width);
		ImGuiEx::U32ColorEdit4("Bg Color##Directive", background_color.to_u32_ptr());
		ImGui::SetNextItemWidth(item_width);
		ImGuiEx::U32ColorEdit3("Bg Grid Color##Directive", background_grid_color.to_u32_ptr());

		ImGui::TreePop();
	}

	ImGui::SeparatorText("Path"); // ===

	ImGui::Checkbox("Show Control Path", &cfg.show_control_path);
	ImGui::Checkbox("Show Control Points", &cfg.show_control_points);
	ImGui::SetNextItemWidth(item_width);
	ImGui::InputFloat("Path Offset", &cfg.path_offset, 1.0f);

	ImGui::SeparatorText("Normal"); // ===

	ImGui::Checkbox("Show Normals of Path", &cfg.show_normals);
	ImGui::SetNextItemWidth(item_width);
	ImGui::InputFloat("Scale", &cfg.normal_scale, 5.0f);

	ImGui::SeparatorText("Background"); // ===

	ImGui::Checkbox("Show Grid", &cfg.show_background_grid);

	ImGui::SeparatorText("Other"); // ===

	ImGui::Checkbox("Show Debug Demos", &cfg.show_debug_demos);
	ImGui::Checkbox("Show Options On Left Panel", &cfg.show_options_on_left_panel);
	ImGui::Checkbox("Fullscren", &cfg.fullscreen);

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
void td_demo::display_demo()
{
	// No scrolling yet
	td_vec2 scrolling;

	// Draw background color and the lines for the grid.
	render_background(scrolling);

	// Avoid moving window by mistake: only move window from the title bar.
	ImGui::GetCurrentContext()->IO.ConfigWindowsMoveFromTitleBarOnly = true;

	cfg.parameter_label_width = ImGui::CalcTextSize("XXXXXXXXXXX").x;

	struct demo_header {
		const char* id;
		const char* description;
	};

	static demo_header demo_headers[demo_type_ALL_DEMO_COUNT] = {
		{text_on_line.id,				 text_on_line.description },
		{text_on_polyline.id,			 text_on_polyline.description },
		{text_on_curve.id,				 text_on_curve.description },
										
		{triangle_on_curve.id,			 triangle_on_curve.description, },
		{tendrilis_on_curve.id,			 tendrilis_on_curve.description, },
		{draw_tendrilis_spiro.id,		 draw_tendrilis_spiro.description, },
		{"demo_type_REGULAR_DEMO_COUNT", NULL },
		{"demo_type_DEBUG_DEMO_START",	 NULL },
		{rect_on_line.id,				 NULL },
		{curve_on_line.id,				 NULL },
		{vbars_on_curve.id,				 NULL },
		{hbars_on_curve.id,				 NULL },
		{draw_arc.id,					 NULL }
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

	ImGuiWindowFlags flags = 0;

	if (cfg.fullscreen)
	{
		flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings;

		const ImGuiViewport* main_vp = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(main_vp->WorkPos);
		ImGui::SetNextWindowSize(main_vp->WorkSize);

		// Remove border
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	}
	else
	{
		const ImGuiViewport* main_vp = ImGui::GetMainViewport();
		float vp_margin = 20.0f;
		ImGui::SetNextWindowPos(ImVec2(main_vp->WorkPos.x + vp_margin, main_vp->WorkPos.y + vp_margin), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(main_vp->WorkSize.x - (vp_margin * 2), main_vp->WorkSize.y - (vp_margin * 2)), ImGuiCond_FirstUseEver);
	}

	bool window_opened = ImGui::Begin("Tendril Demo", NULL, flags);

	if (cfg.fullscreen)
	{
		// Restore border
		ImGui::PopStyleVar();
	}

	if (window_opened)
	{
		ImGui::BeginChild("##LeftPanel", ImVec2(250, 0), ImGuiChildFlags_ResizeX | ImGuiChildFlags_Borders | ImGuiChildFlags_NavFlattened);
		
		if (ImGui::CollapsingHeader("Demos", ImGuiTreeNodeFlags_DefaultOpen))
		{
			if (ImGui::BeginChild("##DemoList", ImVec2(0.0f, 0.0f), ImGuiChildFlags_AutoResizeY))
			{
				for (int i = 0; i < demo_type_REGULAR_DEMO_COUNT; i += 1)
				{
					ImGui::RadioButton(demo_headers[i].id, &selected_demo, i);

					// Display description if there is one
					if (demo_headers[i].description)
					{
						ImGui::SameLine();
						ImGuiEx::HelpMarker(demo_headers[i].description);
					}
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
						ImGui::RadioButton(demo_headers[i].id, &selected_demo, i);
						// Display description if there is one
						if (demo_headers[i].description)
						{
							ImGui::SameLine();
							ImGuiEx::HelpMarker(demo_headers[i].description);
						}
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
		ImGui::Text("Demo: %s", demo_headers[selected_demo].id);

		float label_margin = cfg.parameter_label_width;
		float max_font_size = 150.0f;

		switch (selected_demo)
		{
		case demo_type_TEXT_ON_LINE:
		{
			td_font_store* font = get_font(text_on_line.font_type);

			text_on_line.shape.clear();

			// Convert text to path
			td::insert_text_to_path(font, text_on_line.text.data(), td_vec2(), text_on_line.font_size, &text_on_line.shape);

			td_path* path = apply_various_effect(&text_on_line.control_path);

			bender.set(text_on_line.shape, *path);

			td_canvas_info info = setup_canvas_layout_with_bender(&bender, &text_on_line.control_path.points);

			render_shape(bender.result_path);
			render_control_path(text_on_line.control_path);
			render_control_points(text_on_line.control_path.points, info.control_point_hovered);
			break;
		}
		case demo_type_TEXT_ON_POLYLINE:
		{
			td_font_store* font = get_font(text_on_polyline.font_type);

			text_on_polyline.shape.clear();

			// Convert text to path
			td::insert_text_to_path(font, text_on_polyline.text.data(), td_vec2(), text_on_polyline.font_size, &text_on_polyline.shape);

			td_path* path = apply_various_effect(&text_on_polyline.control_path);

			bender.set(text_on_polyline.shape, *path);

			td_canvas_info info = setup_canvas_layout_with_bender(&bender, &text_on_polyline.control_path.points);

			render_shape(bender.result_path);
			render_control_path(text_on_polyline.control_path);
			render_control_points(text_on_polyline.control_path.points, info.control_point_hovered);
			break;
		}
		case demo_type_TEXT_ON_CURVE:
		{
			td_font_store* font = get_font(text_on_curve.font_type);

			text_on_curve.shape.clear();

			// Convert text to path
			td::insert_text_to_path(font, text_on_curve.text.data(), td_vec2(), text_on_curve.font_size, &text_on_curve.shape);

			td_path* path = apply_various_effect(&text_on_curve.control_path);

			bender.set(text_on_curve.shape, *path, path_bender_flags_INTERPOLATE_TANGENT);

			td_canvas_info info = setup_canvas_layout_with_bender(&bender, &text_on_curve.control_path.points);

			render_shape(bender.result_path);
			render_control_path(text_on_curve.control_path);
			render_control_points(text_on_curve.control_path.points, info.control_point_hovered);
			break;
		}
		
		case demo_type_TRIANGLE_ON_CURVE:
		{
			td_path* path = apply_various_effect(&triangle_on_curve.control_path);

			int flags = path_bender_flags_INTERPOLATE_TANGENT | path_bender_flags_STRETCH_TARGET;
			bender.set(triangle_on_curve.shape, *path, flags);

			td_canvas_info info = setup_canvas_layout_with_bender(&bender, &triangle_on_curve.control_path.points);

			render_shape(bender.result_path);
			render_control_path(triangle_on_curve.control_path);
			render_control_points(triangle_on_curve.control_path.points, info.control_point_hovered);
			break;
		}
		case demo_type_TENDRILIS_ON_CURVE: {
			td_font_store* font = get_font(tendrilis_on_curve.font_type);

			// Adjust text if tendrilis font is used.
			std::string text = font == &tendrilis_font
				? normalized_tendrilis_text(&tendrilis_on_curve.text)
				: tendrilis_on_curve.text;

			tendrilis_on_curve.shape.clear();

			// Convert text to path
			td::insert_text_to_path(font, text.data(), td_vec2(), tendrilis_on_curve.font_size, &tendrilis_on_curve.shape);

			td_path* path = apply_various_effect(&tendrilis_on_curve.control_path);

			bender.set(tendrilis_on_curve.shape, *path, path_bender_flags_INTERPOLATE_TANGENT);

			td_canvas_info info = setup_canvas_layout_with_bender(&bender, &tendrilis_on_curve.control_path.points);

			render_shape(bender.result_path);
			render_control_path(tendrilis_on_curve.control_path);
			render_control_points(tendrilis_on_curve.control_path.points, info.control_point_hovered);
			break;
		}

		case demo_type_DRAW_TENDRILIS_SPIRO: {

			auto& spiro = draw_tendrilis_spiro;

			td_font_store* font = get_font(spiro.font_type);

			// Adjust text if tendrilis font is used.
			std::string text = font == &tendrilis_font
				? normalized_tendrilis_text(&spiro.text)
				: spiro.text;

			spiro.shape.clear();
		
			// Convert text to path
			td::insert_text_to_path(font, text.data(), td_vec2(), spiro.font_size, &spiro.shape);

			td_point_array points = spiro.points;

			// Display control point under cursor in edit state.
			if (spiro.edit_state == td_curve_edit_state_EDIT
				&& spiro.canvas_info.is_canvas_hovered)
			{
				points.push_back(spiro.canvas_info.mouse_pos);
			}

			spiro.control_path.clear();
			points_to_spiro(points, &spiro.control_path);

			td_path* path = apply_various_effect(&spiro.control_path);

			bender.set(spiro.shape, *path, path_bender_flags_INTERPOLATE_TANGENT);

			spiro.canvas_info = setup_canvas_layout_with_bender(&bender, &spiro.points);

			render_shape(bender.result_path);
			render_control_path(spiro.control_path);
			render_control_points(points, spiro.canvas_info.control_point_hovered);

			// Handle interaction
			{
				// On right click go back to view mode.
				if (ImGui::IsMouseClicked(ImGuiMouseButton_Right))
				{
					spiro.edit_state_changed = set_curve_edit_state(&spiro.edit_state, td_curve_edit_state_VIEW) || spiro.edit_state_changed;
				}

				// On left click add a point.
				if (spiro.edit_state == td_curve_edit_state_EDIT
					&& spiro.canvas_info.is_canvas_hovered
					&& spiro.canvas_info.clicked)
				{
					spiro.points.push_back(spiro.canvas_info.mouse_pos);
				}
			}
			break;
		}
		case demo_type_RECT_ON_LINE: {

			td_path* path = apply_various_effect(&rect_on_line.control_path);

			bool smooth = true;
			bender.set(rect_on_line.shape, *path, path_bender_flags_INTERPOLATE_TANGENT);

			td_canvas_info info = setup_canvas_layout_with_bender(&bender, &rect_on_line.control_path.points);

			render_shape(bender.result_path);
			render_control_path(rect_on_line.control_path);
			render_control_points(rect_on_line.control_path.points, info.control_point_hovered);
			break;
		}
		case demo_type_CURVE_ON_LINE: {

			td_path* path = apply_various_effect(&curve_on_line.control_path);

			bool smooth = true;
			bender.set(curve_on_line.shape, *path, path_bender_flags_INTERPOLATE_TANGENT);

			td_canvas_info info = setup_canvas_layout_with_bender(&bender, &curve_on_line.control_path.points);

			render_shape(bender.result_path);
			render_control_path(curve_on_line.control_path);
			render_control_points(curve_on_line.control_path.points, info.control_point_hovered);
			break;
		}
		case demo_type_VBARS_ON_CURVE: {

			td_path* path = apply_various_effect(&vbars_on_curve.control_path);

			bool smooth = true;
			bender.set(vbars_on_curve.shape, *path, path_bender_flags_INTERPOLATE_TANGENT);

			td_canvas_info info = setup_canvas_layout_with_bender(&bender, &vbars_on_curve.control_path.points);

			render_shape(bender.result_path);
			render_control_path(vbars_on_curve.control_path);
			render_control_points(vbars_on_curve.control_path.points, info.control_point_hovered);

			break;
		}
		case demo_type_HBARS_ON_CURVE: {

			td_path* path = apply_various_effect(&hbars_on_curve.control_path);

			bool smooth = true;
			bender.set(hbars_on_curve.shape, *path, path_bender_flags_INTERPOLATE_TANGENT);

			td_canvas_info info = setup_canvas_layout_with_bender(&bender, &hbars_on_curve.control_path.points);

			render_shape(bender.result_path);
			render_control_path(hbars_on_curve.control_path);
			render_control_points(hbars_on_curve.control_path.points, info.control_point_hovered);
			break;
		}
		case demo_type_DRAW_ARC: {

			td_path path;
			path.clear();
			path.move_to(draw_arc.mx, draw_arc.my);
			path.arc_to(draw_arc.rx, draw_arc.ry, draw_arc.xrotation, draw_arc.large_arc, draw_arc.sweep_flag, draw_arc.x, draw_arc.y);

			td_canvas_info info = setup_canvas_layout(path, NULL, NULL);

			render_shape(path);
			break;
		}
		}

		ImGui::EndChild(); // ##Canvas

		ImGui::SameLine();
		ImGui::BeginChild("##RightPanel", ImVec2(400.0f, 0.0f), ImGuiChildFlags_Borders );
		
		switch (selected_demo)
		{
		case demo_type_TEXT_ON_LINE:
		{
			ImGui::SeparatorText("Parameters");
			ImGui::Spacing();
			ImGuiEx::InputText("Text", &text_on_line.text);
			ImGui::SliderFloat("Font size", &text_on_line.font_size, 10, max_font_size);
			display_font_combox("Font ", label_margin, &text_on_line.font_type);

			break;
		}
		case demo_type_TEXT_ON_POLYLINE:
		{
			ImGui::SeparatorText("Parameters");
			ImGui::Spacing();
			ImGuiEx::InputText("Text", &text_on_polyline.text);
			ImGui::SliderFloat("Font size", &text_on_polyline.font_size, 10, max_font_size);
			display_font_combox("Font ", label_margin, &text_on_polyline.font_type);
			break;
		}
		case demo_type_TEXT_ON_CURVE:
		{
			ImGui::SeparatorText("Parameters");
			ImGui::Spacing();
			ImGuiEx::InputText("Text", &text_on_curve.text);
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
			ImGuiEx::InputTextMultiline("Text", &tendrilis_on_curve.text, ImVec2(0, 70));
			ImGui::SliderFloat("Font size", &tendrilis_on_curve.font_size, 10, max_font_size);
			display_font_combox("Font ", label_margin, &tendrilis_on_curve.font_type);
			break;
		}

		case demo_type_DRAW_TENDRILIS_SPIRO: {

			auto& spiro = draw_tendrilis_spiro;
			spiro.edit_state_changed = display_curve_tools_panel(&spiro.edit_state);

			ImGui::SeparatorText("Parameters");
			ImGui::Spacing();
			ImGuiEx::InputTextMultiline("Text", &spiro.text, ImVec2(0,70));
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
		td::path_to_svg_file(path, svg_file_name, TD_CANVAS_WIDTH, TD_CANVAS_HEIGHT, "darkgreen");

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

td_demo::td_canvas_info td_demo::setup_canvas_layout(const td_path& target_path, const td_piecewise_path* pw_directive_path, td_point_array* points)
{
	ImDrawList* draw_list = ImGui::GetWindowDrawList();
	ImVec2 canvas_min = ImGui::GetCursorScreenPos();

	td_demo::td_canvas_info info;

	ImVec2 canvas_size = ImVec2(TD_CANVAS_WIDTH, TD_CANVAS_HEIGHT);

	ImVec2 no_hovered_coord{ -9999.0f,-9999.0f };
	ImVec2 hovered_coord = no_hovered_coord;

	ImVec2 imgui_cursor_pos = ImGui::GetCursorPos();
	ImGui::PushID(texture);
	ImGuiEx::ImageWithBorder((ImTextureID)(intptr_t)texture, canvas_size);

	if (ImGui::IsItemHovered())
	{
		info.mouse_pos = ImGui::GetMousePos() - ImGui::GetItemRectMin();
		hovered_coord = info.mouse_pos;

		if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
		{
			info.clicked = true;
		}

		info.is_canvas_hovered = true;
	}

	ImGui::PopID();

	// Save current imgui cursor before drawing invisible buttons.
	ImVec2 cursor_after_image = ImGui::GetCursorPos();

	int point_hovered_index = -1;

	if (cfg.show_control_points && points)
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

	if (point_hovered_index >= 0)
	{
		info.control_point_hovered = points->at(point_hovered_index);
	}

	return info;
}

td_demo::td_canvas_info td_demo::setup_canvas_layout_with_bender(path_bender* bender, td_point_array* points)
{
	return setup_canvas_layout(bender->result_path, &bender->pw_directive_path, points);
}

void td_demo::render_background(const td_vec2& scrolling)
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


void td_demo::render_shape(const td_path& path)
{
	rasterizer.render_fill_path(path, bitmap.data, bitmap.width, bitmap.height, shape_color);
}

void td_demo::render_control_path(const td_path& path)
{
	if (cfg.show_control_path)
	{
		if (cfg.path_offset != 0.0f)
		{
			tmp_for_offset.clear();
			td::path_to_offset_path(path , &tmp_for_offset, cfg.path_offset);

			rasterizer.render_stroke_path(tmp_for_offset, bitmap.data, bitmap.width, bitmap.height, extra_drawing_color);
		}
		else
		{
			rasterizer.render_stroke_path(path, bitmap.data, bitmap.width, bitmap.height, extra_drawing_color);
		}
	}
}
void td_demo::render_control_points(const td_point_array& points, const td_vec2& hovered_point)
{
	render_control_points(points.data(), points.size(), hovered_point);
}

void td_demo::render_control_points(const td_vec2* points, size_t count, const td_vec2& hovered_point)
{
	if (cfg.show_control_points)
	{
		render_fill_buffer.clear();
		render_stroke_buffer.clear();

		td_vec2 margin = cfg.display_point_size * 0.5f;

		for (int i = 0; i < count; i += 1)
		{
			td_vec2 p = points[i];

			// Ignore the point that is hovered, it will be displayed later.
			if (hovered_point == p)
			{
				continue;
			}

			td_rect r{ p - margin , p + margin };
			render_stroke_buffer.add_rect(r.min, r.max);
			r.reduce(1.0f);
			render_fill_buffer.add_rect(r.min, r.max);
		}

		// @OPT: Ideally we shouldn't need to stroke/fill to rasterize rectangles.
		rasterizer.render_stroke_path(render_stroke_buffer, bitmap.data, bitmap.width, bitmap.height, extra_drawing_color);
		rasterizer.render_fill_path(render_fill_buffer, bitmap.data, bitmap.width, bitmap.height, background_color);

		// Draw hovered point
		render_fill_buffer.clear();
		td_rect r{ hovered_point - margin , hovered_point + margin };
		render_fill_buffer.add_rect(r.min, r.max);
		rasterizer.render_fill_path(render_fill_buffer, bitmap.data, bitmap.width, bitmap.height, extra_drawing_color);
	}
}

void td_demo::render_lines(const td_vec2* points, size_t count, const td_rgba8& color)
{

}

td_font_store* td_demo::get_font(int font_type)
{
	return font_type == td_font_type_REGULAR ? &regular_font : &tendrilis_font;
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
		if (str->at(i) == ' ' || str->at(i) == '\r' || str->at(i) == '\n')
		{
			str->at(i) = '/';
		}
	}
}

static std::string normalized_tendrilis_text(std::string* str)
{
	std::string normalized = *str;
	normalize_tendrilis_text(&normalized);
	return normalized;
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

static bool display_curve_tools_panel(td_curve_edit_state* edit_state)
{
	ImGuiStyle& style = ImGui::GetCurrentContext()->Style;
	ImDrawList* draw_list = ImGui::GetWindowDrawList();

	bool state_changed = false;

	ImGui::SeparatorText("Tools");

	struct item_type {
		td_curve_edit_state state;
		const char* icon;
		const char* id;
		const char* text;
	} items[] = {
		{td_curve_edit_state_VIEW, " " ICON_LC_SPLINE, "##1", "Move curve point"},
		{td_curve_edit_state_EDIT,  " " ICON_LC_GIT_BRANCH_PLUS, "##2", "Add point to curve" }
	};

	item_type selected_item = items[*edit_state];

	for (size_t i = 0; i < td_size(items); i += 1)
	{
		item_type item = items[i];

		ImVec2 screen_pos = ImGui::GetCursorScreenPos();
		float available_width = ImGui::GetContentRegionAvail().x;
		float item_height = (ImGui::CalcTextSize(item.text) + style.FramePadding * 2).y + style.ItemSpacing.y;

		// Add invisible button taking the whole space to have a convenient big box to trigger the "hover" state.
		if (ImGui::InvisibleButton(item.id, ImVec2(available_width, item_height)))
		{
			state_changed = set_curve_edit_state(edit_state, item.state);
		}

		bool is_hovered = ImGui::IsItemHovered();
		bool is_selected = selected_item.state == item.state;

		ImGui::SetCursorScreenPos(screen_pos);

		// Emulate standard radio button interaction 
		if (is_hovered)
		{
			ImGui::PushStyleColor(ImGuiCol_FrameBg, ImGui::GetStyle().Colors[ImGuiCol_FrameBgHovered]);
		}

		if (ImGui::RadioButton(item.icon, is_selected))
		{
			state_changed = set_curve_edit_state(edit_state, item.state);
		}

		if (is_hovered)
		{
			ImGui::PopStyleColor();
		}

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


