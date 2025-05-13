#include "tendril.hpp"
#include "tendril_internal.hpp"

//  Table of content:
//    
//    tendril.hpp
// 
//      td_arena
// 
//    tendril_internal.hpp
// 
//      misc
//      td_path_fragmenter
//      td_path_inserter
//      td_path_printer
//      td_bezier_traverser
//      td_path_to_piecewise
//      td_skip_whitespace
//      td_parse_float
//      td_parse_svg_numbers
//      td_parse_zero_or_one
//

// ============================================================================
// td_arena
// ============================================================================

td_arena::td_arena_chunk td_arena::td_arena_chunk::create(size_t byte_size)
{
    char* data = NULL;

    data = (char*)TD_MALLOC(byte_size);
    if (data == NULL)
    {
        TD_ASSERT(0 && "malloc failed.");
        return td_arena::td_arena_chunk{};
    }

    td_arena::td_arena_chunk chunk;
    chunk.data = data;
    chunk.size = 0;
    chunk.capacity = byte_size;
    return chunk;
}

td_arena::td_arena(size_t chunk_min_capacity, size_t alignment)
{
    TD_ASSERT(alignment > 0);
    TD_ASSERT(td_is_power_of_two(alignment));

    this->chunk_min_capacity = chunk_min_capacity;
    this->alignment = alignment;
}

td_arena::~td_arena()
{
    for (size_t i = 0; i < chunks.size(); i += 1)
    {
        TD_FREE(chunks[i].data);
    }

    chunk_used = 0;
}

void td_arena::clear()
{
    for (size_t i = 0; i < chunk_used; i += 1)
    {
        chunks[i].size = 0;
    }
    chunk_used = 0;
}

void* td_arena::alloc_bytes(size_t byte_size)
{
    // Get number of byte aligned with the upper value.
    size_t aligned_up_byte_size = td_align_up(byte_size, alignment);

    chunk* selected_chunk = NULL;

    // Check if there is space in all already allocated chunks, starting with the last in use.
    if (chunks.size() != 0)
    {
        size_t i = chunk_used == 0 ? 0 : chunk_used - 1;
        for (; i < chunks.size(); i += 1)
        {
            if (chunks[i].size + aligned_up_byte_size <= chunks[i].capacity)
            {
                selected_chunk = &chunks[i];
                break;
            }
        }
    }

    // No fitting chunk was found, create a new chunk.
    if (!selected_chunk)
    {
        size_t bytes_to_allocate = td_align_up(td_max(chunk_min_capacity, aligned_up_byte_size), alignment);
        chunk chunk = td_arena::td_arena_chunk::create(bytes_to_allocate);
        chunks.push_back(chunk);
        selected_chunk = &chunks.back();
    }

    TD_ASSERT(selected_chunk);

    void* mem = (selected_chunk->data + selected_chunk->size);

    TD_ASSERT(mem);

    selected_chunk->size += byte_size;

    // Update number of chunk used.
    chunk_used = selected_chunk - chunks.data() + 1;

    return (void*)mem;
}

td_arena_state td_arena::save_state()
{
    td_arena_state state;
    if (chunks.size())
    {
        state.chunk_used = chunk_used;
        state.chunk_size = chunks[chunk_used - 1].size;
    }
    else
    {
        state.chunk_used = 0;
        state.chunk_size = 0;
    }

    return state;
}

void td_arena::rollback_state(td_arena_state state)
{
    if (!state.chunk_used)
    {
        clear();
        return;
    }

    for (size_t i = state.chunk_used; i < chunks.size(); i += 1)
    {
        chunks[i].size = 0;
    }

    chunk_used = state.chunk_used;
    chunks[chunk_used - 1].size = state.chunk_size;
}

// ============================================================================
// misc
// ============================================================================

int td_str_fmt(char* buffer, size_t buffer_size, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    int len = vsnprintf(buffer, buffer_size, fmt, args);

    va_end(args);
    if (buffer == NULL)
        return len;

#if _MSC_VER
    if (len == -1)
#else
    if (len >= (int)buffer_size) 
#endif
    {
        len = (int)buffer_size - 1;
    }

    buffer[len] = 0;
    return len;
}

int td_path_cmd_point_count(td_path_cmd cmd)
{
    int size = 0;
    switch (cmd)
    {
    case td_path_cmd_MOVE_TO:
    case td_path_cmd_LINE_TO:
    case td_path_cmd_CLOSE:
        size = 1;
        break;
    case td_path_cmd_CUBIC_TO:
        size = 3;
        break;
    default:
        TD_ASSERT(0 && "Unreachable");
    }
    return size;
}

