#ifndef TD_TESTS_HPP
#define TD_TESTS_HPP

#include "tendril.hpp"
#include "tendril_internal.hpp"

namespace td
{
	struct tests
	{
		// @TODO test path.
		// @TODO test transform functions.

		static void all()
		{
			vector();
			
			flatten();

			piecewise();
			
			fragment();
			
			subpath();

			arena();
			
			font();

			color();

			u32_byte_operation();
		}

		static void vector()
		{
			// Constructor
			{
				td_vector<int> v;

				TD_ASSERT(v.size() == 0);
				TD_ASSERT(v.capacity() == 0);
			}
			// reserve
			{
				td_vector<float> v;
				v.reserve(16);

				TD_ASSERT(v.size() == 0);
				TD_ASSERT(v.capacity() == 16);
				
				v.reserve(32);

				TD_ASSERT(v.size() == 0);
				TD_ASSERT(v.capacity() == 32);

				v.reserve(64);
				
				TD_ASSERT(v.size() == 0);
				TD_ASSERT(v.capacity() == 64);
			}

			// push_back
			{
				td_vector<int> v;
				for (int i = 0; i < 16; i += 1)
				{
					v.push_back(i);
				}
				TD_ASSERT(v.size() == 16);
				TD_ASSERT(v.capacity() == 16);
			}

			// resize
			{
				td_vector<int> v;
				v.resize(16);
				for (int i = 0; i < 16; i += 1)
				{
					v.push_back(i);
				}
				TD_ASSERT(v.size() == 32);
				TD_ASSERT(v.capacity() == 32);
				v.resize(32);

				TD_ASSERT(v.size() == 32);
				TD_ASSERT(v.capacity() == 32);
			}

			// contains
			{
				td_vector<int> v;
				for (int i = 0; i < 16; i += 1)
				{
					v.push_back(i);
				}
				for (int i = 0; i < 16; i += 1)
				{
					TD_ASSERT(v.contains(i));
				}
			}
			// copy
			{
				td_vector<int> v;
				for (int i = 0; i < 16; i += 1)
				{
					v.push_back(i);
				}

				td_vector<int> copy = v;

				for (int i = 0; i < 16; i += 1)
				{
					TD_ASSERT(v[i] == copy[i]);
				}

				TD_ASSERT(v.data() != copy.data());
				TD_ASSERT(v.size() == copy.size());
			}
			// insert
			{
				{
					td_vector<int> v;
					int values[] = { 0,1,2,3 };
					v.insert(v.begin(), values, td_size(values));

					for (size_t i = 0; i < v.size(); i += 1)
					{
						TD_ASSERT(v[i] == values[i]);
					}
				}

				{
					td_vector<int> v;
					int values[] = { 3,2,1,0 };
					v.insert(v.begin(), 0);
					v.insert(v.begin(), 1);
					v.insert(v.begin(), 2);
					v.insert(v.begin(), 3);

					for (size_t i = 0; i < v.size(); i += 1)
					{
						TD_ASSERT(v[i] == values[i]);
					}
				}
			}

			// erase
			{
				{
					td_vector<int> v;
					int values[] = { 0, 1, 2, 3 };
					for (size_t i = 0; i < td_size(values); i += 1)
					{
						v.push_back(values[i]);
					}
					
					for (size_t i = 0; i < td_size(values); i += 1)
					{
						v.erase(v.begin());
					}

					TD_ASSERT(v.size() == 0);
				}
			}
			// erase range
			{
				{
					td_vector<int> v;
					int values[] = { 0, 1, 2, 3 };
					for (size_t i = 0; i < td_size(values); i += 1)
					{
						v.push_back(values[i]);
					}

					for (size_t i = 0; i < td_size(values); i += 1)
					{
						v.erase(v.begin(), v.begin() + 1);
					}

					TD_ASSERT(v.size() == 0);
				}

				{
					td_vector<int> v;
					int values[] = { 0, 1, 2, 3 };
					for (size_t i = 0; i < td_size(values); i += 1)
					{
						v.push_back(values[i]);
					}

					v.erase(v.begin(), v.end());

					TD_ASSERT(v.size() == 0);
				}
			}
		}

