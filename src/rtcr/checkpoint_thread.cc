/*
 * \brief  Checkpoint Thread
 * \author Andreas Schoen
 * \date   2019-03-24
 */


#include "checkpoint_thread.h"



using namespace Genode;


namespace Genode {
	// Function for logging the Location of threads
	static inline void print(Output &out, Affinity::Location location)
	{
		print(out, location.xpos(), ",", location.ypos());
	}
}


Rtcr::Checkpoint_thread::Checkpoint_thread(Env &env, const char * name, Cpu_session &cpu, Rtcr::Checkpointer &ckpt, Location location, int threadType)
:
	Thread(env, name, STACK_SIZE, location, Thread::Weight(), cpu),
	_env(env), _name(name), _ckpt(ckpt), _location(location), _threadType(threadType)
{ }

Rtcr::Checkpoint_thread::~Checkpoint_thread()
{ }

		
void Rtcr::Checkpoint_thread::entry()
{				
	{	
	  	PROFILE_SCOPE(_name, "red", _ckpt._timer) 
		log(_name, " : _cpu_session=", _cpu_session, " env.cpu()=", &_env.cpu(), " Location=", _location, " Type=", _threadType);
		// Choose thread behaviour based on _threadType
		if(_threadType == 1){
			_checkpoint_main();
		}else if(_threadType == 2){
			_prepare_ram();
		}else if(_threadType == 3){
			_prepare_pd_cpu_rm_log_timer();
		}else if(_threadType == 4){					
			_checkpoint_fifo_dataspaces();	
		}else if(_threadType == 0){
			_checkpoint_main_no_threads();
		}else{
			Genode::warning("Unknown _threadType! Execution terminated without effect");
		}
	}				
}

// Prepare state ram_session list
void Rtcr::Checkpoint_thread::_prepare_ram()
{	
	{
		PROFILE_SCOPE("_prepare_ram_sessions()", "green", _ckpt._timer)
		_ckpt._prepare_ram_sessions(_ckpt._state._stored_ram_sessions, _ckpt._child.custom_services().ram_root->session_infos());	
	}	
}

// Prepare other state lists
void Rtcr::Checkpoint_thread::_prepare_pd_cpu_rm_log_timer()
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
// Wait while fifo is empty until data is provided or thread is not needed anymore because all dataspaces are checkpointed
void Rtcr::Checkpoint_thread::_waitForData()
{
	while(_ckpt._dataspacethreads_needed && _ckpt._memory_not_managed_fifo.empty() && _ckpt._memory_managed_fifo.empty()){
	
		_ckpt._timer.usleep(10);		
	}
}

