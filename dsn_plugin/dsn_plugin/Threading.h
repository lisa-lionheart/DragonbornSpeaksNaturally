#pragma once
#include <queue>
#include <functional>
#include <mutex>


struct Task;

typedef std::function<void(void)> TaskFunc;

/*
	A queue of tasks to be executed on a specific thread.

	Allows you to safely invoke an action on another thread

*/
class TaskQueue {

	std::map<void*, TaskFunc> pollFunctions;
	std::queue<Task*> queue;
	std::mutex lock;

	static thread_local TaskQueue* executingQueue;

	void AddPoll(void* ctx, const TaskFunc& poller);
	void RemovePoll(void*);
public:
	// Is this queue being pumped by the calling thread
	bool isOwnerThread();

	// Add a task to be run repeatedly
	template<class T>
	void AddPoll(T* inst, void(T::*func)()) {
		AddPoll(inst, std::bind(func, inst));
	}
	template<class T>
	void RemovePoll(T* inst) {
		RemovePoll((void*)inst);
	}

	
	// Execute an action on the thread owning this queue
	

	void ExecuteAction(const TaskFunc& t, bool wait);

	// Pump all the actions until flushed
	void PumpThreadActions();


	
	// Helpers
	inline void ExecuteAction(void (*func)(), bool wait) {
		ExecuteAction(std::bind(func), wait);
	}

	template <class Arg0>
	inline void ExecuteAction(void (*func)(Arg0), Arg0 arg0, bool wait) {
		ExecuteAction(std::bind(func, arg0), wait);
	}

	template <class Target>
	inline void ExecuteAction(Target* inst, void (Target::* func)(), bool wait) {
		ExecuteAction(std::bind(func, inst), wait);
	}

	template <class Target, class Arg0>
	inline void ExecuteAction(Target* inst, void (Target::* func)(Arg0 arg0), Arg0 arg0, bool wait) {
		ExecuteAction(std::bind(func, inst, arg0), wait);
	}

};

