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
	// Forward declaration
	class Target_state;	
	class Checkpointer;
	class Restorer;	
}


struct Cpu_helper : Thread
{
	
	public: 
		enum { STACK_SIZE = 0x2000 };

	Env &_env;
	Rtcr::Checkpointer &_ckpt;
	


	Cpu_helper(Env &env, const char * name, Cpu_session &cpu, Rtcr::Checkpointer &ckpt)
	:
		Thread(env, name, STACK_SIZE, Thread::Location(), Thread::Weight(), cpu),
		_env(env),_ckpt(ckpt)
	{ }
	
		
		
	void entry()
	{				
			{
		  		PROFILE_SCOPE(Thread::name().string(), "red", _ckpt._timer) 
				log(Thread::name().string(), " : _cpu_session=", _cpu_session, " env.cpu()=", &_env.cpu());
				
				_ckpt._prepare_ram_sessions(_ckpt._state._stored_ram_sessions, _ckpt._child.custom_services().ram_root->session_infos());

				//_prepare_pd_sessions(_state._stored_pd_sessions, _child.custom_services().pd_root->session_infos());
				
			}
					
	}
};