		static void flatten()
		{
			// Fragment a line of length 2.0f into two pieces of line of length 1.0f
			{

				td_path path;
				path.move_to(0.0f, 0.0f);
				path.cubic_to(td_vec2{ 3, 3 }, td_vec2{ 5, 3 }, td_vec2{ 9, 0 });

				td_path flatten_path;
				td::to_flatten_path(path, &flatten_path);

				TD_ASSERT(flatten_path.points.size() > 4);

				for (size_t i = 0; i < flatten_path.cmds.size(); i += 1)
				{
					TD_ASSERT(flatten_path.cmds[i] != td_path_cmd_CUBIC_TO);
				}
			}

		}

		static void piecewise()
		{
			{
				td_path p;
				p.move_to(0.0f, 0.0f);
				p.line_to(10.0f, 0.0f);
				p.line_to(20.0f, 0.0f);

				td_piecewise_path pw;

				td::to_piecewise_path(p, &pw);

				{
					TD_ASSERT(pw.points.size() == p.points.size());
					TD_ASSERT(pw.times.size() == p.points.size());
					TD_ASSERT(pw.times.front() == 0.f);
					TD_ASSERT(pw.times.back() == 20.f);
				}
				// time slice
				{

					float  values[] = { 0.0f, 5.0f, 10.0f, 15.0f, 20.0f };
					size_t expect[] = { 0,    0,    1,     1,     1, };

					for (size_t i = 0; i < td_size(values); i += 1)
					{
						size_t actual = pw.lower_point_index_at_time(values[i]);

						TD_ASSERT(expect[i] == actual);
					}
				}

				
				// Points on straight polyline
				{
					float  values[] = { 0.0f, 5.0f, 10.0f, 15.0f , 20.0f };
					td_vec2 expect[] = { {0, 0}, {5.0f, 0}, {10.f, 0}, {15.f, 0}, {20.f, 0} };

					for (size_t i = 0; i < td_size(values); i += 1)
					{
						td_vec2 actual = pw.point_at_time(values[i]);
						TD_ASSERT(expect[i] == actual);
					}
				}

				// Normals
				{
					float  values[] = { 0.0f, 5.0f, 10.0f, 15.0f , 20.0f };
					td_vec2 unit_y(0.0f, 1.0f);
					td_vec2 expect = unit_y;

					for (size_t i = 0; i < td_size(values); i += 1)
					{
						td_vec2 actual = pw.unit_normal_at_time(values[i]);
						TD_ASSERT(expect == actual);
					}
				}
			}
		}

