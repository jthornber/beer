#include "seed_map.h"

using namespace beer;

//----------------------------------------------------------------

seed_map::seed_map(sector_t dev_size)
	: seeds_(dev_size)
{
}

void
seed_map::set(io_range const &loc, seed_t seed)
{
	for (sector_t s = loc.b; s != loc.e; s++)
		seeds_[s] = boost::optional<seed_t>(seed++);
}

boost::optional<seed_t>
seed_map::get(sector_t where) const
{
	return seeds_[where];
}

//----------------------------------------------------------------
