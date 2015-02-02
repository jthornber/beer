#ifndef IO_ENGINE_H
#define IO_ENGINE_H

#include "seed_map.h"
#include "types.h"

#include <boost/intrusive/list.hpp>
#include <deque>
#include <libaio.h>
#include <list>
#include <memory>
#include <string>
#include <vector>

//----------------------------------------------------------------

namespace beer {
	struct io_stats {
	};

	enum io_type {
		OP_READ,
		OP_WRITE,
	};

	struct io_op {
		boost::intrusive::list_member_hook<> member_hook_;

		void *data_;
		iocb control_;

		io_type op_;
		io_range loc_;
	};

	//--------------------------------

	class engine {
	public:
		engine(std::string const &dev_path, unsigned nr_ops);
		~engine();

		void read(io_range const &loc);
		void write(io_range const &loc, seed_t s);
		void discard(io_range const &loc);

		void wait_all();

		// FIXME: replace with an interface that (de)registers
		// stats, so the caller can control lifetime.
		// io_stats get_stats();
		// io_stats clear_stats();

	private:
		io_op *alloc_op(io_type t, io_range const &loc);
		void free_op(io_op *op);

		void wait_one();
		void issue_io(io_op *op);
		void complete_io(io_op *op, int err);

		int fd_;	// FIXME: wrap so auto closes
		io_context_t aio_context_;
		std::vector<io_event> events_;
		std::vector<io_op> ops_;

		typedef boost::intrusive::list<io_op,
					       boost::intrusive::member_hook<io_op,
									     boost::intrusive::list_member_hook<>,
							   &io_op::member_hook_>
					       > op_list;
		op_list free_ops_;
		op_list pending_ops_;

		std::unique_ptr<seed_map> seeds_;
	};
}

//----------------------------------------------------------------

#endif
