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

void td_path::move_to(td_vec2 p)
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

void td_path::line_to(td_vec2 p)
{
    line_to(p.x, p.y);
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

void td_path::cubic_to(td_vec2 p1, td_vec2 p2, td_vec2 p3)
{
    cubic_to(p1.x, p1.y, p2.x, p2.y, p3.x, p3.y);
}

void td_path::add_rect(float min_x, float min_y, float max_x, float max_y)
{
    move_to(min_x, min_y);
    line_to(max_x, min_y);
    line_to(max_x, max_y);
    line_to(min_x, max_y);
    line_to(min_x, min_y);
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

void td_path::add_rect(td_vec2 min, td_vec2 max)
{
    add_rect(min.x, min.y, max.x, max.y);
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

void td::to_flatten_path(const td_path& path, td_path* flatten)
{
    flatten->reserve(flatten->points.size() + path.points.size());
    traverse_flatten_path(path, td_path_inserter::func, flatten);
}

void td::to_piecewise_path(const td_path& path, td_piecewise_path* piecewise, td_operation_flags flags)
{
    piecewise->reserve(piecewise->points.size() + path.points.size());

    if (path.points.empty())
    {
        return;
    }

    td_path_to_piecewise ptp(piecewise, path.points[0]);

    traverse_flatten_path(path, td_path_to_piecewise::func, &ptp);
}

void td::to_fragmented_path(const td_path& path, struct td_path* fragmented, float fragment_length)
{
    fragmented->reserve(path.points.size());

    td_path_fragmenter fragmenter;
    fragmenter.ctx = fragmented;
    fragmenter.fragment_length = fragment_length;
    fragmenter.traverse_func = td_path_inserter::func;
    traverse_flatten_path(path, td_path_fragmenter::func, &fragmenter);
}

void td::to_svg(const td_path& path, const char* filename, size_t width, size_t height, const char* color, td_svg_options option)
{
    FILE* f = 0;
    const char* mode = "wb";

#if defined(_MSC_VER) && _MSC_VER >= 1400
    if (0 != fopen_s(&f, filename, mode))
        f = 0;
#else
    f = fopen(filename, mode);
#endif

    to_svg(path, f, width, height, color, option);

    fclose(f);
}

//  Example of valid svg:
// 
//    <svg height="210" width="400" xmlns="http://www.w3.org/2000/svg">
//      <path d="M150 5 L75 200 L225 200 Z"
//      style="fill:none;stroke:black;stroke-width:1" />
//    </svg>
void td::to_svg(const td_path& path, FILE* file, size_t width, size_t height, const char* color, td_svg_options option)
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

td_vec2 td::transform_along_piecewise(const td_piecewise_path& pw, td_vec2 p)
{
    float t = p.x;
    float height = p.y;
    td_vec2 point = pw.point_at_time(t);
    td_vec2 norm = pw.unit_normal_at_time(t);

    norm.x *= height;
    norm.y *= height;
    return point + norm;
}

td_vec2 td::smooth_transform_along_piecewise(const td_piecewise_path& pw, td_vec2 p)
{
    float t = p.x;
    float height = p.y;
    td_vec2 point = pw.point_at_time(t);
    td_vec2 norm = pw.smoothed_unit_normal_at_time(t);

    norm.x *= height;
    norm.y *= height;
    return point + norm;
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

