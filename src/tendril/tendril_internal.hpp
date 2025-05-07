#include "tendril.hpp"

//  Table of content:
// 
//    misc
//    path.cpp
//      td_path_fragmenter
//      td_path_inserter
//      td_path_printer
//      td_bezier_traverser
//      td_path_to_piecewise
//      td_transform_inserter
//    font.cpp
//      td_codepoint_iterator
//    byte operations

#ifndef TD_TENDRIL_INTERNAL_HPP
#define TD_TENDRIL_INTERNAL_HPP

#include <cstdint> // uint32_t

#define TD_XSTR(x) TD_STR(x)
#define TD_STR(x) #x

#define TD_IS_WHITESPACE(ch) ((ch) == ' ' || (ch) == '\t' || (ch) == '\r' || (ch) == '\n')
#define TD_IS_NUM(ch) ((ch) >= '0' && (ch) <= '9')
#define TD_IS_ALPHA(ch) (((ch) >= 'a' && (ch) <= 'z') || ((ch) >= 'A' && (ch) <= 'Z'))
#define TD_IS_ALPHANUM(ch) (TD_IS_ALPHA(ch) || TD_IS_NUM(ch))

#define TD_PI 3.14159265358979323846f
#define TD_DEGTORAD(x) ((x) * TD_PI / 180.0f)

#define td_fabs(x)     fabsf(x)
#define td_sqrt(x)     sqrtf(x)
#define td_fmod(x, y)  fmodf((x), (y))
#define td_cos(x)      cosf(x)
#define td_sin(x)      sinf(x)
#define td_acos(x)     acosf(x)
#define td_atan2(y, x) atan2f((y), (x))
#define td_atof(str)   atof(str)
#define td_ceil(x)     ceilf(x)
#define td_tan(x)      tanf(x)

// Returns the number of element of a static array
template< class T, size_t N >
static constexpr inline size_t td_size(const T(&array)[N])
{
    return N;
}

template<typename T>
static inline T td_clamp(T v, T min, T max)
{
    return (v < min) ? min : (v > max) ? max : v;
}

static inline float td_signf(float f)
{
    if (f > 0.0f) return 1.0f;
    if (f < 0.0f) return -1.0f;
    return 0.0f;
}

static inline float td_absf(float f)
{
    return (f >= 0) ? f : -f;
}

template<typename T>
static inline T td_abs(T v)
{
    return v < 0 ? -v : v;
}

template<typename T>
static inline T td_min(T left, T right)
{
    return left < right ? left : right;
}

template<typename T>
static inline T td_max(T left, T right)
{
    return left >= right ? left : right;
}


template<class T>
static inline void td_memswap_ptr(T* left, T* right) noexcept
{
    // Use char buffer to avoid constructor
    char memory[sizeof(T)];
    memcpy(memory, (void*)left, sizeof(T));
    memcpy((void*)left, (void*)right, sizeof(T));
    memcpy((void*)right, (void*)memory, sizeof(T));
}

template<class T>
static inline void td_memswap(T& left, T& right) noexcept
{
    td_memswap_ptr(&left, &right);
}

template<typename T>
static inline void td_copy(T* dest, const T* src, size_t count)
{
    memcpy(dest, src, count * sizeof(T));
}

static int td_path_cmd_point_count(td_path_cmd cmd);

static const char* td_utf8_codepoint(const char* str, td_codepoint* out_codepoint);

static size_t td_is_power_of_two(size_t v);

static size_t td_align_up(size_t v, size_t byte_alignment);

static double td_pow(double base, unsigned int exponent);

template <class T>
struct td_less {
    constexpr bool operator()(const T& left, const T& right) const
    {
        return left < right;
    }
};

// Equivalent of std::lower_bound which returns a index instead of an iterator.
template <class T, class Less = td_less<T>>
static inline size_t td_lower_bound(const T* data, size_t start, size_t count, const T& value)
{
    size_t step;
    size_t mid; /* index of the found value */

    while (count > 0)
    {
        step = count >> 1; // count divide by two using bit shift

        mid = start + step;

        if (Less{}(data[mid], value))
        {
            start = mid + 1;
            count -= step + 1;
        }
        else {
            count = step;
        }
    }
    return start;
}

