#include "engine.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/fs.h>
#include <sstream>
#include <stddef.h>
#include <stdexcept>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

using namespace beer;
using namespace std;

//----------------------------------------------------------------

/**
 * container_of - cast a member of a structure out to the containing structure
 * @ptr:	the pointer to the member.
 * @type:	the type of the container struct this is embedded in.
 * @member:	the name of the member within the struct.
 *
 */
#define container_of(ptr, type, member) ({			\
	const decltype( ((type *)0)->member ) *__mptr = (ptr);	\
	(type *)( (char *)__mptr - offsetof(type,member) );})

namespace {
	int open_fd(string const &path) {
		int r = ::open(path.c_str(), O_RDWR | O_DIRECT);
		if (r < 0)
			throw runtime_error("couldn't open device/file\n");

		return r;
	}
}

//----------------------------------------------------------------

engine::engine(string const &dev_path, unsigned nr_ops)
	: fd_(open_fd(dev_path)),
	  aio_context_(0),
	  events_(nr_ops),
	  ops_(nr_ops)
{
	int r = io_setup(nr_ops, &aio_context_);
	if (r < 0)
		throw std::runtime_error("io_setup failed");

	// set up the free list of ops
	for (unsigned i = 0; i < ops_.size(); i++)
		free_ops_.push_back(ops_[i]);
}

engine::~engine()
{
	::close(fd_);
}

// FIXME: remove explicit try/catches

void
engine::read(io_range const &loc)
{
	io_op *op = alloc_op(OP_READ, loc);
	try {
		issue_io(op);

	} catch (...) {
		free_op(op);
		throw;
	}
}

void
engine::write(io_range const &loc, seed_t s)
{
	io_op *op = alloc_op(OP_WRITE, loc);
	try {
		issue_io(op);
	} catch (...) {
		free_op(op);
		throw;
	}
}

void
engine::discard(io_range const &loc)
{

}

void
engine::wait_all()
{
	while (pending_ops_.size())
		wait_one();
}

void
engine::wait_one()
{
	int r;
	unsigned i;

	if (!pending_ops_.size())
		throw runtime_error("no pending ops to wait upon");

	r = io_getevents(aio_context_, 1, pending_ops_.size(), &events_[0], NULL);
	if (r < 0) {
		std::ostringstream out;
		out << "io_getevents failed: " << r;
		throw std::runtime_error(out.str());
	}

	for (i = 0; i < static_cast<unsigned>(r); i++) {
		io_event const &e = events_[i];
		io_op *op = container_of(e.obj, io_op, control_);

		if (e.res < 0)
			complete_io(op, e.res);

		else if (e.res == op->loc_.length() * BYTES_PER_SECTOR)
			complete_io(op, 0);

		else {
			std::ostringstream out;
#if 0
			out << "incomplete io for block " << b->index_
			    << ", e.res = " << e.res
			    << ", e.res2 = " << e.res2
			    << ", offset = " << op->control_.u.c.offset
			    << ", nbytes = " << op->control_.u.c.nbytes;
#endif
			throw std::runtime_error(out.str());
		}
	}
}

void
engine::issue_io(io_op *op)
{
	iocb *cb = &op->control_;
	memset(cb, 0, sizeof(*cb));
	cb->aio_fildes = fd_;
	cb->u.c.buf = op->data_;

	// FIXME: change to * BYTES_PER_SECTOR and let the compiler optimise
	cb->u.c.offset = op->loc_.b * BYTES_PER_SECTOR;
	cb->u.c.nbytes = op->loc_.length() * BYTES_PER_SECTOR;
	cb->aio_lio_opcode = (op->op_ == OP_READ) ? IO_CMD_PREAD : IO_CMD_PWRITE;

	int r = io_submit(aio_context_, 1, &cb);
	if (r != 1) {
		throw runtime_error("io_submit failed\n");
	} else
		pending_ops_.push_back(*op);
}

void
engine::complete_io(io_op *op, int err)
{
	// FIXME: stop ignoring err

	pending_ops_.erase(pending_ops_.iterator_to(*op));
	free_op(op);
}

io_op *
engine::alloc_op(io_type t, io_range const &loc)
{
	while (!free_ops_.size()) {
		cerr << "out of ops, waiting for an io to complete\n";
		wait_one();
	}

	io_op *op = &free_ops_.front();
	free_ops_.erase(free_ops_.iterator_to(*op));;

	int r = posix_memalign(&op->data_, PAGE_SIZE, loc.length() * BYTES_PER_SECTOR);
	if (r) {
		free_ops_.push_front(*op);
		throw std::runtime_error("Couldn't allocate data buffer");
	}

	op->op_ = t;
	op->loc_ = loc;

	return op;
}

void
engine::free_op(io_op *op)
{
	free(op->data_);
	free_ops_.push_front(*op);
}

//----------------------------------------------------------------
