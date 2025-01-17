#include <string.h>
#include <set>
#include "common/IPrefix.h"
#include "skse64_common/SafeWrite.h"
#include "skse64/GameInput.h"
#include "skse64_common/BranchTrampoline.h"
#include "xbyak.h"
#include "SkyrimType.h"

#include "Hooks.h"
#include "Log.h"
#include "ConsoleCommandRunner.h"
#include "EquipmentManager.h"
#include "Threading.h"
#include "GameStateController.h"
#include "DialogueController.h"

using std::string;

namespace Hooks {


	TaskQueue taskQueue;
	
	static const char* VERSION = "0.19";
	
	class RunCommandSink;
	RunCommandSink* runCommandSink = NULL;

	SpeechRecognitionClient* speechClient = NULL;
	ConsoleCommandRunner* console = NULL;

	GameStateController* gameStateController = NULL;
	EquipmentManager* favouritesManager = NULL;
	DialogueControler* dialogueController = NULL;


	void Start(string dllPath, HINSTANCE hInstDll)
	{
		TRACE_METHOD();

		Log::info("DragonBornNaturallySpeaking loaded");
		Log::info("Version %s", VERSION);

		g_branchTrampoline.Create(1024 * 64);
		g_localTrampoline.Create(1024 * 64, (void*)hInstDll);

		Inject();
			   
		speechClient = new SpeechRecognitionClient(dllPath);

		console = new ConsoleCommandRunner(speechClient);					
		dialogueController = new DialogueControler(speechClient);
		
		if (g_SkyrimType == VR) {	
			favouritesManager = new EquipmentManager(speechClient, console);
			gameStateController = new GameStateController(speechClient);
		}
	}

	// Hook of 
	// GfxMovieView::Invoke
	//
	static void __cdecl Invoke(GFxMovieView* movie, const char* gfxMethod, GFxValue* argv, UInt32 argc)
	{
		TRACE_METHOD();
		if (argc >= 1)
		{
			GFxValue commandVal = argv[0];
			if (commandVal.type == GFxValue::kType_String) { // Command
				const char* command = commandVal.data.string;
				//Log::debug("Command is: %s", command); // TEMP
				if (strcmp(command, "PopulateDialogueList") == 0)
				{
					dialogueController->StartDialogue(movie, argv, argc);
				}
				else if (strcmp(command, "UpdatePlayerInfo") == 0)
				{
					if (favouritesManager) {
						favouritesManager->RefreshAll();
					}
				}
			}
		}
	}

	// Called when the game load from a save
	static void __cdecl PostLoad() {
		TRACE_METHOD();
		if (favouritesManager) {
			favouritesManager->RefreshAll();
		}
	}


	class RunCommandSink : public BSTEventSink<InputEvent> {
		EventResult ReceiveEvent(InputEvent** evnArr, InputEventDispatcher* dispatcher) override {
			// Unadvised, called every frame
			TRACE_METHOD();
			taskQueue.PumpThreadActions();
			return kEvent_Continue;
		}
	};


	// Called whilst the game is not paused
	static void __cdecl Loop()
	{
		// Unadvised, called every frame
		TRACE_METHOD();
		if (g_SkyrimType == VR) {
			taskQueue.PumpThreadActions();
		}
		else {
			// The latest version of SkyrimSE will not enter the loop if no menu is displayed.
			// So we use InputEventSink to get a continuous running loop.
			static bool inited = false;
			if (!inited) {
				Log::info("RunCommandSink Initialized");
				runCommandSink = new Hooks::RunCommandSink;
				// Currently in the source code directory is the latest version of SKSE64 instead of SKSEVR,
				// so we can call GetSingleton() directly instead of use a RelocAddr.
				auto inputEventDispatcher = InputEventDispatcher::GetSingleton();
				inputEventDispatcher->AddEventSink(runCommandSink);
				inited = true;
			}
		}
	}

