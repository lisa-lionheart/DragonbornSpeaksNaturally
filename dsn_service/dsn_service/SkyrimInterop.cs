﻿using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Speech.Recognition;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace DSN {


    struct GameState {

        
        public string currentMenu;

        public bool isInCombat;
        public bool isWeaponDrawn;
        public bool isSneaking;

        public bool isMenuOpen() {
            return currentMenu != "None";
        }
    }
    class SkyrimInterop {

        private Configuration config = null;
        private ConsoleInput consoleInput = null;

        private System.Object dialogueLock = new System.Object();
        private DialogueList currentDialogue = null;
        private FavoritesList favoritesList = null;
        private SpellBook spellBook = null;
        private CommandList commandList = null;
        private SpeechRecognitionManager recognizer;
        private Thread submissionThread;
        private Thread listenThread;
        private BlockingCollection<string> commandQueue;
        private ShoutsKnowledge shoutsKnowledge;

        
        public SkyrimInterop(Configuration config, ConsoleInput consoleInput) {
            this.config = config;
            this.consoleInput = consoleInput;

        }

        public void Start() {
            try {
                favoritesList = new FavoritesList(config);
                spellBook = new SpellBook(config);
                shoutsKnowledge = new ShoutsKnowledge(config);
                commandList = config.GetConsoleCommandList();

                commandQueue = new BlockingCollection<string>();
                recognizer = new SpeechRecognitionManager(config);
                recognizer.OnDialogueLineRecognized += Recognizer_OnDialogueLineRecognized;

                // Start in command-mode
                recognizer.StartSpeechRecognition(false, commandList, favoritesList, spellBook, shoutsKnowledge);

                listenThread = new Thread(ListenForInput);
                submissionThread = new Thread(SubmitCommands);
                submissionThread.Start();
                listenThread.Start();
            }
            catch (Exception ex) {
                Trace.TraceError("Failed to initialize speech recognition due to error:");
                Trace.TraceError(ex.ToString());
            }
        }

        public void Join() {
            listenThread.Join();
        }

        public void Stop() {
            // Notify threads to exit
            consoleInput.WriteLine(null);
            commandQueue.Add(null);
            
            recognizer.Stop();
        }

        public void SubmitCommand(string command) {
            commandQueue.Add(sanitize(command));
        }

        private static string sanitize(string command) {
            command = command.Trim();
            return command.Replace("\r", "");
        }

        private void SubmitCommands() {
            while(true) {
                string command = commandQueue.Take();

                // Thread exit signal
                if (command == null) {
                    break;
                }

                Trace.TraceInformation("Sending command: {0}", command);
                Console.Write(command+"\n");
            }
        }

        private void ListenForInput() {
            try {
                // try to restore saved state after reloading the configuration file.
                consoleInput.RestoreSavedState();

                while (true) {
                    string input = consoleInput.ReadLine();

                    // input will be null when Skyrim terminated (stdin closed)
                    if (input == null) {
                        break;
                    }

                    Trace.TraceInformation("Received command: {0}", input);

                    string[] tokens = input.Split('|');
                    string command = tokens[0];

                    if (command.Equals("START_DIALOGUE")) {
                        // Switch to dialogue mode
                        consoleInput.currentDialogue = input;
                        lock (dialogueLock) {
                            currentDialogue = DialogueList.Parse(string.Join("|", tokens, 1, tokens.Length - 1), config);
                        }
                    } else if (command.Equals("STOP_DIALOGUE")) {
                        consoleInput.currentDialogue = null;
                        // Switch to command mode
                        lock (dialogueLock) {
                            currentDialogue = null;
                        }
                    } else if (command.Equals("FAVORITES")) {
                        consoleInput.currentFavoritesList = input;
                        favoritesList.Update(string.Join("|", tokens, 1, tokens.Length - 1));
                    } else if(command.Equals("SPELLBOOK")) {
                        spellBook.UpdateSpells(tokens.Skip(1));
                    } else if(command.Equals("SHOUTS")) {
                        shoutsKnowledge.UpdateShouts(tokens.Skip(1));
                    } else if(command.Equals("GAMESTATE"))
                    {
                        GameState newState;
                        newState.currentMenu = tokens[1];
                        newState.isInCombat = tokens[2].Equals("1");
                        newState.isWeaponDrawn = tokens[3].Equals("1");
                        newState.isSneaking = tokens[4].Equals("1");

                        recognizer.SetGameState(newState);
                    }


                    if(currentDialogue != null) {
                        recognizer.StartSpeechRecognition(true, currentDialogue);
                    } else {
                        recognizer.StartSpeechRecognition(false, commandList, favoritesList, spellBook, shoutsKnowledge);
                    }
                }
            } catch (Exception ex) {
                Trace.TraceError(ex.ToString());
            }
        }

        private void Recognizer_OnDialogueLineRecognized(RecognitionResult result) {
            string line = result.Text;

            lock (dialogueLock) {
                if (currentDialogue != null) {
                    int idx = currentDialogue.GetLineIndex(result.Grammar);
                    if (idx != -1)
                        SubmitCommand("DIALOGUE|" + currentDialogue.id + "|" + idx);
                } else {
                    string command = favoritesList.GetCommandForResult(result);
                    if(command != null) {
                        SubmitCommand(command);
                        return;
                    }

                    command = spellBook.GetCommandForResult(result);
                    if(command != null) {
                        SubmitCommand(command);
                        return;
                    }

                    command = shoutsKnowledge.GetCommandForResult(result);
                    if (command != null) {
                        SubmitCommand(command);
                        return;
                    }

                    command = commandList.GetCommandForPhrase(result.Grammar);
                    if (command != null) {
                        SubmitCommand(command);
                        return;
                    }

                    Trace.TraceWarning("Nothing regonised for grammer {} ", result.Grammar);
                }
            }
        }
    }
}
