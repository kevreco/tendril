#ifndef TD_TENDRIL_HPP
#define TD_TENDRIL_HPP

//  Table of content:
//
//    MACRO:
// 
//      TD_ASSERT
//      TD_MALLOC
//      TD_CALLOC
//      TD_FREE
// 
//    tendril.cpp
// 
//      td_pod_vector
//      td_arena
//
//    math.cpp:
// 
//      td_vec2
//      td_vec2i
//      td_rect
//      td_recti
//      td_bezier
//      td_transform
//      td_rgba8
//      td_rgba32
// 
//    path.cpp:
//    
//      td_path_cmd
//      td_path
//      td_piecewise_path
//      td_path_iterator
//    
//    font.cpp:
//    
//      td_codepoint
//      td_glyph
//      td_font
//      td_font_store
//    
//    operation.cpp:
// 
//    inline functions
//    template functions
// 

#include "stdio.h"  // FILE*
#include <stdint.h> // uint16_t uint32_t
#include <string.h> // memset
#include <stddef.h> // ptrdiff_t

#define STBTT_STATIC
#include "td_stb_truetype.h"

#define TD_VERSION_NUMBER (0)
#define TD_VERSION_TEXT "0.0.0-dev"

#ifndef TD_ASSERT
#include <assert.h>
#define TD_ASSERT assert
#endif

#ifndef TD_MALLOC
#include <stdlib.h>
#define TD_MALLOC malloc
#endif

#ifndef TD_CALLOC
#include <stdlib.h>
#define TD_CALLOC calloc
#endif

#ifndef TD_FREE
#include <stdlib.h>
#define TD_FREE free
#endif

//=============================================================================
// tendril.cpp
//=============================================================================

// Lightweight equivalent of std::vector
// NOTE: It's meant to be used with plain old data, this will not call any destructor of T.
template<typename T>
struct td_pod_vector
{
    struct arr {
        T* data;
        size_t size;
        size_t capacity;
        arr() {
           data = 0;
           size = 0;
           capacity = 0;
        }
    } d ;

    // Provide standard typedefs but we don't use them ourselves.
    typedef T value_type;
    typedef value_type* iterator;
    typedef const value_type* const_iterator;

    // Constructors, destructor
    inline td_pod_vector() { }
    inline td_pod_vector(const td_pod_vector<T>& src) { assign(src); }
    inline ~td_pod_vector() { if (d.data) { TD_FREE(d.data); } }

    inline td_pod_vector<T>& operator=(const td_pod_vector<T>& src) { return assign(src); }
    inline T& operator[](size_t i) { return at(i); }
    inline const T& operator[](size_t i) const { return at(i); }

    inline T& at(size_t i) { TD_ASSERT(i < d.size); return d.data[i]; }
    inline const T& at(size_t i) const { TD_ASSERT(i < d.size); return d.data[i]; }

    inline void clear() { d.size = 0; }

    inline bool empty() const   { return d.size == 0; }
    inline size_t size() const     { return d.size; }
    inline size_t capacity() const { return d.capacity; }
  
    inline T* data() { return d.data; }
    inline const T* data() const { return d.data; }

    inline T* begin()             { return d.data; }
    inline const T* begin() const { return d.data; }
    inline T* end()               { return d.data + d.size; }
    inline const T* end() const   { return d.data + d.size; }
    inline T& front()             { TD_ASSERT(d.size > 0); return d.data[0]; }
    inline const T& front() const { TD_ASSERT(d.size > 0); return d.data[0]; }
    inline T& back()              { TD_ASSERT(d.size > 0); return d.data[d.size - 1]; }
    inline const T& back() const  { TD_ASSERT(d.size > 0); return d.data[d.size - 1]; }

    inline void swap(td_pod_vector<T>& other) { td_memswap_ptr(this, &other); }

    inline void resize(size_t new_size) { ensure_capacity(new_size); d.size = new_size; }

    inline void push_back(const T& v)  { ensure_capacity(d.size + 1); d.data[d.size] = v; d.size += 1; }
    inline void pop_back()             { TD_ASSERT(d.size > 0); d.size -= 1; }
    inline void push_front(const T& v) { insert(d.data, &v, 1); }
    inline T*   erase(const T* it)     { return erase(it, it + 1); }
    inline T*   insert(const T* it, const T& v) { return insert(it, &v, 1); }

