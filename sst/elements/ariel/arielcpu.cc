
#include "arielcpu.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

using namespace SST::ArielComponent;

ArielCPU::ArielCPU(ComponentId_t id, Params& params) :
	Component(id) {

	int verbosity = params.find_integer("verbose", 0);
	output = new SST::Output("ArielComponent[@f:@l:@p] ",
		verbosity, 0, SST::Output::STDOUT);
		
	output->verbose(CALL_INFO, 1, 0, "Creating Ariel component...\n");

	core_count = (uint32_t) params.find_integer("corecount", 1);
	output->verbose(CALL_INFO, 1, 0, "Configuring for %" PRIu32 " cores...\n", core_count);
	
	memory_levels = (uint32_t) params.find_integer("memorylevels", 1);
	output->verbose(CALL_INFO, 1, 0, "Configuring for %" PRIu32 " memory levels.\n", memory_levels);
	
	page_sizes = (uint64_t*) malloc( sizeof(uint64_t) * memory_levels );
	page_counts = (uint64_t*) malloc( sizeof(uint64_t) * memory_levels );
	
	char* level_buffer = (char*) malloc(sizeof(char) * 256);
	for(uint32_t i = 0; i < memory_levels; ++i) {
		sprintf(level_buffer, "pagesize%" PRIu32, i);
		page_sizes[i] = (uint64_t) params.find_integer(level_buffer, 4096);
		
		sprintf(level_buffer, "pagecount%" PRIu32, i);
		page_counts[i] = (uint64_t) params.find_integer(level_buffer, 16777216);
	}
	free(level_buffer);
	
	uint32_t default_level = (uint32_t) params.find_integer("defaultlevel", 0);
	
	output->verbose(CALL_INFO, 1, 0, "Creating memory manager, default allocation from %" PRIu32 " memory pool.\n", default_level);
	memmgr = new ArielMemoryManager(memory_levels, 
		page_sizes, page_counts, output, default_level);
		
	uint32_t maxIssuesPerCycle   = (uint32_t) params.find_integer("maxissuepercycle", 1);
	uint32_t maxCoreQueueLen     = (uint32_t) params.find_integer("maxcorequeue", 64);
	uint32_t maxPendingTransCore = (uint32_t) params.find_integer("maxtranscore", 16);
	int      pipeReadTimeOut     = (int)      params.find_integer("pipetimeout", 10);
	uint64_t cacheLineSize       = (uint64_t) params.find_integer("cachelinesize", 64);
	
	/////////////////////////////////////////////////////////////////////////////////////
	
	named_pipe_base = (char*) malloc(sizeof(char) * 256);
	tmpnam(named_pipe_base);
	
	output->verbose(CALL_INFO, 1, 0, "Base pipe name: %s\n", named_pipe_base);
	
	/////////////////////////////////////////////////////////////////////////////////////
	
	std::string ariel_tool = params.find_string("arieltool", "");
	if("" == ariel_tool) {
		output->fatal(CALL_INFO, -1, "The arieltool parameter specifying which PIN tool to run was not specified\n");
	}
	
	std::string executable = params.find_string("executable", "");
	if("" == executable) {
		output->fatal(CALL_INFO, -1, "The input deck did not specify an executable to be run against PIN\n");
	}
	
	uint32_t app_argc = (uint32_t) params.find_integer("appargcount", 0);
	output->verbose(CALL_INFO, 1, 0, "Model specifies that there are %" PRIu32 " application arguments\n", app_argc);
	
	uint32_t pin_startup_mode = (uint32_t) params.find_integer("arielmode", 1);
	
	const char* execute_binary = PINTOOL_EXECUTABLE;
	const uint32_t pin_arg_count = 16;
  	char** execute_args = (char**) malloc(sizeof(char*) * (pin_arg_count + app_argc));

	output->verbose(CALL_INFO, 1, 0, "Processing application arguments...\n");

  	execute_args[0] = "pintool";
	execute_args[1] = "-t";
	execute_args[2] = (char*) malloc(sizeof(char) * (ariel_tool.size() + 1));
  	strcpy(execute_args[2], ariel_tool.c_str());
  	execute_args[3] = "-p";
  	execute_args[4] = (char*) malloc(sizeof(char) * (strlen(named_pipe_base) + 1));
  	strcpy(execute_args[4], named_pipe_base);
  	execute_args[5] = "-v";
  	execute_args[6] = (char*) malloc(sizeof(char) * 8);
  	sprintf(execute_args[6], "%d", verbosity);
  	execute_args[7] = "-i";
  	execute_args[8] = (char*) malloc(sizeof(char) * 30);
  	sprintf(execute_args[8], "%" PRIu64, (uint64_t) 1000000000);
  	execute_args[9] = "-c";
  	execute_args[10] = (char*) malloc(sizeof(char) * 8);
  	sprintf(execute_args[10], "%" PRIu32, core_count);
	execute_args[11] = "-s";
	execute_args[12] = (char*) malloc(sizeof(char) * 8);
	sprintf(execute_args[12], "%" PRIu32, pin_startup_mode);
  	execute_args[13] = "--";
  	execute_args[14] = (char*) malloc(sizeof(char) * (executable.size() + 1));
  	strcpy(execute_args[14], executable.c_str());
	
	char* argv_buffer = (char*) malloc(sizeof(char) * 256);
	for(uint32_t i = (pin_arg_count - 1); i < (pin_arg_count - 1) + app_argc; ++i) {
		sprintf(argv_buffer, "apparg%" PRIu32, i - (pin_arg_count - 1));
		std::string argv_i = params.find_string(argv_buffer, "");
		
		output->verbose(CALL_INFO, 1, 0, "Found application argument %" PRIu32 " (%s) = %s\n", 
			i - (pin_arg_count - 1), argv_buffer, argv_i.c_str());
		execute_args[i] = (char*) malloc(sizeof(char) * (argv_i.size() + 1));
		strcpy(execute_args[i], argv_i.c_str());
	}
	free(argv_buffer);
	
	output->verbose(CALL_INFO, 1, 0, "Completed processing application arguments.\n");
	
	// Remember that the list of arguments must be NULL terminated for execution
	execute_args[(pin_arg_count - 1) + app_argc] = NULL;
	
	char* pipe_buffer = (char*) malloc(sizeof(char) * 256);
	pipe_fds = (int*) malloc(sizeof(int) * core_count);
	for(uint32_t i = 0; i < core_count; ++i) {
		sprintf(pipe_buffer, "%s-%" PRIu32, named_pipe_base, i);
		output->verbose(CALL_INFO, 1, 0, "Creating pipe: %s ...\n", pipe_buffer);
		
		mkfifo(pipe_buffer, 0666);
	}
	
	output->verbose(CALL_INFO, 1, 0, "Launching PIN...\n");
	forkPINChild(execute_binary, execute_args);
	output->verbose(CALL_INFO, 1, 0, "Returned from launching PIN.\n");
	
	sleep(2);
	
	/////////////////////////////////////////////////////////////////////////////////////
	
	pipe_fds = (int*) malloc(sizeof(int) * core_count);
	for(uint32_t i = 0; i < core_count; ++i) {
		sprintf(pipe_buffer, "%s-%" PRIu32, named_pipe_base, i);
		output->verbose(CALL_INFO, 1, 0, "Connecting to (read) pipe: %s ...\n", pipe_buffer);

		pipe_fds[i] = open(pipe_buffer, O_RDONLY | O_NONBLOCK);

		if(-1 == pipe_fds[i]) {
			output->fatal(CALL_INFO, -1, "Creation of pipe %s failed.\n", pipe_buffer);
		} else {
			output->verbose(CALL_INFO, 2, 0, "Created successfully.\n");
		}
	}
	free(pipe_buffer);
	
	/////////////////////////////////////////////////////////////////////////////////////
	
	
	output->verbose(CALL_INFO, 1, 0, "Creating core to cache links...\n");
	cpu_to_cache_links = (SST::Link**) malloc( sizeof(SST::Link*) * core_count );

	output->verbose(CALL_INFO, 1, 0, "Creating processor cores and cache links...\n");
	cpu_cores = (ArielCore**) malloc( sizeof(ArielCore*) * core_count );
	
	output->verbose(CALL_INFO, 1, 0, "Configuring cores and cache links...\n");
	char* link_buffer = (char*) malloc(sizeof(char) * 256);
	for(uint32_t i = 0; i < core_count; ++i) {
		sprintf(link_buffer, "cache_link_%" PRIu32, i);
	
		cpu_cores[i] = new ArielCore(pipe_fds[i], NULL, i, maxPendingTransCore, output, 
			maxIssuesPerCycle, maxCoreQueueLen, pipeReadTimeOut, cacheLineSize, this,
			memmgr);
		cpu_to_cache_links[i] = configureLink( link_buffer, new Event::Handler<ArielCore>(cpu_cores[i], &ArielCore::handleEvent) );
		cpu_cores[i]->setCacheLink(cpu_to_cache_links[i]);
	}
	free(link_buffer);

	std::string cpu_clock = params.find_string("clock", "1GHz");
	output->verbose(CALL_INFO, 1, 0, "Registering ArielCPU clock at %s\n", cpu_clock.c_str());
	registerClock( cpu_clock, new Clock::Handler<ArielCPU>(this, &ArielCPU::tick ) );

	output->verbose(CALL_INFO, 1, 0, "Clocks registered.\n");

	// Register us as an important component
	registerAsPrimaryComponent();
  	primaryComponentDoNotEndSim();

	stopTicking = true;
	
	output->verbose(CALL_INFO, 1, 0, "Completed initialization of the Ariel CPU.\n");
	fflush(stdout);
}

