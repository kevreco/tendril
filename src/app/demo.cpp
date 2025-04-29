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

		// Quadratic to Cubic bezier
		// P0 = P0
		// P1 = P0 + (2/3) * (P1 - P0)
		// P2 = P2 + (2/3) * (P1 - P2)
		// P3 = P2

		td_vec2 p1;
		p1.x = ctx->current_point.x + (2.0f / 3.0f * ((float)x1 - ctx->current_point.x));
		p1.y = ctx->current_point.y + (2.0f / 3.0f * ((float)y1 - ctx->current_point.y));

		td_vec2 p2;
		p2.x = (float)x2 + (2.0f / 3.0f * ((float)x1 - (float)x2));
		p2.y = (float)y2 + (2.0f / 3.0f * ((float)y1 - (float)y2));

		ctx->path->cubic_to(p1.x, p1.y, p2.x, p2.y, (float)x2, (float)y2);

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

static void* to_id(const char* str);

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
	td_font_store::init_from_file(&regular_font, IMGUI_EXTENSION_TEXT_FONT, 0);
	td_font_store::init_from_file(&tendrilis_font, "./TendrilisExtra-Regular.ttf", 0);
	// Initialize bitmap with static buffer of rgba.
	bitmap = td_bitmap{ bitmap_buffer, TD_CANVAS_WIDTH , TD_CANVAS_HEIGHT };

	td_vec2 shorten_canvas_size = canvas_size;
	shorten_canvas_size.x *= 0.75f;

	// Text on line
	{
		td_path line;
		line.move_to(td_vec2{ 0.10f,        1.0f / 2.0f });
		line.line_to(td_vec2{ 1.0f - 0.10f, 1.0f / 2.0f });

		td::elements_multiply(&line, shorten_canvas_size);

		text_on_line.path = line;
	}

	// Text on polyline
	{
		td_path line;
		line.move_to(td_vec2{ 0.10f,        1.0f / 2.0f });
		line.line_to(td_vec2{ 0.5f,         1.0f / 3.0f });
		line.line_to(td_vec2{ 1.0f - 0.10f,  1.0f / 2.0f });

		td::elements_multiply(&line, shorten_canvas_size);

		text_on_polyline.path = line;
	}

	// Text on bezier curve
	{
		td_path curve;
		curve.move_to(td_vec2{ 0.15f, 0.50f });
		curve.cubic_to(td_vec2{ 0.15f + 0.15f, 0.35f }, td_vec2{ 1.0f - 0.15f - 0.15f, 0.35f }, td_vec2{ 1.0f - 0.15f, 0.50f });

		td::elements_multiply(&curve, shorten_canvas_size);

		text_on_bezier_curve.path = curve;
	}


	// Triangle on curve
	{
		
		td_path curve;
		curve.move_to(td_vec2{ 0.15f, 0.50f });
		curve.cubic_to(td_vec2{ 0.15f + 0.15f, 0.35f }, td_vec2{ 1.0f - 0.15f - 0.15f, 0.35f }, td_vec2{ 1.0f - 0.15f, 0.50f });

		td_path triangle;
		triangle.move_to(td_vec2{ 0.0f, 0.5f });
		triangle.line_to(td_vec2{ 1.0f, 0.0f });
		triangle.line_to(td_vec2{ 0.0f, -0.5f });
		triangle.close();

		triangle_on_curve.target = triangle;

		td::elements_multiply(&curve, shorten_canvas_size);

		triangle_on_curve.path = curve;

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
	// Draw tendrilis (other)
	{
		if (draw_tendrilis_other.edit_state == td_curve_edit_state_EDIT)
		{
			// Add first point that will be displayed under the mouse cursor.
			draw_tendrilis_other.points.push_back({});
		}
	}
	//
	// DEBUG
	//

	// Rect on line
	{
		td_path rect;
		rect.move_to(td_vec2{ 0.0f, 0.0f });
		rect.line_to(td_vec2{ 1.0f, 0.0f });
		rect.line_to(td_vec2{ 1.0f, 1.0f });
		rect.line_to(td_vec2{ 0.0f, 1.0f });
		rect.close();
		td::elements_multiply(&rect, td_vec2(1.0f, -1.0f));
		td::elements_multiply(&rect, td_vec2(100.f, 100.0f));

		td_path line;
		line.move_to(td_vec2{ 0.10f,        1.0f / 2.0f });
		line.line_to(td_vec2{ 1.0f - 0.10f,  1.0f / 2.0f });

		td::elements_multiply(&line, shorten_canvas_size);

		rect_on_line.target = rect;
		rect_on_line.path = line;
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

		td::elements_multiply(&line, shorten_canvas_size);

		curve_on_line.target = curve;
		curve_on_line.path = line;
	}

	// Grid on bezier curve
	{

		td_path curve;
		curve.move_to(td_vec2{ 0.15f, 0.50f });
		curve.cubic_to(td_vec2{ 0.15f + 0.15f, 0.35f }, td_vec2{ 1.0f - 0.15f - 0.15f, 0.35f }, td_vec2{ 1.0f - 0.15f, 0.50f });

		td_path bars;

		for (int i = 0; i < 10; i += 1)
		{
			if (i % 2)
			{
				bars.move_to(td_vec2{ 0.1f * i, 0.0f });
				bars.line_to(td_vec2{ 0.1f * i + 0.1f, 0.0f });

				bars.line_to(td_vec2{ 0.1f * i + 0.1f, 1.0f });
				bars.line_to(td_vec2{ 0.1f * i, 1.0f });
				bars.close();
			}
		}

		td_path target_curve = bars;

		td::elements_multiply(&target_curve, td_vec2(1.0f, -1.0f));
		td::elements_multiply(&target_curve, td_vec2(100.f, 100.0f));

		td::elements_multiply(&curve, shorten_canvas_size);
		grid_on_bezier_curve.path = curve;
		grid_on_bezier_curve.target = target_curve;
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
}

void td_demo::display_canvas_background(const td_vec2& scrolling)
{
	td_vec2i canvas_min = td_vec2i(0, 0);
	td_vec2i canvas_size = td_vec2i(TD_CANVAS_WIDTH, TD_CANVAS_HEIGHT);
	td_vec2i canvas_max{ canvas_min.x + canvas_size.x, canvas_min.y + canvas_size.y };

	// Draw background grid
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
	cfg.parameter_label_width = ImGui::CalcTextSize("XXXXXXXXXX").x;

	bool reset_requested = false;

	enum demo_type {
		demo_type_TEXT_ON_LINE,
		demo_type_TEXT_ON_POLYLINE,
		demo_type_TEXT_ON_BEZIER_CURVE,
		
		demo_type_TRIANGLE_ON_CURVE,
		demo_type_TENDRILIS_ON_CURVE,
		demo_type_DRAW_TENDRILIS_SPIRO,
		demo_type_DRAW_TENDRILIS_OTHER,

		demo_type_REGULAR_DEMO_COUNT,
		demo_type_DEBUG_DEMO_START,
		demo_type_RECT_ON_LINE,
		demo_type_CURVE_ON_LINE,
		demo_type_GRID_ON_BEZIER_CURVE,
		demo_type_ALL_DEMO_COUNT
	};

	static const char* ids[demo_type_ALL_DEMO_COUNT] = {
		text_on_line.id,
		text_on_polyline.id,
		text_on_bezier_curve.id,

		triangle_on_curve.id,
		tendrilis_on_curve.id,
		draw_tendrilis_spiro.id,
		draw_tendrilis_other.id,
		"demo_type_REGULAR_DEMO_COUNT",
		"demo_type_DEBUG_DEMO_START",
		rect_on_line.id,
		curve_on_line.id,
		grid_on_bezier_curve.id,
	};

	path_bender bender;

	const ImGuiViewport* main_viewport = ImGui::GetMainViewport();
	float vp_margin = 20.0f;
	ImGui::SetNextWindowPos(ImVec2(main_viewport->WorkPos.x + vp_margin, main_viewport->WorkPos.y + vp_margin), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(main_viewport->WorkSize.x - (vp_margin * 2), main_viewport->WorkSize.y - (vp_margin * 2)), ImGuiCond_FirstUseEver);

	if (ImGui::Begin("Tendril Demo"))
	{
		if (ImGui::Button("Reset demo"))
		{
			ImGui::OpenPopup("##PopUpReset");
		}

		if (ImGui::BeginPopup("##PopUpReset"))
		{
			ImGui::Text("Reset Confirmation:");
			if (ImGui::Selectable("Yes"))
			{
				reset_requested = true;
			}
			ImGui::Selectable("No");
			ImGui::EndPopup();
		}

		if (ImGui::BeginChild("##DemoListChild", ImVec2(300, 0), ImGuiChildFlags_ResizeX | ImGuiChildFlags_Borders |  ImGuiChildFlags_NavFlattened ))
		{
			if (ImGui::CollapsingHeader("Demos", ImGuiTreeNodeFlags_DefaultOpen))
			{
				if (ImGui::BeginTable("##DemoList", 1, ImGuiTableFlags_RowBg))
				{
					for (int i = 0; i < demo_type_REGULAR_DEMO_COUNT; i += 1)
					{
						ImGui::TableNextRow();
						ImGui::TableNextColumn();

						bool selected = false;
						char selected_label[64];
						snprintf(selected_label, sizeof(selected_label), "##%d", i);
						ImGuiSelectableFlags selectable_flags = ImGuiSelectableFlags_SpanAllColumns;
						if (ImGui::Selectable(selected_label, &selected, selectable_flags, ImVec2(0, 20)))
						{
							selected_demo = i;

						}
						ImGui::SameLine();
						ImGui::RadioButton(ids[i], &selected_demo, i);
					}

					ImGui::EndTable();
				}
			}

			if (ImGui::CollapsingHeader("Debug demos"))
			{
				if (ImGui::BeginTable("##DebugDemoList", 1, ImGuiTableFlags_RowBg))
				{
					for (int i = demo_type_DEBUG_DEMO_START + 1; i < demo_type_ALL_DEMO_COUNT; i += 1)
					{
						ImGui::TableNextRow();
						ImGui::TableNextColumn();

						bool selected = false;
						char selected_label[64];
						snprintf(selected_label, sizeof(selected_label), "##%d", i);
						ImGuiSelectableFlags selectable_flags = ImGuiSelectableFlags_SpanAllColumns;
						if (ImGui::Selectable(selected_label, &selected, selectable_flags, ImVec2(0, 20)))
						{
							selected_demo = i;

						}
						ImGui::SameLine();
						ImGui::RadioButton(ids[i], &selected_demo, i);
					}

					ImGui::EndTable();
				}
			}

			if (ImGui::CollapsingHeader("Options", ImGuiTreeNodeFlags_DefaultOpen))
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

				ImGui::Checkbox("Show Path", &show_path);
				ImGui::Checkbox("Edit Path Points", &edit_path_points);
				ImGui::SetNextItemWidth(item_width);
				ImGui::InputFloat("Path Offset", &path_offset, 1.0f);

				ImGui::SeparatorText("Normal"); // ===

				ImGui::Checkbox("Show Normals of Path", &show_normals);
				ImGui::SetNextItemWidth(item_width);
				ImGui::InputFloat("Normal Scale", &normal_scale, 5.0f);
			}

			if (ImGui::CollapsingHeader("Path Effects"))
			{
				ImGui::Checkbox("Wobble", &wobble.enabled);
				ImGui::Checkbox("Periodic Wave", &period.enabled);
			}
		}
		
		ImGui::EndChild();

		// Right side: draw properties
		ImGui::SameLine();

		ImGui::BeginChild("##Canvas", ImVec2(canvas_size.x + 35, 0));

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

			display_canvas(&bender, to_id(text_on_line.id), &text_on_line.path.points);

			ImGui::SeparatorText("Parameters");
			ImGui::Spacing();
			ImGui::Text("Text");   ImGui::SameLine(label_margin);  ImGui::InputText("##Text", &text_on_line.text);
			ImGui::Text("Font size");  ImGui::SameLine(label_margin);  ImGui::SliderFloat("##Font size", &text_on_line.font_size, 10, max_font_size);
			display_font_combox("Font: ", label_margin, &text_on_line.font_type);
			
			break;
		}
		case demo_type_TEXT_ON_POLYLINE:
		{
			td_font_store* font = get_font(text_on_polyline.font_type);

			text_on_polyline.target.clear();
			td::insert_text_to_path(font, text_on_polyline.text.data(), td_vec2(), text_on_polyline.font_size, &text_on_polyline.target);

			td_path* path = apply_various_effect(&text_on_polyline.path);

			bender.set(text_on_polyline.target, *path);

			display_canvas(&bender, to_id(text_on_polyline.id), &text_on_polyline.path.points);

			ImGui::SeparatorText("Parameters");
			ImGui::Spacing();
			ImGui::Text("Text");   ImGui::SameLine(label_margin);  ImGui::InputText("##Text", &text_on_polyline.text);
			ImGui::Text("Font size");  ImGui::SameLine(label_margin);  ImGui::SliderFloat("##Font size", &text_on_polyline.font_size, 10, max_font_size);
			display_font_combox("Font: ", label_margin, &text_on_polyline.font_type);
			break;
		}
		case demo_type_TEXT_ON_BEZIER_CURVE:
		{
			td_font_store* font = get_font(text_on_bezier_curve.font_type);

			text_on_bezier_curve.target.clear();
			td::insert_text_to_path(font, text_on_bezier_curve.text.data(), td_vec2(), text_on_bezier_curve.font_size, &text_on_bezier_curve.target);

			td_path* path = apply_various_effect(&text_on_bezier_curve.path);

			bender.set(text_on_bezier_curve.target, *path, path_bender_flags_INTERPOLATE_TANGENT);

			display_canvas(&bender, to_id(text_on_bezier_curve.id), &text_on_bezier_curve.path.points);

			ImGui::SeparatorText("Parameters");
			ImGui::Spacing();
			ImGui::Text("Text");   ImGui::SameLine(label_margin);  ImGui::InputText("##Text", &text_on_bezier_curve.text);
			ImGui::Text("Font size");  ImGui::SameLine(label_margin);  ImGui::SliderFloat("##Font size", &text_on_bezier_curve.font_size, 10, max_font_size);
			display_font_combox("Font: ", label_margin, &text_on_bezier_curve.font_type);
			break;
		}
		
		case demo_type_TRIANGLE_ON_CURVE:
		{
			td_path* path = apply_various_effect(&triangle_on_curve.path);

			int flags = path_bender_flags_INTERPOLATE_TANGENT | path_bender_flags_STRETCH_TARGET;
			bender.set(triangle_on_curve.target, *path, flags);

			display_canvas(&bender, to_id(triangle_on_curve.id), &triangle_on_curve.path.points);

			break;
		}
		case demo_type_TENDRILIS_ON_CURVE: {
			td_font_store* font = get_font(tendrilis_on_curve.font_type);

			tendrilis_on_curve.target.clear();
			td::insert_text_to_path(font, tendrilis_on_curve.text.data(), td_vec2(), tendrilis_on_curve.font_size, &tendrilis_on_curve.target);

			td_path* path = apply_various_effect(&tendrilis_on_curve.path);

			bender.set(tendrilis_on_curve.target, *path, path_bender_flags_INTERPOLATE_TANGENT);

			display_canvas(&bender, to_id(tendrilis_on_curve.id), &tendrilis_on_curve.path.points);

			ImGui::SeparatorText("Parameters");
			ImGui::Spacing();
			ImGui::Text("Text");   ImGui::SameLine(label_margin);  ImGui::InputText("##Text", &tendrilis_on_curve.text);
			ImGui::Text("Font size");  ImGui::SameLine(label_margin);  ImGui::SliderFloat("##Font size", &tendrilis_on_curve.font_size, 10, max_font_size);
			display_font_combox("Font: ", label_margin, &tendrilis_on_curve.font_type);
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

			canvas_mouse_state mouse_state = display_canvas(&bender, to_id(spiro.id), &spiro.points);

			bool edit_state_changed = display_curve_toolstrip(&spiro.edit_state);

			// On right click go back to view mode
			if (ImGui::IsMouseClicked(ImGuiMouseButton_Right))
			{
				edit_state_changed = set_curve_edit_state(&spiro.edit_state, td_curve_edit_state_VIEW) || edit_state_changed;
			}

			// Add or remove point depending on curve edit state.
			if (edit_state_changed)
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
				if (mouse_state.hovered)
				{
					spiro.points[spiro.points.size() - 1] = mouse_state.pos;
				}

				if (mouse_state.hovered && mouse_state.clicked)
				{
					spiro.points.push_back(mouse_state.pos);
				}
			}

			ImGui::SeparatorText("Parameters");
			ImGui::Spacing();
			ImGui::Text("Text");   ImGui::SameLine(label_margin);  ImGui::InputText("##Text", &spiro.text);
			ImGui::Text("Font size");  ImGui::SameLine(label_margin);  ImGui::SliderFloat("##Font size", &spiro.font_size, 10, max_font_size);
			display_font_combox("Font: ", label_margin, &spiro.font_type);
			break;
		}
		case demo_type_DRAW_TENDRILIS_OTHER: {

			auto& other = draw_tendrilis_other;

			td_font_store* font = get_font(other.font_type);

			// Adjust text if tendrilis font is used.
			if (font == &tendrilis_font)
			{
				normalize_tendrilis_text(&other.text);
			}

			other.target.clear();
			td::insert_text_to_path(font, other.text.data(), td_vec2(), other.font_size, &other.target);
			
			other.path.clear();

			if (other.points.size() > 2)
			{
				point_to_path(other.points, &other.path);
			}
			td_path* path = apply_various_effect(&other.path);

			bender.set(other.target, *path, path_bender_flags_INTERPOLATE_TANGENT);

			canvas_mouse_state state = display_canvas(&bender, to_id(other.id), &other.points);

			bool edit_state_changed = display_curve_toolstrip(&other.edit_state);
			
			// On right click go back to view mode
			if (ImGui::IsMouseClicked(ImGuiMouseButton_Right))
			{
				edit_state_changed = set_curve_edit_state(&other.edit_state, td_curve_edit_state_VIEW) || edit_state_changed;
			}

			// Add or remove point depending on curve edit state.
			if (edit_state_changed)
			{
				if (other.edit_state == td_curve_edit_state_EDIT)
				{
					other.points.push_back({});
				}
				else
				{
					other.points.erase(other.points.end() - 1);
				}
			}

			// Update point from mouse position depending on curve edit state.
			if (other.edit_state == td_curve_edit_state_EDIT)
			{
				if (state.hovered)
				{
					other.points.back() = state.pos;
				}

				if (state.hovered && state.clicked)
				{
					other.points.push_back(state.pos);
				}
			}
	
			ImGui::SeparatorText("Parameters");
			ImGui::Spacing();
			ImGui::Text("Text");   ImGui::SameLine(label_margin);  ImGui::InputText("##Text", &other.text);
			ImGui::Text("Font size");  ImGui::SameLine(label_margin);  ImGui::SliderFloat("##Font size", &other.font_size, 10, max_font_size);
			display_font_combox("Font: ", label_margin, &other.font_type);
			break;
		}
		case demo_type_RECT_ON_LINE: {

			td_path* path = apply_various_effect(&rect_on_line.path);

			bool smooth = true;
			bender.set(rect_on_line.target, *path, path_bender_flags_INTERPOLATE_TANGENT);

			display_canvas(&bender, to_id(rect_on_line.id), &rect_on_line.path.points);
			break;
		}
		case demo_type_CURVE_ON_LINE: {

			td_path* path = apply_various_effect(&curve_on_line.path);

			bool smooth = true;
			bender.set(curve_on_line.target, *path, path_bender_flags_INTERPOLATE_TANGENT);

			display_canvas(&bender, to_id(curve_on_line.id), &curve_on_line.path.points);
			break;
		}
		case demo_type_GRID_ON_BEZIER_CURVE: {

			td_path* path = apply_various_effect(&grid_on_bezier_curve.path);

			bool smooth = true;
			bender.set(grid_on_bezier_curve.target, *path, path_bender_flags_INTERPOLATE_TANGENT);

			display_canvas(&bender, to_id(grid_on_bezier_curve.id), &grid_on_bezier_curve.path.points);
			break;
		}
		}

		ImGui::EndChild();
	}
	ImGui::End();

	if (reset_requested)
	{
		td_demo new_demo;
		td_memswap_ptr(&new_demo, this);
	}
}