// Read dataspace arguments from fifo and checkpoint these dataspaces	
void Rtcr::Checkpoint_thread::_checkpoint_fifo_dataspaces()
{	
	// Create string for profiler
	char args_buf[50];
	Genode::snprintf(args_buf, sizeof(args_buf), "%s _checkpoint_dataspace_content" , _name);
	char *p = args_buf;        
	const char *ds_content = p;

	while(_ckpt._dataspacethreads_needed || !_ckpt._memory_not_managed_fifo.empty() || !_ckpt._memory_managed_fifo.empty()){
		
		_waitForData();

		// Unmanaged Dataspaces	
		if(!_ckpt._memory_not_managed_fifo.empty())
		{
			Rtcr::Dataspace_translation_info *memory_info = _ckpt._memory_not_managed_fifo.dequeue();
			
			Genode::log("dataspace args dequeued by ", _name);
			{							
				PROFILE_SCOPE(ds_content, "purple", _ckpt._timer);
	
				_ckpt._checkpoint_dataspace_content(memory_info->ckpt_ds_cap, memory_info->resto_ds_cap, 0, memory_info->size);	
			}							
		}
		// Managed dataspaces
		if(!_ckpt._memory_managed_fifo.empty())
		{
			Rtcr::Dataspace_translation_info *memory_info = _ckpt._memory_managed_fifo.dequeue();
			Rtcr::Simplified_managed_dataspace_info::Simplified_designated_ds_info *sdd_info = _ckpt._sdd_fifo.dequeue();
			
			Genode::log("dataspace args dequeued by ", _name);
			{					
				PROFILE_SCOPE(ds_content, "blue", _ckpt._timer)

				_ckpt._checkpoint_dataspace_content(memory_info->ckpt_ds_cap, sdd_info->dataspace_cap, sdd_info->addr, sdd_info->size);
			}							
		}

	}	
}

	
void Rtcr::Checkpoint_thread::_checkpoint_main(){

	using Genode::log;

	// Pause child
	{		
		PROFILE_SCOPE("child pause", "brown", _ckpt._timer)
		_ckpt._child.pause();	
	}

	Genode::log(_ckpt._child);
	Genode::log(_ckpt._state);

	{
  		PROFILE_SCOPE("checkpoint()", "yellow", _ckpt._timer)
		
		Affinity::Space affinity_space = _ckpt._state._env.cpu().affinity_space();
	
		Genode::log(" Space Width = ", affinity_space.width());
		Checkpoint_thread ram_thread(_ckpt._state._env, "Thread prepare_ram entry", _ckpt._state._env.cpu(), _ckpt, affinity_space.location_of_index(1), 2);
		Checkpoint_thread rest_thread(_ckpt._state._env, "Thread prepare_pd_cpu_rm_log_timer entry", _ckpt._state._env.cpu(), _ckpt, affinity_space.location_of_index(2), 3);
		
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
		// Prepare state lists
		// implicitly _copy_dataspaces modified with the child's currently known dataspaces and copy dataspaces
		{
			PROFILE_SCOPE("_create_region_map_dataspaces_list()", "green", _ckpt._timer)
			_ckpt._region_maps = _ckpt._create_region_map_dataspaces_list(_ckpt._child.custom_services().pd_root->session_infos(), rm_sessions);
		}

		{
		  	PROFILE_SCOPE("Thread prepare_ram start", "red", _ckpt._timer) 
			ram_thread.start();
		}	
		{
	  		PROFILE_SCOPE("Threads prepare_pd_cpu_rm_log_timer start", "red", _ckpt._timer) 
			rest_thread.start();
		}			
		{
	  		PROFILE_SCOPE("Thread prepare_ram join", "red", _ckpt._timer) 
			ram_thread.join();
		}
		{
	  		PROFILE_SCOPE("Thread prepare_pd_cpu_rm_log_timer join", "red", _ckpt._timer) 
			rest_thread.join();
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
		// Create and start dataspace copy threads (_threadType 4)
		Checkpoint_thread thread_ds0(_ckpt._state._env, "Thread ds0", _ckpt._state._env.cpu(), _ckpt, affinity_space.location_of_index(1), 4);
		Checkpoint_thread thread_ds1(_ckpt._state._env, "Thread ds1", _ckpt._state._env.cpu(), _ckpt, affinity_space.location_of_index(2), 4);
		thread_ds0.start();
		thread_ds1.start();
		

		// Copy child dataspaces' content args and to stored dataspaces' content args to fifos
		{
			PROFILE_SCOPE("_checkpoint_dataspaces()", "green", _ckpt._timer)			
			_ckpt._checkpoint_dataspaces(_threadType);	
		}
		
		// Wait for dataspaces to be copied		
		thread_ds0.join();
		thread_ds1.join();

		// Clean up
		{
			PROFILE_SCOPE("_destroy_lists", "green", _ckpt._timer)
			
			_ckpt._destroy_list(_ckpt._kcap_mappings);
			_ckpt._destroy_list(_ckpt._dataspace_translations);
			_ckpt._destroy_list(_ckpt._region_maps);
			_ckpt._destroy_list(_ckpt._managed_dataspaces);		
		}

	}  

	//Genode::log(_ckpt._child);
	//Genode::log(_ckpt._state);

	// Resume child
	{		
		PROFILE_SCOPE("child resume", "brown", _ckpt._timer)   
		_ckpt._child.resume();	
	}
		
}

// Execute checkpointing sequentially without threads 
void Rtcr::Checkpoint_thread::_checkpoint_main_no_threads(){

	// Pause child
	{		
		PROFILE_SCOPE("child pause", "brown", _ckpt._timer)
		_ckpt._child.pause();	
	}
	{
  		PROFILE_SCOPE("checkpoint()", "yellow", _ckpt._timer)
	  		
		
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
			_ckpt._checkpoint_dataspaces(_threadType);	
		}
			

		//Genode::log(_child);
		//Genode::log(_state);

		// Clean up
		{
			PROFILE_SCOPE("_destroy_lists", "green", _ckpt._timer)
			
			_ckpt._destroy_list(_ckpt._kcap_mappings);
			_ckpt._destroy_list(_ckpt._dataspace_translations);
			_ckpt._destroy_list(_ckpt._region_maps);
			_ckpt._destroy_list(_ckpt._managed_dataspaces);		
		}
		

	}

	// Resume child
	{		
		PROFILE_SCOPE("child resume", "brown", _ckpt._timer)   
		_ckpt._child.resume();	
	}
}


