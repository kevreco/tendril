#ifndef TD_TENDRIL_EXTENSIONS_HPP
#define TD_TENDRIL_EXTENSIONS_HPP

#include "app.hpp"

#include <random>
#include <string>
#include <stdarg.h>

namespace td {

    static float random_float(float min, float max)
    {
        static std::random_device rd;     // Will be used to obtain a seed for the random number engine
        static std::mt19937 engine(rd()); // Standard mersenne_twister_engine seeded with rd()
        std::uniform_real_distribution<> distrib((float)min, (float)max);
        return (float)distrib(engine);
    }

    static td_vec2 random_vec2(float min, float max)
    {
        return td_vec2(random_float(min, max), random_float(min, max));
    }

    static void randomize(std::vector<td_vec2>* points, float min, float max)
    {
        td_vec2* ptr = points->data();
        for (size_t i = 0; i < points->size(); i += 1)
        {
            ptr[i] = random_vec2(min, max);
        }
    }
    static inline int string_printfv(std::string* str, const char* fmt, va_list args)
    {
        va_list args_copy;
        va_copy(args_copy, args);

        int add_len = vsnprintf(NULL, 0, fmt, args_copy);
        TD_ASSERT(add_len >= 0);

        size_t current_size = str->size();
        str->resize(str->size() + add_len);

        add_len = vsnprintf((char*)str->data() + current_size, add_len + 1, fmt, args);

        return add_len;
    }

    static inline int string_printf(std::string* str, const char* fmt, ...)
    {
        va_list args;
        va_start(args, fmt);
        int len = string_printfv(str, fmt, args);
        va_end(args);
        return len;
    }
}

#endif // TD_TENDRIL_EXTENSIONS_HPP