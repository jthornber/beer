#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>

//----------------------------------------------------------------

namespace beer {
	typedef uint64_t sector_t;
	typedef uint64_t seed_t;

	struct io_range {
		io_range() {
		}

		io_range(sector_t _b, sector_t _e)
			: b(_b),
			  e(_e) {
		}

		sector_t length() const {
			return e - b;
		}

		sector_t b, e;
	};

	unsigned const BYTES_PER_SECTOR = 512;
	unsigned const PAGE_SIZE = 4096;
}

//----------------------------------------------------------------

#endif
