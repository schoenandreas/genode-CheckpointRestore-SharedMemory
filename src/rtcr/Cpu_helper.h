#include <base/log.h>
#include <base/thread.h>
#include <base/component.h>
#include <base/heap.h>
#include <base/attached_rom_dataspace.h>
#include <base/printf.h>
#include <util/volatile_object.h>
#include <cpu_session/connection.h>
#include <cpu_thread/client.h>


using namespace Genode;

namespace Rtcr {
	class Target_state;

	// Forward declaration
	class Checkpointer;
	class Restorer;
	
}


struct Cpu_helper : Thread
{

	
	public: 
		enum { STACK_SIZE = 0x2000 };

	Env &_env;
	Allocator &_alloc;
	Rtcr::Target_child &_child;
	Rtcr::Target_state &_state;
	Timer::Connection &_timer;
	Rtcr::Checkpointer &_ckpt;
	


	Cpu_helper(Env &env, const char * name, Cpu_session &cpu, Allocator &alloc, Rtcr::Target_child &child, Rtcr::Target_state &state, Timer::Connection &timer, Rtcr::Checkpointer &ckpt )
	:
		Thread(env, name, STACK_SIZE, Thread::Location(), Thread::Weight(), cpu),
		_env(env),_alloc(alloc),_child(child), _state(state),_timer(timer),_ckpt(ckpt)
	{ }
	
		
		
	void entry()
	{		


		//int i = 10;
		//while(i>1){		
			{
		  		PROFILE_SCOPE(Thread::name().string(), "red", _timer) 
				log(Thread::name().string(), " : _cpu_session=", _cpu_session, " env.cpu()=", &_env.cpu());
				
				_ckpt._prepare_ram_sessions(_state._stored_ram_sessions, _child.custom_services().ram_root->session_infos());   //private error

				//_prepare_pd_sessions(_state._stored_pd_sessions, _child.custom_services().pd_root->session_infos());
				
				//i--;
			}
			//_timer.msleep(10);
		//}
			
		
	}
};

