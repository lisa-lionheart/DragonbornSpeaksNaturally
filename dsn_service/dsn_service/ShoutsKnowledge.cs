using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Speech.Recognition;

namespace DSN
{
    internal class ShoutsKnowledge : ISpeechRecognitionGrammarProvider
    {
        private Configuration config;

        private Dictionary<Grammar, string> activateGrammars = new Dictionary<Grammar,string>();
        private Dictionary<Grammar, string> equipGrammars = new Dictionary<Grammar, string>();

        private string equipPrefix;
        private Boolean enabled;
        private Boolean enableWordsOfPower;
        private Boolean enableEquip;

        public ShoutsKnowledge(Configuration config)
        {
            this.config = config;
            this.enabled = config.Get("Shouts", "enabled", "true").Equals("true");
            this.enableWordsOfPower = config.Get("Shouts", "wordsOfPower", "true").Equals("true");
            this.enableEquip = config.Get("Shouts", "enableEquip", "true").Equals("true");
            this.equipPrefix = config.Get("Favorites", "equipPhrasePrefix", "equip");
        }

        public ICollection<Grammar> GetGrammars(GameState gameState)
        {
            List<Grammar> result = new List<Grammar>(); 
            if (!gameState.isMenuOpen())
            {
                // Only permit activation when weapon is out and not sneaking
                if(gameState.isWeaponDrawn && !gameState.isSneaking)
                {
                    result.AddRange(activateGrammars.Keys);
                }

                // Always allow equip (when not looking at the menu)
                result.AddRange(equipGrammars.Keys);                
            }
            return result;
        }

        internal void UpdateShouts(IEnumerable<string> enumerable)
        {
            if(!enabled) {
                return;
            }

            equipGrammars.Clear();
            activateGrammars.Clear();

            foreach (string token in enumerable)
            {
                Trace.TraceInformation("Parsing shout " + token);

                string[] parts = token.Split(',');
                if (parts.Length < 3) {
                    continue;
                }

                string id = parts[0];
                string name = parts[1];

                if (enableEquip)
                {
                    AddShoutEquip(name, id);
                }

                if (enableWordsOfPower)
                {
                    // First worrd
                    AddShoutActivation(GetWord(parts, 1), id, 1);

                    // First + second
                    if (parts.Length >= 4)
                    {
                        AddShoutActivation(GetWord(parts, 1) + ", " + GetWord(parts, 2), id, 2);
                    }

                    // All three
                    if (parts.Length >= 5)
                    {
                        AddShoutActivation(GetWord(parts, 1) + ", " + GetWord(parts, 2) + ", " + GetWord(parts, 3), id, 3);
                    }
                }
            }
            Trace.TraceInformation("Shouts updated, " + (equipGrammars.Count+activateGrammars.Count) + " grammers");
        }

        string GetWord(string[] tokens, int i)
        {
            i++;
            return config.Get("Shouts", tokens[i], tokens[i]);
        }

        private void AddShoutEquip(string name, string id)
        {
            GrammarBuilder gb = new GrammarBuilder();
            gb.Append(equipPrefix + " " + name);
            gb.Append("select " + name);

            string command = "COMMAND|player.equipshout " + id;
            equipGrammars.Add(new Grammar(gb), command);
        }

        internal string GetCommandForResult(RecognitionResult result)
        {
            string command = null;
            if (!equipGrammars.TryGetValue(result.Grammar, out command))
            {
                activateGrammars.TryGetValue(result.Grammar, out command);
            }
            return command;
        }


        private void AddShoutActivation(string phrase, string id, int power)
        {
            GrammarBuilder gb = new GrammarBuilder();
            gb.Append(phrase);

            // Attempt to vary the length of the key press to do the full shout maybe
            string command = "player.equipshout " + id + "; sleep 10; press z " + (power * 100);
            Trace.TraceInformation("Phrase '{0}' mapped to '{1}'", phrase, command);
            activateGrammars.Add(new Grammar(gb), "COMMAND|"+command);
        }
    }
}