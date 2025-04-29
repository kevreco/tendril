#ifndef TD_EFFECTS_WOBBLER_HPP
#define TD_EFFECTS_WOBBLER_HPP

#include "app.hpp"
#include "effects.hpp"

struct wobbler : td_effect
{
	bool enabled = false;
	size_t counter = 0;
	size_t frequency = 12;
	std::vector<td_vec2> delta_points;
	td_path wobble_path;

	wobbler() : td_effect{ this, (td_effect_funct_t)apply }
	{
	}

	static const td_path* apply(wobbler* w, const td_path* path)
	{
		if (!w->enabled)
		{
			return path;
		}
		if (path->points.size() > w->delta_points.size())
		{
			w->delta_points.resize(path->points.size());
		}
		w->wobble_path = *path;

		if (w->counter == 0)
		{
			w->counter = w->frequency;


			td::randomize(&w->delta_points, -3, 3);
		}

		for (size_t i = 0; i < w->wobble_path.points.size(); i += 1)
		{
			w->wobble_path.points[i] += w->delta_points[i];
		}

		w->counter -= 1;

		return &w->wobble_path;
	}
};

#endif // TD_EFFECTS_WOBBLER_HPP