const char* td_utf8_codepoint(const char* str, td_codepoint* out_codepoint)
{
    if (0xf0 == (0xf8 & str[0])) {
        /* 4 byte utf8 codepoint */
        *out_codepoint = ((0x07 & str[0]) << 18) | ((0x3f & str[1]) << 12) |
            ((0x3f & str[2]) << 6) | (0x3f & str[3]);
        str += 4;
    }
    else if (0xe0 == (0xf0 & str[0])) {
        /* 3 byte utf8 codepoint */
        *out_codepoint =
            ((0x0f & str[0]) << 12) | ((0x3f & str[1]) << 6) | (0x3f & str[2]);
        str += 3;
    }
    else if (0xc0 == (0xe0 & str[0])) {
        /* 2 byte utf8 codepoint */
        *out_codepoint = ((0x1f & str[0]) << 6) | (0x3f & str[1]);
        str += 2;
    }
    else {
        /* 1 byte utf8 codepoint otherwise */
        *out_codepoint = str[0];
        str += 1;
    }

    return (const char*)str;
}

size_t td_is_power_of_two(size_t v)
{
    return 0 == (v & (v - 1));
}

size_t td_align_up(size_t v, size_t byte_alignment)
{
    // Alignment must be a power of two
    TD_ASSERT(td_is_power_of_two(byte_alignment) && "Must align to a power of two.");

    // Equivalent of v % byte_alignment since it's a power of two.
    size_t mod = v & (byte_alignment - 1);
    return mod
        ? v + byte_alignment - mod
        : v;
}

double td_pow(double base, unsigned int exponent)
{
    double value = 1;
    for (; exponent; exponent >>= 1) {
        if (exponent & 1)
            value *= base;
        base *= base;
    }
    return value;
}

// ============================================================================
// td_path_fragmenter
// ============================================================================

td_path_fragmenter::td_path_fragmenter(float _fragement_length)
{
    fragment_length = _fragement_length;
}

void td_path_fragmenter::func(void* ctx, td_path_cmd cmd, const td_vec2* points)
{
    td_path_fragmenter* f = (td_path_fragmenter*)ctx;

    if (cmd == td_path_cmd_MOVE_TO) {
        f->traverse_func(f->ctx, td_path_cmd_MOVE_TO, points);
        f->current_point = points[0];
        return;
    }

    if (cmd == td_path_cmd_LINE_TO)
    {
        td_vec2 a = f->current_point;
        td_vec2 b = points[0];
        td_vec2 dir = b - a;
        float segment_dist = td::length(dir);
        float acc_dist = 0.f;

        // While there is a segment larger the frag_len, cut the line into pieces. 
        if (segment_dist - acc_dist > f->fragment_length)
        {
            // Normalize direction
            dir = dir / segment_dist;
            do
            {
                acc_dist += f->fragment_length;
                td_vec2 p = a + (dir * acc_dist);

                f->traverse_func(f->ctx, td_path_cmd_LINE_TO, &p);
            } while (segment_dist - acc_dist > f->fragment_length);
        }

        // Add remaining point
        f->traverse_func(f->ctx, td_path_cmd_LINE_TO, &points[0]);

        f->current_point = b;
    }

    if (cmd == td_path_cmd_CUBIC_TO)
    {
        TD_ASSERT("Cannot fragment curve. Not Implemented yet. Please use flatten your curve before.");
    }

    if (cmd == td_path_cmd_CLOSE)
    {
        f->traverse_func(f->ctx, td_path_cmd_CLOSE, &points[0]);
    }
}

// ============================================================================
// td_path_inserter
// ============================================================================

void td_path_inserter::func(void* ctx, td_path_cmd cmd, const td_vec2* points)
{
    td_path* p = (td_path*)(ctx);
    switch (cmd)
    {
    case td_path_cmd_MOVE_TO:
        p->move_to(points[0]);
        break;
    case td_path_cmd_LINE_TO:
        p->line_to(points[0]);
        break;
    case td_path_cmd_CUBIC_TO:
        p->cubic_to(points[0], points[1], points[2]);
        break;
    case td_path_cmd_CLOSE:
        p->close();
        break;
    default:
        TD_ASSERT(0 && "Unreachable");
    }
}

// ============================================================================
// td_path_printer
// ============================================================================

void td_path_printer::func(void* ctx, td_path_cmd cmd, const td_vec2* points)
{
    switch (cmd)
    {
    case td_path_cmd_MOVE_TO:
        printf("(%f - %f)\n", points[0].x, points[0].y);
        break;
    case td_path_cmd_LINE_TO:
        printf("(%f - %f)\n", points[0].x, points[0].y);
        break;
    case td_path_cmd_CUBIC_TO:
        printf("(%f - %f)/(%f - %f)/(%f - %f)\n",
            points[0].x, points[0].y,
            points[1].x, points[1].y,
            points[2].x, points[2].y
        );
        break;
    case td_path_cmd_CLOSE:
        printf("(%f - %f)\n", points[0].x, points[0].y);
        break;
    default:
        TD_ASSERT(0 && "Unreachable");
    }
}

// ============================================================================
// td_bezier_traverser
// ============================================================================

td_bezier_traverser::td_bezier_traverser(td_traverse_func_t _traverse_func, void* _ctx)
{
    traverse_func = _traverse_func;
    ctx = _ctx;
}

void td_bezier_traverser::func(void* ctx, const td_vec2& point)
{
    td_bezier_traverser* t = (td_bezier_traverser*)ctx;
    t->traverse_func(t->ctx, td_path_cmd_LINE_TO, &point);
}