void td_demo::display_svg_widget(const td_path& path, float button_width)
{
	if (ImGui::Button("Save to SVG"))
	{
		td::to_svg(path, svg_file_name, TD_CANVAS_WIDTH, TD_CANVAS_HEIGHT, "darkgreen");

		ImGuiContext* ctx = ImGui::GetCurrentContext();
		if (ctx->PlatformIO.Platform_OpenInShellFn != NULL)
		{
			ctx->PlatformIO.Platform_OpenInShellFn(ctx, svg_file_name);
		}
	}
	ImGui::SameLine(button_width);
	ImGui::InputText("##SVG Export", svg_file_name, sizeof(svg_file_name));
}

td_demo::canvas_mouse_state td_demo::display_canvas(path_bender* bender, void* id, td_point_array* points)
{
	ImDrawList* draw_list = ImGui::GetWindowDrawList();
	ImVec2 canvas_min = ImGui::GetCursorScreenPos();

	td_demo::canvas_mouse_state state;

	ImVec2 canvas_size = ImVec2(TD_CANVAS_WIDTH, TD_CANVAS_HEIGHT);

	ImVec2 no_hovered_coord{ -9999.0f,-9999.0f };
	ImVec2 hovered_coord = no_hovered_coord;
	state.pos = no_hovered_coord;

	ImVec2 imgui_cursor_pos = ImGui::GetCursorPos();
	ImGui::PushID(id);
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

	if (edit_path_points && points)
	{
		ImVec2 grab_size = ImVec2(grab_point_size.x, grab_point_size.y);
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

	display_svg_widget(bender->result_path, cfg.parameter_label_width);
	ImGui::Spacing();
	// No scrolling yet.
	const td_vec2 scrolling{ 0.0f, 0.0f };

	// Display background color and the lines for the grid.
	display_canvas_background(scrolling);

	rasterizer.render_fill_path(bender->result_path, bitmap.data, bitmap.width, bitmap.height, green_tendrilis);
	td_vec2 point_hovered{ -9999.0f,-9999.0f };
	if (point_hovered_index >= 0)
	{
		point_hovered = points->at(point_hovered_index);
	}
	display_bender_overlay(this, points, &bender->pw_directive_path, point_hovered);

	bool loaded_or_updated = app_backend::load_or_update_texture(bitmap.data, bitmap.width, bitmap.height, &texture);
	TD_ASSERT(loaded_or_updated);
	return state;
}

td_font_store* td_demo::get_font(int font_type)
{
	return font_type == td_font_type_REGULAR ? &regular_font : &tendrilis_font;
}

static void* to_id(const char* str)
{
	return (void*)str;
}

static void display_bender_overlay(td_demo* demo, const td_point_array* directive_path_points, const td_piecewise_path* directive_pw_path, const td_vec2& point_hovered)
{
	const td_point_array* points = directive_path_points;
	const td_piecewise_path* pw = directive_pw_path;
	td_path stroke_path;
	td_path fill_path;
	// Display path
	if (pw->points.size())
	{
		if (demo->show_path)
		{
			td_vec2 point = pw->points[0];
			point += pw->unit_normal_at(0) * demo->path_offset;
			stroke_path.move_to(point);

			for (int i = 1; i < pw->points.size(); i += 1)
			{
				point = pw->points[i];
				point += pw->unit_normal_at(i) * demo->path_offset;
				stroke_path.line_to(point);
			}
		}

		if (demo->show_normals)
		{
			for (int i = 0; i < pw->points.size(); i += 1)
			{
				td_vec2 p = pw->points[i];
				td_vec2 n = pw->unit_normal_at(i) * demo->normal_scale;

				td_vec2 normal_start = p - n;
				td_vec2 normal_end = p + n;
				stroke_path.move_to(normal_start);
				stroke_path.line_to(normal_end);
			}
		}
	}

	// Draw directive path points.
	if (demo->edit_path_points && points)
	{
		for (int i = 0; i < points->size(); i += 1)
		{
			td_vec2 p = points->at(i);
			// Ignore the point that is hovered, it will be displayed later.
			if (point_hovered == p)
			{
				continue;
			}

			td_vec2 margin = demo->display_point_size * 0.5f;
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

	td_vec2 margin = demo->display_point_size * 0.5f;
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
	bool state_changed = false;

	ImGui::SeparatorText("Toolstrip");

	struct item_type {
		td_curve_edit_state state;
		const char* icon;
		const char* tooltip;
	} items[] = {
		{td_curve_edit_state_VIEW, ICON_LC_SPLINE, "Move curve point"},
		{td_curve_edit_state_EDIT, ICON_LC_GIT_BRANCH_PLUS, "Add point to curve" }
	};

	item_type selected_item = items[*edit_state];

	for (size_t i = 0; i < td_size(items); i += 1)
	{
		item_type item = items[i];


		int color = selected_item.state == item.state
			? ImGuiCol_CheckMark
			: ImGuiCol_FrameBg;

		ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[color]);


		if (ImGui::Button(item.icon))
		{
			state_changed = set_curve_edit_state(edit_state, item.state);
		}

		ImGui::SetItemTooltip(item.tooltip);

		// Restore button color
		ImGui::PopStyleColor();

		// Display items on the same line
		if (i != td_size(items) - 1)
		{
			ImGui::SameLine();
		}
	}

	return state_changed;
}

static void display_font_combox(const char* label, float label_margin, int* selected_index)
{
	ImGui::Text(label);
	ImGui::SameLine(label_margin);

	int index = *selected_index;
	const char* items[] = { "Regular","Tendrilis" };

	const char* combo_preview_value = items[index];
	if (ImGui::BeginCombo("##Font", combo_preview_value))
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


