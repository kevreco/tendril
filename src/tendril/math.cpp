#include "tendril.hpp"

#include <math.h> // sqrt

//  Table of content:
// 
//    misc
//    td_vec2
//    td_rect
//    td_recti
//    td_bezier
//    td_transform
//    td_rgba8
//    td_rgba32
//    td_line_line_intersection
//    td_line_offset_by

// ============================================================================
// misc
// ============================================================================

float td::lerp(float a, float b, float t)
{
    return (a * (1.0f - t)) + (b * t);
}

float td::inv_lerp(float a, float b, float t)
{
    return (t - a) / (b - a);
}

// ============================================================================
// td_vec2
// ============================================================================

float td::squared_length(const td_vec2& v)
{
    return (v.x * v.x) + (v.y * v.y);
}

float td::length(const td_vec2& v)
{
    return td_sqrt(squared_length(v));
}

void td::normalize(td_vec2* v)
{
    float len = length(*v);

    if (len == 1 || len == 0) {
        return;
    }

    float factor = 1.0f / len;
    v->x *= factor;
    v->y *= factor;
}

td_vec2 td::normalized(const td_vec2& v)
{
    td_vec2 n = v;
    normalize(&n);
    return n;
}

td_vec2 td::lerp(const td_vec2& a, const td_vec2& b, float t)
{
    return (a * (1.0f - t)) + (b * t);
}

td_vec2 td::inv_lerp(const td_vec2& a, const td_vec2& b, float t)
{
    return (td_vec2(t, t) - a) / (b - a);
}

float td::dot(const td_vec2& a, const td_vec2& b)
{
    return a.x * b.x + a.y * b.y;
}

float td::cross(const td_vec2& a, const td_vec2& b)
{
    return (a.x * b.y) - (a.y * b.x);
}

td_vec2 td::sign(const td_vec2& v)
{
    return td_vec2(td_signf(v.x), td_signf(v.y));
}

td_vec2 td::perpendicular(const td_vec2& that)
{
    return td_vec2(-that.y, that.x);
}

// ============================================================================
// td_rect
// ============================================================================

td_rect::td_rect() : min(), max()
{
}

td_rect::td_rect(float min_x, float min_y, float max_x, float max_y) : min(min_x, min_y), max(max_x, max_y)
{
}

td_rect::td_rect(const td_vec2& _min, const td_vec2& _max) : min(_min), max(_max)
{
}

td_rect::td_rect(const td_rect& rect) : min(rect.min), max(rect.max)
{
}

bool td_rect::contains(const td_vec2& p) const
{
    return contains_inclusive(p);
}

bool td_rect::contains_exclusive(const td_vec2& p) const
{
    return (p.x > min.x && p.x < max.x
        && p.y > min.y && p.y < max.y);
}

bool td_rect::contains_inclusive(const td_vec2& p) const
{
    return (p.x >= min.x && p.x <= max.x
        && p.y >= min.y && p.y <= max.y);
}

bool td_rect::contains(const td_vec2& _min, const td_vec2& _max) const
{
    return _min.x >= min.x && _min.y >= min.y && _max.x <= max.x && _max.y <= max.y;
}

bool td_rect::contains(const td_rect& r) const
{
    return contains(r.min, r.max);
}

bool td_rect::contains(const td_vec2& p1, const td_vec2& p2, const td_vec2& p3, const td_vec2& p4) const
{
    return p1.x >= min.x && p1.x <= max.x
        && p1.y >= min.y && p1.y <= max.y
        && p2.x >= min.x && p2.x <= max.x
        && p2.y >= min.y && p2.y <= max.y
        && p3.x >= min.x && p3.x <= max.x
        && p3.y >= min.y && p3.y <= max.y
        && p4.x >= min.x && p4.x <= max.x
        && p4.y >= min.y && p4.y <= max.y;
}

void td_rect::expand(float amount)
{
    min.x -= amount;
    min.y -= amount;
    max.x += amount;
    max.y += amount;
}

void td_rect::reduce(float amount)
{
    min.x += amount;
    min.y += amount;
    max.x -= amount;
    max.y -= amount;
}

void td_rect::add(const td_vec2& p)
{
    if (min.x > p.x)
        min.x = p.x;
    if (min.y > p.y)
        min.y = p.y;
    if (max.x < p.x)
        max.x = p.x;
    if (max.y < p.y)
        max.y = p.y;
}

