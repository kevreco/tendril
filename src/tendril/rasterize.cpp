#include "tendril.hpp"
#include "tendril_internal.hpp"

//  Table of content:
//
//    td_rasterizer
//    bytes operations

#ifdef __cplusplus
extern "C" {
#endif

#define STANDALONE_
#define FALL_THROUGH

#include "td_ftgrays.c"

#ifdef __cplusplus
}
#endif

// Convert point to FreeType point by scaling it.
#define TO_FT_POS(x) (FT_Pos)((x) * 64)

struct td_contour_builder
{
    td_rasterizer* r;
    const td_path& path;
    const td_stroke_option& opt;
    int anchor_index = 0;
    int i = 0;
    bool was_closed = false;

    td_contour_builder(td_rasterizer* rasterizer, const td_path& _path, const td_stroke_option& _opt)
        : r(rasterizer), path(_path), opt(_opt)
    {
        r->points.reserve(r->points.size() + _path.points.size() * 2);
        r->tags.reserve(r->tags.size() + _path.points.size() * 2);
    }

    void add_contour()
    {
        int count = i - anchor_index;
        if (count)
        {
            td_rasterizer::add_half_stroke(add_point_func, this, path.points, anchor_index, i - 1, was_closed, opt);

            if (was_closed)
            {
                r->contours.push_back(r->points.size() - 1);
            }

            td_rasterizer::add_half_stroke(add_point_func, this, path.points, i - 1, anchor_index, was_closed, opt);

            r->contours.push_back(r->points.size() - 1);
        }
    }

    static void add_point_func(void* ctx, const td_vec2& p)
    {
        td_contour_builder* b = (td_contour_builder*)ctx;
        b->r->points.push_back({ TO_FT_POS(p.x), TO_FT_POS(p.y) });
        b->r->tags.push_back({ FT_CURVE_TAG_ON });
        b->r->clip_rect.add(p);
    }

    static void traverse_func(void* ctx, td_path_cmd cmd, const td_vec2* points)
    {
        td_contour_builder* b = (td_contour_builder*)ctx;
        switch (cmd)
        {
        case td_path_cmd_MOVE_TO:
        {
            b->add_contour();

            b->anchor_index = b->i;
            b->i += 1;
            b->was_closed = false;
        }
        break;
        case td_path_cmd_LINE_TO:
        {
            b->i += 1;
        }
        break;
        case td_path_cmd_CUBIC_TO:
        {
            TD_ASSERT(0 && "stroke must be flatten");
        }
        break;
        case td_path_cmd_CLOSE:
        {
            b->i += 1;
            b->was_closed = true;
        }
        break;
        default:
            TD_ASSERT(0 && "Unreachable");
        }
    }
};


struct td_rasterizer_context {
    td_rgba8* bitmap = NULL;
    int height = 0;
    int width = 0;
    td_rgba8 color{0,0,0,0};
    td_composite_operation_func_t operation = NULL;
};

static void td_rasterizer_spanner(int y, int count, const FT_Span* spans, void* user)
{
    td_rasterizer_context* ctx = (td_rasterizer_context*)user;

    td_rgba8 color = ctx->color;
    size_t image_witdh = ctx->width;
    uint32_t* dest = (uint32_t*)(ctx->bitmap + y * image_witdh);

    // Render all spans
    while (count--)
    {
        unsigned char alpha = spans[count].coverage;

        ctx->operation(dest + spans[count].x, spans[count].len, color, alpha);
    }
}

void td_rasterizer::command_begin()
{
    points.clear();
    contours.clear();
    tags.clear();
    clip_rect = td_rect{ 0.0f, 0.0f , 0.0f , 0.0f };
}