void ArielCPU::finish() {
	output->verbose(CALL_INFO, 1, 0, "Ariel Processor Information:\n");
	output->verbose(CALL_INFO, 1, 0, "Completed at: %" PRIu64 " nanoseconds.\n", (uint64_t) getCurrentSimTimeNano() );
	output->verbose(CALL_INFO, 1, 0, "Ariel Component Statistics (By Core)\n");
	for(uint32_t i = 0; i < core_count; ++i) {
		cpu_cores[i]->printCoreStatistics();
	}
}

int ArielCPU::forkPINChild(const char* app, char** args) {
	pid_t the_child;

	// Fork this binary, then exec to get around waiting for
	// child to exit.
	the_child = fork();

	if(the_child != 0) {
		// This is the parent, return the PID of our child process
		return (int) the_child;
	} else {
		output->verbose(CALL_INFO, 1, 0,
			"Launching executable: %s...\n", app);

		int ret_code = execvp(app, args);
		perror("execve");

		output->verbose(CALL_INFO, 1, 0,
			"Call to execvp returned: %d\n", ret_code);

		output->fatal(CALL_INFO, -1, 
			"Error executing: %s under a PIN fork\n",
			app);
	}

	return 0;
}

bool ArielCPU::tick( SST::Cycle_t ) {
	stopTicking = false;
	
	output->verbose(CALL_INFO, 16, 0, "Main processor tick, will issue to individual cores...\n");

	// Keep ticking unless one of the cores says it is time to stop.
	for(uint32_t i = 0; i < core_count; ++i) {
		cpu_cores[i]->tick();
		
		if(cpu_cores[i]->isCoreHalted()) {
			stopTicking = true;
			break;
		}
	}
	
	// Its time to end, that's all folks
	if(stopTicking) {
		primaryComponentOKToEndSim();
	}
	
	return stopTicking;
}

ArielCPU::~ArielCPU() {
	delete memmgr;
	
	free(page_sizes);
	free(page_counts);
	
	char* pipe_buffer = (char*) malloc(sizeof(char) * 256);
	for(uint32_t i = 0; i < core_count; ++i) {
		close(pipe_fds[i]);
		
		sprintf(pipe_buffer, "%s-%" PRIu32, named_pipe_base, i);
		remove(pipe_buffer);
	}
	free(pipe_buffer);
}