void td_rect::clip_with(const td_rect& r)
{
    min.x = td_clamp(min.x, r.min.x, r.max.x);
    min.y = td_clamp(min.y, r.min.y, r.max.y);
    max.x = td_clamp(max.x, r.min.x, r.max.x);
    max.y = td_clamp(max.y, r.min.y, r.max.y);
}

// ============================================================================
// td_recti
// ============================================================================

td_recti::td_recti() : min(), max()
{
}

td_recti::td_recti(td_vec2i _min, td_vec2i _max) : min(_min), max(_max)
{
}

// ============================================================================
// td_bezier
// ============================================================================

td_vec2* td_bezier::ptr()
{
    return (td_vec2*)this;
}

td_vec2& td_bezier::operator[](size_t idx)
{
    TD_ASSERT(idx >= 0 || idx <= 3);
    return ((td_vec2*)(void*)(char*)this)[idx];
}
td_vec2 td_bezier::operator[](size_t idx) const
{
    TD_ASSERT(idx >= 0 || idx <= 3);
    return ((const td_vec2*)(const void*)(const char*)this)[idx];
}

td_bezier::td_bezier() : p{}
{
}

td_bezier::td_bezier(td_vec2 p1, td_vec2 p2, td_vec2 p3, td_vec2 p4)
{
    p[0] = p1;
    p[1] = p2;
    p[2] = p3;
    p[3] = p4;
}

td_bezier::td_bezier(td_vec2* p, int count)
{
    memcpy(p, p, count * sizeof(td_vec2));
}

td_vec2 td_bezier::eval_at(float t) const
{
    td_vec2 v;
    v.x = (1 - t) * (1 - t) * (1 - t) * p[0].x
        + 3 * (1 - t) * (1 - t) * t * p[1].x
        + 3 * (1 - t) * t * t * p[2].x
        + t * t * t * p[3].x;
    v.y = (1 - t) * (1 - t) * (1 - t) * p[0].y
        + 3 * (1 - t) * (1 - t) * t * p[1].y
        + 3 * (1 - t) * t * t * p[2].y
        + t * t * t * p[3].y;
    return v;
}

td_vec2 td_bezier::tangent(float t) const
{
    return (p[1] - p[0]) * 3.0f * (1 - t) * (1 - t)
        + (p[2] - p[1]) * 6.0f * t * (1 - t)
        + (p[3] - p[2]) * 3.0f * t * t;
}

td_vec2 td_bezier::unit_tangent(float t) const
{
    td_vec2 v = tangent(t);
    td::normalize(&v);
    return v;
}

// Use Tiller and Hanson algorithm to (approximately) offset the bezier curve.
td_bezier td_bezier::offset_by(float offset_val) const
{
    td_vec2 p0 = p[0];
    td_vec2 p1 = p[1];
    td_vec2 p2 = p[2];
    td_vec2 p3 = p[3];

    td_vec2 line1[2];
    td_vec2 line2[2];
    td_vec2 line3[2];

    td_line_offset_by(p[0], p[1], line1, offset_val);
    td_line_offset_by(p[1], p[2], line2, offset_val);
    td_line_offset_by(p[2], p[3], line3, offset_val);

    td_vec2 intersect1;
    td_line_line_intersection(line1[0], line1[1], line2[0], line2[1], &intersect1);
    td_vec2 intersect2;
    td_line_line_intersection(line2[0], line2[1], line3[0], line3[1], &intersect2);

    return td_bezier{ line1[0], intersect1, intersect2,  line3[1]};
}

td_vec2 td_bezier::offset_at(float t, float offset) const
{
    td_vec2 p = eval_at(t);
    td_vec2 n = unit_normal(t);

    return td_vec2{ p + n * offset };
}

td_vec2 td_bezier::raw_normal(float t) const
{
    td_vec2 v = tangent(t);
    return td_vec2(-v.y, v.x);
}

td_vec2 td_bezier::unit_normal(float t) const
{
    td_vec2 v = raw_normal(t);
    td::normalize(&v);
    return v;
}

bool td_bezier::within_tolerance(float tolerance) const
{
    return within_tolerance(p[0], p[1], p[2], p[3], tolerance);
}