// Equivalent of std::upper_bound which returns a index instead of an iterator.
template <class T, class Less = td_less<T>>
static inline size_t td_upper_bound(const T* data, size_t start, size_t count, const T& value)
{
    size_t step;
    size_t mid; /* index of the found value */

    while (count > 0)
    {
        step = count >> 1; // count divide by two using bit shift

        mid = start + step;

        if (!Less{}(value, data[mid]))
        {
            start = mid + 1;
            count -= step + 1;
        }
        else {
            count = step;
        }
    }
    return start;
}


//=============================================================================
// path.cpp
//=============================================================================

struct td_path_fragmenter
{
    // Path destination of the fragmented path
    void* ctx = NULL;
    td_traverse_func_t traverse_func = NULL;

    float fragment_length;

    td_vec2 current_point;

    td_path_fragmenter(float _frag_len = 1.0f);

    static void func(void* ctx, td_path_cmd cmd, const td_vec2* points);
};

struct td_path_inserter {
    static void func(void* ctx, td_path_cmd cmd, const td_vec2* points);
};

// For debugging purpose.
struct td_path_printer {
    static void func(void* ctx, td_path_cmd cmd, const td_vec2* points);
};

struct td_bezier_traverser
{
    td_traverse_func_t traverse_func;
    void* ctx;

    td_bezier_traverser(td_traverse_func_t _traverse_func, void* _ctx);

    static void func(void* ctx, const td_vec2& point);
};

struct td_path_to_piecewise
{
    td_piecewise_path* piecewise;
    td_vec2 current_point;
    float current_length = 0.0f;

    td_path_to_piecewise(td_piecewise_path* _piecewise, const td_vec2 first_point);

    static void func(void* ctx, td_path_cmd cmd, const td_vec2* points);
};

struct td_transform_inserter {
    td_path* path;
    td_vec2 points[3];
    td_transform transform;

    td_transform_inserter(td_path* _path, const td_transform& trans);

    static void func(void* ctx, td_path_cmd cmd, const td_vec2* points);
};


static inline const char* td_skip_whitespace(const char* begin, const char* end)
{
    const char* cursor = begin;
    while (cursor < end && TD_IS_WHITESPACE(cursor[0]))
    {
        cursor += 1;
    }
    return cursor;
}

//=============================================================================
// font.cpp
//=============================================================================

struct td_codepoint_iterator
{
    const char* cursor;
    const char* end;

    td_codepoint codepoint = -1;

    td_codepoint_iterator(const char* begin, const char* _end);

    bool has_next() const;

    bool get_next();
};

// ============================================================================
// rasterize.cpp
// ============================================================================

// For each component
// c = x.c * a  / 255
uint32_t td_u32_byte_mul(uint32_t x, uint32_t alpha);

// For each component
// c = x.c + (y.c - x.c) * a
uint32_t td_u32_byte_lerp(uint32_t x, uint32_t y, uint32_t alpha);

// c = x + (y - x) * a
uint8_t td_byte_lerp(uint8_t x, uint8_t y, uint8_t f);

// For each component
// c = (x.c * a + y.c * b) / 255
uint32_t td_u32_byte_interpolate(uint32_t x, uint32_t a, uint32_t y, uint32_t b);

// For each component c
// c = mod(x.c - y.c, 255)
uint32_t td_byte_sub_mod(uint32_t x, uint32_t y);

// For each component c
// c = mod(x.c = y.c, 255)
uint32_t td_byte_add_mod(uint32_t x, uint32_t y);

// For each component c
// c = min(x.c + y.c, 255)
uint32_t td_byte_add(uint32_t x, uint32_t y);

// memset specialize for uint32_t
void td_uint32_memset(uint32_t* dest, uint32_t value, size_t value_count);

#endif // TD_TENDRIL_INTERNAL_HPP