    inline T* insert(const T* it, const T* begin, const T* end) { TD_ASSERT(begin <= end); return insert(it, begin, end - begin); }
    
    void ensure_capacity(size_t new_capacity);
    
    td_pod_vector<T>& assign(const td_pod_vector<T>& src);
   
    T* insert_space_at(size_t index, const T* value, size_t count);
   
    T* insert(const T* it, const T* value, size_t count);
    
    T* erase(const T* it, const T* last);
    
    void reserve(size_t new_capacity);

    bool contains(const T& v) const;
};

template<class T>
using td_vector = td_pod_vector<T>;

struct td_arena_state {
    size_t chunk_used;
    size_t chunk_size;
};

struct td_arena
{
    struct td_arena_chunk {
        char* data = NULL;
        size_t size = 0;
        size_t capacity = 0;

        static td_arena_chunk create(size_t byte_size);
    };

    typedef td_arena_chunk chunk;

    td_vector<chunk> chunks;
    size_t chunk_used = 0;
    size_t chunk_min_capacity = 0;
    size_t alignment = sizeof(uintptr_t);

    td_arena(size_t chunk_min_capacity = 256, size_t alignment = sizeof(uintptr_t));

    ~td_arena();

    void clear();

    void* alloc_bytes(size_t byte_size);

    // Allocate memory.
    template<class T>
    T* alloc(size_t count);

    // Allocate zero initialize memory.
    template<class T>
    T* zalloc(size_t count);

    td_arena_state save_state();

    void rollback_state(td_arena_state state);
};

//=============================================================================
// math.cpp
//=============================================================================

namespace td {

    float lerp(float a, float b, float t);

    float inv_lerp(float a, float b, float t);
}

struct td_vec2 
{
#ifdef TD_VEC2_CLASS_EXTRA
    TD_VEC2_CLASS_EXTRA
#endif
    float x;
    float y;

    td_vec2() : x(0.0f), y(0.0f) { }
    td_vec2(float _x, float _y) : x(_x), y(_y) {}

    float operator[](int i) const { TD_ASSERT(i == 0 || i == 1); return ((float*)(this))[i]; }
    float& operator[](int i) { TD_ASSERT(i == 0 || i == 1); return ((float*)(this))[i]; }

    td_vec2 operator-() const { return td_vec2(-x, -y); }
    td_vec2 operator+() const { return *this; }

    td_vec2 operator+(const td_vec2& v) const { return td_vec2(x + v.x, y + v.y); }
    td_vec2 operator-(const td_vec2& v) const { return td_vec2(x - v.x, y - v.y); }
    td_vec2 operator*(const td_vec2& v) const { return td_vec2(x * v.x, y * v.y); }
    td_vec2 operator/(const td_vec2& v) const { return td_vec2(x / v.x, y / v.y); }

    td_vec2 operator+(const float val) const { return td_vec2(x + val, y + val); }
    td_vec2 operator-(const float val) const { return td_vec2(x - val, y - val); }
    td_vec2 operator*(const float val) const { return td_vec2(x * val, y * val); }

    td_vec2& operator+=(const td_vec2& v) { x += v.x; y += v.y; return *this; }
    td_vec2& operator-=(const td_vec2& v) { x -= v.x; y -= v.y; return *this; }
    td_vec2& operator*=(const td_vec2& v) { x *= v.x; y *= v.y; return *this; }
    td_vec2& operator/=(const td_vec2& v) { x /= v.x; y /= v.y; return *this; }

    td_vec2& operator+=(const float val) { x += val; y += val; return *this; }
    td_vec2& operator-=(const float val) { x -= val; y -= val; return *this; }
    td_vec2& operator*=(const float val) { x *= val; y *= val; return *this; }

    td_vec2 operator/(const float val) const
    {
        float iVal = 1.0f / val;
        return td_vec2(x * iVal, y * iVal);
    }

    td_vec2& operator/=(const float val)
    {
        float inv_val = 1.0f / val;
        x *= inv_val; y *= inv_val;
        return *this;
    }

    bool operator==(const td_vec2& v) const { return x == v.x && y == v.y; }