void td_rasterizer::command_add_fill(const td_path& path)
{
    if (path.points.empty())
    {
        return;
    }

    size_t i = points.size();

    points.resize(points.size() + path.points.size());
    tags.resize(tags.size() + path.points.size());

    td_path_iterator it(path);
    while (it.get_next())
    {
        switch (it.cmd)
        {
        case td_path_cmd_MOVE_TO:
            points[i].x = TO_FT_POS(it.points[0].x);
            points[i].y = TO_FT_POS(it.points[0].y);
            tags[i] = FT_CURVE_TAG_ON;

            if (i)
            {
                contours.push_back(i - 1);
            }
            i += 1;

            clip_rect.add(it.points[0]);

            break;
        case td_path_cmd_LINE_TO:
            points[i].x = TO_FT_POS(it.points[0].x);
            points[i].y = TO_FT_POS(it.points[0].y);
            tags[i] = FT_CURVE_TAG_ON;
            i += 1;
            clip_rect.add(it.points[0]);
            break;
        case td_path_cmd_CUBIC_TO:
            points[i].x = TO_FT_POS(it.points[0].x);
            points[i].y = TO_FT_POS(it.points[0].y);
            tags[i] = FT_CURVE_TAG_CUBIC;
            i += 1;
            clip_rect.add(it.points[0]);
            points[i].x = TO_FT_POS(it.points[1].x);
            points[i].y = TO_FT_POS(it.points[1].y);
            tags[i] = FT_CURVE_TAG_CUBIC;
            i += 1;
            clip_rect.add(it.points[1]);
            points[i].x = TO_FT_POS(it.points[2].x);
            points[i].y = TO_FT_POS(it.points[2].y);
            tags[i] = FT_CURVE_TAG_ON;
            i += 1;
            clip_rect.add(it.points[2]);
            break;
        case td_path_cmd_CLOSE:
            points[i].x = TO_FT_POS(it.points[0].x);
            points[i].y = TO_FT_POS(it.points[0].y);
            tags[i] = FT_CURVE_TAG_ON;
            i += 1;
            clip_rect.add(it.points[0]);
            break;
        default:
            TD_ASSERT(0 && "Unreachable");
        }
    }

    // Add last contour 
    if (i)
    {
        contours.push_back(i - 1);
    }
}

void td_rasterizer::command_add_stroke(const td_path& path, const td_stroke_option& opt)
{
    if (path.points.empty())
    {
        return;
    }
    
    td_path flatten_path;
    td::to_flatten_path(path, &flatten_path);

    td_contour_builder builder(this, flatten_path, opt);

    td::traverse_flatten_path(path, td_contour_builder::traverse_func, (void*)&builder);

    // Add last contour
    builder.add_contour();
}

void td_rasterizer::command_end(td_rgba8* data, int width, int height, td_rgba8 color)
{
    FT_Outline ft_outline;

    ft_outline.n_contours = (unsigned short)contours.size();
    ft_outline.n_points = (unsigned short)points.size();

    TD_ASSERT(sizeof(points[0]) == sizeof(FT_Vector) && "ft_vector must have the same layout as FT_Vector");

    ft_outline.points = (FT_Vector*)points.data();
    ft_outline.tags = tags.data();
    // Indices of contour endpoints
    ft_outline.contours = contours.data();

    ft_outline.flags = 0;

    FT_Raster_Params ft_raster_params;
    ft_raster_params.target = 0;
    ft_raster_params.source = &ft_outline;
    ft_raster_params.flags = FT_RASTER_FLAG_DIRECT | FT_RASTER_FLAG_AA | FT_RASTER_FLAG_CLIP;
    ft_raster_params.gray_spans = td_rasterizer_spanner;

    td_rasterizer_context ctx;
    ctx.bitmap = data;
    ctx.width = width;
    ctx.height = height;
    ctx.color = color;
    ctx.operation = operation;

    ft_raster_params.user = &ctx;

    // Expand for anti_aliasing
    clip_rect.expand(2.0f);
    clip_rect.clip_with(td_rect{ 0.0f, 0.0f, (float)width, (float)height });

    ft_raster_params.clip_box.xMin = (FT_Pos)clip_rect.min.x;
    ft_raster_params.clip_box.yMin = (FT_Pos)clip_rect.min.y;
    ft_raster_params.clip_box.xMax = (FT_Pos)clip_rect.max.x;
    ft_raster_params.clip_box.yMax = (FT_Pos)clip_rect.max.y;

    FT_Raster ft_raster;
    int res = gray_raster_new(0, &ft_raster);
    TD_ASSERT(res == Smooth_Err_Ok);

    res = gray_raster_render(ft_raster, &ft_raster_params);
    TD_ASSERT(res == Smooth_Err_Ok);
}


