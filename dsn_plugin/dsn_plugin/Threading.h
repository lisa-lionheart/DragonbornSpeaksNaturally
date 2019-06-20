#pragma once
#include <queue>
#include <functional>
#include <mutex>




struct Task;

/*
	A queue of tasks to be executed on a specific thread.

	Allows you to safely invoke an action on another thread

*/
class TaskQueue {

	std::queue<Task*> queue;
	std::mutex lock;

	static thread_local TaskQueue* executingQueue;

public:
	// Is this queue being pumped by the calling thread
	bool isOwnerThread();
	
	// Execute an action on the thread owning this queue
	void ExecuteAction(const std::function<void()>& func, bool wait);

	// Pump all the actions until flushed
	void PumpThreadActions();
};


// Defined queues
extern TaskQueue g_GameThreadTaskQueue;
extern TaskQueue g_ClientThreadTaskQueue;

#define OUR_ASSERT(cond) if(!cond) { Log::error("Assertion failed ("#cond") file: " + std::string(__FILE__) + " line: " + std::to_string(__LINE__)); }

// Assertions to check that our code is executing in the right place
#define ASSERT_IS_GAME_THREAD() OUR_ASSERT(g_GameThreadTaskQueue.isOwnerThread());
#define ASSERT_IS_CLIENT_THREAD() OUR_ASSERT(g_ClientThreadTaskQueue.isOwnerThread());