		static void fragment()
		{
			// Fragment a line of length 2.0f into two pieces of line of length 1.0f
			{

				td_path path;
				path.move_to(0.0f, 0.0f);
				path.line_to(2.0f, 0.0f);

				td_path fragmented_path;

				td::to_fragmented_path(path, &fragmented_path, 1.0f);

				TD_ASSERT(fragmented_path.points.size() == 3);
				
				{

					td_vec2  expect[] = { {0.0f, 0.0f}, {1.0f, 0.0f}, {2.0f, 0.0f} };

					for (size_t i = 0; i < td_size(expect); i += 1)
					{
						TD_ASSERT(expect[i] == fragmented_path.points[i]);
					}
				}
			}

			// Fragment of length 0.5f should remain.
			{

				td_path p;
				p.move_to(0.0f, 0.0f);
				p.line_to(0.5f, 0.0f);

				td_path fragmented_path;

				to_fragmented_path(p, &fragmented_path, 1.0f);

				TD_ASSERT(fragmented_path.points.size() == 2);

				{
					td_vec2  expect[] = { {0.0f, 0.0f}, {0.5f, 0.0f} };

					for (size_t i = 0; i < td_size(expect); i += 1)
					{
						TD_ASSERT(expect[i] == fragmented_path.points[i]);
					}
				}
			}

			// Fragment of length of 2.5f.
			{

				td_path p;
				p.move_to(0.0f, 0.0f);
				p.line_to(2.5f, 0.0f);

				td_path fragmented_path;

				td::to_fragmented_path(p , &fragmented_path, 1.0f);

				TD_ASSERT(fragmented_path.points.size() == 4);

				{

					td_vec2  expect[] = { {0.0f, 0.0f}, {1.0f, 0.0f}, {2.0f, 0.0f}, {2.5f, 0.0f} };

					for (size_t i = 0; i < td_size(expect); i += 1)
					{
						TD_ASSERT(expect[i] == fragmented_path.points[i]);
					}
				}
			}

			// Fragment of length of 2.0f, starting at 0.5.
			{

				td_path p;
				p.move_to(0.5f, 0.0f);
				p.line_to(1.0f, 0.0f);
				p.line_to(2.5f, 0.0f);

				td_path fragmented_path;

				to_fragmented_path(p, &fragmented_path, 1.0f);

				TD_ASSERT(fragmented_path.points.size() == 4);

				{

					td_vec2  expect[] = { {0.5f, 0.0f}, {1.0f, 0.0f}, {2.0f, 0.0f}, {2.5f, 0.0f} };

					for (size_t i = 0; i < td_size(expect); i += 1)
					{
						TD_ASSERT(expect[i] == fragmented_path.points[i]);
					}
				}
			}

			// Fragment a closed shape
			{

				td_path path;
				path.add_rect(0.0f, 0.0f, 100.0f, 10.0f);

				td_path fragmented_path;

				td::to_fragmented_path(path, &fragmented_path, 10.0f);

				
				// Divide each side of the rect by two
				TD_ASSERT(fragmented_path.points.size() == 23);

				TD_ASSERT(path.cmds.back() == td_path_cmd_CLOSE);
				TD_ASSERT(path.points.back() == path.points.front());
				
				TD_ASSERT(fragmented_path.points.back() == fragmented_path.points.front());
				TD_ASSERT(fragmented_path.cmds.back() == td_path_cmd_CLOSE);
			}
		}
		static void subpath()
		{
			{
				td_path path;

				td_subpath_iterator it(path);

				TD_ASSERT(it.subpath.cmds.data == NULL);
				TD_ASSERT(it.subpath.cmds.size == 0);
				TD_ASSERT(it.subpath.points.data == NULL);
				TD_ASSERT(it.subpath.points.size == 0);
				
				bool next = it.get_next();
				TD_ASSERT(!next);

				TD_ASSERT(it.subpath.cmds.data == NULL);
				TD_ASSERT(it.subpath.cmds.size == 0);
				TD_ASSERT(it.subpath.points.data == NULL);
				TD_ASSERT(it.subpath.points.size == 0);
			}

			{
				td_path path;
				path.move_to(0.0f, 0.0f);
				path.line_to(1.0f, 0.0f);

				td_subpath_iterator it(path);

				bool next;
				next = it.get_next();

				TD_ASSERT(next);

				TD_ASSERT(it.subpath.cmds.data != NULL);
				TD_ASSERT(it.subpath.cmds.size == 2);
				TD_ASSERT(it.subpath.cmds[0] == td_path_cmd_MOVE_TO);
				TD_ASSERT(it.subpath.cmds[1] == td_path_cmd_LINE_TO);
				TD_ASSERT(it.subpath.points.data != NULL);
				TD_ASSERT(it.subpath.points.size == 2);
			}
			
			{
				td_path path;
				path.move_to(0.0f, 0.0f);
				path.line_to(1.0f, 0.0f);
				path.move_to(2.0f, 0.0f);
				path.line_to(3.0f, 0.0f);
				path.line_to(4.0f, 0.0f);
				td_subpath_iterator it(path);

				bool next;

				next = it.get_next();
				next = it.get_next();
				TD_ASSERT(next);

				TD_ASSERT(it.subpath.cmds.data != NULL);
				TD_ASSERT(it.subpath.cmds.size == 3);
				TD_ASSERT(it.subpath.cmds[0] == td_path_cmd_MOVE_TO);
				TD_ASSERT(it.subpath.cmds[1] == td_path_cmd_LINE_TO);
				TD_ASSERT(it.subpath.cmds[2] == td_path_cmd_LINE_TO);
				TD_ASSERT(it.subpath.points.data != NULL);
				TD_ASSERT(it.subpath.points.size == 3);
			}
			{
				td_path path;
				path.move_to(0.0f, 0.0f);
				path.line_to(1.0f, 0.0f);
				path.move_to(2.0f, 0.0f);
				path.line_to(3.0f, 0.0f);
				path.close();

				td_subpath_iterator it(path);

				bool next;

				next = it.get_next();
				next = it.get_next();
				TD_ASSERT(next);

				TD_ASSERT(it.subpath.cmds.data != NULL);
				TD_ASSERT(it.subpath.cmds.size == 3);
				TD_ASSERT(it.subpath.cmds[0] == td_path_cmd_MOVE_TO);
				TD_ASSERT(it.subpath.cmds[1] == td_path_cmd_LINE_TO);
				TD_ASSERT(it.subpath.cmds[2] == td_path_cmd_CLOSE);
				TD_ASSERT(it.subpath.points.data != NULL);
				TD_ASSERT(it.subpath.points.size == 3);
			}

			{
				td_path path;
				path.move_to(0.0f, 0.0f);
				path.line_to(1.0f, 0.0f);
				path.move_to(2.0f, 0.0f);
				path.cubic_to(3.0f, 0.0f, 4.0f, 0.0f, 5.0f, 0.0f);

				td_subpath_iterator it(path);

				bool next;

				next = it.get_next();
				next = it.get_next();
				TD_ASSERT(next);

				TD_ASSERT(it.subpath.cmds.data != NULL);
				TD_ASSERT(it.subpath.cmds.size == 2);
				TD_ASSERT(it.subpath.cmds[0] == td_path_cmd_MOVE_TO);
				TD_ASSERT(it.subpath.cmds[1] == td_path_cmd_CUBIC_TO);
				TD_ASSERT(it.subpath.points.data != NULL);
				TD_ASSERT(it.subpath.points.size == 4);
			}

			{
				td_path path;
				path.move_to(0.0f, 0.0f);
				path.line_to(1.0f, 0.0f);
				path.move_to(2.0f, 0.0f);
				path.line_to(3.0f, 0.0f);

				td_subpath_iterator it(path);

				bool next;

				next = it.get_next();
				next = it.get_next();
				TD_ASSERT(next);
				next = it.get_next();
				TD_ASSERT(!next);
				next = it.get_next();
				TD_ASSERT(!next);
			}
		}