void td_rasterizer::render_fill_path(const td_path& path, td_rgba8* data, int width, int height, td_rgba8 color)
{
    if (path.points.empty())
    {
        return;
    }
    
    command_begin();
    command_add_fill(path);
    command_end(data, width, height, color);
}

// [tendril] implementation of add_half_stroke is from  https://github.com/a-e-k/canvas_ity
// 
// Trace along a series of points from a subpath in the scratch polylines
// and add new points to the main polylines with the stroke expansion on
// one side.  Calling this again with the ends reversed adds the other
// half of the stroke.  If the original subpath was closed, each pass
// adds a complete closed loop, with one adding the inside and one adding
// the outside.  The two will wind in opposite directions.  If the original
// subpath was open, each pass ends with one of the line caps and the two
// passes together form a single closed loop.  In either case, this handles
// adding line joins, including inner joins.  Care is taken to fill tight
// inside turns correctly by adding additional windings.  See Figure 10 of
// "Converting Stroked Primitives to Filled Primitives" by Diego Nehab, for
// the inspiration for these extra windings.
//
void td_rasterizer::add_half_stroke(td_add_point_func_t add_point, void* ctx, const td_point_array& points, size_t beginning, size_t ending, bool closed, const td_stroke_option& opt)
{
    static float const epsilon = 1.0e-4f;

    // Add 0.5f to round up the line_width.
    float half = (opt.line_width + 0.5f) * 0.5f ;
    float miter_ratio = 0.0f;
    if (opt.join_style == td_join_style_MITER && opt.miter_limit > 0.0f)
    {
        miter_ratio = opt.miter_limit * opt.miter_limit * half * half;
    }

    td_vec2 in_direction = td_vec2(0.0f, 0.0f);
    float in_length = 0.0f;

    td_vec2 point = points[beginning];
    size_t finish = beginning;
    size_t index = beginning;
    do
    {
        td_vec2 next = points[index];
        td_vec2 dir = next - point;
        float out_length = td::length(dir);
        float inv_out_length = 1.0f / td_max(epsilon, out_length);

        // Normalized dir.
        td_vec2 out_direction{ dir.x * inv_out_length, dir.y * inv_out_length };

        if (in_length != 0.0f && out_length >= epsilon)
        {
            if (closed && finish == beginning)
            {
                finish = index;
            }
            td_vec2 side_in = point + td::perpendicular(in_direction) * half;
            td_vec2 side_out = point + td::perpendicular(out_direction) * half;
            float turn = td::dot(td::perpendicular(in_direction), out_direction);
            if (fabsf(turn) < epsilon)
            {
                turn = 0.0f;
            }
            td_vec2 offset = turn == 0.0f ? td_vec2(0.0f, 0.0f) :
                (out_direction - in_direction) * half / turn;
            bool tight = (td::dot(offset, in_direction) < -in_length &&
                           td::dot(offset, out_direction) > out_length);
            if (turn > 0.0f
                && tight)
            {
                td_memswap(side_in, side_out);
                td_memswap(in_direction, out_direction);
                add_point(ctx, side_out);
                add_point(ctx, point);
                add_point(ctx, side_in);
            }
            if ((turn > 0.0f && !tight) ||
                 (turn != 0.0f && opt.join_style == td_join_style_MITER &&
                     td::dot(offset, offset) <= miter_ratio))
            {
                add_point(ctx, point + offset);
            }
            else if (opt.join_style == td_join_style_ROUNDED)
            {
                float cosine = td::dot(in_direction, out_direction);
                float angle = td_acos(
                    td_min(td_max(cosine, -1.0f), 1.0f));
                float alpha = 4.0f / 3.0f * td_tan(0.25f * angle);
                add_point(ctx, side_in);
                td_bezier::traverse_flatten(
                    add_point, ctx,
                    side_in,
                    (side_in + in_direction * half * alpha),
                    (side_out - out_direction * half * alpha),
                    side_out
                );
            }
            else
            {
                add_point(ctx, side_in);
                add_point(ctx, side_out);
            }

            if (turn > 0.0f && tight)
            {
                add_point(ctx, side_out);
                add_point(ctx, point);
                add_point(ctx, side_in);
                td_memswap(in_direction, out_direction);
            }
        }
        if (out_length >= epsilon)
        {
            in_direction = out_direction;
            in_length = out_length;
            point = next;
        }
        index = (index == ending ? beginning :
                  ending > beginning ? index + 1 :
                  index - 1);
    } while (index != finish);

    if (closed || in_length == 0.0f)
    {
        return;
    }

    td_vec2 ahead = in_direction * half;
    td_vec2 side = td::perpendicular(ahead);
    if (opt.cap_style == td_cap_style_BUTT)
    {
        add_point(ctx, point + side);
        add_point(ctx, point - side);
    }
    else if (opt.cap_style == td_cap_style_SQUARE)
    {
        add_point(ctx, point + ahead + side);
        add_point(ctx, point + ahead - side);
    }
    else if (opt.cap_style == td_cap_style_CIRCLE)
    {
        static float const alpha = 0.55228475f; // 4/3*tan(pi/8)

        add_point(ctx, point + side);

        td_bezier::traverse_flatten(
            add_point, ctx,
            point + side,
            point + side + ahead * alpha,
            point + ahead + side * alpha,
            point + ahead
        );
        td_bezier::traverse_flatten(
            add_point, ctx,
            point + ahead,
            point + ahead - side * alpha,
            point - side + ahead * alpha,
            point - side
        );
    }
}