void td_bezier::split(td_bezier* left, td_bezier* right)
{
    // Split bezier
    float x12 = (x1 + x2) * 0.5f;     float y12 = (y1 + y2) * 0.5f;
    float x23 = (x2 + x3) * 0.5f;     float y23 = (y2 + y3) * 0.5f;
    float x34 = (x3 + x4) * 0.5f;     float y34 = (y3 + y4) * 0.5f;
    float x123 = (x12 + x23) * 0.5f;   float y123 = (y12 + y23) * 0.5f;
    float x234 = (x23 + x34) * 0.5f;   float y234 = (y23 + y34) * 0.5f;
    float x1234 = (x123 + x234) * 0.5f; float y1234 = (y123 + y234) * 0.5f;

    left->p[0].x = x1;
    left->p[0].y = y1;
    left->p[1].x = x12;
    left->p[1].y = y12;
    left->p[2].x = x123;
    left->p[2].y = y123;
    left->p[3].x = x1234;
    left->p[3].y = y1234;

    right->p[0].x = x1234;
    right->p[0].y = y1234;
    right->p[1].x = x234;
    right->p[1].y = y234;
    right->p[2].x = x34;
    right->p[2].y = y34;
    right->p[3].x = x4;
    right->p[3].y = y4;
}

bool td_bezier::equals(const td_bezier& left, const td_bezier& right)
{
    return left.p[0] == right.p[0]
        && left.p[1] == right.p[1]
        && left.p[2] == right.p[2]
        && left.p[3] == right.p[3];
}

bool td_bezier::within_tolerance(float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4, float tolerance)
{
    float dx = x4 - x1;
    float dy = y4 - y1;
    float d2 = td_abs((x2 - x4) * dy - (y2 - y4) * dx);
    float d3 = td_abs((x3 - x4) * dy - (y3 - y4) * dx);

    return (d2 + d3) * (d2 + d3) < tolerance * (dx * dx + dy * dy);
}

bool td_bezier::within_tolerance(const td_vec2& p1, const td_vec2& p2, const td_vec2& p3, const td_vec2& p4, float tolerance)
{
    return within_tolerance(p1.x, p1.y, p2.x, p2.y, p3.x, p3.y, p4.x, p4.y, tolerance);
}

// This is using the De Casteljau's algorithm
// Code was taken from https://github.com/ocornut/imgui
void td_bezier::traverse_flatten(td_add_point_func_t add_point, void* ctx,
    float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4,
    int level, float tessellation_tolerance)
{
    if (within_tolerance(x1, y1, x2, y2, x3, y3, x4, y4, tessellation_tolerance))
    {
        add_point(ctx, { x4, y4 });
    }
    else if (level < 10)
    {
        // Split bezier
        float x12 = (x1 + x2) * 0.5f;     float y12 = (y1 + y2) * 0.5f;
        float x23 = (x2 + x3) * 0.5f;     float y23 = (y2 + y3) * 0.5f;
        float x34 = (x3 + x4) * 0.5f;     float y34 = (y3 + y4) * 0.5f;
        float x123 = (x12 + x23) * 0.5f;   float y123 = (y12 + y23) * 0.5f;
        float x234 = (x23 + x34) * 0.5f;   float y234 = (y23 + y34) * 0.5f;
        float x1234 = (x123 + x234) * 0.5f; float y1234 = (y123 + y234) * 0.5f;

        traverse_flatten(add_point, ctx, x1, y1, x12, y12, x123, y123, x1234, y1234, level + 1, tessellation_tolerance);
        traverse_flatten(add_point, ctx, x1234, y1234, x234, y234, x34, y34, x4, y4, level + 1, tessellation_tolerance);
    }
}

void td_bezier::traverse_flatten(td_add_point_func_t add_point, void* ctx,
    const td_vec2& p1, const td_vec2& p2, const td_vec2& p3, const td_vec2& p4,
    int level, float tessellation_tolerance)
{
    traverse_flatten(add_point, ctx, p1.x, p1.y, p2.x, p2.y, p3.x, p3.y, p4.x, p4.y, level, tessellation_tolerance);
}

// ============================================================================
// td_transform
// ============================================================================

td_transform td_transform::zero()
{
    return td_transform{ { 0, 0, 0, 0, 0, 0 } };
}