    bool operator!=(const td_vec2& v) const { return !((*this) == v); }
};

namespace td {

    float squared_length(const td_vec2& v);

    float length(const td_vec2& v);

    void normalize(td_vec2* v);

    td_vec2 normalized(const td_vec2& v);

    td_vec2 lerp(const td_vec2& a, const td_vec2& b, float t);

    td_vec2 inv_lerp(const td_vec2& a, const td_vec2 b, float t);

    float dot(const td_vec2& a, const td_vec2& b);

    float cross(const td_vec2& a, const td_vec2& b);

    td_vec2 sign(const td_vec2& v);
   
    td_vec2 const perpendicular(td_vec2 that);
}

struct td_vec2i
{
    int x;
    int y;

    td_vec2i() : x(0), y(0) {}
    td_vec2i(int _x, int _y) : x(_x), y(_y) {}

    int& operator[] (size_t i)
    {
        TD_ASSERT(i == 0 || i == 1);
        return ((int*)(void*)(char*)this)[i];
    }

    int operator[] (size_t i) const
    {
        TD_ASSERT(i == 0 || i == 1);
        return ((const int*)(const void*)(const char*)this)[i];
    }
};

struct td_rect
{
    td_vec2 min;
    td_vec2 max;

    td_rect();
    td_rect(float min_x, float min_y, float max_x, float max_y);
    td_rect(const td_vec2& _min, const td_vec2& _max);
    td_rect(const td_rect& rect);

    // Equivalent of contains_inclusive
    bool contains(const td_vec2& p) const;

    // If the point is strictly inside the box
    bool contains_exclusive(const td_vec2& p) const;
    // If the point if contains inside the box or on the side
    bool contains_inclusive(const td_vec2& p) const;

    // Inclusive
    bool contains(const td_vec2& _min, const td_vec2& _max) const;
    // Inclusive
    bool contains(const td_rect& r) const;
    // Inclusive
    bool contains(const td_vec2& p1, const td_vec2& p2, const td_vec2& p3, const td_vec2& p4) const;

    void expand(float amount);
    void reduce(float amount);

    void add(const td_vec2& p);
    void clip_with(const td_rect& r);
};

struct td_recti
{
    td_vec2i min;
    td_vec2i max;

    td_recti();
    td_recti(td_vec2i _min, td_vec2i _max);
};

typedef void (*td_add_point_func_t)(void* ctx, const td_vec2& point);

#define TD_BEZIER_DEFAULT_TOLERANCE 1.25f

union td_bezier
{
   
    td_vec2 p[4];
    struct {
        float x1;
        float y1;
        float x2;
        float y2;
        float x3;
        float y3;
        float x4;
        float y4;
    };

    td_vec2* ptr();

    td_vec2& operator[] (size_t idx);
    td_vec2 operator[] (size_t idx) const;

    td_bezier();

    td_bezier(td_vec2 p1, td_vec2 p2, td_vec2 p3, td_vec2 p4);

    td_bezier(td_vec2* p, int count);

    td_vec2 at(float t) const;

    td_vec2 tangent(float t) const;

    td_vec2 normal_raw(float t) const;

    bool within_tolerance(float tolerance) const;

    static bool equals(const td_bezier& left, const td_bezier& right);

    static bool within_tolerance(float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4, float tolerance);
    static bool within_tolerance(const td_vec2& p1, const td_vec2& p2, const td_vec2& p3, const td_vec2& p4, float tolerance);

    static void traverse_flatten(td_add_point_func_t add_point, void* ctx,
        float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4,
        int level = 0, float tessellation_tolerance = TD_BEZIER_DEFAULT_TOLERANCE);

    static void traverse_flatten(td_add_point_func_t add_point, void* ctx,
        const td_vec2& p1, const td_vec2& p2, const td_vec2& p3, const td_vec2& p4,
        int level = 0, float tessellation_tolerance = TD_BEZIER_DEFAULT_TOLERANCE);
};

union td_transform
{
    struct {
        float m[6];
    };
    struct {
        float m11;
        float m12;
        float m21;
        float m22;
        float dx;
        float dy;
    };