		static void arena()
		{
			{
				td_arena a;
			}

			{
				td_arena a{32};
				char* ignore1 = a.zalloc<char>(16);

				TD_ASSERT(a.chunk_used == 1);

				char* ignore2 = a.zalloc<char>(16);

				TD_ASSERT(a.chunk_used == 1);

				char* ignore3 = a.zalloc<char>(16);

				TD_ASSERT(a.chunk_used == 2);

				char* ignore4 = a.zalloc<char>(32);

				TD_ASSERT(a.chunk_used == 3);
				
			}

			// Save/rollback empty arena
			{
				td_arena a{ 32 };

				td_arena_state state = a.save_state();
				a.rollback_state(state);
			}

			// Save/rollback arena with a single bucket
			{
				td_arena a{ 32 };

				TD_ASSERT(a.chunk_used == 0);
				TD_ASSERT(a.chunks.size() == 0);

				td_arena_state state = a.save_state();
				char* ignore1 = a.zalloc<char>(16);

				TD_ASSERT(a.chunk_used == 1);
				TD_ASSERT(a.chunks.size() == 1);

				a.rollback_state(state);

				TD_ASSERT(a.chunk_used == 0);
				TD_ASSERT(a.chunks.size() == 1);
			}

			// Save/rollback, arena with a multiple bucket, restored to zero
			{
				td_arena a{ 32 };

				TD_ASSERT(a.chunk_used == 0);
				TD_ASSERT(a.chunks.size() == 0);

				td_arena_state state = a.save_state();
				
				char* ignore = a.zalloc<char>(16);
				ignore = a.zalloc<char>(32);
				ignore = a.zalloc<char>(64);

				TD_ASSERT(a.chunk_used == 3);
				TD_ASSERT(a.chunks.size() == 3);

				a.rollback_state(state);

				TD_ASSERT(a.chunk_used == 0);
				TD_ASSERT(a.chunks.size() == 3);

				ignore = a.zalloc<char>(16);
				ignore = a.zalloc<char>(32);
				ignore = a.zalloc<char>(64);

				TD_ASSERT(a.chunk_used == 3);
				TD_ASSERT(a.chunks.size() == 3);

				ignore = a.zalloc<char>(64);

				TD_ASSERT(a.chunk_used == 4);
				TD_ASSERT(a.chunks.size() == 4);
			}

			// Save/rollback arena with a multiple bucket, restored to first bucket
			{
				td_arena a{ 32 };

				TD_ASSERT(a.chunk_used == 0);
				TD_ASSERT(a.chunks.size() == 0);

				char* ignore = a.zalloc<char>(16);

				td_arena_state state = a.save_state();

				ignore = a.zalloc<char>(32);
				ignore = a.zalloc<char>(64);

				TD_ASSERT(a.chunk_used == 3);
				TD_ASSERT(a.chunks.size() == 3);

				a.rollback_state(state);

				TD_ASSERT(a.chunk_used == 1);
				TD_ASSERT(a.chunks.size() == 3);

				ignore = a.zalloc<char>(32);
				ignore = a.zalloc<char>(64);

				TD_ASSERT(a.chunk_used == 3);
				TD_ASSERT(a.chunks.size() == 3);

				ignore = a.zalloc<char>(64);

				TD_ASSERT(a.chunk_used == 4);
				TD_ASSERT(a.chunks.size() == 4);
			}
				
		}

