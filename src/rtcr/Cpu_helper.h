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

namespace Genode {

	static inline void print(Output &out, Affinity::Location location)
	{
		print(out, location.xpos(), ",", location.ypos());
	}
}


struct Cpu_helper : Thread
{
	
	
	
	
	private: 
		enum { STACK_SIZE = 0x2000 };
		Env &_env;
		const char * _name;
		Rtcr::Checkpointer &_ckpt;
		Affinity::Location const _location;
		int threadType;
		
	
	public:
	
	

	Cpu_helper(Env &env, const char * name, Cpu_session &cpu, Rtcr::Checkpointer &ckpt, Location location, int threadType)
	:
		Thread(env, name, STACK_SIZE, location, Thread::Weight(), cpu),
		_env(env), _name(name), _ckpt(ckpt), _location(location), threadType(threadType)
	{ }
	
		
		
	void entry()
	{				
			{	
		  		PROFILE_SCOPE(_name, "red", _ckpt._timer) 
				log(_name, " : _cpu_session=", _cpu_session, " env.cpu()=", &_env.cpu(), " Location=", _location, " Type=", threadType);
				if(threadType == 0){
					checkpoint_main();
				}else if(threadType == 1){
					prepare_ram();
				}else if(threadType == 2){
					prepare_pd_cpu_rm_log_timer();
				}else if(threadType == 3){					
					checkpoint_dataspaces();	
				}else if(threadType == 5){
					checkpoint_main_no_threads();
				}
			}
					
	}
	
	void prepare_ram()
	{
		
		{
			PROFILE_SCOPE("_prepare_ram_sessions()", "green", _ckpt._timer)
			_ckpt._prepare_ram_sessions(_ckpt._state._stored_ram_sessions, _ckpt._child.custom_services().ram_root->session_infos());	
		}
		

	}

	void prepare_pd_cpu_rm_log_timer()
	{
		

		{
			PROFILE_SCOPE("_prepare_pd_sessions()", "green", _ckpt._timer)
			_ckpt._prepare_pd_sessions(_ckpt._state._stored_pd_sessions, _ckpt._child.custom_services().pd_root->session_infos());
		}
		
		{
			PROFILE_SCOPE("_prepare_cpu_sessions()", "green", _ckpt._timer)
			_ckpt._prepare_cpu_sessions(_ckpt._state._stored_cpu_sessions, _ckpt._child.custom_services().cpu_root->session_infos());
		}

		if(_ckpt._child.custom_services().rm_root)
		{
			PROFILE_SCOPE("_prepare_rm_sessions()", "green", _ckpt._timer)
			_ckpt._prepare_rm_sessions(_ckpt._state._stored_rm_sessions, _ckpt._child.custom_services().rm_root->session_infos());
		}
		
		if(_ckpt._child.custom_services().log_root)
		{
			PROFILE_SCOPE("_prepare_log_sessions()", "green", _ckpt._timer)
			_ckpt._prepare_log_sessions(_ckpt._state._stored_log_sessions, _ckpt._child.custom_services().log_root->session_infos());
		}
		
		if(_ckpt._child.custom_services().timer_root)
		{
			PROFILE_SCOPE("_prepare_timer_sessions()", "green", _ckpt._timer)
			_ckpt._prepare_timer_sessions(_ckpt._state._stored_timer_sessions, _ckpt._child.custom_services().timer_root->session_infos());
		}
	}
	
	void waitForData()
	{
		while(_ckpt._dataspacethreads_needed && _ckpt._memory_not_managed_fifo.empty() && _ckpt._memory_managed_fifo.empty()){
		
			_ckpt._timer.usleep(10);		
		}
	}
	
	void checkpoint_dataspaces()
	{
		while(_ckpt._dataspacethreads_needed || !_ckpt._memory_not_managed_fifo.empty() || !_ckpt._memory_managed_fifo.empty()){
		
			waitForData();
			
			if(!_ckpt._memory_not_managed_fifo.empty()){
				Rtcr::Dataspace_translation_info *memory_info = _ckpt._memory_not_managed_fifo.dequeue();
				const char * tmp[] = {  _name };
				Genode::log("dataspace args dequeued from ", tmp[0]);
				{					
  					PROFILE_SCOPE((_name), "purple", _ckpt._timer)

					_ckpt._checkpoint_dataspace_content(memory_info->ckpt_ds_cap, memory_info->resto_ds_cap, 0, memory_info->size);	
	
				}							
			}
			if(!_ckpt._memory_managed_fifo.empty()){
				Rtcr::Dataspace_translation_info *memory_info = _ckpt._memory_managed_fifo.dequeue();
				Rtcr::Simplified_managed_dataspace_info::Simplified_designated_ds_info *sdd_info = _ckpt._sdd_fifo.dequeue();
				const char * tmp[] = {  _name };
				Genode::log("dataspace args dequeued from ", tmp[0]);
				{					
  					PROFILE_SCOPE((_name), "blue", _ckpt._timer)

					_ckpt._checkpoint_dataspace_content(memory_info->ckpt_ds_cap, sdd_info->dataspace_cap, sdd_info->addr, sdd_info->size);
	
				}							
			}

		}	
	}
	
