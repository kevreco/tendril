#include "tendril.hpp"
#include "tendril_internal.hpp"

//
// Table of content:
// 
//    td_path
//    td_piecewise_path
//
//    td::transform_along_piecewise
//    td::smooth_transform_along_piecewise
//    td::elements_sum
//    td::elements_multiply
// 

// ============================================================================
// td_path
// ============================================================================

void td_path::clear()
{
    points.clear();
    cmds.clear();
}

void td_path::reserve(size_t count)
{
    points.reserve(count);
    cmds.reserve(count);
}

void td_path::clone_to(td_path* p) const
{
    p->clear();
    p->add_path(*this);
}

void td_path::move_to(float x, float y)
{
    td_vec2* p = add_command(td_path_cmd_MOVE_TO);
    p[0] = td_vec2(x, y);
    point_anchor_index = points.size() - 1;
}

void td_path::move_to(const td_vec2& p)
{
    move_to(p.x, p.y);
}

void td_path::line_to(float x, float y)
{
    if (points.size() == 0)
    {
        move_to(0.0f, 0.0f);
    }

    td_vec2* p = add_command(td_path_cmd_LINE_TO);
    p[0] = td_vec2(x, y);
}

void td_path::line_to(const td_vec2& p)
{
    line_to(p.x, p.y);
}

void td_path::quad_to(float x1, float y1, float x2, float y2)
{
    if (points.size() == 0)
    {
        move_to(0.0f, 0.0f);
    }

    // Quadratic to Cubic bezier
    // P0 = P0
    // P1 = P0 + (2/3) * (P1 - P0)
    // P2 = P2 + (2/3) * (P1 - P2)
    // P3 = P2

    td_vec2 p0 = points[points.size() - 1];
    td_vec2 p1;
    p1.x = p0.x + (2.0f / 3.0f * (x1 - p0.x));
    p1.y = p0.y + (2.0f / 3.0f * (y1 - p0.y));

    td_vec2 p2;
    p2.x = x2 + (2.0f / 3.0f * (x1 - x2));
    p2.y = y2 + (2.0f / 3.0f * (y1 - y2));

    cubic_to(p1.x, p1.y, p2.x, p2.y, x2, y2);
}

void td_path::quad_to(const td_vec2& p1, const td_vec2& p2)
{
    quad_to(p1.x, p1.y, p2.x, p2.y);
}

void td_path::cubic_to(float x1, float y1, float x2, float y2, float x3, float y3)
{
    if (points.size() == 0)
    {
        move_to(0.0f, 0.0f);
    }

    td_vec2* points = add_command(td_path_cmd_CUBIC_TO);
    points[0] = td_vec2(x1, y1);
    points[1] = td_vec2(x2, y2);
    points[2] = td_vec2(x3, y3);
}

void td_path::cubic_to(const td_vec2& p1, const td_vec2& p2, const td_vec2& p3)
{
    cubic_to(p1.x, p1.y, p2.x, p2.y, p3.x, p3.y);
}

void td_path::add_rect(float min_x, float min_y, float max_x, float max_y)
{
    move_to(min_x, min_y);
    line_to(max_x, min_y);
    line_to(max_x, max_y);
    line_to(min_x, max_y);
    close();
}

void td_path::close()
{
    if (points.size() == 0)
    {
        return;
    }

    td_vec2* p = add_command(td_path_cmd_CLOSE);
    p[0] = points[point_anchor_index];
}

void td_path::add_rect(const td_vec2& min, const td_vec2& max)
{
    add_rect(min.x, min.y, max.x, max.y);
}

