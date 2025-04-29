#ifndef TD_EFFECTS_PERIODIC_HPP
#define TD_EFFECTS_PERIODIC_HPP

#include "app.hpp"
#include "effects.hpp"

struct periodic : td_effect
{
	bool enabled = false;
	float period = 0;
	float period_increment = 0.05f;
	float amplitude = 5.0f;

	td_path result_path;

	periodic() : td_effect{ this, (td_effect_funct_t)apply }
	{
	}

	static const td_path* apply(periodic* w, const td_path* path)
	{
		if (!w->enabled)
		{
			return path;
		}

		w->result_path = *path;

		for (size_t i = 0; i < w->result_path.points.size(); i += 1)
		{
			td_vec2 p = w->result_path.points[i];
			w->result_path.points[i] = td_vec2(p.x ,  p.y + (sinf(p.x + w->period) * w->amplitude));
		}

		w->period += w->period_increment;

		return &w->result_path;
	}
};

#endif // TD_EFFECTS_PERIODIC_HPP