    float& operator[] (size_t i) { TD_ASSERT(i >= 0 || i < 6); return m[i]; }
    float operator[] (size_t i) const { TD_ASSERT(i >= 0 || i < 6); return m[i]; }

    static td_transform zero();

    static td_transform identity();

    void reset();

    td_vec2 map(td_vec2 p);

    void map_many(td_vec2* dst, const td_vec2* src, int count);

    void translate(float x, float y);

    void translate(td_vec2 p);

    void set_translation(float x, float y);

    void set_translation(td_vec2 p);

    void scale(float x, float y);

    void scale(td_vec2 p);

    void set_scale(float x, float y);

    void multiply(const td_transform& m);

    float determinant() const;
    td_transform inversed() const;
};

#define TD_U32_R_SHIFT 0
#define TD_U32_G_SHIFT 8
#define TD_U32_B_SHIFT 16
#define TD_U32_A_SHIFT 24

#define TD_U32_R_MASK (0xFFu << TD_U32_R_SHIFT)
#define TD_U32_G_MASK (0xFFu << TD_U32_G_SHIFT)
#define TD_U32_B_MASK (0xFFu << TD_U32_B_SHIFT)
#define TD_U32_A_MASK (0xFFu << TD_U32_A_SHIFT)

#define TD_U32_GET_R(x) (((uint32_t)(x) >> TD_U32_R_SHIFT) & 0xFFu)
#define TD_U32_GET_G(x) (((uint32_t)(x) >> TD_U32_G_SHIFT) & 0xFFu)
#define TD_U32_GET_B(x) (((uint32_t)(x) >> TD_U32_B_SHIFT) & 0xFFu)
#define TD_U32_GET_A(x) (((uint32_t)(x) >> TD_U32_A_SHIFT) & 0xFFu)
#define TD_U32_GET_ALPHA TD_U32_GET_A
#define TD_U32_GET_INV_ALPHA(x) (255 - TD_U32_GET_ALPHA(x))

#define TD_U32_SET_R(x, r) ((uint32_t)(x) & ~TD_U32_R_MASK) | ((r) << TD_U32_R_SHIFT)
#define TD_U32_SET_G(x, g) ((uint32_t)(x) & ~TD_U32_G_MASK) | ((g) << TD_U32_G_SHIFT)
#define TD_U32_SET_B(x, b) ((uint32_t)(x) & ~TD_U32_B_MASK) | ((b) << TD_U32_B_SHIFT)
#define TD_U32_SET_A(x, a) ((uint32_t)(x) & ~TD_U32_A_MASK) | ((a) << TD_U32_A_SHIFT)
#define TD_U32_SET_ALPHA TD_U32_SET_A

#define TD_U32_FROM_RGBA8(r, g, b, a) (((uint32_t)(a) << TD_U32_A_SHIFT) | ((uint32_t)(b) << TD_U32_B_SHIFT) | ((uint32_t)(g) << TD_U32_G_SHIFT) | ((uint32_t)(r) << TD_U32_R_SHIFT))

struct td_rgba8
{
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    uint8_t a = 0;

    td_rgba8() {
    }
    td_rgba8(unsigned char _r, unsigned char _g, unsigned char _b, unsigned char _a)
    {
        r = _r;
        g = _g;
        b = _b;
        a = _a;
    }
    uint32_t to_u32() const { return TD_U32_FROM_RGBA8(r, g, b, a); }

    uint32_t* to_u32_ptr() { return (uint32_t*)this; }

    static inline td_rgba8 premultiplied_with_alpha(td_rgba8 c)
    {
        uint32_t a = c.a + 1;

        return td_rgba8{
            (uint8_t)((c.r * a) >> 8),
            (uint8_t)((c.g * a) >> 8),
            (uint8_t)((c.b * a) >> 8),
            c.a,
        };
    }
};

union td_rgba32
{
    uint32_t u32;

    td_rgba32() { u32 = 0; }
    td_rgba32(uint32_t col)
    {
        u32 = col;
    }

    td_rgba32(td_rgba8 c)
    {
        u32 = TD_U32_FROM_RGBA8(c.r, c.g, c.b, c.a);
    }

    td_rgba32(unsigned char r, unsigned char g, unsigned char b, unsigned char a)
    {
        u32 = TD_U32_FROM_RGBA8(r, g, b, a);
    }