	void checkpoint_main(){
	
		using Genode::log;

		//if(verbose_debug) Genode::log("Ckpt::\033[33m", __func__, "\033[0m()");

		// Pause child
		{		
		PROFILE_SCOPE("child pause", "brown", _ckpt._timer)    //scope profiling open
		_ckpt._child.pause();	
		}
		
		{
	  	PROFILE_SCOPE("checkpoint()", "yellow", _ckpt._timer)    //scope profiling open
		
		Affinity::Space affinity_space = _ckpt._state._env.cpu().affinity_space();
		
		//Thread_capability thread = _state._env.cpu().create_thread(_state._env.pd_session_cap(),"thread classless",Thread::Location() , Thread::Weight(), 0);	
			
		//{
		//	PROFILE_SCOPE("Threads", "red", _ckpt._timer) 	
		Genode::log(" Space Width = ", affinity_space.width());
		Cpu_helper thread0(_ckpt._state._env, "Thread prepare_ram entry", _ckpt._state._env.cpu(), _ckpt, affinity_space.location_of_index(1), 1);
		Cpu_helper thread2(_ckpt._state._env, "Thread prepare_pd_cpu_rm_log_timer entry", _ckpt._state._env.cpu(), _ckpt, affinity_space.location_of_index(2), 2);
			
		
		//}
		
		{
	  		PROFILE_SCOPE("_create_kcap_mappings()", "green", _ckpt._timer) 	

		// Create mapping of badge to kcap
		_ckpt._kcap_mappings = _ckpt._create_kcap_mappings();
		}	


		// Create a list of region map dataspaces which are known to child
		// These dataspaces are ignored when creating copy dataspaces
		// For new intercepted sessions which trade managed dataspaces between child and themselves,
		// the region map dataspace capability has to be inserted into this list
		Genode::List<Rtcr::Rm_session_component> *rm_sessions = nullptr;
		if(_ckpt._child.custom_services().rm_root) rm_sessions = &_ckpt._child.custom_services().rm_root->session_infos();
		{
	  		PROFILE_SCOPE("_create_region_map_dataspaces_list()", "green", _ckpt._timer)

		_ckpt._region_maps = _ckpt._create_region_map_dataspaces_list(_ckpt._child.custom_services().pd_root->session_infos(), rm_sessions);
		}


		{
		  	PROFILE_SCOPE("Thread prepare_ram start", "red", _ckpt._timer) 
			thread0.start();
		}

		
		{
		  	PROFILE_SCOPE("Threads prepare_pd_cpu_rm_log_timer start", "red", _ckpt._timer) 
			thread2.start();
		}		
		
		{
		  	PROFILE_SCOPE("Thread prepare_ram join", "red", _ckpt._timer) 
			thread0.join();
		}
		{
		  	PROFILE_SCOPE("Thread prepare_pd_cpu_rm_log_timer join", "red", _ckpt._timer) 
			thread2.join();
		}
	
		// Create a list of managed dataspaces
		{
			PROFILE_SCOPE("_create_managed_dataspace_list()", "green", _ckpt._timer)

			_ckpt._create_managed_dataspace_list(_ckpt._child.custom_services().ram_root->session_infos());
		}

		// Detach all designated dataspaces
		{
			PROFILE_SCOPE("_detach_designated_dataspaces()", "green", _ckpt._timer)
			
			_ckpt._detach_designated_dataspaces(_ckpt._child.custom_services().ram_root->session_infos());		
		}

		Cpu_helper thread_ds1(_ckpt._state._env, "Thread ds1", _ckpt._state._env.cpu(), _ckpt, affinity_space.location_of_index(1), 3);
		Cpu_helper thread_ds2(_ckpt._state._env, "Thread ds2 ", _ckpt._state._env.cpu(), _ckpt, affinity_space.location_of_index(2), 3);
		thread_ds1.start();
		thread_ds2.start();
		

		// Copy child dataspaces' content and to stored dataspaces' content
		{
			PROFILE_SCOPE("_checkpoint_dataspaces()", "green", _ckpt._timer)
			
			_ckpt._checkpoint_dataspaces();	
		}
		
		//wait for dataspaces to be copied		
		thread_ds1.join();
		thread_ds2.join();

		// Clean up
		{
			PROFILE_SCOPE("_destroy_lists", "green", _ckpt._timer)
			
			_ckpt._destroy_list(_ckpt._kcap_mappings);
			_ckpt._destroy_list(_ckpt._dataspace_translations);
			_ckpt._destroy_list(_ckpt._region_maps);
			_ckpt._destroy_list(_ckpt._managed_dataspaces);		
		}

		
		//unexplained time loss??	

		}  //scope profiling close

		//if(true) Genode::log(_ckpt._child);
		//if(true) Genode::log(_ckpt._state);

		// Resume child
		{		
		PROFILE_SCOPE("child resume", "brown", _ckpt._timer)    //scope profiling open
		_ckpt._child.resume();	
		}
		
	}