		static void font()
		{
			struct item {
				td_codepoint codepoint;
				size_t page_x;
				size_t page_y;
				size_t glyph_index;
			};

			item items[] = {
				// Page 0 (x), Subpage 0 (y)
				{ 0,   0, 0, 0 },
				{ 1,   0, 0, 1 },
				{ 255, 0, 0, 255 },
				// Page 0 (x), Subpage 1 (y)
				{ 256, 0, 1, 0 },
				{ 257, 0, 1, 1 },
				{ 511, 0, 1, 255 },
				// Page 1 (x), Subpage 0 (y)
				{ (256 * 256),       1, 0, 0 },
				{ (256 * 256 + 1),   1, 0, 1 },
				// Page 1 (x), Subpage 1 (y)
				{ (256 * 256 + 256), 1, 1, 0 },
				// Page 15 (x), Subpage 0 (y)
				{ (256 * 256 * 16 - 1), 15, 255, 255 },
				// Page 16 (x), Subpage 0 (y)
				{ (256 * 256 * 16 ), 16, 0, 0 },

				// Max page, Max subpage and max glyph index
				{ ~0u, 0u, 0u, 0u },
			};

			for (const item& i : items)
			{
				size_t page_x;
				size_t page_y;
				size_t glyph_index;

				td_font_store::get_glyph_coordinate(i.codepoint, &page_x, &page_y, &glyph_index);
				TD_ASSERT(page_x == i.page_x);
				TD_ASSERT(page_y == i.page_y);
				TD_ASSERT(glyph_index == i.glyph_index);
			}
		}

		static void color()
		{
			{
				td_rgba32 c(0, 0, 0, 0);

				TD_ASSERT(c.u32 == 0);
				TD_ASSERT(c.r() == 0 && c.g() == 0 && c.b() == 0 && c.a() == 0);
			}
			{
				td_rgba32 c(255, 255, 255, 255);

				TD_ASSERT(c.r() == 255 && c.g() == 255 && c.b() == 255 && c.a() == 255);
			}

			{
				td_rgba32 c(128, 255, 255, 255);

				TD_ASSERT(c.r() == 128 && c.g() == 255 && c.b() == 255 && c.a() == 255);
			}

			{
				td_rgba32 c(0, 0, 0, 0);
				c.set_r(128);

				TD_ASSERT(c.r() == 128 && c.g() == 0 && c.b() == 0 && c.a() == 0);
			}
			{
				td_rgba32 c(0, 0, 0, 0);
				c.set_g(128);

				TD_ASSERT(c.r() == 0 && c.g() == 128 && c.b() == 0 && c.a() == 0);
			}
			{
				td_rgba32 c(0, 0, 0, 0);
				c.set_b(128);

				TD_ASSERT(c.r() == 0 && c.g() == 0 && c.b() == 128 && c.a() == 0);
			}
			{
				td_rgba32 c(0, 0, 0, 0);
				c.set_a(128);

				TD_ASSERT(c.r() == 0 && c.g() == 0 && c.b() == 0 && c.a() == 128);
			}
			{
				td_rgba32 c(0, 0, 0, 0);
				c.set_r(1);
				c.set_g(2);
				c.set_b(3);
				c.set_a(4);

				TD_ASSERT(c.r() == 1 && c.g() == 2 && c.b() == 3 && c.a() == 4);
			}
		}

		static void u32_byte_operation()
		{
			{
				td_rgba32 white{ 255, 255, 255, 255 };
				td_rgba32 black{ 0, 0, 0, 0 };
				td_rgba32 gray{ 128, 128, 128, 128 };

				uint32_t result;
				
				result = td_u32_byte_lerp(black.u32, white.u32, 128);
				
				TD_ASSERT(result == gray.u32);

				result = td_u32_byte_lerp(black.u32, white.u32, 0);
				
				TD_ASSERT(result == black.u32);

				result = td_u32_byte_lerp(black.u32, white.u32, 255);
				
				TD_ASSERT(result == white.u32);

			}
			//td_rgba32::print(td_rgba32{ result });
		}
	};
}

#endif // TD_TESTS_HPP