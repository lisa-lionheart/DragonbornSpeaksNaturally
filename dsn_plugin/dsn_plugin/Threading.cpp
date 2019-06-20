#include "Threading.h"
#include "Log.h"
#include "Utils.h"

TaskQueue g_GameThreadTaskQueue;
TaskQueue g_ClientThreadTaskQueue;


struct Task {
	std::function<void(void)> func;
	bool blocking;
	volatile bool done;
};


thread_local TaskQueue* TaskQueue::executingQueue = NULL;


void TaskQueue::ExecuteAction(const std::function<void(void)>& func, bool wait) {
	Task* task = new Task();
	task->func = func;
	task->blocking = wait;

	lock.lock();
	queue.push(task);
	lock.unlock();

	if (wait) {
		Log::debug("Blocking on task");
		while (!task->done) {
			Sleep(1);
		}
		Log::debug("Finished Blocking on task");
		delete task;
	}
}

void TaskQueue::PumpThreadActions() {
	executingQueue = this;

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

		Log::debug("Executing game thread task " + Utils::fmt_hex((uint32_t)task));

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