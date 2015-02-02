#ifndef SECTOR_MAP_H
#define SECTOR_MAP_H

#include "types.h"

#include <boost/optional.hpp>
#include <vector>

//----------------------------------------------------------------

namespace beer {
	// Keeps track of the data in each sector
	class seed_map {
	public:
		seed_map(sector_t dev_size);

		void set(io_range const &loc, seed_t seed);
		boost::optional<seed_t> get(sector_t where) const;

	private:
		std::vector<boost::optional<seed_t> > seeds_;
	};
}

//----------------------------------------------------------------

#endif