    operator uint32_t() const { return u32; }
    unsigned char r() const { return TD_U32_GET_R(u32); }
    unsigned char g() const { return TD_U32_GET_G(u32); }
    unsigned char b() const { return TD_U32_GET_B(u32); }
    unsigned char a() const { return TD_U32_GET_A(u32); }

    void set_r(unsigned char r)
    {
        u32 = TD_U32_SET_R(u32, r);
    }

    void set_g(uint32_t g)
    {
        u32 = TD_U32_SET_G(u32, g);
    }

    void set_b(uint32_t b)
    {
        u32 = TD_U32_SET_B(u32, b);
    }

    void set_a(uint32_t a)
    {
        u32 = TD_U32_SET_A(u32, a);
    }

    static inline td_rgba32 premultiplied_with_alpha(td_rgba32 c)
    {
        uint32_t a = TD_U32_GET_A(c) + 1;

        return td_rgba32{
             (uint8_t)((TD_U32_GET_R(c) * a) >> 8),
             (uint8_t)((TD_U32_GET_G(c) * a) >> 8),
             (uint8_t)((TD_U32_GET_B(c) * a) >> 8),
            (uint8_t)TD_U32_GET_A(c),
        };
    }

    static void print(uint32_t i)
    {
        td_rgba32 c{ i };
        printf("%d %d %d %d \n", c.r(), c.g(), c.b(), c.a());
    }
};

//=============================================================================
// path.cpp
//=============================================================================

enum td_path_cmd
{
    td_path_cmd_NONE,
    td_path_cmd_MOVE_TO,
    td_path_cmd_LINE_TO,
    td_path_cmd_CUBIC_TO,
    td_path_cmd_CLOSE
};

typedef void (*td_traverse_func_t)(void* ctx, td_path_cmd cmd, const td_vec2* points);

enum td_operation_flags
{
    td_operation_flags_NONE,
    td_operation_flags_LINE = 1 << 0,
    td_operation_flags_CURVE = 1 << 1,
    td_operation_flags_ALL = 0xfffffff,
};

enum td_svg_options
{
    td_svg_options_NONE,
    td_svg_options_FILL,
    td_svg_options_STROKE,
};

typedef td_vector<td_vec2> td_point_array;

struct td_path {

    td_vector<td_path_cmd> cmds;

    td_point_array points;
    size_t point_anchor_index;     // Index of the "move_to" point of the current shape. In case we need to "close" a shape

    void clear();
    void reserve(size_t count);

    void clone_to(td_path* p) const;

    void move_to(float x, float y);

    void move_to(const td_vec2& p);

    void line_to(float x, float y);

    void line_to(const td_vec2& p);

    void quad_to(float x1, float y1, float x2, float y2);

    void quad_to(const td_vec2& p1, const td_vec2& p2);

    void cubic_to(float x1, float y1, float x2, float y2, float x3, float y3);

    void cubic_to(const td_vec2& p1, const td_vec2& p2, const td_vec2& p3);
    
    void add_rect(float min_x, float min_y, float max_x, float max_y);

    void add_rect(const td_vec2& min, const td_vec2& max);

    void arc(float cx, float cy, float px, float py, float qx, float qy, float start_angle, float angle_sweep);
    
    void arc(const td_vec2& c, const td_vec2& p, const td_vec2& q, float start_angle, float angle_sweep);

    // Draw an arc using an API equivalent to the SVG specification:
    //    A rx ry rotation large-arc-flag sweep-flag x y
    // rx, ry : horizontal and vertical Radiuses.
    // rotation: x axis rotation in radian.
    // large_arc_flag: short way or long way.
    // sweep_flag: clockwise or counter-clockwise.
    // x, y: position of the end of the arc.
    void arc_to(float rx, float ry, float rotation, bool large_arc_flag, bool sweep_flag, float x, float y);

    void close();

    void add_path(const td_path& p);

private:

    td_vec2* add_command(td_path_cmd cmd);
};

// Break a path into multiple pieces.
struct td_piecewise_path
{
    // List of points
    td_vector<td_vec2> points;
    // List of summed length
    td_vector<float> times;

    float length() const;

