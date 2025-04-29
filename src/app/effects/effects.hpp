#ifndef TD_EFFECTS_HPP
#define TD_EFFECTS_HPP

#include "app.hpp"

typedef td_path* (*td_effect_funct_t)(void* effect, td_path* path);

struct td_effect
{
	void* ctx;
	td_effect_funct_t func;

	td_effect(void* _ctx, td_effect_funct_t _func) : ctx(_ctx), func(_func) {}
};

namespace td
{
	static td_path* apply_effect(td_effect* effect, td_path* path)
	{
		return effect->func(effect, path);
	}
};

#endif // TD_EFFECTS_HPP