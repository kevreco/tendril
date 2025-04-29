#include "tendril.hpp"
#include "tendril_internal.hpp"

//  Table of content:
// 
//    types
//      td_glyph 
//      td_font
//      td_font_store
//      td_codepoint_iterator
// 
//    functions
//      td::traverse_glyph
//      td::insert_text_to_path
//

// ============================================================================
// td_glyph
// ============================================================================

td_glyph::td_glyph() :
    vertices(NULL),
    vertices_count(0),
    index(0),
    advance_width(0),
    left_side_bearing(0),
    box()
{
}

// ============================================================================
// td_font
// ============================================================================

td_font::td_font() :
    data(NULL),
    ascent(0),
    descent(0),
    line_gap(0),
    box()
{
    memset(&info, 0, sizeof(stbtt_fontinfo));
}

float td_font::get_scale(float pixel_size)
{
    return stbtt_ScaleForMappingEmToPixels(&info, pixel_size);
}

bool td_font::init_from_file(td_font* f, const char* filename, int ttc_index)
{
    FILE* fp = fopen(filename, "rb");
    if (fp == NULL) {
        return NULL;
    }

    fseek(fp, 0, SEEK_END);
    long length = ftell(fp);
    if (length == -1L) {
        fclose(fp);
        return NULL;
    }

    void* data = TD_MALLOC(length);
    if (data == NULL) {
        fclose(fp);
        return NULL;
    }

    fseek(fp, 0, SEEK_SET);
    size_t nread = fread(data, 1, length, fp);
    fclose(fp);

    if (nread != length) {
        TD_FREE(data);
        return NULL;
    }

    return init_from_data(f, data, length, ttc_index);
}

bool td_font::init_from_data(td_font* f, void* data, unsigned int length, int ttc_index)
{
    int offset = stbtt_GetFontOffsetForIndex((unsigned char*)data, ttc_index);
    if (offset == -1 || !stbtt_InitFont(&f->info, (unsigned char*)data, offset)) {

        return false;
    }
    f->data = data;
    stbtt_GetFontVMetrics(&f->info, &f->ascent, &f->descent, &f->line_gap);
    stbtt_GetFontBoundingBox(&f->info, &f->box.min.x, &f->box.min.y, &f->box.max.x, &f->box.max.y);

    return true;
}

void td_font::destroy(td_font* f)
{
    TD_FREE(f->data);
}

// ============================================================================
// td_font_store
// ============================================================================

void td_font_store::init_from_file(td_font_store* fs, const char* filename, int ttc_index)
{
    td_font::init_from_file(&fs->font, filename, ttc_index);
}

void td_font_store::destroy(td_font_store* fs)
{
    for (int x = 0; x < PageCountX; x += 1)
    {
        if (fs->pages[x])
        {
            for (int y = 0; y < PageCountY; y += 1)
            {
                if (fs->pages[x][y])
                {
                    for (int g = 0; g < GlyphCountPerPage; g += 1)
                    {
                        if (fs->pages[x][y][g])
                        {
                            stbtt_FreeShape(&fs->font.info, fs->pages[x][y][g]->vertices);
                        }
                    }
                }
            }
        }
    }

    td_font::destroy(&fs->font);
}

td_font* td_font_store::get_font()
{
    return &font;
}

td_glyph* td_font_store::get_glyph(td_codepoint code)
{
    size_t page_x;
    size_t page_y;
    size_t glyph_index;
    get_glyph_coordinate(code, &page_x, &page_y, &glyph_index);
    return glyph_from_page(page_x, page_y, glyph_index, code);
}

td_glyph* td_font_store::glyph_from_page(size_t page_x, size_t page_y, size_t glyph_index, td_codepoint code)
{
    if (!pages[page_x])
    {
        pages[page_x] = page_arena.zalloc<td_glyph**>(PageCountY);
    }

    if (!pages[page_x][page_y])
    {
        pages[page_x][page_y] = page_arena.zalloc<td_glyph*>(GlyphCountPerPage);
    }

    if (!pages[page_x][page_y][glyph_index])
    {
        pages[page_x][page_y][glyph_index] = create_glyph(code);
    }

    return pages[page_x][page_y][glyph_index];
}

td_glyph* td_font_store::create_glyph(td_codepoint code)
{
    td_glyph* g = glyph_arena.zalloc<td_glyph>(1);

    g->index = stbtt_FindGlyphIndex(&font.info, code);

    g->vertices_count = stbtt_GetGlyphShape(&font.info, g->index, &g->vertices);

    stbtt_GetGlyphHMetrics(&font.info, g->index, &g->advance_width, &g->left_side_bearing);
    
    if (!stbtt_GetGlyphBox(&font.info, g->index, &g->box.min.x, &g->box.min.y, &g->box.max.x, &g->box.max.y))
    {
        g->box = td_recti{};
    }

    return g;
}

