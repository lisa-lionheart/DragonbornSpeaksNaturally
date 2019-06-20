using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Speech.Recognition;

namespace DSN
{
    internal class SpellBook : ISpeechRecognitionGrammarProvider
    {
        private Configuration config;

        enum SpellType
        {
            SPELL,
            POWER
        }

        enum Mode
        {
            SINGLE,
            CONTINOUS
        }

        private struct Entry
        {
            public string id; // Form ID
            public SpellType type;
            public string name;
            public Mode mode;
        }

        Dictionary<Grammar, string> grammars = new Dictionary<Grammar,string>();

        private string mainHand;
        private Boolean enabled;
        private Boolean enableCast;
        private string castPhrase;
        private string activatePhrase;
        private string equipPhrase;

        public SpellBook(Configuration config)
        {
            this.mainHand = config.Get("Favorites", "mainHand", "right");
            this.equipPhrase = config.Get("Favorites", "equipPhrasePrefix", "equip");

            this.enabled = config.Get("SpellBook", "enable", "true").Equals("true");
            this.enableCast = config.Get("SpellBook", "enableCast", "true").Equals("true");
            this.castPhrase = config.Get("SpellBook", "castPhrasePrefix", "cast");
            this.activatePhrase = config.Get("SpellBook", "activatePhrasePrefix", "activate");
        }

        public ICollection<Grammar> GetGrammars(GameState gameState)
        {
            // Only allow casting and equipping when weapon is drawn
            if (!gameState.isMenuOpen() && gameState.isWeaponDrawn)
            {
                return grammars.Keys;
            }
            else
            {
                return new List<Grammar>();
            }
        }

        internal string GetCommandForResult(RecognitionResult result)
        {
            string command = null;
            grammars.TryGetValue(result.Grammar, out command);
            return command;
        }

        internal void UpdateSpells(IEnumerable<string> tokens)
        {
            if(!enabled)
            {
                return;
            }

            grammars.Clear();

            foreach (string spelldesc in tokens)
            {
                string[] parts = spelldesc.Split(',');
                if(parts.Length < 4) {
                    continue;
                }

                Entry spell = new Entry();
                spell.id = parts[0];
                spell.type = (SpellType)Enum.Parse(typeof(SpellType), parts[1]);
                spell.name = parts[2];
                spell.mode = (Mode)Enum.Parse(typeof(Mode), parts[3]);


                if (spell.type == SpellType.POWER)
                {
                    ActivationGrammar(spell);
                } else if(spell.type == SpellType.SPELL)
                {
                    EquipGrammar(spell);

                    // Only one shot spells
                    if (spell.mode == Mode.SINGLE)
                    {
                        ActivationGrammar(spell);
                    }
                }

            }
            Trace.TraceInformation("Spell book updated, " + grammars.Count + " grammers");
        }

        private void EquipGrammar(Entry spell)
        {
            AddGrammar(this.equipPhrase + " " + spell.name, "COMMAND|player.equipspell " + spell.id + " " + mainHand);
            AddGrammar(this.equipPhrase + " " + spell.name + " right", "COMMAND|player.equipspell " + spell.id + " right");
            AddGrammar(this.equipPhrase + " " + spell.name + " left", "COMMAND|player.equipspell " + spell.id + " left");
        }


        private void ActivationGrammar(Entry spell)
        {
            if (enableCast)
            {

                if (spell.type == SpellType.POWER)
                {
                    AddGrammar(this.activatePhrase + " " + spell.name, "COMMAND|trycast " + spell.id + " voice");
                }
                else if (spell.type == SpellType.SPELL)
                {
                    AddGrammar(this.castPhrase + " " + spell.name, "COMMAND|trycast " + spell.id + " " + mainHand);
                }
            }
        }

        private void AddGrammar(string phrase, string command)
        {
            GrammarBuilder gb = new GrammarBuilder();
            gb.Append(phrase);

            Trace.TraceInformation("Phrase '{0}' mapped to '{1}'", phrase, command);
            grammars.Add(new Grammar(gb), command);
        }
    }
}
