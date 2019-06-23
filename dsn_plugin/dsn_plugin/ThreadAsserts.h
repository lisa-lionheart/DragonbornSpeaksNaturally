#pragma once 

#include "Hooks.h"

// Assertions to check that our code is executing in the right place
#ifdef _DEBUG
#define SOFT_ASSERT(cond) Log::error("Asertion failed at %s:%d", __FILE__, __LINE__)
#define ASSERT_IS_GAME_THREAD() SOFT_ASSERT(Hooks::taskQueue.isOwnerThread());
#define ASSERT_IS_CLIENT_THREAD() SOFT_ASSERT(Hooks::client->isOwnerThread());
#else
#define ASSERT_IS_GAME_THREAD()
#define ASSERT_IS_CLIENT_THREAD()
#endif