void td_font_store::get_glyph_coordinate(td_codepoint code, size_t* page_x, size_t* page_y, size_t* glyph_index)
{
    if (code > MaxUnicode)
    {
        *page_x = 0;
        *page_y = 0;
        *glyph_index = 0;
        return;
    }

    // Get glyph index
    *glyph_index = code & (GlyphCountPerPage - 1);
    // Remove first byte
    code >>= 8;
    // Get Page Y index
    *page_y = code & (PageCountY - 1);
    // Remove second byte
    code >>= 8;

    // Get one of the first 15 pages or get last page
    *page_x = code <= 15 ? code & (16 - 1) : 16;
}

// ============================================================================
// td_codepoint_iterator
// ============================================================================

td_codepoint_iterator::td_codepoint_iterator(const char* begin, const char* _end)
{
    cursor = begin;
    end = _end;
}

bool td_codepoint_iterator::has_next() const
{
    return cursor < end;
}


bool td_codepoint_iterator::get_next()
{
    if (has_next())
    {
        cursor = td_utf8_codepoint(cursor, &codepoint);
        return true;
    }
    return false;
}

// ============================================================================
// functions
// ============================================================================

namespace td {

    void traverse_glyph(td_font_store* fs, td_glyph* g, td_traverse_func_t func, void* ctx)
    {
        td_vec2 current_point;
        td_vec2 points[3];
       
        for (int i = 0; i < g->vertices_count; i++)
        {
            switch (g->vertices[i].type) {
            case STBTT_vmove:
                points[0].x = g->vertices[i].x;
                points[0].y = g->vertices[i].y;
                current_point = points[0];
                func(ctx, td_path_cmd_MOVE_TO, points);
                break;
            case STBTT_vline:
                points[0].x = g->vertices[i].x;
                points[0].y = g->vertices[i].y;
                current_point = points[0];
                func(ctx, td_path_cmd_LINE_TO, points);
                break;
            case STBTT_vcurve:
                // Quadratic to Cubic bezier
                // P0 = P0
                // P1 = P0 + (2/3)*(P1 - P0)
                // P2 = P2 + (2/3)*(P1 - P2)
                // P3 = P2
                points[0].x = current_point.x  + (2.0f / 3.0f * (g->vertices[i].cx - current_point.x));
                points[0].y = current_point.y  + (2.0f / 3.0f * (g->vertices[i].cy - current_point.y));
                points[1].x = g->vertices[i].x + (2.0f / 3.0f * (g->vertices[i].cx - g->vertices[i].x));
                points[1].y = g->vertices[i].y + (2.0f / 3.0f * (g->vertices[i].cy - g->vertices[i].y));
                points[2].x = g->vertices[i].x;
                points[2].y = g->vertices[i].y;
                current_point = points[2];
                func(ctx, td_path_cmd_CUBIC_TO, points);
                break;
            case STBTT_vcubic:
                points[0].x = g->vertices[i].cx;
                points[0].y = g->vertices[i].cy;
                points[1].x = g->vertices[i].cx1;
                points[1].y = g->vertices[i].cy1;
                points[2].x = g->vertices[i].x;
                points[2].y = g->vertices[i].y;
                current_point = points[2];
                func(ctx, td_path_cmd_CUBIC_TO, points);
                break;
            default:
                TD_ASSERT(0 && "Unreachable");
            }
        }
    }

    // Return text length
    float insert_text_to_path(td_font_store* fs, const char* begin, const char* end, td_vec2 pos, float pixel_size, td_path* path)
    {

        float scale = fs->get_font()->get_scale(pixel_size);

        td_transform transform = td_transform::identity();

        // We need to inverse scale on Y axis to account for the coordinate system 0:0 at top left.
        // True Type font use the opposite direction.
        //   From:                To:
        //           0 --> 1 (x)       (y)
        //           |                 1
        //           v                 ^
        //           1                 |
        //          (y)                0 --> 1 (x)
        //           
        transform.set_scale(scale, -scale);

        td_transform_inserter inserter(path, transform);

        float text_length = 0;

        td_codepoint_iterator it(begin, end);
        while (it.get_next())
        {
            td_glyph* g = fs->get_glyph(it.codepoint);

            traverse_glyph(fs, g, td_transform_inserter::func, &inserter);

            float glyph_advance = g->advance_width * scale;
            pos.x += glyph_advance;
            text_length += glyph_advance;
            inserter.transform.set_translation(pos.x, pos.y);
        }

        return text_length;
    }

    // Return text length
    float insert_text_to_path(td_font_store* fs, const char* begin, td_vec2 pos, float pixel_size, td_path* path)
    {
        return insert_text_to_path(fs, begin, begin + strlen(begin), pos, pixel_size, path);
    }
}