// http://www.w3.org/TR/SVG11/implnote.html#ArcImplementationNotes
void td_path::arc_to(float rx, float ry, float x_axis_rotation, bool large_arc_flag, bool sweep_flag, float x, float y)
{
    const float epsilon = 1e-05f;

    // Get current point.
    td_vec2 p0 = points[points.size() - 1];
    float p0x = p0.x;
    float p0y = p0.y;

    // (F.6.6.1) Ensure radii are positive
    if (rx < 0.0f) rx = -rx;
    if (ry < 0.0f) ry = -rx;

    // (F.6.6) Ensure radii are non-zero
    if (rx < epsilon || ry < epsilon)
    {
        line_to(x, y);
        return;
    }

    // (F.6.5.1)
    // 
    
    // Get middle point between the current point and the final point.
    float dx = (p0x - x) * 0.5f;
    float dy = (p0y - y) * 0.5f;

    // Compute (x1', y1')
    float cos_a = td_cos(x_axis_rotation);
    float sin_a = td_sin(x_axis_rotation);
    float x1 = cos_a * dx + sin_a * dy;
    float y1 = -sin_a * dx + cos_a * dy;

    // (F.6.6.2) Ensure radii are large enough.
    float rxrx = rx * rx;
    float ryry = ry * ry;
    float x1x1 = x1 * x1;
    float y1y1 = y1 * y1;

    float radii = x1x1 / rxrx + y1y1 / ryry;
    if (radii > 1.0f)
    {
        float sqrt_raddi = td_sqrt(radii);
        rx *= sqrt_raddi;
        ry *= sqrt_raddi;
        rxrx = rx * rx;
        ryry = ry * ry;
    }

    float sign = (large_arc_flag == sweep_flag) ? -1.0f : 1.0f;

    // (F.6.5.2) Compute (cx', cy')

    float scale_squared = (rxrx * ryry - rxrx * y1y1 - ryry * x1x1) / (rxrx * y1y1 + ryry * x1x1);
    
    if (scale_squared < 0.0f)
        scale_squared = 0.0f;

    float scale = sign * td_sqrt(scale_squared);

    float cx1 = scale * ((rx * y1) / ry);
    float cy1 = scale * -((ry * x1) / rx);

    // (F.6.5.3) Compute (cx, cy) from (cx', cy')
    float tmpx = (p0.x + x) * 0.5f;
    float tmpy = (p0.y + y) * 0.5f;
    float cx = (cos_a * cx1 - sin_a * cy1) + tmpx;
    float cy = (sin_a * cx1 + cos_a * cy1) + tmpy;

    // (F.6.5.4) Compute start_angle and sweep_angle
    float ux = (x1 - cx1) / rx;
    float uy = (y1 - cy1) / ry;
    float vx = (-x1 - cx1) / rx;
    float vy = (-y1 - cy1) / ry;

    // start_angle
    sign = (uy < 0.0f) ? -1.0f : 1.0f;
    float v = ux / td_sqrt(ux * ux + uy * uy);
    if (v < -1.0f) v = -1.0f;
    if (v > 1.0f) v = 1.0f;

    float start_angle = sign * td_acos(v);

    // sweep_angle
    sign = (ux * vy - uy * vx < 0.0f) ? -1.0f : 1.0f;
    v = ux * vx + uy * vy / td_sqrt((ux * ux + uy * uy) * (vx * vx + vy * vy));
    if (v < -1.0f) v = -1.0f;
    if (v > 1.0f) v = 1.0f;
    
    float sweep_angle = sign * td_acos(v);

    if (!sweep_flag && sweep_angle > 0.0f)
    {
        sweep_angle -= TD_PI * 2.0f;
    }
    else if (sweep_flag && sweep_angle < 0.0f)
    {
        sweep_angle += TD_PI * 2.0f;
    }

    // Rotate radiuses to get relative points necessary for the arc function.
    float px = rx * cos_a;
    float py = rx * sin_a;
    float qx = -(ry * sin_a);
    float qy = ry * cos_a;

    // We now have all parameters we need to convert the arc to bezier curves.
    arc(cx, cy, px, py, qx, qy, start_angle, sweep_angle);
}