    void clear();

    void push_back(td_vec2 p, float time);
    void reserve(size_t count);

    td_vec2 point_at_time(float t) const;

    td_vec2 unit_normal_at_time(float t) const;
    td_vec2 smoothed_unit_normal_at_time(float t) const;

    td_vec2 unit_normal_at(size_t index) const;

    td_vec2 tangeant_at(size_t index) const;

    td_vec2 normal_at(size_t index) const;

    bool try_get_lower_point_index_at_time(float time, size_t* index) const;

    size_t lower_point_index_at_time(float time) const;
};

template<class T>
struct td_array_view
{
    const T* data = NULL;
    size_t size = 0;
    inline const T& operator[](size_t i) const { return data[i]; }

    td_array_view() { data = 0; size = 0; }
    td_array_view(const T* _data, size_t _size) { data = _data; size = _size; }
    td_array_view(const td_array_view<T>& v) { data = v.data; size = v.size; }
    td_array_view<T>& operator=(const td_array_view<T>& v) { data = v.data; size = v.size; return *this; }
};

struct td_path_view
{
    td_array_view<td_path_cmd> cmds;
    td_array_view<td_vec2> points;

    bool is_closed() const { return cmds.size && cmds[cmds.size - 1] == td_path_cmd_CLOSE; }
};

struct td_path_iterator
{
    td_path_view view;
    const td_vec2* points = NULL;

    td_path_cmd cmd = td_path_cmd_NONE;
    int cmd_index = 0;
    int point_index = 0;

    td_path_iterator(const td_path& _path);
    td_path_iterator(td_path_view _view);

    bool has_next() const;

    bool get_next();
};

// @TODO remove td_subpath_iterator if not used anymore.
// Iterate all sub path starting with move_to
struct td_subpath_iterator
{
    td_path_view source;
    td_path_view subpath;

    size_t points_count = 0;
    size_t cmds_count = 0;

    int cmd_index = 0;
    int point_index = 0;

    td_subpath_iterator(const td_path& _path);

    bool has_next() const;

    bool get_next();
};

namespace td {

    void traverse_path(const td_path& path, td_traverse_func_t func, void* ctx);

    void traverse_flatten_path(const td_path& path, td_traverse_func_t func, void* ctx);

    // Copy path and turn curves into segments.
    void to_flatten_path(const td_path& path, struct td_path* flatten);

    // Create piecewise version of the path
    // https://en.wikipedia.org/wiki/Piecewise_linear_function
    void to_piecewise_path(const td_path& path, td_piecewise_path* piecewise, td_operation_flags flags = td_operation_flags_ALL);

    // Copy path segments lines and curves to small lines of "fragment_length" size. 
    // Must not be confused with to_piecewise_path.
    void to_fragmented_path(const td_path& path, td_path* fragmented, float fragment_length = 5.0f);

    // Create SVG file with a single path.
    void to_svg_file(const td_path& path, const char* filename, size_t width, size_t height, const char* color = "black", td_svg_options option = td_svg_options_FILL);

    // Create SVG file with a single path.
    void to_svg_file(const td_path& path, FILE* file, size_t width, size_t height, const char* color = "black", td_svg_options option = td_svg_options_FILL);

    td_vec2 transform_along_piecewise(const td_piecewise_path& pw, td_vec2 p);
    td_vec2 smooth_transform_along_piecewise(const td_piecewise_path& pw, td_vec2 p);

    // Add 'p' to all points.
    void elements_sum(td_path* path, td_vec2 p);
    void elements_sum(td_piecewise_path* path, td_vec2 p);
    void elements_sum(td_vec2* points, size_t count, td_vec2 p);

    // Multiply all points by 'p' 
    void elements_multiply(td_path* path, td_vec2 p);
    void elements_multiply(td_piecewise_path* path, td_vec2 p);
    void elements_multiply(td_vec2* points, size_t count, td_vec2 p);
}

//=============================================================================
// font.cpp
//=============================================================================

// Representation of UTF-8 Unicode codpoint.
typedef unsigned int td_codepoint;

struct td_glyph {
    stbtt_vertex* vertices;
    int vertices_count;
    int index;
    int advance_width;
    int left_side_bearing;
    td_recti box;