// ============================================================================
// td_path_to_piecewise
// ============================================================================

td_path_to_piecewise::td_path_to_piecewise(td_piecewise_path* _piecewise,
        const td_vec2 first_point
)
{
    piecewise = _piecewise;
    current_point = first_point;
}

void td_path_to_piecewise::func(void* ctx, td_path_cmd cmd, const td_vec2* points)
{
    td_path_to_piecewise* to_pw = (td_path_to_piecewise*)ctx;
    td_piecewise_path* pw = to_pw->piecewise;

    switch (cmd)
    {
    case td_path_cmd_MOVE_TO:
    case td_path_cmd_LINE_TO:
    case td_path_cmd_CLOSE: {

        float time = td::length(points[0] - to_pw->current_point);
        to_pw->current_length += time;
        pw->push_back(points[0], to_pw->current_length);

        to_pw->current_point = points[0];
        break;
    }
    case td_path_cmd_CUBIC_TO: {
        TD_ASSERT(0 && "Path must be flatten.");
        break;
    }
    default:
        TD_ASSERT(0 && "Unreachable");
    }
}

// ============================================================================
// td_transform_inserter
// ============================================================================

td_transform_inserter::td_transform_inserter(td_path* _path, const td_transform& trans)
{
    path = _path;
    transform = trans;
}

void td_transform_inserter::func(void* ctx, td_path_cmd cmd, const td_vec2* points)
{
    td_transform_inserter* i = (td_transform_inserter*)ctx;
    switch (cmd)
    {
    case td_path_cmd_MOVE_TO:
        i->path->move_to(i->transform.map(points[0]));
        break;
    case td_path_cmd_LINE_TO:
        i->path->line_to(i->transform.map(points[0]));
        break;
    case td_path_cmd_CUBIC_TO:
        i->transform.map_many(i->points, points, 3);
        i->path->cubic_to(i->points[0], i->points[1], i->points[2]);
        break;
    default:
        TD_ASSERT(0 && "Unreachable");
    }
}

const char* td_skip_whitespace(const char* begin, const char* end)
{
    const char* cursor = begin;
    while (cursor < end && TD_IS_WHITESPACE(cursor[0]))
    {
        cursor += 1;
    }
    return cursor;
}

// Adapted from https://github.com/nothings/stb/blob/master/stb_c_lexer.h
int td_parse_float(const char* p, const char* end, float* result)
{
    const char* s = p;
    double value = 0;
    int base = 10;
    int exponent = 0;
    bool minus = false;

    if (p < end)
    {
        if (p[0] == '+')
        {
            p += 1;
        }
        else if (p[0] == '-')
        {
            minus = true;
            p += 1;
        }
    }
    if (p >= end)
    {
        *result = (float)(minus ? -value : value);
        return p - s;
    }

    while (p < end) {
        if (*p >= '0' && *p <= '9')
            value = value * base + (*p++ - '0');
        else
            break;
    }

    if (p >= end)
    {
        *result = (float)(minus ? -value : value);
        return p - s;
    }

    if (*p == '.') {
        double pow, addend = 0;
        ++p;
        for (pow = 1; p < end; pow *= base) {
            if (*p >= '0' && *p <= '9')
                addend = addend * base + (*p++ - '0');
            else
                break;
        }
        value += addend / pow;
    }

    if (p >= end)
    {
        *result = (float)(minus ? -value : value);
        return p - s;
    }

    exponent = (*p == 'e' || *p == 'E');

    if (exponent)
    {
        int sign = p[1] == '-';
        unsigned int exponent = 0;
        double power = 1;
        ++p;
        if (p < end && (*p == '-' || *p == '+'))
            ++p;
        while (p < end && (*p >= '0' && *p <= '9'))
            exponent = exponent * 10 + (*p++ - '0');

        power = td_pow(10, exponent);
        if (sign)
            value /= power;
        else
            value *= power;
    }

    *result = (float)(minus ? -value : value);

    return p - s;
}

int td_parse_svg_numbers(const char* cursor, const char* end, float* values, int count)
{
    const char* s = cursor;
    for (int i = 0; i < count; i += 1)
    {
        cursor = td_skip_whitespace(cursor, end);

        int advance = td_parse_float(cursor, end, values + i);
        if (!advance)
        {
            return 0;
        }
        cursor += advance;

        cursor = td_skip_whitespace(cursor, end);
        if (cursor < end && cursor[0] == ',')
            cursor += 1;
        cursor = td_skip_whitespace(cursor, end);

    }

    return cursor - s;
}

int td_parse_zero_or_one(const char* cursor, const char* end, bool* flag)
{
    const char* s = cursor;
    cursor = td_skip_whitespace(cursor, end);

    if (cursor < end)
    {
        if (cursor[0] == '1')
        {
            *flag = true;
            cursor += 1;
        }
        else if (cursor[0] == '0')
        {
            *flag = false;
            cursor += 1;
        }
    }

    cursor = td_skip_whitespace(cursor, end);
    if (cursor < end && cursor[0] == ',')
        cursor += 1;
    cursor = td_skip_whitespace(cursor, end);
    return cursor - s;
}