void td_rasterizer::render_stroke_path(const td_path& path, td_rgba8* data, int width, int height, td_rgba8 color, const td_stroke_option& opt)
{
    if (path.points.empty())
    {
        return;
    }
    command_begin();
    command_add_stroke(path, opt);
    command_end(data, width, height, color);
}

void td_rasterizer::render_horizontal_line(int x1, int x2, int y, td_rgba8* data, int width, int height, td_rgba8 color)
{
    uint32_t * dest = (uint32_t*)(data + y * width);

    if (x1 > x2)
    {
        td_memswap(x1, x2);
    }

    int count = x2 - x1;
    unsigned char alpha = 255;
    operation(dest + x1, count, color, alpha);
}

void td_rasterizer::render_vertical_line(int x, int y1, int y2, td_rgba8* data, int width, int height, td_rgba8 color)
{
    uint32_t* dest = (uint32_t*)(data);

    if (y1 > y2)
    {
        td_memswap(y1, y2);
    }

    unsigned char alpha = 255;

    for (size_t y = y1; y < y2; y += 1)
    {
        uint32_t* line = dest + y * width;
        operation(line + x, 1, color, alpha);
    }
}

void td_rasterizer::composite_operation_clear(uint32_t* dest, uint32_t count, td_rgba8 color, unsigned char alpha)
{
    unsigned char lerp_alpha = td_byte_lerp(0, color.a, alpha);
    uint32_t inv_alpha = 255 - lerp_alpha;
    for (uint32_t i = 0; i < count; i += 1)
    {
        dest[i] = TD_U32_SET_ALPHA(dest[i], inv_alpha);
    }
}

void td_rasterizer::composite_operation_source_over(uint32_t* dest, uint32_t count, td_rgba8 color, unsigned char alpha)
{
    uint32_t src_u32 = td_rgba8::premultiplied_with_alpha(color).to_u32();

    if (alpha != 255)
        src_u32 = td_u32_byte_mul(src_u32, alpha);

    for (size_t i = 0; i < count; i++)
    {
        uint32_t a = src_u32;
        uint32_t b = td_u32_byte_mul(dest[i], TD_U32_GET_INV_ALPHA(a));

        dest[i] = a + b;
    }
}

// For each component
// c = x.c * a  / 255
uint32_t td_u32_byte_mul(uint32_t x, uint32_t alpha)
{
    alpha += 1;
    uint32_t rb = x & 0x00ff00ff;
    uint32_t ga = x & 0xff00ff00;
    rb = (rb)*alpha >> 8;
    ga = ((ga >> 8) * alpha);
    return (rb & 0x00ff00ff) | (ga & 0xff00ff00);
}