	void checkpoint_main_no_threads(){

		

		// Pause child
		{		
		PROFILE_SCOPE("child pause", "brown", _ckpt._timer)    //scope profiling open
		_ckpt._child.pause();	
		}
		{
	  		PROFILE_SCOPE("checkpoint()", "yellow", _ckpt._timer)    //scope profiling open
	  		
		
		{
	  		PROFILE_SCOPE("_create_kcap_mappings()", "green", _ckpt._timer) 	

		// Create mapping of badge to kcap
		_ckpt._kcap_mappings = _ckpt._create_kcap_mappings();
		}	


		// Create a list of region map dataspaces which are known to child
		// These dataspaces are ignored when creating copy dataspaces
		// For new intercepted sessions which trade managed dataspaces between child and themselves,
		// the region map dataspace capability has to be inserted into this list
		Genode::List<Rtcr::Rm_session_component> *rm_sessions = nullptr;
		if(_ckpt._child.custom_services().rm_root) rm_sessions = &_ckpt._child.custom_services().rm_root->session_infos();
		{
	  		PROFILE_SCOPE("_create_region_map_dataspaces_list()", "green", _ckpt._timer)

		_ckpt._region_maps = _ckpt._create_region_map_dataspaces_list(_ckpt._child.custom_services().pd_root->session_infos(), rm_sessions);
		}

	
		// Prepare state lists
		// implicitly _copy_dataspaces modified with the child's currently known dataspaces and copy dataspaces
		{
			PROFILE_SCOPE("_prepare_ram_sessions()", "green", _ckpt._timer)

		_ckpt._prepare_ram_sessions(_ckpt._state._stored_ram_sessions, _ckpt._child.custom_services().ram_root->session_infos());
		}
		{
			PROFILE_SCOPE("_prepare_pd_sessions()", "green", _ckpt._timer)

		_ckpt._prepare_pd_sessions(_ckpt._state._stored_pd_sessions, _ckpt._child.custom_services().pd_root->session_infos());
		}
		{
			PROFILE_SCOPE("_prepare_cpu_sessions()", "green", _ckpt._timer)

		_ckpt._prepare_cpu_sessions(_ckpt._state._stored_cpu_sessions, _ckpt._child.custom_services().cpu_root->session_infos());
		}
		
		if(_ckpt._child.custom_services().rm_root)
			{
			PROFILE_SCOPE("_prepare_rm_sessions()", "green", _ckpt._timer)

			_ckpt._prepare_rm_sessions(_ckpt._state._stored_rm_sessions, _ckpt._child.custom_services().rm_root->session_infos());
			}
			
		if(_ckpt._child.custom_services().log_root)
			{
			PROFILE_SCOPE("_prepare_log_sessions()", "green", _ckpt._timer)

			_ckpt._prepare_log_sessions(_ckpt._state._stored_log_sessions, _ckpt._child.custom_services().log_root->session_infos());
			}
			
		if(_ckpt._child.custom_services().timer_root)
			{
			PROFILE_SCOPE("_prepare_timer_sessions()", "green", _ckpt._timer)

			_ckpt._prepare_timer_sessions(_ckpt._state._stored_timer_sessions, _ckpt._child.custom_services().timer_root->session_infos());
			}
			

		// Create a list of managed dataspaces
		{
			PROFILE_SCOPE("_create_managed_dataspace_list()", "green", _ckpt._timer)

			_ckpt._create_managed_dataspace_list(_ckpt._child.custom_services().ram_root->session_infos());
		}
		
		
		// Detach all designated dataspaces
		{
			PROFILE_SCOPE("_detach_designated_dataspaces()", "green", _ckpt._timer)
			
			_ckpt._detach_designated_dataspaces(_ckpt._child.custom_services().ram_root->session_infos());		
		}
		

		// Copy child dataspaces' content and to stored dataspaces' content
		{
			PROFILE_SCOPE("_checkpoint_dataspaces()", "green",_ckpt. _timer)
			
			_ckpt._checkpoint_dataspaces();	
		}
			

		//if(true) Genode::log(_child);
		//if(true) Genode::log(_state);

		// Clean up
		{
			PROFILE_SCOPE("_destroy_lists", "green", _ckpt._timer)
			
			_ckpt._destroy_list(_ckpt._kcap_mappings);
			_ckpt._destroy_list(_ckpt._dataspace_translations);
			_ckpt._destroy_list(_ckpt._region_maps);
			_ckpt._destroy_list(_ckpt._managed_dataspaces);		
		}
		

		}  //scope profiling close

		// Resume child
		{		
		PROFILE_SCOPE("child resume", "brown", _ckpt._timer)    //scope profiling open
		_ckpt._child.resume();	
		}
	}

};