// Based on:
//   "Drawing ellipses and elliptical arcs with piecewise cubic Bezier curve approximations"
//   By Jerry R. Van Aken
//
// (cx, cy) is the center point of the ellipse, and P and Q are the end points
// of a pair of conjugate diameters of the ellipse.
// P and Q are defined relative to the center C.
// Parameters start_angle and angle_sweep are, respectively, the starting angle for the arc and the angle swept out by the arc.
void td_path::arc(float cx, float cy, float px, float py, float qx, float qy, float start_angle, float angle_sweep)
{
    const float maxphi = TD_PI / 2;
    float tmp_x;
    float tmp_y;
    float p1_x; // bezier start point
    float p1_y;
    float c1_x; // bezier control point 1
    float c1_y;
    float c2_x; // bezier control point 2
    float c2_y;
    float p2_x; // bezier end point
    float p2_y;

    if (angle_sweep == 0)
        return;  // zero-length arc

    // Generate new conjugate diameter end points P' and Q'
    // by rotating points P and Q by starting angle astart
    if (start_angle != 0)
    {
        float cosa = td_cos(start_angle);
        float sina = td_sin(start_angle);
        tmp_x = px * cosa + qx * sina;
        tmp_y = py * cosa + qy * sina;
        qx = qx * cosa - px * sina;
        qy = qy * cosa - py * sina;
        px = tmp_x;
        py = tmp_y;
    }

    // Set the starting point for the elliptical arc
    p2_x = px + cx;
    p2_y = py + cy;

    // If the sweep angle is negative, convert it to a
    // positive angle pointing in the opposite direction
    if (angle_sweep < 0)
    {
        qx = -qx;
        qy = -qy;
        angle_sweep = -angle_sweep;
    }
    if (angle_sweep > 2 * TD_PI)
        angle_sweep = 2 * TD_PI;

    // If the arc's sweep angle is too big to be accurately
    // drawn as a single Bezier curve segment, partition it
    // into smaller angles of uniform size 'phi'
    int nsegs = 1;
    float phi = angle_sweep;
    if (angle_sweep > maxphi)
    {
        nsegs = (int)td_ceil(angle_sweep / maxphi);
        phi = angle_sweep / nsegs;
    }

    // Use tuning parameter 'tau' to calculate initial
    // Bezier control point c2
    float tau = (4.0f / 3) * td_tan(phi / 4);
    c2_x = p2_x - tau * qx;
    c2_y = p2_y - tau * qy;

    // For each elliptical arc of 'phi' radians, plot a
    // Bezier curve segment to approximate the arc
    float cosp = td_cos(phi);
    float sinp = td_sin(phi);
    for (int i = 0; i < nsegs; ++i)
    {
        p1_x = p2_x;
        p1_y = p2_y;
        c1_x = p1_x + (p1_x - c2_x);
        c1_y = p1_y + (p1_y - c2_y);
        tmp_x = px * cosp + qx * sinp;
        tmp_y = py * cosp + qy * sinp;
        qx = qx * cosp - px * sinp;
        qy = qy * cosp - py * sinp;
        px = tmp_x;
        py = tmp_y;
        p2_x = px + cx;
        p2_y = py + cy;
        c2_x = p2_x - tau * qx;
        c2_y = p2_y - tau * qy;
        // cubic bezier [p1, c1, c2, p2]
        cubic_to(c1_x, c1_y, c2_x, c2_y, p2_x, p2_y);
    }
}

void td_path::arc(const td_vec2& c, const td_vec2& p, const td_vec2& q, float start_angle, float angle_sweep)
{
    arc(c.x, c.y, p.x, p.y, q.x, q.y, start_angle, angle_sweep);
}

void td_path::add_path(const td_path& path)
{
    size_t cmds_start = cmds.size();
    cmds.resize(cmds.size() + path.cmds.size());
    td_copy(cmds.data() + cmds_start, path.cmds.begin(), path.cmds.size());

    size_t points_start = points.size();
    points.resize(points.size() + path.points.size());
    td_copy(points.data() + points_start, path.points.begin(), path.points.size());

    this->point_anchor_index = path.point_anchor_index;
}

