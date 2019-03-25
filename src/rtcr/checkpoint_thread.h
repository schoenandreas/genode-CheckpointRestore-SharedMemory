/*
 * \brief  Checkpoint Thread
 * \author Andreas Schoen
 * \date   2019-03-24
 */

#ifndef _RTCR_CHECKPOINT_THREAD_H_
#define _RTCR_CHECKPOINT_THREAD_H_


/* Rtcr includes */
#include "target_state.h"
#include "target_child.h"
#include "checkpointer.h"

/* Profiler includes */
#include "util/profiler.h"

using namespace Genode;

namespace Rtcr {
	// Forward declaration
	class Target_state;	
	class Checkpointer;
	class Restorer;	
	class Checkpoint_thread;
}


struct Rtcr::Checkpoint_thread : Thread
{	
	private: 

		enum { STACK_SIZE = 0x2000 };
		Env &_env;
		const char * _name;
		Rtcr::Checkpointer &_ckpt;
		Affinity::Location const _location;
		int _threadType;


		void entry();
		
		void _prepare_ram();

		void _prepare_pd_cpu_rm_log_timer();
	
		void _waitForData();
	
		void _checkpoint_fifo_dataspaces();
	
		void _checkpoint_main();

		void _checkpoint_main_no_threads();

	
	public:
	
		Checkpoint_thread(Env &env, const char * name, Cpu_session &cpu, Rtcr::Checkpointer &ckpt, Location location, int threadType);
		~Checkpoint_thread();
		
};
#endif /* _RTCR_CHECKPOINT_THREAD_H_ */