// For each component
// c = x.c + (y.c - x.c) * a
uint32_t td_u32_byte_lerp(uint32_t x, uint32_t y, uint32_t alpha)
{
    alpha += 1;

    uint32_t rb = (((y & 0x00ff00ff) - (x & 0x00ff00ff)) * alpha) >> 8;
    uint32_t ga = (((y & 0xff00ff00) >> 8) - ((x & 0xff00ff00) >> 8)) * alpha;
    rb += (x & 0x00ff00ff);
    ga += (x & 0xff00ff00);
    return (rb & 0x00ff00ff) | (ga & 0xff00ff00);
}

// c = x + (y - x) * a
uint8_t td_byte_lerp(uint8_t x, uint8_t y, uint8_t f)
{
    return x + (((y - x) * f) >> 8);
}

// For each component
// c = (x.c * a + y.c * b) / 255
uint32_t td_u32_byte_interpolate(uint32_t x, uint32_t a, uint32_t y, uint32_t b)
{
    uint32_t rb = (((x & 0x00ff00ff) * a) + ((y & 0x00ff00ff) * b)) >> 8;

    uint32_t ga = ((((x & 0xff00ff00) >> 8) * a) + ((y & 0xff00ff00) >> 8) * b);

    return (rb & 0x00ff00ff) | (ga & 0xff00ff00);
}

// For each component c
// c = mod(x.c - y.c, 255)
uint32_t td_byte_sub_mod(uint32_t x, uint32_t y)
{
    uint32_t rb = 0xff00ff00 + (x & 0x00ff00ff) - (y & 0x00ff00ff);
    uint32_t ga = 0x00ff00ff + (x & 0xff00ff00) - (y & 0xff00ff00);

    return (rb & 0x00ff00ff) | (ga & 0xff00ff00);
}

// For each component c
// c = mod(x.c = y.c, 255)
uint32_t td_byte_add_mod(uint32_t x, uint32_t y)
{
    uint32_t rb = 0xff00ff00 + (x & 0x00ff00ff) + (y & 0x00ff00ff);
    uint32_t ga = 0x00ff00ff + (x & 0xff00ff00) + (y & 0xff00ff00);

    return (rb & 0x00ff00ff) | (ga & 0xff00ff00);
}

// For each component c
// c = min(x.c + y.c, 255)
uint32_t td_byte_add(uint32_t x, uint32_t y)
{
    uint32_t rb = (x & 0x00ff00ff) + (y & 0x00ff00ff);
    // If the operation overflows we need to combine the overlowed bits in 0xff00ff00 to the 0x00ff00ff
    uint32_t rb_to_combine = (0x00ff00ff + 0x00010001) - ((rb & 0xff00ff00) >> 8);
    rb |= rb_to_combine;

    // Same as for rb but we need to shift x and y first.
    uint32_t ga = ((x & 0xff00ff00) >> 8) + ((y & 0xff00ff00) >> 8);
    uint32_t ga_to_combine = (0x00ff00ff + 0x00010001) - ((ga & 0xff00ff00) >> 8);
    ga |= ga_to_combine;

    ga = (ga << 8); // Shift back.

    return (rb & 0x00ff00ff) | (ga & 0xff00ff00);
}

void td_uint32_memset(uint32_t* dest, uint32_t value, size_t value_count)
{

    if (value_count >= 8)
    {
        // Pack two 32 bit value side by side
        uint64_t value64 = (((uint64_t)value) << ((uint64_t)32)) | ((uint64_t)value);

        do
        {
            ((uint64_t*)dest)[0] = value64;
            ((uint64_t*)dest)[1] = value64;
            ((uint64_t*)dest)[2] = value64;
            ((uint64_t*)dest)[3] = value64;

            dest += 8;
            value_count -= 8;
        } while (value_count >= 8);
    }

    if (value_count >= 4)
    {
        do
        {
            dest[0] = value;
            dest[1] = value;
            dest[2] = value;
            dest[3] = value;

            dest += 4;
            value_count -= 4;
        } while (value_count >= 4);
    }

    while (value_count--)
    {
        *dest++ = value;
    }
}