td_vec2* td_path::add_command(td_path_cmd cmd)
{
    int size = td_path_cmd_point_count(cmd);

    cmds.push_back(cmd);

    points.resize(points.size() + size);
    return points.data() + points.size() - size;
}

// ============================================================================
// td_piecewise_path
// ============================================================================

float td_piecewise_path::length() const
{
    return times.size() ? times.back() : 0.0f;
}

void td_piecewise_path::clear()
{
    points.clear();
    times.clear();
}

void td_piecewise_path::push_back(td_vec2 p, float time)
{
    points.push_back(p);

    times.push_back(time);
}

void td_piecewise_path::reserve(size_t count)
{
    points.reserve(count);
    times.reserve(count);
}

td_vec2 td_piecewise_path::point_at_time(float t) const
{
    size_t index;
    if (try_get_lower_point_index_at_time(t, &index))
    {
        // Get ratio [0.0f - 1.0f] of what represent 't' between T[0] and T[1]
        float time_for_lerp = td::inv_lerp(times[index], times[index + 1], t);
        // Standard lerp between P[0] and P[1] 
        return td::lerp(points[index], points[index + 1], time_for_lerp);
    }
    
    return td_vec2();
}

td_vec2 td_piecewise_path::unit_normal_at_time(float t) const
{
    size_t index;
    if (try_get_lower_point_index_at_time(t, &index))
    {
        return td::normalized(normal_at(index));
    }

    return td_vec2();
}

td_vec2 td_piecewise_path::smoothed_unit_normal_at_time(float t) const
{
    size_t index;
    if (try_get_lower_point_index_at_time(t, &index))
    {
        // Get ratio [0.0f - 1.0f] of what represent 't' between T[0] and T[1]
        float time_for_lerp = td::inv_lerp(times[index], times[index + 1], t);
        // Standard lerp between tangent[0] and tangent[1] 
        td_vec2 interpolated_normal = td::lerp(normal_at(index), normal_at(index + 1), time_for_lerp);

        return td::normalized(interpolated_normal);
    }

    return td_vec2();
}

td_vec2 td_piecewise_path::unit_normal_at(size_t index) const
{
    return td::normalized(normal_at(index));
}

td_vec2 td_piecewise_path::tangeant_at(size_t index) const
{
    TD_ASSERT(points.size() >= 1);

    if (index == points.size() - 1)
    {
        return points[points.size() - 1] - points[points.size() - 1 - 1];
    }
    // Adjust previous and current since point at index == 0 does not have previous point.
    size_t current = index;
    size_t next = index + 1;

    return points[next] - points[current];
}

td_vec2 td_piecewise_path::normal_at(size_t index) const
{
    td_vec2 tangent = tangeant_at(index);
    return td_vec2(-tangent.y, tangent.x);
}

bool td_piecewise_path::try_get_lower_point_index_at_time(float time, size_t* index) const
{
    size_t i = lower_point_index_at_time(time);
    *index = i;
    return i != -1;
}

size_t td_piecewise_path::lower_point_index_at_time(float time) const
{
    if (times.empty())
    {
        return -1;
    }
    // The lower bound is the smallest value that would round up to the estimated value
    // We rounded down the value by starting with the index a and the remove one from the result.
    size_t rounded_up_index = td_upper_bound(times.data(), 0, times.size() - 1, time);
    size_t rounded_down_index = rounded_up_index - 1;
    return rounded_down_index;
}

typedef void (*td_traverse_func_t)(void* ctx, td_path_cmd cmd, const td_vec2* points);

// ============================================================================
// td_path_iterator
// ============================================================================

td_path_iterator::td_path_iterator(const td_path& _path)
{
    view = td_path_view
    {
        { _path.cmds.data(), _path.cmds.size() },
        { _path.points.data(), _path.points.size() }
    };
}

