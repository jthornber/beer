#include "engine.h"

#include <fcntl.h>
#include <iostream>
#include <linux/fs.h>
#include <memory>
#include <random>
#include <stdexcept>
#include <stdint.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

using namespace beer;
using namespace std;

//----------------------------------------------------------------

namespace {
	sector_t k(sector_t n) {
		return n * 2;
	}

	sector_t meg(sector_t n) {
		return n * 2 * 1024;
	}

	sector_t gig(sector_t n) {
		return 1024 * meg(n);
	}

	sector_t dev_size(string const &path) {
		struct stat info;

		int r = ::stat(path.c_str(), &info);
		if (r)
			throw runtime_error("Couldn't stat dev path");

		if (S_ISREG(info.st_mode) && info.st_size)
			return info.st_size / BYTES_PER_SECTOR;

		else if (S_ISBLK(info.st_mode)) {
			// To get the size of a block device we need to
			// open it, and then make an ioctl call.
			int fd = ::open(path.c_str(), O_RDONLY);
			if (fd < 0)
				throw runtime_error("couldn't open block device to ascertain size");

			sector_t size;
			r = ::ioctl(fd, BLKGETSIZE64, &size);
			if (r) {
				::close(fd);
				throw runtime_error("ioctl BLKGETSIZE64 failed");
			}
			::close(fd);
			return size / BYTES_PER_SECTOR;

		} else
			// FIXME: needs a better message
			throw runtime_error("bad path");
	}

	class io_load {
	public:
		virtual ~io_load() {}
		virtual void operator()(engine &e, unsigned count) = 0;
	};

	class random_read : public io_load {
	public:
		random_read(io_range const &region, sector_t io_size)
			: region_(region),
			  io_size_(io_size),
			  noise_(region_.b, region_.e - io_size_) {
		}

		virtual void operator()(engine &e, unsigned count) {
			for (unsigned n = 0; n < count; n++) {
				sector_t b = noise_(gen_);
				e.read(io_range(b, b + io_size_));
			}
		}

	private:
		io_range region_;
		sector_t io_size_;
		linear_congruential_engine<sector_t, 16807, 0, (1ull << 31) - 1> gen_;
		uniform_int_distribution<sector_t> noise_;
	};

	class random_write : public io_load {
	public:
		random_write(io_range const &region, sector_t io_size)
			: region_(region),
			  io_size_(io_size),
			  noise_(region_.b, region_.e - io_size_) {
		}

		virtual void operator()(engine &e, unsigned count) {

			for (unsigned n = 0; n < count; n++) {
				sector_t b = noise_(gen_);
				e.write(io_range(b, b + io_size_), 0);
			}
		}

	private:
		io_range region_;
		sector_t io_size_;
		linear_congruential_engine<sector_t, 16807, 0, (1ull << 31) - 1> gen_;
		uniform_int_distribution<sector_t> noise_;
	};

	class blend : public io_load {
	public:
		blend(io_load &lhs, io_load &rhs, unsigned lhs_percent)
			: lhs_(lhs),
			  rhs_(rhs),
			  lhs_percent_(lhs_percent),
			  noise_(0, 100) {
		}

		virtual void operator()(engine &e, unsigned count) {
			for (unsigned i = 0; i < count; i++) {
				if (noise_(gen_) < lhs_percent_)
					lhs_(e, 1);
				else
					rhs_(e, 1);
			}
		}

	private:
		io_load &lhs_;
		io_load &rhs_;
		unsigned lhs_percent_;
		linear_congruential_engine<sector_t, 16807, 0, (1ull << 31) - 1> gen_;
		uniform_int_distribution<sector_t> noise_;
	};

	void latency_benchmark(engine &e) {
		random_write io(io_range(0, meg(10)), meg(4));

		for (unsigned loop = 0; loop < 2500; loop++) {
			io(e, 1);
			e.wait_all();
		}
	}

	void hotspot_benchmark(engine &e) {
		random_write background(io_range(0, gig(1)), k(256));
		random_write foreground(io_range(meg(100), meg(110)), k(256));
		blend io(background, foreground, 25);

		for(unsigned i = 0; i < 200; i++) {
			io(e, 100);
			e.wait_all();
		}
	}
}

//----------------------------------------------------------------

int main(int argc, char **argv)
{
	if (argc != 2) {
		cerr << "Usage: beer <device path>\n";
		exit(1);
	}

	string dev(argv[1]);
	engine e(dev.c_str(), 4096);

	//latency_benchmark(e);
	hotspot_benchmark(e);

	return 0;
}

//----------------------------------------------------------------
