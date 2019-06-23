
#include<vector>
#include "skse64/ScaleformMovie.h"
#include "skse64/ScaleformValue.h"

#include "Log.h"
#include "Hooks.h"
#include "DialogueController.h"
#include "ThreadAsserts.h"

using std::string;
using std::vector;


DialogueControler::DialogueControler(SpeechRecognitionClient* _client) {
	TRACE_METHOD();
	client = _client;
	client->RegisterHandler(this, "DIALOGUE", &DialogueControler::HandleResponse, true);
}

void DialogueControler::StartDialogue(GFxMovieView* movie, GFxValue* argv, UInt32 argc) {

	Log::info("Starting dialouge");

	dialogueMenu = movie;
	currentDialogueId++;

	string command = "START_DIALOGUE|";
	command += std::to_string(this->currentDialogueId);

	for (int j = 1; j < argc - 1; j = j + 3)
	{
		GFxValue dialogueLine = argv[j];
		const char * line = dialogueLine.data.string;
		Log::info("Dialogue lne '%s'", line);
		command += "|" + string(line);
	}

	Hooks::taskQueue.AddPoll(this, &DialogueControler::ProccessEvents);
	client->WriteLine(command);
}

void DialogueControler::StopDialogue() {
	dialogueMenu = NULL;
	Hooks::taskQueue.RemovePoll(this);
	client->WriteLine("STOP_DIALOGUE");
}


bool DialogueControler::IsInDialogue() {
	return dialogueMenu != NULL;
}


void DialogueControler::HandleResponse(const TokenList& tokens) {
	ASSERT_IS_GAME_THREAD();
	
	int dialogueId = std::stoi(tokens[1]);
	int indexId = std::stoi(tokens[2]);
	
	if (dialogueId == currentDialogueId) {

		if (indexId >= 0) {
			GFxValue topicIndexVal;
			dialogueMenu->GetVariable(&topicIndexVal, "_level0.DialogueMenu_mc.TopicList.iSelectedIndex");

			int currentTopicIndex = topicIndexVal.data.number;
			if (currentTopicIndex != indexId) {

				dialogueMenu->Invoke("_level0.DialogueMenu_mc.TopicList.SetSelectedTopic", NULL, "%d", indexId);
				dialogueMenu->Invoke("_level0.DialogueMenu_mc.TopicList.doSetSelectedIndex", NULL, "%d", indexId);
				dialogueMenu->Invoke("_level0.DialogueMenu_mc.TopicList.UpdateList", NULL, NULL, 0);
			}

			dialogueMenu->Invoke("_level0.DialogueMenu_mc.onSelectionClick", NULL, "%d", 1.0);
		}
		
		if (indexId == -2) { // Indicates a "goodbye" phrase was spoken, hide the menu
			dialogueMenu->Invoke("_level0.DialogueMenu_mc.StartHideMenu", NULL, NULL, 0);
		}
	}
}

void DialogueControler::ProccessEvents() {
	ASSERT_IS_GAME_THREAD();
	
	if (dialogueMenu == NULL) {
		return;
	}

	// Menu exiting, avoid NPE
	if (dialogueMenu->GetPause() == 0) {
		StopDialogue();
		return;
	}

	GFxValue stateVal;
	dialogueMenu->GetVariable(&stateVal, "_level0.DialogueMenu_mc.eMenuState");
	int menuState = stateVal.data.number;
	if (menuState != lastMenuState) {

		lastMenuState = menuState;
		if (menuState == 2) // NPC Responding
		{
			// fix issue #11 (SSE crash when teleport with a dialogue line).
			// It seems no side effects have been found at present.
			StopDialogue();
			return;
		}
	}
	
}