td_path_iterator::td_path_iterator(td_path_view _view)
{
    view = _view;
}

bool td_path_iterator::has_next() const
{
    return cmd_index < view.cmds.size;
}

bool td_path_iterator::get_next()
{
    if (has_next())
    {
        cmd = view.cmds[cmd_index];

        points = view.points.data + point_index;

        point_index += td_path_cmd_point_count(cmd);
        cmd_index += 1;
        return true;
    }
    return false;
}

// ============================================================================
// td_subpath_iterator
// ============================================================================

td_subpath_iterator::td_subpath_iterator(const td_path& _path)
{
    if (_path.points.size())
    {
        source = td_path_view
        {
            { _path.cmds.data(), _path.cmds.size() },
            { _path.points.data(), _path.points.size() }
        };
        subpath.points.data = _path.points.data();
        subpath.cmds.data = _path.cmds.data();
    }
}

bool td_subpath_iterator::has_next() const
{
    return cmd_index < source.cmds.size;
}

bool td_subpath_iterator::get_next()
{
    subpath.points.data += points_count;
    subpath.cmds.data += cmds_count;

    cmds_count = 0;
    points_count = 0;

    if (has_next())
    {
        while (cmd_index < source.cmds.size)
        {
            td_path_cmd cmd = source.cmds[cmd_index];

            if (cmd == td_path_cmd_MOVE_TO && cmds_count != 0)
            {
                break;
            }

            cmds_count += 1;

            points_count += td_path_cmd_point_count(cmd);

            cmd_index += 1;
        }

        subpath.cmds.size = cmds_count;
        subpath.points.size = points_count;

        return true;
    }
    return false;
}

void td::traverse_path(const td_path& path, td_traverse_func_t func, void* ctx)
{
    td_path_iterator it(path);

    while (it.get_next())
    {
        func(ctx, it.cmd, it.points);
    }
}

void td::traverse_flatten_path(const td_path& path, td_traverse_func_t func, void* ctx)
{
    // @OPT: If td_path does not contains curve we can iterate normally.

    const float threshold = 0.12f;

    td_bezier_traverser bezier_traverser(func, ctx);

    td_vec2 current_point = { 0, 0 };

    td_path_iterator it(path);

    while (it.get_next())
    {
        switch (it.cmd) {
        case td_path_cmd_MOVE_TO:
        case td_path_cmd_LINE_TO:
        case td_path_cmd_CLOSE:
            func(ctx, it.cmd, it.points);

            current_point = it.points[0];
            break;
        case td_path_cmd_CUBIC_TO:

            td_bezier::traverse_flatten(td_bezier_traverser::func, &bezier_traverser, current_point, it.points[0], it.points[1], it.points[2], 0, threshold);

            current_point = it.points[2];

            break;
        default:
            TD_ASSERT(0 && "Unreachable");
        }
    }
}

void td::path_to_flatten_path(const td_path& path, td_path* flatten)
{
    flatten->reserve(flatten->points.size() + path.points.size());
    traverse_flatten_path(path, td_path_inserter::func, flatten);
}

void td::path_to_piecewise_path(const td_path& path, td_piecewise_path* piecewise, td_operation_flags flags)
{
    piecewise->reserve(piecewise->points.size() + path.points.size());

    if (path.points.empty())
    {
        return;
    }

    td_path_to_piecewise ptp(piecewise, path.points[0]);

    traverse_flatten_path(path, td_path_to_piecewise::func, &ptp);
}

void td::path_to_fragmented_path(const td_path& path, struct td_path* fragmented, float fragment_length)
{
    fragmented->reserve(path.points.size());

    td_path_fragmenter fragmenter;
    fragmenter.ctx = fragmented;
    fragmenter.fragment_length = fragment_length;
    fragmenter.traverse_func = td_path_inserter::func;
    traverse_flatten_path(path, td_path_fragmenter::func, &fragmenter);
}