    td_glyph();
};

struct td_font
{
    void* data;
    int ascent;
    int descent;
    int line_gap;
    td_recti box;
    stbtt_fontinfo info;
    
    td_font();

    float get_scale(float pixel_size);

    static bool init_from_file(td_font* f, const char* filename, int ttc_index);

    static bool init_from_data(td_font* f, void* data, unsigned int length, int ttc_index);

    static void destroy(td_font* f);
};

struct td_font_store
{
    static int const MaxUnicode = 0x10FFFF; // 1114111
    static int const GlyphCountPerPage = 256;

    static int const PageCountX = 17;
    static int const PageCountY = GlyphCountPerPage;

    td_font font;
    // 17 x 256 pages of 256 glyph pointers.
    // 1114111 (17 x 256 x 256)) can contain all unicode codepoints.
    // Pages are allocated on a need basis when a associated glyph is requested.
    td_glyph*** pages[PageCountX] = { 0 };

    // Arena allocator to contain pages.
    td_arena page_arena{ PageCountY };
    // Arena allocator to contain glyphs data.
    td_arena glyph_arena{ GlyphCountPerPage };

    static void init_from_file(td_font_store* fs, const char* filename, int ttc_index);

    static void destroy(td_font_store* fs);

    td_font* get_font();

    td_glyph* get_glyph(td_codepoint code);

    td_glyph* glyph_from_page(size_t page_x, size_t page_y, size_t glyph_index, td_codepoint code);

    td_glyph* create_glyph(td_codepoint code);

    static void get_glyph_coordinate(td_codepoint code, size_t* page_x, size_t* page_y, size_t* glyph_index);
};

namespace td {

    // Traverse the path of the a glyph.
    void traverse_glyph(td_font_store* fs, td_glyph* glyph, td_traverse_func_t func, void* ctx);

    // Returns text length
    float insert_text_to_path(td_font_store* fs, const char* begin, const char* end, td_vec2 pos, float pixel_size, td_path* path);

    // Returns text length
    float insert_text_to_path(td_font_store* fs, const char* begin, td_vec2 pos, float pixel_size, td_path* path);
}


//=============================================================================
// rasterize.cpp
//=============================================================================

enum td_join_style
{
    td_join_style_MITER,
    td_join_style_BEVEL,
    td_join_style_ROUNDED
};

enum td_cap_style
{
    td_cap_style_BUTT,
    td_cap_style_SQUARE,
    td_cap_style_CIRCLE
};

struct td_stroke_option
{
    float line_width = 1.0f;
    td_join_style join_style = td_join_style_MITER;
    td_cap_style cap_style = td_cap_style_BUTT;

    /// @brief  Set the limit on maximum pointiness allowed for miter joins.
    ///
    /// If the distance from the point where the lines intersect to the
    /// point where the outside edges of the join intersect exceeds this
    /// ratio relative to the line width, then a bevel join will be used
    /// instead, and the miter will be lopped off.  Larger values allow
    /// pointier miters.  Only affects stroking and only when the line join
    /// style is miter.  Must be positive.  If it is not, this does nothing.
    /// Defaults to 10.0.
    ///
    /// @param limit  the limit on maximum pointiness allowed for miter joins
    ///
    float miter_limit = 10.0f;
    // Tessellation tolerance when using add_casteljau_cubic_to
    float bezier_tesselation_tolerance = 1.25f;
};

typedef void (*td_composite_operation_func_t)(uint32_t* dest, uint32_t count, td_rgba8 color, unsigned char alpha);

struct td_rasterizer
{
    // Equivalent of FT_VECTOR from FreeType
    struct ft_vector {
        signed long x;
        signed long y;
    };
    
    // FreeType points
    td_pod_vector<ft_vector> points;
    // FreeType contour indices
    td_pod_vector<unsigned short> contours;
    // FreeType tags
    td_pod_vector<unsigned char> tags;

    // Clip rect which expand for each added point in command_add_stroke and command_add_fill
    td_rect clip_rect; 

    td_composite_operation_func_t operation = composite_operation_source_over;

