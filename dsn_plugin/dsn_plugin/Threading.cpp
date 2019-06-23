#include "Threading.h"
#include "Log.h"
#include "Utils.h"

TaskQueue g_GameThreadTaskQueue;
TaskQueue g_ClientThreadTaskQueue;

using std::vector;
using std::string;
using std::map;

struct Task {
	std::function<void(void)> func;
	bool blocking;
	volatile bool done;
};


thread_local TaskQueue* TaskQueue::executingQueue = NULL;

void TaskQueue::AddPoll(void* name, const TaskFunc& poller) {
	lock.lock();
	pollFunctions[name] = poller;
	lock.unlock();
}

void TaskQueue::RemovePoll(void* name) {
	lock.lock();
	pollFunctions.erase(name);
	lock.unlock();
}

void TaskQueue::ExecuteAction(const TaskFunc& func, bool wait) {
	Task* task = new Task();
	task->func = func;
	task->blocking = wait;

	lock.lock();
	queue.push(task);
	lock.unlock();

	if (wait) {
		while (!task->done) {
			Sleep(1);
		}
		delete task;
	}
}

void TaskQueue::PumpThreadActions() {
	executingQueue = this;
	
	lock.lock();
	auto pollerCopy = pollFunctions;
	lock.unlock();

	for (auto itr = pollerCopy.begin(); itr != pollerCopy.end(); itr++) {
		itr->second();
	}

	while (true) {
		lock.lock();
		Task* task = NULL;
		if (queue.size() > 0) {
			task = queue.front();
			queue.pop();
		}
		lock.unlock();

		if (task == NULL) {
			return;
		}

		Log::debug("Executing game thread task %p", task);

		task->func();
		task->done = true;

		if (!task->blocking) {
			delete task;
		}
	}
}

bool TaskQueue::isOwnerThread() {
	return executingQueue == this;
}