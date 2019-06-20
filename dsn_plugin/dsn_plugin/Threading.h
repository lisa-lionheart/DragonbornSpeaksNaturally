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

// Assertions to check that our code is executing in the right place
#ifdef _DEBUG
#define SOFT_ASSERT(cond) Log::error("Asertion failed at %s:%d", __FILE__, __LINE__)
#define ASSERT_IS_GAME_THREAD() SOFT_ASSERT(g_GameThreadTaskQueue.isOwnerThread());
#define ASSERT_IS_CLIENT_THREAD() SOFT_ASSERT(g_ClientThreadTaskQueue.isOwnerThread());
#else
#define ASSERT_IS_GAME_THREAD()
#define ASSERT_IS_CLIENT_THREAD()
#endif