td_transform td_transform::identity()
{
    return td_transform{ {
        1 /* m11 */, 0 /* m12 */,
        0 /* m21 */, 1 /* m22 */,
        0 /* dx  */, 0 /* dy  */
    } };
}

void td_transform::reset()
{
    *this = identity();
}

td_vec2 td_transform::map(td_vec2 p)
{
    return td_vec2
    {
        m11 * p.x + m21 * p.y + dx,
        m12 * p.x + m22 * p.y + dy
    };
}

void td_transform::map_many(td_vec2* dst, const td_vec2* src, int count)
{
    for (int i = 0; i < count; i += 1)
    {
        dst[i] = map(src[i]);
    }
}

void td_transform::translate(float x, float y)
{
    dx += x * m11 + y * m21;
    dy += y * m22 + x * m12;
}

void td_transform::translate(td_vec2 p)
{
    translate(p.x, p.y);
}

void td_transform::set_translation(float x, float y)
{
    dx = x;
    dy = y;
}
void td_transform::set_translation(td_vec2 p)
{
    set_translation(p.x, p.y);
}

void td_transform::scale(float x, float y)
{
    m11 *= x;
    m12 *= x;
    m21 *= y;
    m22 *= y;
}

void td_transform::scale(td_vec2 p)
{
    scale(p.x, p.y);
}

void td_transform::set_scale(float x, float y)
{
    m11 = x;
    m12 = 0;
    m21 = 0;
    m22 = y;
}

void td_transform::multiply(const td_transform& m)
{
    float tmp_m11 = m11 * m.m11 + m12 * m.m21;
    float tmp_m12 = m11 * m.m12 + m12 * m.m22;
    float tmp_m21 = m21 * m.m11 + m22 * m.m21;
    float tmp_m22 = m21 * m.m12 + m22 * m.m22;
    float tmp_dx = dx * m.m11 + dy * m.m21 + m.dx;
    float tmp_dy = dx * m.m12 + dy * m.m22 + m.dy;
    m11 = tmp_m11;
    m12 = tmp_m12;
    m21 = tmp_m21;
    m22 = tmp_m22;
    dx = tmp_dx;
    dy = tmp_dy;
}


float td_transform::determinant() const
{
    return m11 * m12 - m22 * m21;
}

td_transform td_transform::inversed() const
{
    float det = determinant();

    if (det == 0)
    {
        return identity();
    }

    td_transform t;
    t.m11 = m12 / det;
    t.m22 = -m22 / det;
    t.m21 = -m21 / det;
    t.m12 = m11 / det;
    t.dx = m21 * dy - m12 * dx;
    t.dy = m22 * dx - m11 * dy;
    return t;
}

// ============================================================================
// td_rgba8
// ============================================================================

int td_rgba8_to_hex(char* buffer, size_t buffer_len, td_rgba8 color)
{
    return td_rgba32_to_hex(buffer, buffer_len, color.to_u32());
}

// ============================================================================
// td_rgba32
// ============================================================================

int td_rgba32_to_hex(char* buffer, size_t buffer_len, td_rgba32 color)
{
    return td_str_fmt(buffer, buffer_len, "#%02X%02X%02X%02X", color.r(), color.g(), color.b(), color.a());
}

bool td_line_line_intersection(const td_vec2& line1a, const td_vec2& line1b, const td_vec2& line2a, const td_vec2& line2b, td_vec2* intersection)
{
    const td_vec2 line1_dir = line1b - line1a;
    const td_vec2 line2_dir = line2b - line2a;
    // const td_vec2 a = line1a; // Not used
    const td_vec2 b = line1b;
    const td_vec2 c = line2a;
    const td_vec2 d = line2b;

    const float ABxCD = td::cross(line1_dir, line2_dir);

    // Collinear or parallel

    if (ABxCD == 0.0f)
    {
        return false;
    }

    float left_term = b.x * line1a.y - b.y * line1a.x;
    float right_term = d.x * line2a.y - d.y * line2a.x;

    td_vec2 numer = line2_dir * left_term - line1_dir * right_term;
    *intersection = numer / ABxCD;

    return true;
}

void td_line_offset_by(const td_vec2& a, const td_vec2& b, td_vec2* line_out, float offset)
{
    td_vec2 n = td::normalized(td::perpendicular(b - a)) * offset;
    line_out[0] = a + n;
    line_out[1] = b + n;
}