void td::path_to_svg_file(const td_path& path, const char* filename, size_t width, size_t height, const char* color, td_svg_options option)
{
    FILE* f = 0;
    const char* mode = "wb";

#if defined(_MSC_VER) && _MSC_VER >= 1400
    if (0 != fopen_s(&f, filename, mode))
        f = 0;
#else
    f = fopen(filename, mode);
#endif

    path_to_svg_file(path, f, width, height, color, option);

    fclose(f);
}

//  Example of valid svg:
// 
//    <svg height="210" width="400" xmlns="http://www.w3.org/2000/svg">
//      <path d="M150 5 L75 200 L225 200 Z"
//      style="fill:none;stroke:black;stroke-width:1" />
//    </svg>
void td::path_to_svg_file(const td_path& path, FILE* file, size_t width, size_t height, const char* color, td_svg_options option)
{
    const char* path_type = option == td_svg_options_STROKE ? "stroke" : "fill";

    fprintf(file, "<svg width=\"%zu\" height=\"%zu\" xmlns=\"http://www.w3.org/2000/svg\">", width, height);
    fprintf(file, "\n  <path d=\"");

    td_path_iterator it(path);
    while (it.get_next())
    {
        switch (it.cmd)
        {
        case td_path_cmd_MOVE_TO:
        {
            fprintf(file, "M %f %f ", it.points[0].x, it.points[0].y);
            break;
        }
        case td_path_cmd_LINE_TO:
        {
            fprintf(file, "L %f %f ", it.points[0].x, it.points[0].y);
            break;
        }
        case td_path_cmd_CUBIC_TO:
        {
            fprintf(file, "Q %f %f %f %f %f %f ", it.points[0].x, it.points[0].y, it.points[1].x, it.points[1].y, it.points[2].x, it.points[2].y);
            break;
        }
        case td_path_cmd_CLOSE:
        {
            fprintf(file, "Z ");
            break;
        }
        default:
            TD_ASSERT(0 && "Unreachable");
        }
    }

    fprintf(file, "\"\n style=\" %s:%s; \" />", path_type, color);
    fprintf(file, "\n</svg>");
}

td_vec2 td::transform_along_piecewise(const td_piecewise_path& pw, const td_vec2& p, bool interpolate_tangent)
{
    float t = p.x;
    float height = p.y;
    td_vec2 point = pw.point_at_time(t);
    td_vec2 norm = interpolate_tangent ? pw.smoothed_unit_normal_at_time(t) :  pw.unit_normal_at_time(t);

    norm.x *= height;
    norm.y *= height;
    return point + norm;
}

void td::transform_along_piecewise(const td_piecewise_path& pw, td_point_array* points, bool interpolate_tangent)
{
    for (size_t i = 0; i < points->size(); i += 1)
    {
        points->at(i) = transform_along_piecewise(pw, points->at(i), interpolate_tangent);
    }
}

void td::elements_sum(td_path* path, td_vec2 p)
{
    elements_sum(path->points.data(), path->points.size(), p);
}

void td::elements_sum(td_piecewise_path* path, td_vec2 p)
{
    elements_sum(path->points.data(), path->points.size(), p);
}

void td::elements_sum(td_vec2* points, size_t count, td_vec2 p)
{
    for (size_t i = 0; i < count; i += 1)
    {
        points[i] += p;
    }
}

void td::elements_multiply(td_path* path, td_vec2 p)
{
    elements_multiply(path->points.data(), path->points.size(), p);
}

void td::elements_multiply(td_piecewise_path* path, td_vec2 p)
{
    elements_multiply(path->points.data(), path->points.size(), p);
}

void td::elements_multiply(td_vec2* points, size_t count, td_vec2 p)
{
    for (size_t i = 0; i < count; i += 1)
    {
        points[i] *= p;
    }
}