	static uintptr_t loopEnter = 0x0;
	static uintptr_t loopCallTarget = 0x0;
	static uintptr_t invokeTarget = 0x0;
	static uintptr_t invokeReturn = 0x0;
	static uintptr_t loadEventEnter = 0x0;
	static uintptr_t loadEventTarget = 0x0;

	static uintptr_t INVOKE_ENTER_ADDR[3];
	static uintptr_t INVOKE_TARGET_ADDR[3];

	static uintptr_t LOOP_ENTER_ADDR[3];
	static uintptr_t LOOP_TARGET_ADDR[3];

	static uintptr_t LOAD_EVENT_ENTER_ADDR[3];
	static uintptr_t LOAD_EVENT_TARGET_ADDR[3];

	/**
		Inject hooks into the game, here be dragons
	*/
	void Inject(void)
	{
		TRACE_METHOD();

		// "call" Scaleform invocation
		INVOKE_ENTER_ADDR[SE] = 0xED68EE;
		INVOKE_ENTER_ADDR[VR] = 0xF343CE;
		INVOKE_ENTER_ADDR[VR_BETA] = 0xF343CE;

		// Target of "call" invocation
		INVOKE_TARGET_ADDR[SE] = 0xED7DF0; // SkyrimSE 0xED7DF0 0x00007FF7C38D7DF0
		INVOKE_TARGET_ADDR[VR] = 0xF30F20; // SkyrimVR 0xF2D9B0 0x00007FF73284D9B0
		INVOKE_TARGET_ADDR[VR_BETA] = 0xF30F20; // SkyrimVR 0xF2D9B0 0x00007FF73284D9B0

		// "CurrentTime" GFxMovie.SetVariable (rax+80)
		LOOP_ENTER_ADDR[SE] = 0xECD637; // SkyrimSE 0xECD637 0x00007FF712D4D637
		LOOP_ENTER_ADDR[VR] = 0x8AC25C; // 0x8AA36C 0x00007FF7321CA36C SKSE UIManager process hook:  0x00F17200 + 0xAD8
		LOOP_ENTER_ADDR[VR_BETA] = 0x8AC25C; // 0x8AA36C 0x00007FF7321CA36C SKSE UIManager process hook:  0x00F17200 + 0xAD8

		// "CurrentTime" GFxMovie.SetVariable Target (rax+80)
		LOOP_TARGET_ADDR[SE] = 0xF28C90; // SkyrimSE 0xF28C90 0x00007FF712DA8C90
		LOOP_TARGET_ADDR[VR] = 0xF85C50; // SkyrimVR 0xF82710 0x00007FF7328A2710 SKSE UIManager process hook:  0x00F1C650
		LOOP_TARGET_ADDR[VR_BETA] = 0xF85C50; // SkyrimVR 0xF82710 0x00007FF7328A2710 SKSE UIManager process hook:  0x00F1C650

		// "Finished loading game" print statement, initialize player orientation?
		LOAD_EVENT_ENTER_ADDR[VR] = 0x5852A4;

		// Initialize player orientation target addr
		LOAD_EVENT_TARGET_ADDR[VR] = 0x6AB5E0;

		RelocAddr<uintptr_t> kHook_Invoke_Enter(INVOKE_ENTER_ADDR[g_SkyrimType]);
		RelocAddr<uintptr_t> kHook_Invoke_Target(INVOKE_TARGET_ADDR[g_SkyrimType]);
		RelocAddr<uintptr_t> kHook_Loop_Enter(LOOP_ENTER_ADDR[g_SkyrimType]);
		RelocAddr<uintptr_t> kHook_Loop_Call_Target(LOOP_TARGET_ADDR[g_SkyrimType]);
		uintptr_t kHook_Invoke_Return = kHook_Invoke_Enter + 0x14;

		invokeTarget = kHook_Invoke_Target;
		invokeReturn = kHook_Invoke_Return;
		loopCallTarget = kHook_Loop_Call_Target;
		loopEnter = kHook_Loop_Enter;

		Log::info("Loop Enter: %p", kHook_Loop_Enter.GetUIntPtr());
		Log::info("Loop Target: %p", kHook_Loop_Call_Target.GetUIntPtr());

		/***
		Post Load HOOK - VR Only
		**/
		if (g_SkyrimType == VR) {
			RelocAddr<uintptr_t> kHook_LoadEvent_Enter(LOAD_EVENT_ENTER_ADDR[g_SkyrimType]);
			RelocAddr<uintptr_t> kHook_LoadEvent_Target(LOAD_EVENT_TARGET_ADDR[g_SkyrimType]);
			loadEventEnter = kHook_LoadEvent_Enter;
			loadEventTarget = kHook_LoadEvent_Target;

			Log::info("LoadEvent Enter: %p", kHook_LoadEvent_Enter.GetUIntPtr());
			Log::info("LoadEvent Target: %p", kHook_LoadEvent_Target.GetUIntPtr());

			struct Hook_LoadEvent_Code : Xbyak::CodeGenerator {
				Hook_LoadEvent_Code(void* buf) : Xbyak::CodeGenerator(4096, buf)
				{
					// Invoke original virtual method
					mov(rax, (uintptr_t)loadEventTarget);
					call(rax);

					// Call our method
					sub(rsp, 0x30);
					mov(rax, (uintptr_t)PostLoad);
					call(rax);
					add(rsp, 0x30);

					// Return 
					mov(rax, loadEventEnter + 0x5);
					jmp(rax);
				}
			};
			void* codeBuf = g_localTrampoline.StartAlloc();
			Hook_LoadEvent_Code loadEventCode(codeBuf);
			g_localTrampoline.EndAlloc(loadEventCode.getCurr());
			g_branchTrampoline.Write5Branch(kHook_LoadEvent_Enter, uintptr_t(loadEventCode.getCode()));
		}

		/***
		Loop HOOK
		**/
		struct Hook_Loop_Code : Xbyak::CodeGenerator {
			Hook_Loop_Code(void* buf) : Xbyak::CodeGenerator(4096, buf)
			{
				// Invoke original virtual method
				mov(rax, loopCallTarget);
				call(rax);

				// Call our method
				sub(rsp, 0x30);
				mov(rax, (uintptr_t)Loop);
				call(rax);
				add(rsp, 0x30);

				// Return 
				mov(rax, loopEnter + 0x6); // set to 0x5 when branching for SKSE UIManager
				jmp(rax);
			}
		};
		void* codeBuf = g_localTrampoline.StartAlloc();
		Hook_Loop_Code loopCode(codeBuf);
		g_localTrampoline.EndAlloc(loopCode.getCurr());
		//g_branchTrampoline.Write6Branch(kHook_Loop_Enter, uintptr_t(loopCode.getCode()));
		g_branchTrampoline.Write5Branch(kHook_Loop_Enter, uintptr_t(loopCode.getCode()));

		/***
		Invoke "call" HOOK
		**/
		struct Hook_Entry_Code : Xbyak::CodeGenerator {
			Hook_Entry_Code(void* buf) : Xbyak::CodeGenerator(4096, buf)
			{
				push(rcx);
				push(rdx);
				push(r8);
				push(r9);
				sub(rsp, 0x30);
				mov(rax, (uintptr_t)Invoke);
				call(rax);
				add(rsp, 0x30);
				pop(r9);
				pop(r8);
				pop(rdx);
				pop(rcx);

				mov(rax, invokeTarget);
				call(rax);

				mov(rbx, ptr[rsp + 0x50]);
				mov(rsi, ptr[rsp + 0x60]);
				add(rsp, 0x40);
				pop(rdi);

				mov(rax, invokeReturn);
				jmp(rax);
			}
		};

		codeBuf = g_localTrampoline.StartAlloc();
		Hook_Entry_Code entryCode(codeBuf);
		g_localTrampoline.EndAlloc(entryCode.getCurr());
		g_branchTrampoline.Write5Branch(kHook_Invoke_Enter, uintptr_t(entryCode.getCode()));
	}

}