    void render_fill_path(const td_path& path, td_rgba8* data, int width, int height, td_rgba8 color);
    void render_stroke_path(const td_path& path, td_rgba8* data, int width, int height, td_rgba8 color, const td_stroke_option& opt = td_stroke_option{});
    void render_horizontal_line(int x1, int x2, int y, td_rgba8* data, int width, int height, td_rgba8 color);

    void render_vertical_line(int x, int y1, int y2, td_rgba8* data, int width, int height, td_rgba8 color);

    // This blending will create transparent pixels.
    static void composite_operation_clear(uint32_t* dest, uint32_t count, td_rgba8 color, unsigned char alpha);
    // This blending will add the source over the destination.
    static void composite_operation_source_over(uint32_t* dest, uint32_t count, td_rgba8 color, unsigned char alpha);

    // Private API

    void command_begin();
    void command_add_fill(const td_path& path);
    void command_add_stroke(const td_path& path, const td_stroke_option& opt = td_stroke_option{});
    void command_end(td_rgba8* data, int width, int height, td_rgba8 color);

    static void add_half_stroke(td_add_point_func_t add_point, void* ctx, const td_point_array& points, size_t beginning, size_t ending, bool closed, const td_stroke_option& opt);
};

//=============================================================================
// td_arena implementation
//=============================================================================

// Allocate memory.
template<class T>
inline T* td_arena::alloc(size_t count)
{
    return (T*)alloc_bytes(sizeof(T) * count);
}

// Allocate zero initialize memory.
template<class T>
inline T* td_arena::zalloc(size_t count)
{
    T* mem = alloc<T>(count);
    memset(mem, 0, sizeof(T) * count);
    return mem;
}

//=============================================================================
// td_pod_vector implementation
//=============================================================================

template<typename T>
void td_pod_vector<T>::ensure_capacity(size_t new_capacity)
{
    if (new_capacity > d.capacity)
    {
        size_t to_reserve = d.capacity ? d.capacity * 2 : 8;
        to_reserve = new_capacity > to_reserve ? new_capacity : to_reserve;
        reserve(to_reserve);
    }
}

template<typename T>
td_pod_vector<T>& td_pod_vector<T>::assign(const td_pod_vector<T>& src)
{
    resize(src.d.size);
    if (src.d.data)
    {
        memcpy(d.data, src.d.data, d.size * sizeof(T));
    }
    return *this;
}

template<typename T>
T* td_pod_vector<T>::insert_space_at(size_t index, const T* value, size_t count)
{
    TD_ASSERT(index >= 0);
    TD_ASSERT(index <= d.size);
    size_t count_to_move = d.size - index;

    ensure_capacity(d.size + count);
    if (count_to_move > 0)
    {
        memmove(
            d.data + index + count,
            d.data + index,
            count_to_move * sizeof(T));
    }
    d.size += count;
    return d.data + index;
}

template<typename T>
T* td_pod_vector<T>::insert(const T* it, const T* value, size_t count)
{
    const ptrdiff_t index = it - d.data;
    T* mem = insert_space_at(index, value, count);
    memcpy(mem, value, count * sizeof(T));
    return mem;
}

template<typename T>
T* td_pod_vector<T>::erase(const T* it, const T* last)
{
    TD_ASSERT(it >= begin() && it < end() && last >= it && last <= end());
    const ptrdiff_t count = last - it;
    const ptrdiff_t offset = it - d.data;
    memmove(d.data + offset, d.data + offset + count, (d.size - (size_t)offset - (size_t)count) * sizeof(T));
    d.size -= count;
    return d.data + offset;
}

template<typename T>
void td_pod_vector<T>::reserve(size_t new_capacity)
{
    if (new_capacity > d.capacity)
    {
        T* new_data = (T*)TD_MALLOC(new_capacity * sizeof(T));
        if (d.data)
        {
            memcpy(new_data, d.data, d.size * sizeof(T));
            TD_FREE(d.data);
        }
        d.data = new_data;
        d.capacity = new_capacity;
    }
}


template<typename T>
bool td_pod_vector<T>::contains(const T& v) const
{
    const T* cursor = d.data;
    const T* end = d.data + d.size;
    while (cursor < end)
    {
        if (*cursor == v)
        {
            return true;
        }
        cursor += 1;
    }
    return false;
}

#endif /* TD_TENDRIL_HPP */