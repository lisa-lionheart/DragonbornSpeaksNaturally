#pragma once
#include <vector>
#include <string>
#include "SpeechRecognitionClient.h"

class SpeechRecognitionClient;
class GFxValue;
class GFxMovieView;

typedef std::vector<std::string> DialogueList;

class DialogueControler {

public:
	DialogueControler(SpeechRecognitionClient* client);


	void StartDialogue(GFxMovieView* movie, GFxValue* argv, UInt32 argc);

	bool IsInDialogue();

	void ProccessEvents();

private:


	// Dialogue was closed
	void StopDialogue();

	// Handle response from the service
	void HandleResponse(const TokenList& tokens);

	SpeechRecognitionClient* client;
	
	int currentDialogueId = 0;
	int lastMenuState = -1;

	GFxMovieView* dialogueMenu = NULL;
};