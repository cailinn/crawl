/**
 * @file
 * @brief Misc religion related functions.
**/

#include "AppHdr.h"

#include "religion.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <sstream>

#include "ability.h"
#include "acquire.h"
#include "act-iter.h"
#include "areas.h"
#include "attitude-change.h"
#include "branch.h"
#include "chardump.h"
#include "coordit.h"
#include "dactions.h"
#include "database.h"
#include "decks.h"
#include "delay.h"
#include "describe-god.h"
#include "dgnevent.h"
#include "dlua.h"
#include "english.h"
#include "env.h"
#include "exercise.h"
#include "godabil.h"
#include "godcompanions.h"
#include "godconduct.h"
#include "goditem.h"
#include "godpassive.h"
#include "godprayer.h"
#include "godwrath.h"
#include "hints.h"
#include "hiscores.h"
#include "invent.h"
#include "itemname.h"
#include "itemprop.h"
#include "items.h"
#include "libutil.h"
#include "makeitem.h"
#include "message.h"
#include "misc.h"
#include "mon-gear.h" // give_shield
#include "mon-place.h"
#include "mutation.h"
#include "notes.h"
#include "output.h"
#include "player-stats.h"
#include "prompt.h"
#include "shopping.h"
#include "skills.h"
#include "spl-book.h"
#include "spl-miscast.h"
#include "sprint.h"
#include "state.h"
#include "stringutil.h"
#include "terrain.h"
#include "transform.h"
#include "view.h"

#ifdef DEBUG_RELIGION
#    define DEBUG_DIAGNOSTICS
#    define DEBUG_GIFTS
#    define DEBUG_SACRIFICE
#    define DEBUG_PIETY
#endif

#define PIETY_HYSTERESIS_LIMIT 1

/// the name of the ally hepliaklqana granted the player
#define HEPLIAKLQANA_ALLY_NAME_KEY "hepliaklqana_ally_name"

// Item offering messages for the gods:
// & is replaced by "is" or "are" as appropriate for the item.
// % is replaced by "s" or "" as appropriate.
// Text between [] only appears if the item already glows.
// First message is if there's no piety gain; second is if piety gain is
// one; third is if piety gain is more than one.
static const char *_Sacrifice_Messages[NUM_GODS][NUM_PIETY_GAIN] =
{
    // No god
    {
        " & eaten by a bored swarm of bugs.",
        " & eaten by a swarm of bugs.",
        " & eaten by a ravening swarm of bugs."
    },
    // Zin
    {
        " barely glow% and disappear%.",
        " glow% silver and disappear%.",
        " glow% blindingly silver and disappear%.",
    },
    // TSO
    {
        " glow% a dingy golden colour and disappear%.",
        " glow% a golden colour and disappear%.",
        " glow% a brilliant golden colour and disappear%.",
    },
    // Kikubaaqudgha
    {
        " convulse% and rot% away.",
        " convulse% madly and rot% away.",
        " convulse% furiously and rot% away.",
    },
    // Yredelemnul
    {
        " slowly crumble% to dust.",
        " crumble% to dust.",
        " turn% to dust in an instant.",
    },
    // Xom (no sacrifices)
    {
        " & eaten by a bored bug.",
        " & eaten by a bug.",
        " & eaten by a greedy bug.",
    },
    // Vehumet
    {
        " fade% into nothingness.",
        " burn% into nothingness.",
        " explode% into nothingness.",
    },
    // Okawaru
    {
        " slowly burn% to ash.",
        " & consumed by flame.",
        " & consumed in a burst of flame.",
    },
    // Makhleb
    {
        " disappear% without a sign.",
        " flare% red and disappear%.",
        " flare% blood-red and disappear%.",
    },
    // Sif Muna
    {
        " & gone without a[dditional] glow.",
        " glow% slightly [brighter ]for a moment, and & gone.",
        " glow% [brighter ]for a moment, and & gone.",
    },
    // Trog
    {
        " & slowly consumed by flames.",
        " & consumed in a column of flame.",
        " & consumed in a roaring column of flame.",
    },
    // Nemelex (no sacrifices)
    {
        " & eaten by a bored swarm of bugs.",
        " & eaten by a swarm of bugs.",
        " & eaten by a ravening swarm of bugs."
    },
    // Elyvilon
    {
        " barely shimmer% and break% into pieces.",
        " shimmer% and break% into pieces.",
        " shimmer% wildly and break% into pieces.",
    },
    // Lugonu
    {
        " disappear% into the void.",
        " & consumed by the void.",
        " & voraciously consumed by the void.",
    },
    // Beogh
    {
        " slowly crumble% into the ground.",
        " crumble% into the ground.",
        " disintegrate% into the ground.",
    },
    // Jiyva
    {
        " slowly dissolve% into ooze.",
        " dissolve% into ooze.",
        " disappear% with a satisfied slurp.",
    },
    // Fedhas
    {
        " & slowly absorbed by the ecosystem.",
        " & absorbed by the ecosystem.",
        " & instantly absorbed by the ecosystem.",
    },
    // Cheibriados (slow god, so better sacrifices are slower)
    {
        " freeze% in place and instantly disappear%.",
        " freeze% in place and disappear%.",
        " freeze% in place and slowly fade%.",
    },
    // Ashenzari
    {
        " flicker% black.",
        " pulsate% black.",          // unused
        " strongly pulsate% black.", // unused
    },
    // Dithmenos
    {
        " slowly dissolves into the shadows.",
        " dissolves into the shadows.",
        " rapidly dissolves into the shadows.",
    },
    // Gozag
    {
        " softly glitters and disappears.",
        " glitters and disappears.",
        " brightly glitters and disappears.",
    },
    // Qazlal
    {
        " slowly dissolves into the earth.",
        " is consumed by the earth.",
        " is consumed by a violent tear in the earth.",
    },
    // Ru
    {
        " disappears in a small burst of power.",
        " disappears in a burst of power",
        " disappears in an immense burst of power",
    },
    // Pakellas
    {
        " slowly breaks apart.",
        " falls apart.",
        " is torn apart in a burst of bright light.",
    },
    // Hepliaklqana
    {
        " slowly fade% into mist.",
        " fade% into mist.",
        " instantly & vanished into memory.",
    },
};

const vector<god_power> god_powers[NUM_GODS] =
{
    // no god
    { },

    // Zin
    { { 1, ABIL_ZIN_RECITE, "recite Zin's Axioms of Law" },
      { 2, ABIL_ZIN_VITALISATION, "call upon Zin for vitalisation" },
      { 3, ABIL_ZIN_IMPRISON, "call upon Zin to imprison the lawless" },
      { 5, ABIL_ZIN_SANCTUARY, "call upon Zin to create a sanctuary" },
      {-1, ABIL_ZIN_DONATE_GOLD, "donate money to Zin" },
      { 7, ABIL_ZIN_CURE_ALL_MUTATIONS,
           "Zin will cure all your mutations... once.",
           "Zin is no longer ready to cure all your mutations." },
    },

    // TSO
    { { 1, "You and your allies can gain power from killing the unholy and evil.",
           "You and your allies can no longer gain power from killing the unholy and evil." },
      { 2, ABIL_TSO_DIVINE_SHIELD, "call upon the Shining One for a divine shield" },
      { 4, ABIL_TSO_CLEANSING_FLAME, "channel blasts of cleansing flame", },
      { 5, ABIL_TSO_SUMMON_DIVINE_WARRIOR, "summon a divine warrior" },
      { 7, ABIL_TSO_BLESS_WEAPON,
           "The Shining One will bless your weapon with holy wrath... once.",
           "The Shining One is no longer ready to bless your weapon." },
    },

    // Kikubaaqudgha
    { { 1, ABIL_KIKU_RECEIVE_CORPSES, "receive cadavers from Kikubaaqudgha" },
      { 2, "Kikubaaqudgha is protecting you from necromantic miscasts and death curses.",
           "Kikubaaqudgha no longer protects you from necromantic miscasts or death curses." },
      { 4, "Kikubaaqudgha is protecting you from unholy torment.",
           "Kikubaaqudgha will no longer protect you from unholy torment." },
      { 5, ABIL_KIKU_TORMENT, "invoke torment by sacrificing a corpse" },
      { 7, ABIL_KIKU_BLESS_WEAPON,
           "Kikubaaqudgha will grant you a Necronomicon or bloody your weapon with pain... once.",
           "Kikubaaqudgha is no longer ready to enhance your necromancy." },
      { 7, ABIL_KIKU_GIFT_NECRONOMICON,
           "Kikubaaqudgha will grant you a Necronomicon.",
           "Kikubaaqudgha is no longer ready to enhance your necromancy." },
    },

    // Yredelemnul
    { { 1, ABIL_YRED_ANIMATE_REMAINS, "animate remains" },
      { 2, ABIL_YRED_RECALL_UNDEAD_SLAVES, "recall your undead slaves" },
      { 2, ABIL_YRED_INJURY_MIRROR, "mirror injuries on your foes" },
      { 3, ABIL_YRED_ANIMATE_DEAD, "animate legions of the dead" },
      { 4, ABIL_YRED_DRAIN_LIFE, "drain ambient lifeforce" },
      { 5, ABIL_YRED_ENSLAVE_SOUL, "enslave living souls" },
    },

    // Xom
    { },
    // Vehumet
    { { 1, "gain magical power from killing" },
      { 3, "Vehumet is aiding your destructive magics.",
           "Vehumet will no longer aid your destructive magics." },
      { 4, "Vehumet is extending the range of your destructive magics.",
           "Vehumet will no longer extend the range of your destructive magics." },
    },

    // Okawaru
    { { 1, ABIL_OKAWARU_HEROISM, "gain great but temporary skills" },
      { 5, ABIL_OKAWARU_FINESSE, "speed up your combat" },
    },

    // Makhleb
    { { 1, "gain health from killing" },
      { 2, ABIL_MAKHLEB_MINOR_DESTRUCTION, "harness Makhleb's destructive might" },
      { 3, ABIL_MAKHLEB_LESSER_SERVANT_OF_MAKHLEB, "summon a lesser servant of Makhleb" },
      { 4, ABIL_MAKHLEB_MAJOR_DESTRUCTION, "hurl Makhleb's greater destruction" },
      { 5, ABIL_MAKHLEB_GREATER_SERVANT_OF_MAKHLEB, "summon a greater servant of Makhleb" },
    },

    // Sif Muna
    { { 1, ABIL_SIF_MUNA_CHANNEL_ENERGY, "tap ambient magical fields" },
      { 2, "Sif Muna is protecting you from the effects of miscast magic.",
           "Sif Muna no longer protects you from the effects of miscast magic." },
      { 4, ABIL_SIF_MUNA_FORGET_SPELL, "freely open your mind to new spells",
          "forget spells at will" },
    },

    // Trog
    {
      { 1, ABIL_TROG_BERSERK, "go berserk at will" },
      { 2, ABIL_TROG_REGEN_MR, "call upon Trog for regeneration and protection "
                               "from hostile enchantments" },
      { 4, ABIL_TROG_BROTHERS_IN_ARMS, "call in reinforcements" },
      {-1, ABIL_TROG_BURN_SPELLBOOKS, "call upon Trog to burn spellbooks in your surroundings" },
    },

    // Nemelex
    { { 3, ABIL_NEMELEX_TRIPLE_DRAW, "choose one out of three cards" },
      { 4, ABIL_NEMELEX_DEAL_FOUR, "deal four cards at a time" },
      { 5, ABIL_NEMELEX_STACK_FIVE, "order the top five cards of a deck, losing the rest",
                                    "stack decks" },
    },

    // Elyvilon
    { { 1, ABIL_ELYVILON_LESSER_HEALING, "provide lesser healing for yourself" },
      { 2, ABIL_ELYVILON_HEAL_OTHER, "heal and attempt to pacify others" },
      { 3, ABIL_ELYVILON_PURIFICATION, "purify yourself" },
      { 4, ABIL_ELYVILON_GREATER_HEALING, "provide greater healing for yourself" },
      { 5, ABIL_ELYVILON_DIVINE_VIGOUR, "call upon Elyvilon for divine vigour" },
      { 1, ABIL_ELYVILON_LIFESAVING, "call on Elyvilon to save your life" },
    },

    // Lugonu
    { { 1, ABIL_LUGONU_ABYSS_EXIT, "depart the Abyss", "depart the Abyss at will" },
      { 2, ABIL_LUGONU_BEND_SPACE, "bend space around yourself" },
      { 3, ABIL_LUGONU_BANISH, "banish your foes" },
      { 4, ABIL_LUGONU_CORRUPT, "corrupt the fabric of space" },
      { 5, ABIL_LUGONU_ABYSS_ENTER, "gate yourself to the Abyss" },
      { 7, ABIL_LUGONU_BLESS_WEAPON, "Lugonu will corrupt your weapon with distortion... once.",
                                     "Lugonu is no longer ready to corrupt your weapon." },
    },

    // Beogh
    { { 1, "Beogh aids your use of armour.",
           "Beogh no longer aids your use of armour." },
      { 2, ABIL_BEOGH_SMITING, "smite your foes" },
      { 3, "gain orcish followers" },
      { 4, ABIL_BEOGH_RECALL_ORCISH_FOLLOWERS, "recall your orcish followers" },
      { 5, "walk on water" },
      { 5, ABIL_BEOGH_GIFT_ITEM, "give items to your followers" },
    },

    // Jiyva
    { { 1, ABIL_JIYVA_CALL_JELLY, "request a jelly" },
      { 2, ABIL_JIYVA_JELLY_PARALYSE, "temporarily halt your jellies' item consumption" },
      { 4, ABIL_JIYVA_SLIMIFY, "turn your foes to slime" },
      { 5, ABIL_JIYVA_CURE_BAD_MUTATION, "call upon Jiyva to remove your harmful mutations" },
    },

    // Fedhas
    {
      { 0, "pray to speed up the decay of corpses" },
      { 1, ABIL_FEDHAS_EVOLUTION, "induce evolution" },
      { 2, ABIL_FEDHAS_SUNLIGHT, "call sunshine" },
      { 3, ABIL_FEDHAS_PLANT_RING, "cause a ring of plants to grow" },
      { 4, ABIL_FEDHAS_SPAWN_SPORES, "spawn explosive spores" },
      { 5, ABIL_FEDHAS_RAIN, "control the weather" },
    },

    // Cheibriados
    { { 0, ABIL_CHEIBRIADOS_TIME_BEND, "bend time to slow others" },
      { 1, "Cheibriados slows and strengthens your metabolism.",
           "Cheibriados no longer slows and strengthens your metabolism." },
      { 3, ABIL_CHEIBRIADOS_DISTORTION, "warp the flow of time around you" },
      { 4, ABIL_CHEIBRIADOS_SLOUCH, "inflict damage on those overly hasty" },
      { 5, ABIL_CHEIBRIADOS_TIME_STEP, "step out of the time flow" },
    },

    // Ashenzari
    { { 0, ABIL_ASHENZARI_CURSE, "curse your items" },
      { 1, ABIL_ASHENZARI_SCRYING, "scry through walls" },
      { 2, "The more cursed you are, the more Ashenzari supports your skills.",
           "Ashenzari no longer supports your skills." },
      { 3, "Ashenzari reveals the unseen.",
           "Ashenzari no longer reveals the unseen." },
      { 4, "Ashenzari keeps your mind clear.",
           "Ashenzari no longer keeps your mind clear." },
      { 5, ABIL_ASHENZARI_TRANSFER_KNOWLEDGE,
           "Ashenzari helps you to reconsider your skills.",
           "Ashenzari no longer helps you to reconsider your skills." },
    },

    // Dithmenos
    { { 2, ABIL_DITHMENOS_SHADOW_STEP, "step into the shadows of nearby creatures" },
      { 3, "You now sometimes bleed smoke when heavily injured by enemies.",
           "You no longer bleed smoke." },
      { 4, "Your shadow now sometimes tangibly mimics your actions.",
           "Your shadow no longer tangibly mimics your actions." },
      { 5, ABIL_DITHMENOS_SHADOW_FORM, "transform into a swirling mass of shadows" },
    },

    // Gozag
    { { 0, ABIL_GOZAG_POTION_PETITION, "petition Gozag for potion effects" },
      { 0, ABIL_GOZAG_CALL_MERCHANT, "fund merchants seeking to open stores in the dungeon" },
      { 0, ABIL_GOZAG_BRIBE_BRANCH, "bribe branches to halt enemies' attacks and recruit allies" },
    },

    // Qazlal
    {
      { 0, "Qazlal grants you immunity to your own clouds." },
      { 1, "You are surrounded by a storm.", "Your storm dissipates completely." },
      { 2, ABIL_QAZLAL_UPHEAVAL, "call upon nature to destroy your foes" },
      { 3, ABIL_QAZLAL_ELEMENTAL_FORCE, "give life to nearby clouds" },
      { 4, "The storm surrounding you is powerful enough to repel missilies.",
           "The storm surrounding you is now too weak to repel missiles." },
      { 4, "You adapt resistances upon receiving elemental damage.",
           "You no longer adapt resistances upon receiving elemental damage." },
      { 5, ABIL_QAZLAL_DISASTER_AREA, "call upon nature's wrath in a wide area around you" },
    },

    // Ru
    { { 1, "You exude an aura of power that intimidates your foes.",
           "You no longer exude an aura of power that intimidates your foes." },
      { 2, "Your aura of power can strike those that harm you.",
           "Your aura of power no longer strikes those that harm you." },
      { 3, ABIL_RU_DRAW_OUT_POWER, "heal your body and restore your magic" },
      { 4, ABIL_RU_POWER_LEAP, "gather your power into a mighty leap" },
      { 5, ABIL_RU_APOCALYPSE, "wreak a terrible wrath on your foes" },
    },
    // Pakellas
    {
      { 1, ABIL_PAKELLAS_QUICK_CHARGE,
           "spend your magic to charge your devices" },
      { 2, "Pakellas will collect and distill excess magic from your kills.",
           "Pakellas no longer collects and distills excess magic from your "
           "kills." },
      { 3, ABIL_PAKELLAS_DEVICE_SURGE,
           "spend magic to empower your devices" },
      { 7, ABIL_PAKELLAS_SUPERCHARGE,
           "Pakellas will now supercharge a wand or rod... once.",
           "Pakellas is no longer ready to supercharge a wand or rod." },
    },

    // Hepliaklqana
    { { 0, ABIL_HEPLIAKLQANA_RECALL, "recall your ancestor" },
      { 3, ABIL_HEPLIAKLQANA_ROMANTICIZE, "heal & protect your ancestor" },
      { 5, ABIL_HEPLIAKLQANA_TRANSFERENCE, "trade places with your ancestor" }
    },
};

vector<god_power> get_god_powers(god_type god)
{
    vector<god_power> ret;
    for (const auto& power : god_powers[god])
    {
        if (!(power.abil != ABIL_NON_ABILITY
              && fixup_ability(power.abil) == ABIL_NON_ABILITY))
        {
            ret.push_back(power);
        }
    }
    return ret;
}

/**
 * Print a description of getting/losing this power.
 *
 * @param gaining If true, use this->gain; otherwise, use this->loss.
 * @param fmt  A string containing "%s" that will be used as a format
 *             string with our string as parameter; it is not used if
 *             our string begins with a capital letter. IF THIS DOES
 *             NOT CONTAIN "%s", OR CONTAINS OTHER FORMAT SPECIFIERS,
 *             BEHAVIOUR IS UNDEFINED.
 * @return a string suitable for being read by the user.
 */
void god_power::display(bool gaining, const char* fmt) const
{
    // hack: don't mention the necronomicon alone unless it wasn't
    // already mentioned by the other message
    if (abil == ABIL_KIKU_GIFT_NECRONOMICON
        && you.species != SP_FELID)
    {
        return;
    }
    const char* str = gaining ? gain : loss;
    if (isupper(str[0]))
        god_speaks(you.religion, str);
    else
        god_speaks(you.religion, make_stringf(fmt, str).c_str());
}

static void _place_delayed_monsters();

bool is_evil_god(god_type god)
{
    return god == GOD_KIKUBAAQUDGHA
           || god == GOD_MAKHLEB
           || god == GOD_YREDELEMNUL
           || god == GOD_BEOGH
           || god == GOD_LUGONU;
}

bool is_good_god(god_type god)
{
    return god == GOD_ZIN
           || god == GOD_SHINING_ONE
           || god == GOD_ELYVILON;
}

bool is_chaotic_god(god_type god)
{
    return god == GOD_XOM
           || god == GOD_MAKHLEB
           || god == GOD_LUGONU
           || god == GOD_JIYVA;
}

bool is_unknown_god(god_type god)
{
    return god == GOD_NAMELESS;
}

bool is_unavailable_god(god_type god)
{
    return god == GOD_JIYVA && jiyva_is_dead();
}

bool god_has_name(god_type god)
{
    return god != GOD_NO_GOD && god != GOD_NAMELESS;
}

god_type random_god()
{
    god_type god;

    do
    {
        god = static_cast<god_type>(random2(NUM_GODS - 1) + 1);
    }
    while (is_unavailable_god(god));

    return god;
}

string get_god_likes(god_type which_god, bool verbose)
{
    if (which_god == GOD_NO_GOD || which_god == GOD_XOM)
        return "";

    string text = uppercase_first(god_name(which_god));
    vector<string> likes;
    vector<string> really_likes;

    // Unique/unusual piety gain methods first.
    switch (which_god)
    {
    case GOD_SIF_MUNA:
        likes.emplace_back("you train your various spell casting skills");
        break;

    case GOD_FEDHAS:
    {
        string like = "you promote the decay of nearby corpses";
        if (verbose)
           like += " by <w>p</w>raying";
        likes.push_back(like);
        break;
    }

    case GOD_TROG:
    {
        string like = "you destroy spellbooks";
        if (verbose)
           like += " via the <w>a</w> command";
        likes.push_back(like);
        break;
    }

    case GOD_JIYVA:
    {
        string like = "you sacrifice items";
        if (verbose)
            like += " by allowing slimes to consume them";
        likes.push_back(like);
        break;
    }

    case GOD_CHEIBRIADOS:
    {
        string like = "you kill fast things";
        if (verbose)
            like += ", relative to your speed";
        likes.push_back(like);
        break;
    }

    case GOD_ASHENZARI:
        likes.emplace_back("you explore the world (preferably while bound by "
                           "curses)");
        break;

    case GOD_SHINING_ONE:
        likes.emplace_back("you meet creatures to determine whether they need "
                           "to be eradicated");
        break;

    case GOD_LUGONU:
        likes.emplace_back("you banish creatures to the Abyss");
        break;

    case GOD_GOZAG:
        likes.emplace_back("you collect gold");
        break;

    case GOD_RU:
      likes.emplace_back("you make personal sacrifices");
      break;

    default:
        break;
    }

    switch (which_god)
    {
    case GOD_ZIN:
    {
        string like = "you donate money";
        likes.push_back(like);
        break;
    }

    case GOD_BEOGH:
    {
        string like = "you bless dead orcs";
        if (verbose)
            like += " (by standing over their remains and <w>p</w>raying)";
        likes.push_back(like);
        break;
    }

    case GOD_NEMELEX_XOBEH:
    case GOD_HEPLIAKLQANA:
    case GOD_ELYVILON:
        likes.emplace_back("you explore the world");
        break;

    default:
        break;
    }

    switch (which_god)
    {
    case GOD_MAKHLEB:
    case GOD_LUGONU:
    case GOD_QAZLAL:
    case GOD_PAKELLAS:
        likes.emplace_back("you or your allies kill living beings");
        break;

    case GOD_TROG:
        likes.emplace_back("you or your god-given allies kill living beings");
        break;

    case GOD_YREDELEMNUL:
    case GOD_KIKUBAAQUDGHA:
        likes.emplace_back("you or your undead slaves kill living beings");
        break;

    case GOD_BEOGH:
        likes.emplace_back("you or your allied orcs kill living beings");
        break;

    case GOD_OKAWARU:
    case GOD_VEHUMET:
    case GOD_DITHMENOS:
        likes.emplace_back("you kill living beings");
        break;

    default:
        break;
    }

    switch (which_god)
    {
    case GOD_ZIN:
        likes.emplace_back("you or your allies kill unclean or chaotic beings");
        break;

    case GOD_SHINING_ONE:
        likes.emplace_back("you or your allies kill living unholy or evil beings");
        break;

    default:
        break;
    }

    switch (which_god)
    {
    case GOD_SHINING_ONE:
    case GOD_MAKHLEB:
    case GOD_LUGONU:
    case GOD_QAZLAL:
    case GOD_PAKELLAS:
        likes.emplace_back("you or your allies kill the undead");
        break;

    case GOD_BEOGH:
        likes.emplace_back("you or your allied orcs kill the undead");
        break;

    case GOD_OKAWARU:
    case GOD_VEHUMET:
    case GOD_DITHMENOS:
        likes.emplace_back("you kill the undead");
        break;

    default:
        break;
    }

    switch (which_god)
    {
    case GOD_SHINING_ONE:
    case GOD_MAKHLEB:
    case GOD_LUGONU:
    case GOD_QAZLAL:
    case GOD_PAKELLAS:
        likes.emplace_back("you or your allies kill demons");
        break;

    case GOD_TROG:
        likes.emplace_back("you or your god-given allies kill demons");
        break;

    case GOD_KIKUBAAQUDGHA:
        likes.emplace_back("you or your undead slaves kill demons");
        break;

    case GOD_BEOGH:
        likes.emplace_back("you or your allied orcs kill demons");
        break;

    case GOD_OKAWARU:
    case GOD_VEHUMET:
    case GOD_DITHMENOS:
        likes.emplace_back("you kill demons");
        break;

    default:
        break;
    }

    switch (which_god)
    {
    case GOD_YREDELEMNUL:
        likes.emplace_back("you or your undead slaves kill artificial beings");
        break;

    default:
        break;
    }

    switch (which_god)
    {
    case GOD_MAKHLEB:
    case GOD_LUGONU:
    case GOD_QAZLAL:
    case GOD_PAKELLAS:
        likes.emplace_back("you or your allies kill holy beings");
        break;

    case GOD_TROG:
        likes.emplace_back("you or your god-given allies kill holy beings");
        break;

    case GOD_YREDELEMNUL:
        likes.emplace_back("your undead slaves kill holy beings");
        break;

    case GOD_KIKUBAAQUDGHA:
        likes.emplace_back("you or your undead slaves kill holy beings");
        break;

    case GOD_BEOGH:
        likes.emplace_back("you or your allied orcs kill holy beings");
        break;

    case GOD_OKAWARU:
    case GOD_VEHUMET:
    case GOD_DITHMENOS:
        likes.emplace_back("you kill holy beings");
        break;

    default:
        break;
    }

    // Especially appreciated kills.
    switch (which_god)
    {
    case GOD_YREDELEMNUL:
        really_likes.emplace_back("you kill holy beings");
        break;

    case GOD_BEOGH:
        really_likes.emplace_back("you kill the priests of other religions");
        break;

    case GOD_TROG:
        really_likes.emplace_back("you kill wizards and other users of magic");
        break;

    case GOD_DITHMENOS:
        really_likes.emplace_back("you kill beings that bring fire to the "
                                  "dungeon");
        break;
    default:
        break;
    }

    if (likes.empty() && really_likes.empty())
        text += " doesn't like anything? This is a bug; please report it.";
    else
    {
        text += " likes it when ";
        text += comma_separated_line(likes.begin(), likes.end());
        text += ".";

        if (!really_likes.empty())
        {
            text += " ";
            text += uppercase_first(god_name(which_god));

            text += " especially likes it when ";
            text += comma_separated_line(really_likes.begin(),
                                         really_likes.end());
            text += ".";
        }
    }

    return text;
}

string get_god_dislikes(god_type which_god, bool /*verbose*/)
{
    // Return early for the special cases.
    if (which_god == GOD_NO_GOD || which_god == GOD_XOM)
        return "";

    string text;
    vector<string> dislikes;        // Piety loss
    vector<string> really_dislikes; // Penance

    if (god_hates_cannibalism(which_god))
        really_dislikes.emplace_back("you perform cannibalism");

    if (is_good_god(which_god))
    {
        really_dislikes.emplace_back("you desecrate holy remains");

        if (which_god == GOD_SHINING_ONE)
            really_dislikes.emplace_back("you drink blood");
        else
            dislikes.emplace_back("you drink blood");

        really_dislikes.emplace_back("you use necromancy");
        really_dislikes.emplace_back("you use unholy magic or items");
        really_dislikes.emplace_back("you attack non-hostile holy beings");
        really_dislikes.emplace_back("you or your allies kill non-hostile holy beings");

        if (which_god == GOD_ZIN)
            dislikes.emplace_back("you attack neutral beings");
        else
            really_dislikes.emplace_back("you attack neutral beings");
    }

    switch (which_god)
    {
    case GOD_ZIN:     case GOD_SHINING_ONE:  case GOD_ELYVILON:
    case GOD_OKAWARU:
        really_dislikes.emplace_back("you attack allies");
        break;

    case GOD_BEOGH:
        really_dislikes.emplace_back("you attack allied orcs");
        break;

    case GOD_JIYVA:
        really_dislikes.emplace_back("you attack fellow slimes");
        break;

    case GOD_FEDHAS:
        dislikes.emplace_back("you or your allies destroy plants");
        dislikes.emplace_back("allied flora die");
        really_dislikes.emplace_back("you use necromancy on corpses, chunks or skeletons");
        break;

    case GOD_SIF_MUNA:
        really_dislikes.emplace_back("you destroy spellbooks");
        break;

    case GOD_DITHMENOS:
        dislikes.emplace_back("you use fiery magic or items");
        break;

    default:
        break;
    }

    switch (which_god)
    {
    case GOD_ELYVILON:
        dislikes.emplace_back("you allow allies to die");
        break;

    default:
        break;
    }

    switch (which_god)
    {
    case GOD_ZIN:
        dislikes.emplace_back("you deliberately mutate yourself");
        really_dislikes.emplace_back("you transform yourself");
        really_dislikes.emplace_back("you polymorph monsters");
        really_dislikes.emplace_back("you use unclean or chaotic magic or items");
        really_dislikes.emplace_back("you butcher sentient beings");
        dislikes.emplace_back("you or your allies attack monsters in a "
                              "sanctuary");
        break;

    case GOD_SHINING_ONE:
        really_dislikes.emplace_back("you poison monsters");
        really_dislikes.emplace_back("you attack intelligent monsters in an "
                                     "unchivalric manner");
        break;

    case GOD_ELYVILON:
        really_dislikes.emplace_back("you kill living things while asking for "
                                     "your life to be spared");
        break;

    case GOD_YREDELEMNUL:
        really_dislikes.emplace_back("you use holy magic or items");
        break;

    case GOD_PAKELLAS:
        really_dislikes.emplace_back("you channel magical energy");
        break;

    case GOD_TROG:
        really_dislikes.emplace_back("you memorise spells");
        really_dislikes.emplace_back("you attempt to cast spells");
        really_dislikes.emplace_back("you train magic skills");
        break;

    case GOD_BEOGH:
        really_dislikes.emplace_back("you desecrate orcish remains");
        really_dislikes.emplace_back("you destroy orcish idols");
        break;

    case GOD_JIYVA:
        really_dislikes.emplace_back("you kill slimes");
        break;

    case GOD_CHEIBRIADOS:
        really_dislikes.emplace_back("you hasten yourself or others");
        really_dislikes.emplace_back("use unnaturally quick items");
        break;

    default:
        break;
    }

    if (dislikes.empty() && really_dislikes.empty())
        return "";

    if (!dislikes.empty())
    {
        text += uppercase_first(god_name(which_god));
        text += " dislikes it when ";
        text += comma_separated_line(dislikes.begin(), dislikes.end(),
                                     " or ", ", ");
        text += ".";

        if (!really_dislikes.empty())
            text += " ";
    }

    if (!really_dislikes.empty())
    {
        text += uppercase_first(god_name(which_god));
        text += " strongly dislikes it when ";
        text += comma_separated_line(really_dislikes.begin(),
                                     really_dislikes.end(),
                                     " or ", ", ");
        text += ".";
    }

    return text;
}


god_iterator::god_iterator() :
    i(0) { } // might be ok to start with GOD_ZIN instead?

god_iterator::operator bool() const
{
    return i < NUM_GODS;
}

const god_type god_iterator::operator*() const
{
    if (i < NUM_GODS)
        return (god_type)i;
    else
        return GOD_NO_GOD;
}

const god_type god_iterator::operator->() const
{
    return **this;
}

god_iterator& god_iterator::operator++()
{
    ++i;
    return *this;
}

god_iterator god_iterator::operator++(int)
{
    god_iterator copy = *this;
    ++(*this);
    return copy;
}


bool active_penance(god_type god)
{
    // Ashenzari's penance isn't active; Nemelex's penance is only active
    // when the penance counter is above 100; good gods only have active
    // wrath when they hate your current god.
    return player_under_penance(god)
           && !is_unavailable_god(god)
           && god != GOD_ASHENZARI
           && god != GOD_GOZAG
           && god != GOD_RU
           && god != GOD_HEPLIAKLQANA
           && (god != GOD_NEMELEX_XOBEH || you.penance[god] > 100)
           && (god == you.religion && !is_good_god(god)
               || god_hates_your_god(god, you.religion));
}

// True for gods whose wrath is passive and expires with XP gain.
bool xp_penance(god_type god)
{
    return player_under_penance(god)
           && !is_unavailable_god(god)
           && (god == GOD_ASHENZARI
               || god == GOD_GOZAG
               || god == GOD_HEPLIAKLQANA);
}

void dec_penance(god_type god, int val)
{
    if (val <= 0 || you.penance[god] <= 0)
        return;

#ifdef DEBUG_PIETY
    mprf(MSGCH_DIAGNOSTICS, "Decreasing penance by %d", val);
#endif
    if (you.penance[god] <= val)
    {
        const bool had_halo = have_passive(passive_t::halo);
        const bool had_umbra = have_passive(passive_t::umbra);

        you.penance[god] = 0;

        mark_milestone("god.mollify",
                       "mollified " + god_name(god) + ".");

        const bool dead_jiyva = (god == GOD_JIYVA && jiyva_is_dead());

        simple_god_message(
            make_stringf(" seems mollified%s.",
                         dead_jiyva ? ", and vanishes" : "").c_str(),
            god);

        if (dead_jiyva)
            add_daction(DACT_REMOVE_JIYVA_ALTARS);

        take_note(Note(NOTE_MOLLIFY_GOD, god));

        if (you_worship(god))
        {
            // Redraw piety display and, in case the best skill is Invocations,
            // redraw the god title.
            you.redraw_title = true;

            if (!had_halo && have_passive(passive_t::halo))
            {
                mprf(MSGCH_GOD, "Your divine halo returns!");
                invalidate_agrid(true);
            }
            if (!had_umbra && have_passive(passive_t::umbra))
            {
                mprf(MSGCH_GOD, "Your aura of darkness returns!");
                invalidate_agrid(true);
            }
            // Orcish bonuses are now once more effective.
            if (have_passive(passive_t::bonus_ac))
                 you.redraw_armour_class = true;
            if (have_passive(passive_t::sinv))
            {
                mprf(MSGCH_GOD, "Your vision regains its divine sight.");
                autotoggle_autopickup(false);
            }
            if (have_passive(passive_t::stat_boost))
            {
                simple_god_message(" restores the support of your attributes.");
                redraw_screen();
                notify_stat_change();
            }

            // TSO's halo is once more available.
            if (god == GOD_SHINING_ONE
                     && you.piety >= piety_breakpoint(0))
            {
                mprf(MSGCH_GOD, "Your divine halo returns!");
                invalidate_agrid(true);
            }
            else if (god == GOD_QAZLAL && you.piety >= piety_breakpoint(0))
            {
                mprf(MSGCH_GOD, "A storm instantly forms around you!");
                you.redraw_armour_class = true; // also handles shields
            }
            // When you've worked through all your penance, you get
            // another chance to make hostile slimes strict neutral.
            else if (god == GOD_JIYVA)
                add_daction(DACT_SLIME_NEW_ATTEMPT);
            else if (god == GOD_PAKELLAS)
                pakellas_id_device_charges();
        }
        else if (god == GOD_PAKELLAS)
        {
            // Penance just ended w/o worshipping Pakellas;
            // notify the player that MP regeneration will start again.
            mprf(MSGCH_GOD, god, "You begin regenerating magic.");
        }
    }
    else if (god == GOD_NEMELEX_XOBEH && you.penance[god] > 100)
    { // Nemelex's penance works actively only until 100
        if ((you.penance[god] -= val) > 100)
            return;
        mark_milestone("god.mollify",
                       "partially mollified " + god_name(god) + ".");
        simple_god_message(" seems mollified... mostly.", god);
        take_note(Note(NOTE_MOLLIFY_GOD, god));
    }
    else
    {
        you.penance[god] -= val;
        return;
    }

    // We only get this far if we just mollified a god.
    // If we just mollified a god, see if we have any angry gods left.
    // If we don't, clear the stored wrath / XP counter.
    god_iterator it;
    for (; it; ++it)
    {
        if (active_penance(*it))
            break;
    }

    if (it)
        return;

    you.attribute[ATTR_GOD_WRATH_COUNT] = 0;
    you.attribute[ATTR_GOD_WRATH_XP] = 0;
}

void dec_penance(int val)
{
    dec_penance(you.religion, val);
}

static bool _need_water_walking()
{
    return you.ground_level() && you.species != SP_MERFOLK
           && grd(you.pos()) == DNGN_DEEP_WATER;
}

bool jiyva_is_dead()
{
    return you.royal_jelly_dead
           && !you_worship(GOD_JIYVA) && !you.penance[GOD_JIYVA];
}

void set_penance_xp_timeout()
{
    if (you.attribute[ATTR_GOD_WRATH_XP] > 0)
        return;

    // TODO: make this more random?
    you.attribute[ATTR_GOD_WRATH_XP] +=
        max(div_rand_round(exp_needed(you.experience_level + 1)
                          - exp_needed(you.experience_level),
                          200),
            1);
}

static void _inc_penance(god_type god, int val)
{
    if (val <= 0)
        return;

    if (!player_under_penance(god))
    {
        god_acting gdact(god, true);

        take_note(Note(NOTE_PENANCE, god));

        const bool had_halo = have_passive(passive_t::halo);
        const bool had_umbra = have_passive(passive_t::umbra);

        you.penance[god] += val;
        you.penance[god] = min((uint8_t)MAX_PENANCE, you.penance[god]);

        if (had_halo && !have_passive(passive_t::halo))
        {
            mprf(MSGCH_GOD, god, "Your divine halo fades away.");
            invalidate_agrid();
        }
        if (had_umbra && !have_passive(passive_t::umbra))
        {
            mprf(MSGCH_GOD, god, "Your aura of darkness fades away.");
            invalidate_agrid();
        }

        // Orcish bonuses don't apply under penance.
        if (will_have_passive(passive_t::bonus_ac))
            you.redraw_armour_class = true;

        if (will_have_passive(passive_t::water_walk)
            && _need_water_walking() && !have_passive(passive_t::water_walk))
        {
            fall_into_a_pool(grd(you.pos()));
        }

        if (will_have_passive(passive_t::stat_boost))
        {
            redraw_screen();
            notify_stat_change();
        }

        // Neither does Trog's regeneration or magic resistance.
        if (god == GOD_TROG)
        {
            if (you.duration[DUR_TROGS_HAND])
                trog_remove_trogs_hand();

            make_god_gifts_disappear(); // only on level
        }
        // Neither does Zin's divine stamina.
        else if (god == GOD_ZIN)
        {
            if (you.duration[DUR_DIVINE_STAMINA])
                zin_remove_divine_stamina();
        }
        // Neither does TSO's halo or divine shield.
        else if (god == GOD_SHINING_ONE)
        {
            if (you.duration[DUR_DIVINE_SHIELD])
                tso_remove_divine_shield();

            make_god_gifts_disappear(); // only on level
        }
        // Neither does Ely's divine vigour.
        else if (god == GOD_ELYVILON)
        {
            if (you.duration[DUR_DIVINE_VIGOUR])
                elyvilon_remove_divine_vigour();
        }
        else if (god == GOD_JIYVA)
        {
            if (you.duration[DUR_SLIMIFY])
                you.duration[DUR_SLIMIFY] = 0;
        }
        else if (god == GOD_QAZLAL)
        {
            if (you.piety >= piety_breakpoint(0))
            {
                mprf(MSGCH_GOD, god, "The storm surrounding you dissipates.");
                you.redraw_armour_class = true;
            }
            if (you.duration[DUR_QAZLAL_FIRE_RES])
            {
                mprf(MSGCH_DURATION, "Your resistance to fire fades away.");
                you.duration[DUR_QAZLAL_FIRE_RES] = 0;
            }
            if (you.duration[DUR_QAZLAL_COLD_RES])
            {
                mprf(MSGCH_DURATION, "Your resistance to cold fades away.");
                you.duration[DUR_QAZLAL_COLD_RES] = 0;
            }
            if (you.duration[DUR_QAZLAL_ELEC_RES])
            {
                mprf(MSGCH_DURATION,
                     "Your resistance to electricity fades away.");
                you.duration[DUR_QAZLAL_ELEC_RES] = 0;
            }
            if (you.duration[DUR_QAZLAL_AC])
            {
                mprf(MSGCH_DURATION,
                     "Your resistance to physical damage fades away.");
                you.duration[DUR_QAZLAL_AC] = 0;
                you.redraw_armour_class = true;
            }
        }
        else if (god == GOD_PAKELLAS)
        {
            if (you.duration[DUR_DEVICE_SURGE])
                you.duration[DUR_DEVICE_SURGE] = 0;
        }

        if (you_worship(god))
        {
            // Redraw piety display and, in case the best skill is Invocations,
            // redraw the god title.
            you.redraw_title = true;
        }
    }
    else
    {
        you.penance[god] += val;
        you.penance[god] = min((uint8_t)MAX_PENANCE, you.penance[god]);
    }

    set_penance_xp_timeout();
}

static void _inc_penance(int val)
{
    _inc_penance(you.religion, val);
}

static void _set_penance(god_type god, int val)
{
    you.penance[god] = val;
}

static void _inc_gift_timeout(int val)
{
    if (200 - you.gift_timeout < val)
        you.gift_timeout = 200;
    else
        you.gift_timeout += val;
}

// These are sorted in order of power.
static monster_type _yred_servants[] =
{
    MONS_MUMMY, MONS_WIGHT, MONS_FLYING_SKULL, MONS_WRAITH,
    MONS_VAMPIRE, MONS_PHANTASMAL_WARRIOR, MONS_SKELETAL_WARRIOR,
    MONS_FLAYED_GHOST, MONS_VAMPIRE_KNIGHT, MONS_GHOUL, MONS_BONE_DRAGON,
    MONS_PROFANE_SERVITOR
};

#define MIN_YRED_SERVANT_THRESHOLD 3
#define MAX_YRED_SERVANT_THRESHOLD ARRAYSZ(_yred_servants)

static bool _yred_high_level_servant(monster_type type)
{
    return type == MONS_BONE_DRAGON
           || type == MONS_PROFANE_SERVITOR;
}

int yred_random_servants(unsigned int threshold, bool force_hostile)
{
    if (threshold == 0)
    {
        if (force_hostile)
        {
            // This implies wrath - scale the threshold with XL.
            threshold =
                MIN_YRED_SERVANT_THRESHOLD
                + (MAX_YRED_SERVANT_THRESHOLD - MIN_YRED_SERVANT_THRESHOLD)
                  * you.experience_level / 27;
        }
        else
            threshold = ARRAYSZ(_yred_servants);
    }
    else
    {
        threshold = min(static_cast<unsigned int>(ARRAYSZ(_yred_servants)),
                        threshold);
    }

    const unsigned int servant = random2(threshold);

    // Skip some of the weakest servants, once the threshold is high.
    if ((servant + 2) * 2 < threshold)
        return -1;

    monster_type mon_type = _yred_servants[servant];

    // Cap some of the strongest servants.
    if (!force_hostile && _yred_high_level_servant(mon_type))
    {
        int current_high_level = 0;
        for (auto &entry : companion_list)
        {
            monster* mons = monster_by_mid(entry.first);
            if (!mons)
                mons = &entry.second.mons.mons;
            if (_yred_high_level_servant(mons->type))
                current_high_level++;
        }

        if (current_high_level >= 3)
            return -1;
    }

    int how_many = (mon_type == MONS_FLYING_SKULL) ? 2 + random2(4)
                                                   : 1;

    mgen_data mg(mon_type, !force_hostile ? BEH_FRIENDLY : BEH_HOSTILE,
                 !force_hostile ? &you : 0, 0, 0, you.pos(), MHITYOU, MG_NONE,
                 GOD_YREDELEMNUL);

    if (force_hostile)
        mg.non_actor_summoner = "the anger of Yredelemnul";

    int created = 0;
    if (force_hostile)
    {
        mg.extra_flags |= (MF_NO_REWARD | MF_HARD_RESET);

        for (; how_many > 0; --how_many)
        {
            if (create_monster(mg))
                created++;
        }
    }
    else
    {
        for (; how_many > 0; --how_many)
            delayed_monster(mg);
    }

    return created;
}

static bool _need_missile_gift(bool forced)
{
    skill_type sk = best_skill(SK_SLINGS, SK_THROWING);
    // Default to throwing if all missile skills are at zero.
    if (you.skills[sk] == 0)
        sk = SK_THROWING;
    return forced
           || (you.piety >= piety_breakpoint(2)
               && random2(you.piety) > 70
               && one_chance_in(8)
               && x_chance_in_y(1 + you.skills[sk], 12));
}

static bool _give_nemelex_gift(bool forced = false)
{
    // But only if you're not flying over deep water.
    if (!(feat_has_solid_floor(grd(you.pos()))
          || feat_is_watery(grd(you.pos())) && species_likes_water(you.species)))
    {
        return false;
    }

    // Nemelex will give at least one gift early.
    if (forced
        || !you.num_total_gifts[GOD_NEMELEX_XOBEH]
           && x_chance_in_y(you.piety + 1, piety_breakpoint(1))
        || one_chance_in(3) && x_chance_in_y(you.piety + 1, MAX_PIETY))
    {

        misc_item_type gift_type = random_choose_weighted(
                                        4, MISC_DECK_OF_WAR,
                                        4, MISC_DECK_OF_DESTRUCTION,
                                        2, MISC_DECK_OF_ESCAPE,
                                        0);

        int thing_created = items(true, OBJ_MISCELLANY, gift_type, 1, 0,
                                  GOD_NEMELEX_XOBEH);

        move_item_to_grid(&thing_created, you.pos(), true);

        if (thing_created != NON_ITEM)
        {
            // Piety|Common  | Rare  |Legendary
            // --------------------------------
            //     0:  95.00%,  5.00%,  0.00%
            //    20:  86.00%, 10.50%,  3.50%
            //    40:  77.00%, 16.00%,  7.00%
            //    60:  68.00%, 21.50%, 10.50%
            //    80:  59.00%, 27.00%, 14.00%
            //   100:  50.00%, 32.50%, 17.50%
            //   120:  41.00%, 38.00%, 21.00%
            //   140:  32.00%, 43.50%, 24.50%
            //   160:  23.00%, 49.00%, 28.00%
            //   180:  14.00%, 54.50%, 31.50%
            //   200:   5.00%, 60.00%, 35.00%
            const int common_weight = 95 - (90 * you.piety / MAX_PIETY);
            const int rare_weight   = 5  + (55 * you.piety / MAX_PIETY);
            const int legend_weight = 0  + (35 * you.piety / MAX_PIETY);

            const deck_rarity_type rarity = random_choose_weighted(
                common_weight, DECK_RARITY_COMMON,
                rare_weight,   DECK_RARITY_RARE,
                legend_weight, DECK_RARITY_LEGENDARY,
                0);

            item_def &deck(mitm[thing_created]);

            deck.deck_rarity = rarity;
            deck.flags |= ISFLAG_KNOW_TYPE;

            simple_god_message(" grants you a gift!");
            // included in default force_more_message
            canned_msg(MSG_SOMETHING_APPEARS);

            _inc_gift_timeout(5 + random2avg(9, 2));
            you.num_current_gifts[you.religion]++;
            you.num_total_gifts[you.religion]++;
            take_note(Note(NOTE_GOD_GIFT, you.religion));
        }
        return true;
    }

    return false;
}

static bool _give_pakellas_gift()
{
    // Break early if giving a gift now means it would be lost.
    if (!(feat_has_solid_floor(grd(you.pos()))
        || feat_is_watery(grd(you.pos())) && species_likes_water(you.species)))
    {
        return false;
    }

    object_class_type gift_type = OBJ_UNASSIGNED;

    if (you.num_total_gifts[GOD_PAKELLAS] == 0)
        gift_type = OBJ_WANDS;
    else if (you.species == SP_FELID)
        gift_type = coinflip() ? OBJ_WANDS : OBJ_MISCELLANY;
    else if (you.num_total_gifts[GOD_PAKELLAS] == 1)
        gift_type = OBJ_RODS;
    else
    {
        gift_type = random_choose_weighted(5, OBJ_WANDS,
                                           5, OBJ_MISCELLANY,
                                           3, OBJ_RODS,
                                           0);
    }

    if (acquirement(gift_type, you.religion))
    {
        simple_god_message(" grants you a gift!");
        // included in default force_more_message

        if (you.num_total_gifts[GOD_PAKELLAS] > 0)
            _inc_gift_timeout(150 + random2avg(29, 2));
        you.num_current_gifts[you.religion]++;
        you.num_total_gifts[you.religion]++;
        take_note(Note(NOTE_GOD_GIFT, you.religion));

        return true;
    }

    return false;
}

void mons_make_god_gift(monster* mon, god_type god)
{
    const god_type acting_god =
        (crawl_state.is_god_acting()) ? crawl_state.which_god_acting()
                                      : GOD_NO_GOD;

    if (god == GOD_NO_GOD && acting_god == GOD_NO_GOD)
        return;

    if (god == GOD_NO_GOD)
        god = acting_god;

    if (mon->flags & MF_GOD_GIFT)
    {
        dprf("Monster '%s' was already a gift of god '%s', now god '%s'.",
             mon->name(DESC_PLAIN, true).c_str(),
             god_name(mon->god).c_str(),
             god_name(god).c_str());
    }

    mon->god = god;
    mon->flags |= MF_GOD_GIFT;
}

bool mons_is_god_gift(const monster* mon, god_type god)
{
    return (mon->flags & MF_GOD_GIFT) && mon->god == god;
}

bool is_yred_undead_slave(const monster* mon)
{
    return mon->alive() && mon->holiness() & MH_UNDEAD
           && mon->attitude == ATT_FRIENDLY
           && mons_is_god_gift(mon, GOD_YREDELEMNUL);
}

bool is_orcish_follower(const monster* mon)
{
    return mon->alive() && mons_genus(mon->type) == MONS_ORC
           && mon->attitude == ATT_FRIENDLY
           && mons_is_god_gift(mon, GOD_BEOGH);
}

bool is_fellow_slime(const monster* mon)
{
    return mon->alive() && mons_is_slime(mon)
           && mon->attitude == ATT_STRICT_NEUTRAL
           && mons_is_god_gift(mon, GOD_JIYVA);
}

static bool _is_plant_follower(const monster* mon)
{
    return mon->alive() && mons_is_plant(mon)
           && mon->attitude == ATT_FRIENDLY;
}

static bool _has_jelly()
{
    ASSERT(you_worship(GOD_JIYVA));

    for (monster_iterator mi; mi; ++mi)
        if (mons_is_god_gift(*mi, GOD_JIYVA))
            return true;
    return false;
}

bool is_follower(const monster* mon)
{
    if (you_worship(GOD_YREDELEMNUL))
        return is_yred_undead_slave(mon);
    else if (will_have_passive(passive_t::convert_orcs))
        return is_orcish_follower(mon);
    else if (you_worship(GOD_JIYVA))
        return is_fellow_slime(mon);
    else if (you_worship(GOD_FEDHAS))
        return _is_plant_follower(mon);
    else
        return mon->alive() && mon->friendly();
}


static void _delayed_gift_callback(const mgen_data &mg, monster *&mon,
                                   int placed)
{
    if (placed <= 0)
        return;

    // Make sure monsters are shown.
    viewwindow();
    more();
    _inc_gift_timeout(4 + random2avg(7, 2));
    you.num_current_gifts[you.religion]++;
    you.num_total_gifts[you.religion]++;
    take_note(Note(NOTE_GOD_GIFT, you.religion));
}

static bool _jiyva_mutate()
{
    simple_god_message(" alters your body.");

    const int rand = random2(100);

    if (rand < 5)
        return delete_mutation(RANDOM_SLIME_MUTATION, "Jiyva's grace", true, false, true);
    else if (rand < 30)
        return delete_mutation(RANDOM_NON_SLIME_MUTATION, "Jiyva's grace", true, false, true);
    else if (rand < 55)
        return mutate(RANDOM_MUTATION, "Jiyva's grace", true, false, true);
    else if (rand < 75)
        return mutate(RANDOM_SLIME_MUTATION, "Jiyva's grace", true, false, true);
    else
        return mutate(RANDOM_GOOD_MUTATION, "Jiyva's grace", true, false, true);
}

/**
 * What's the name of the ally Hepliaklqana granted the player?
 *
 * @return      The ally's name.
 */
string hepliaklqana_ally_name()
{
    return you.props[HEPLIAKLQANA_ALLY_NAME_KEY].get_string();
}

/**
 * How much HD should the ally granted by Hepliaklqana have?
 *
 * @return      The player's xl * 2/3.
 */
static int _hepliaklqana_ally_hd()
{
    if (crawl_state.game_is_arena())
        return 27; // v0v
    // round up
    return (you.experience_level - 1) * 2 / 3 + 1;
}

/**
 * How much max HP should the ally granted by Hepliaklqana have?
 *
 * @return      5/hd from 1-11 HD, 10/hd from 12-18.
 *              (That is, 5 HP at 1 HD, 120 at 18.)
 */
static int _hepliaklqana_ally_hp()
{
    const int HD = _hepliaklqana_ally_hd();
    return HD * 5 + max(0, (HD - 12) * 5);
}

/**
 * Creates a mgen_data with the information needed to create the ancestor
 * granted by Hepliaklqana.
 *
 * XXX: should this be populating a mgen_data passed by reference, rather than
 * returning one on the stack?
 *
 * @return    The mgen_data that creates a hepliaklqana ancestor.
 */
mgen_data hepliaklqana_ancestor_gen_data()
{
    const monster_type type = you.props.exists(HEPLIAKLQANA_ALLY_TYPE_KEY) ?
        (monster_type)you.props[HEPLIAKLQANA_ALLY_TYPE_KEY].get_int() :
        MONS_ANCESTOR;
    mgen_data mg(type, BEH_FRIENDLY, &you, 0, 0, you.pos());
    mg.god = GOD_HEPLIAKLQANA;
    mg.hd = _hepliaklqana_ally_hd();
    mg.hp = _hepliaklqana_ally_hp();
    mg.extra_flags |= MF_NO_REWARD;
    mg.mname = hepliaklqana_ally_name();
    mg.props[MON_GENDER_KEY]
        = you.props[HEPLIAKLQANA_ALLY_GENDER_KEY].get_int();
    return mg;
}

/// Print a message for an ancestor's *something* being gained.
static void _regain_memory(const monster &ancestor, string memory)
{
    mprf("%s regains the memory of %s %s.",
         ancestor.name(DESC_YOUR, true).c_str(),
         ancestor.pronoun(PRONOUN_POSSESSIVE, true).c_str(),
         memory.c_str());
}

/// Print a message for an ancestor's item being gained/type upgraded.
static void _regain_item_memory(const monster &ancestor,
                                object_class_type base_type,
                                int sub_type)
{
    _regain_memory(ancestor, item_base_name(base_type, sub_type));
}

/**
 * Update the ancestor's stats after the player levels up. Upgrade HD and HP,
 * and give appropriate messaging for that and any other notable upgrades
 * (spells, resists, etc).
 */
void upgrade_hepliaklqana_ancestor()
{
    monster* ancestor = hepliaklqana_ancestor_mon();
    if (!ancestor || !ancestor->alive())
        return;

    const int old_hd = ancestor->get_experience_level();
    ancestor->set_hit_dice(_hepliaklqana_ally_hd());
    if (old_hd == ancestor->get_experience_level())
        return; // assume nothing changes except at different HD

    const int old_mhp = ancestor->max_hit_points;
    ancestor->max_hit_points = _hepliaklqana_ally_hp();
    ancestor->hit_points =
        div_rand_round(ancestor->hit_points * ancestor->max_hit_points,
                       old_mhp);

    mprf("%s remembers more of %s old skill.",
         ancestor->name(DESC_YOUR, true).c_str(),
         ancestor->pronoun(PRONOUN_POSSESSIVE, true).c_str());

    // assumption: ancestors can lose weapons (very rarely - tukima's),
    // and it's weird for them to just reappear, so only upgrade existing ones
    if (ancestor->weapon())
        upgrade_hepliaklqana_weapon(*ancestor, *ancestor->weapon(), true);
    // but shields can't be lost, and *can* be gained (knight at hd 5)
    // so give them out as appropriate
    if (ancestor->shield())
        upgrade_hepliaklqana_shield(*ancestor, *ancestor->shield(), true);
    else
    {
        give_shield(ancestor);
        const item_def* shield = ancestor->shield();
        if (shield)
            _regain_item_memory(*ancestor, shield->base_type, shield->sub_type);
    }
    set_ancestor_spells(*ancestor, true);

    // TODO: misc messages

    const int HD = ancestor->get_experience_level();
    // not a big fan of this hardcoding
    // consider using _hepliaklqana_ancestor_resists
    if (HD == 11)
        _regain_memory(*ancestor, "gloves of protection from fire");
    if (HD == 12)
        _regain_memory(*ancestor, "cloak of protection from cold");
    // also hardcoded....
    if (HD == 15)
        _regain_memory(*ancestor, "ring of see invisible");
    // if innate relec comes back, those are clearly boots...

    // spiny
    if (HD == 16 && ancestor->type == MONS_ANCESTOR_KNIGHT)
        _regain_memory(*ancestor, "spiked armour");
}

/**
 * What type of weapon should a given ancestor have?
 *
 * @param ancestor      The ancestor in question.
 * @return              An appropriate weapon_type.
 */
static weapon_type _hepliaklqana_weapon_type(const monster &ancestor)
{
    switch (ancestor.type)
    {
        case MONS_ANCESTOR_HEXER:
            return ancestor.get_experience_level() < 16 ? WPN_DAGGER
                                                        : WPN_QUICK_BLADE;
        case MONS_ANCESTOR_KNIGHT:
            return ancestor.get_experience_level() < 8 ? WPN_LONG_SWORD
                                                       : WPN_BROAD_AXE;
        case MONS_ANCESTOR_BATTLEMAGE:
            return WPN_QUARTERSTAFF;
        default:
            return NUM_WEAPONS; // should never happen
    }
}

/**
 * What brand should an ancestor's weapon have, if any?
 *
 * @param ancestor      The ancestor in question.
 * @return              An appropriate weapon_type.
 */
static brand_type _hepliaklqana_weapon_brand(const monster &ancestor)
{
    switch (ancestor.type)
    {
        case MONS_ANCESTOR_HEXER:
            return ancestor.get_experience_level() < 9 ?    SPWPN_NORMAL :
                   ancestor.get_experience_level() < 18 ?   SPWPN_DRAINING :
                                                            SPWPN_ANTIMAGIC;
        case MONS_ANCESTOR_KNIGHT:
            return ancestor.get_experience_level() < 10 ?   SPWPN_NORMAL :
                   ancestor.get_experience_level() < 18 ?   SPWPN_FLAMING :
                                                            SPWPN_SPEED;
        case MONS_ANCESTOR_BATTLEMAGE:
        default:
            return SPWPN_NORMAL;
    }
}

/**
 * Setup an ancestor's weapon after their class is chosen, when the player
 * levels up, or after they're resummoned (or initially created for wrath).
 *
 * @param[in]   ancestor      The ancestor for whom the weapon is intended.
 * @param[out]  item          The item to be configured.
 * @param       notify        Whether messages should be printed when something
 *                            changes. (Weapon type or brand.)
 * @return                    True iff the ancestor should have a weapon.
 */
void upgrade_hepliaklqana_weapon(const monster &ancestor, item_def &item,
                                  bool notify)
{
    ASSERT(mons_is_hepliaklqana_ancestor(ancestor.type));
    if (ancestor.type == MONS_ANCESTOR)
        return; // bare-handed!

    const weapon_type old_type = static_cast<weapon_type>(item.sub_type);
    const brand_type old_brand = static_cast<brand_type>(item.brand);

    item.base_type = OBJ_WEAPONS;
    item.sub_type = _hepliaklqana_weapon_type(ancestor);
    item.brand = _hepliaklqana_weapon_brand(ancestor);
    item.plus = ancestor.get_experience_level() / 2;
    item.flags |= ISFLAG_IDENT_MASK | ISFLAG_SUMMONED;

    if (!notify)
        return;

    if (old_type != item.sub_type)
        _regain_item_memory(ancestor, item.base_type, item.sub_type);

    if (old_brand != item.brand)
    {
        mprf("%s remembers %s %s %s.",
             ancestor.name(DESC_YOUR, true).c_str(),
             ancestor.pronoun(PRONOUN_POSSESSIVE, true).c_str(),
             apostrophise(item_base_name(item.base_type,
                                         item.sub_type)).c_str(),
             brand_type_name(item.brand, item.brand != SPWPN_DRAINING));
        // 'remembers... draining' reads better than 'drain', but 'flame'
        // reads better than 'flaming'
    }
}

/**
 * What kind of shield should a knight-ancestor of the given HD be given?
 *
 * @param HD        The HD (XL) of the knight in question.
 * @return          An appropriate type of shield, or NUM_ARMOURS.
 */
armour_type _hepliaklqana_shield_type(int HD)
{
    if (HD < 5)
        return NUM_ARMOURS;
    if (HD < 9)
        return ARM_BUCKLER;
    if (HD < 17)
        return ARM_SHIELD;
    return ARM_LARGE_SHIELD;
}

/**
 * Setup an ancestor's weapon after their class is chosen, when the player
 * levels up, or after they're resummoned (or initially created for wrath).
 *
 * @param[in]   ancestor      The ancestor for whom the weapon is intended.
 * @param[out]  item          The item to be configured.
 * @param       notify        Whether messages should be printed when something
 *                            changes. (Shield type or ego.)
 * @return                    True iff the ancestor should have a weapon.
 */
void upgrade_hepliaklqana_shield(const monster &ancestor, item_def &item,
                                  bool notify)
{
    ASSERT(mons_is_hepliaklqana_ancestor(ancestor.type));
    if (ancestor.type != MONS_ANCESTOR_KNIGHT)
        return; // only knights get shields!

    const int HD = ancestor.get_experience_level();
    const armour_type shield_type = _hepliaklqana_shield_type(HD);
    if (shield_type == NUM_ARMOURS)
        return; // no shield yet!

    const armour_type old_type = static_cast<armour_type>(item.sub_type);
    const special_armour_type old_ego = (special_armour_type)item.brand;

    item.base_type = OBJ_ARMOUR;
    item.sub_type = shield_type;
    item.brand = HD < 14 ? SPARM_NORMAL : SPARM_REFLECTION;
    item.plus = ancestor.get_experience_level() / 3;
    item.flags |= ISFLAG_IDENT_MASK | ISFLAG_SUMMONED;
    item.quantity = 1;

    if (!notify)
        return;

    if (old_type != item.sub_type)
        _regain_item_memory(ancestor, item.base_type, item.sub_type);

    if (old_ego != item.brand)
    {
        mprf("%s remembers %s %s %s.",
             ancestor.name(DESC_YOUR, true).c_str(),
             ancestor.pronoun(PRONOUN_POSSESSIVE, true).c_str(),
             apostrophise(item_base_name(item.base_type,
                                         item.sub_type)).c_str(),
             armour_ego_name(item, false));
    }
}

bool vehumet_is_offering(spell_type spell)
{
    return you.vehumet_gifts.count(spell);
}

void vehumet_accept_gift(spell_type spell)
{
    if (vehumet_is_offering(spell))
    {
        you.vehumet_gifts.erase(spell);
        you.seen_spell.set(spell);
        you.duration[DUR_VEHUMET_GIFT] = 0;
    }
}

static void _add_to_old_gifts(spell_type spell)
{
    you.old_vehumet_gifts.insert(spell);
}

static bool _is_old_gift(spell_type spell)
{
    return you.old_vehumet_gifts.count(spell);
}

static set<spell_type> _vehumet_eligible_gift_spells(set<spell_type> excluded_spells)
{
    set<spell_type> eligible_spells;

    const int gifts = you.num_total_gifts[you.religion];
    if (gifts >= NUM_VEHUMET_GIFTS)
        return eligible_spells;

    const int min_lev[] = {1,1,2,3,3,4,4,5,5,6,6,6,8};
    const int max_lev[] = {1,2,3,4,5,7,7,7,7,7,7,7,9};
    COMPILE_CHECK(ARRAYSZ(min_lev) == NUM_VEHUMET_GIFTS);
    COMPILE_CHECK(ARRAYSZ(max_lev) == NUM_VEHUMET_GIFTS);
    int min_level = min_lev[gifts];
    int max_level = max_lev[gifts];

    if (min_level > you.experience_level)
        return eligible_spells;

    set<spell_type> backup_spells;
    for (int i = 0; i < NUM_SPELLS; ++i)
    {
        spell_type spell = static_cast<spell_type>(i);
        if (!is_valid_spell(spell))
            continue;

        if (excluded_spells.count(spell))
            continue;

        if (vehumet_supports_spell(spell)
            && !you.has_spell(spell)
            && is_player_spell(spell)
            && spell_difficulty(spell) <= max_level
            && spell_difficulty(spell) >= min_level)
        {
            if (!you.seen_spell[spell] && !_is_old_gift(spell))
                eligible_spells.insert(spell);
            else
                backup_spells.insert(spell);
        }
    }
    // Don't get stuck just because all spells have been seen/offered.
    if (eligible_spells.empty())
    {
        if (backup_spells.empty())
        {
            // This is quite improbable to happen, but in this case just
            // skip the gift and increment the gift counter.
            if (gifts <= 12)
            {
                you.num_current_gifts[you.religion]++;
                you.num_total_gifts[you.religion]++;
            }
        }
        return backup_spells;
    }
    return eligible_spells;
}

static int _vehumet_weighting(spell_type spell)
{
    int bias = 100 + elemental_preference(spell, 10);
    return bias;
}

static spell_type _vehumet_find_spell_gift(set<spell_type> excluded_spells)
{
    set<spell_type> eligible_spells = _vehumet_eligible_gift_spells(excluded_spells);
    spell_type spell = SPELL_NO_SPELL;
    int total_weight = 0;
    int this_weight = 0;
    for (auto elig : eligible_spells)
    {
        this_weight = _vehumet_weighting(elig);
        total_weight += this_weight;
        if (x_chance_in_y(this_weight, total_weight))
            spell = elig;
    }
    return spell;
}

static set<spell_type> _vehumet_get_spell_gifts()
{
    set<spell_type> offers;
    unsigned int num_offers = you.num_total_gifts[you.religion] == 12 ? 3 : 1;
    while (offers.size() < num_offers)
    {
        spell_type offer = _vehumet_find_spell_gift(offers);
        if (offer == SPELL_NO_SPELL)
            break;
        offers.insert(offer);
    }
    return offers;
}

///////////////////////////////
bool do_god_gift(bool forced)
{
    ASSERT(!you_worship(GOD_NO_GOD));

    god_acting gdact;

#if defined(DEBUG_DIAGNOSTICS) || defined(DEBUG_GIFTS)
    int old_num_current_gifts = you.num_current_gifts[you.religion];
    int old_num_total_gifts = you.num_total_gifts[you.religion];
#endif

    bool success = false;

    // Consider a gift if we don't have a timeout and aren't under penance
    if (forced || !player_under_penance() && !you.gift_timeout)
    {
        // Remember to check for water/lava.
        switch (you.religion)
        {
        default:
            break;

        case GOD_NEMELEX_XOBEH:
            success = _give_nemelex_gift(forced);
            break;

        case GOD_PAKELLAS:
            if (forced && coinflip()
                || you.piety >= piety_breakpoint(1)
                   && you.num_total_gifts[you.religion] == 0
                || you.piety >= piety_breakpoint(3)
                   && you.num_total_gifts[you.religion] == 1
                || !forced && random2(you.piety) > piety_breakpoint(3)
                   && one_chance_in(4))
            {
                success = _give_pakellas_gift();
            }
            break;

        case GOD_OKAWARU:
        case GOD_TROG:
        {
            // Break early if giving a gift now means it would be lost.
            if (!(feat_has_solid_floor(grd(you.pos()))
                || feat_is_watery(grd(you.pos())) && species_likes_water(you.species)))
            {
                break;
            }

            // Should gift catnip instead.
            if (you.species == SP_FELID)
                break;

            const bool need_missiles = _need_missile_gift(forced);
            object_class_type gift_type;

            if (forced && coinflip()
                || (!forced && you.piety >= piety_breakpoint(4)
                    && random2(you.piety) > 120
                    && one_chance_in(4)))
            {
                if (you_worship(GOD_TROG)
                    || (you_worship(GOD_OKAWARU) && coinflip()))
                {
                    gift_type = OBJ_WEAPONS;
                }
                else
                    gift_type = OBJ_ARMOUR;
            }
            else if (need_missiles)
                gift_type = OBJ_MISSILES;
            else
                break;

            success = acquirement(gift_type, you.religion);
            if (success)
            {
                simple_god_message(" grants you a gift!");
                // included in default force_more_message

                if (gift_type == OBJ_MISSILES)
                    _inc_gift_timeout(4 + roll_dice(2, 4));
                else
                {
                    // Okawaru charges extra for armour acquirements.
                    if (you_worship(GOD_OKAWARU) && gift_type == OBJ_ARMOUR)
                        _inc_gift_timeout(30 + random2avg(15, 2));

                    _inc_gift_timeout(30 + random2avg(19, 2));
                }
                you.num_current_gifts[you.religion]++;
                you.num_total_gifts[you.religion]++;
                take_note(Note(NOTE_GOD_GIFT, you.religion));
            }
            break;
        }

        case GOD_YREDELEMNUL:
            if (forced
                || (random2(you.piety) >= piety_breakpoint(2)
                    && one_chance_in(4)))
            {
                unsigned int threshold = MIN_YRED_SERVANT_THRESHOLD
                                         + you.num_current_gifts[you.religion] / 2;
                threshold = max(threshold,
                    static_cast<unsigned int>(MIN_YRED_SERVANT_THRESHOLD));
                threshold = min(threshold,
                    static_cast<unsigned int>(MAX_YRED_SERVANT_THRESHOLD));

                if (yred_random_servants(threshold) != -1)
                {
                    delayed_monster_done(" grants you @an@ undead servant@s@!",
                                          "", _delayed_gift_callback);
                    success = true;
                }
            }
            break;

        case GOD_JIYVA:
            if (forced || you.piety >= piety_breakpoint(2)
                          && random2(you.piety) > 50
                          && one_chance_in(4) && !you.gift_timeout
                          && you.can_safely_mutate())
            {
                if (_jiyva_mutate())
                {
                    _inc_gift_timeout(15 + roll_dice(2, 4));
                    you.num_current_gifts[you.religion]++;
                    you.num_total_gifts[you.religion]++;
                }
                else
                    mpr("You feel as though nothing has changed.");
            }
            break;

        case GOD_KIKUBAAQUDGHA:
        case GOD_SIF_MUNA:
        {
            book_type gift = NUM_BOOKS;
            // Break early if giving a gift now means it would be lost.
            if (!feat_has_solid_floor(grd(you.pos())))
                break;

            // Kikubaaqudgha gives the lesser Necromancy books in a quick
            // succession.
            if (you_worship(GOD_KIKUBAAQUDGHA))
            {
                if (you.piety >= piety_breakpoint(0)
                    && you.num_total_gifts[you.religion] == 0)
                {
                    gift = BOOK_NECROMANCY;
                }
                else if (you.piety >= piety_breakpoint(2)
                         && you.num_total_gifts[you.religion] == 1)
                {
                    gift = BOOK_DEATH;
                }
            }
            else if (forced || you.piety >= piety_breakpoint(5)
                               && random2(you.piety) > 100)
            {
                // Sif Muna special: Keep quiet if acquirement fails
                // because the player already has seen all spells.
                if (you_worship(GOD_SIF_MUNA))
                    success = acquirement(OBJ_BOOKS, you.religion, true);
            }

            if (gift != NUM_BOOKS)
            {
                int thing_created = items(true, OBJ_BOOKS, gift, 1, 0,
                                          you.religion);
                // Replace a Kiku gift by a custom-random book.
                if (you_worship(GOD_KIKUBAAQUDGHA))
                {
                    make_book_kiku_gift(mitm[thing_created],
                                        gift == BOOK_NECROMANCY);
                }
                if (thing_created == NON_ITEM)
                    return false;

                // Mark the book type as known to avoid duplicate
                // gifts if players don't read their gifts for some
                // reason.
                mark_had_book(gift);

                move_item_to_grid(&thing_created, you.pos(), true);

                if (thing_created != NON_ITEM)
                    success = true;
            }

            if (success)
            {
                simple_god_message(" grants you a gift!");
                // included in default force_more_message

                you.num_current_gifts[you.religion]++;
                you.num_total_gifts[you.religion]++;
                // Timeouts are meaningless for Kiku.
                if (!you_worship(GOD_KIKUBAAQUDGHA))
                    _inc_gift_timeout(40 + random2avg(19, 2));
                take_note(Note(NOTE_GOD_GIFT, you.religion));
            }
            break;
        }              // End of book gods.

        case GOD_VEHUMET:
            const int gifts = you.num_total_gifts[you.religion];
            if (forced || !you.duration[DUR_VEHUMET_GIFT]
                          && (you.piety >= piety_breakpoint(0) && gifts == 0
                              || you.piety >= piety_breakpoint(0) + random2(6) + 18 * gifts && gifts <= 5
                              || you.piety >= piety_breakpoint(4) && gifts <= 11 && one_chance_in(20)
                              || you.piety >= piety_breakpoint(5) && gifts <= 12 && one_chance_in(20)))
            {
                set<spell_type> offers = _vehumet_get_spell_gifts();
                if (!offers.empty())
                {
                    you.vehumet_gifts = offers;
                    string prompt = " offers you knowledge of ";
                    for (auto it = offers.begin(); it != offers.end(); ++it)
                    {
                        if (it != offers.begin())
                        {
                            if (offers.size() > 2)
                                prompt += ",";
                            prompt += " ";
                            auto next = it;
                            next++;
                            if (next == offers.end())
                                prompt += "and ";
                        }
                        prompt += spell_title(*it);
                        _add_to_old_gifts(*it);
                        take_note(Note(NOTE_OFFERED_SPELL, *it));
                    }
                    prompt += ".";
                    if (gifts >= NUM_VEHUMET_GIFTS - 1)
                    {
                        prompt += " These spells will remain available"
                                  " as long as you worship Vehumet.";
                    }

                    you.duration[DUR_VEHUMET_GIFT] = (100 + random2avg(100, 2)) * BASELINE_DELAY;
                    if (gifts >= 5)
                        _inc_gift_timeout(30 + random2avg(30, 2));
                    you.num_current_gifts[you.religion]++;
                    you.num_total_gifts[you.religion]++;

                    simple_god_message(prompt.c_str());
                    // included in default force_more_message

                    success = true;
                }
                else
                    success = false;
            }
            break;
        }                       // switch (you.religion)
    }                           // End of gift giving.

    if (success)
        you.running.stop();

#if defined(DEBUG_DIAGNOSTICS) || defined(DEBUG_GIFTS)
    if (old_num_current_gifts < you.num_current_gifts[you.religion])
    {
        mprf(MSGCH_DIAGNOSTICS, "Current number of gifts from this god: %d",
             you.num_current_gifts[you.religion]);
    }
    if (old_num_total_gifts < you.num_total_gifts[you.religion])
    {
        mprf(MSGCH_DIAGNOSTICS, "Total number of gifts from this god: %d",
             you.num_total_gifts[you.religion]);
    }
#endif
    return success;
}

string god_name(god_type which_god, bool long_name)
{
    if (which_god == GOD_JIYVA)
    {
        return god_name_jiyva(long_name) +
               (long_name? " the Shapeless" : "");
    }

    if (long_name)
    {
        const string shortname = god_name(which_god, false);
        const string longname = getMiscString(shortname + " lastname");
        return longname.empty()? shortname : longname;
    }

    switch (which_god)
    {
    case GOD_NO_GOD:        return "No God";
    case GOD_RANDOM:        return "random";
    case GOD_NAMELESS:      return "nameless";
    case GOD_ZIN:           return "Zin";
    case GOD_SHINING_ONE:   return "the Shining One";
    case GOD_KIKUBAAQUDGHA: return "Kikubaaqudgha";
    case GOD_YREDELEMNUL:   return "Yredelemnul";
    case GOD_VEHUMET:       return "Vehumet";
    case GOD_OKAWARU:       return "Okawaru";
    case GOD_MAKHLEB:       return "Makhleb";
    case GOD_SIF_MUNA:      return "Sif Muna";
    case GOD_TROG:          return "Trog";
    case GOD_NEMELEX_XOBEH: return "Nemelex Xobeh";
    case GOD_ELYVILON:      return "Elyvilon";
    case GOD_LUGONU:        return "Lugonu";
    case GOD_BEOGH:         return "Beogh";
    case GOD_FEDHAS:        return "Fedhas";
    case GOD_CHEIBRIADOS:   return "Cheibriados";
    case GOD_XOM:           return "Xom";
    case GOD_ASHENZARI:     return "Ashenzari";
    case GOD_DITHMENOS:     return "Dithmenos";
    case GOD_GOZAG:         return "Gozag";
    case GOD_QAZLAL:        return "Qazlal";
    case GOD_RU:            return "Ru";
    case GOD_PAKELLAS:      return "Pakellas";
    case GOD_HEPLIAKLQANA:  return "Hepliaklqana";
    case GOD_JIYVA: // This is handled at the beginning of the function
    case GOD_ECUMENICAL:    return "an unknown god";
    case NUM_GODS:          return "Buggy";
    }
    return "";
}

string god_name_jiyva(bool second_name)
{
    string name = "Jiyva";
    if (second_name)
        name += " " + you.jiyva_second_name;

    return name;
}

god_type str_to_god(const string &_name, bool exact)
{
    string target(_name);
    trim_string(target);
    lowercase(target);

    if (target.empty())
        return GOD_NO_GOD;

    int      num_partials = 0;
    god_type partial      = GOD_NO_GOD;
    for (god_iterator it; it; ++it)
    {
        god_type god = *it;
        string name = lowercase_string(god_name(god, false));

        if (name == target)
            return god;

        if (!exact && name.find(target) != string::npos)
        {
            // Return nothing for ambiguous partial names.
            num_partials++;
            if (num_partials > 1)
                return GOD_NO_GOD;
            partial = god;
        }
    }

    if (!exact && num_partials == 1)
        return partial;

    return GOD_NO_GOD;
}

void god_speaks(god_type god, const char *mesg)
{
    ASSERT(!crawl_state.game_is_arena());

    int orig_mon = mgrd(you.pos());

    monster fake_mon;
    fake_mon.type       = MONS_PROGRAM_BUG;
    fake_mon.hit_points = 1;
    fake_mon.god        = god;
    fake_mon.set_position(you.pos());
    fake_mon.foe        = MHITYOU;
    fake_mon.mname      = "FAKE GOD MONSTER";

    mprf(MSGCH_GOD, god, "%s", do_mon_str_replacements(mesg, &fake_mon).c_str());

    fake_mon.reset();
    mgrd(you.pos()) = orig_mon;
}

void religion_turn_start()
{
    if (you.turn_is_over)
        religion_turn_end();

    crawl_state.clear_god_acting();
}

void religion_turn_end()
{
    ASSERT(you.turn_is_over);
    _place_delayed_monsters();
}

/** Punish the character for some divine transgression.
 *
 * @param piety_loss The amount of penance imposed; may be scaled.
 * @param penance The amount of penance imposed; may be scaled.
 */
void dock_piety(int piety_loss, int penance)
{
    static int last_piety_lecture   = -1;
    static int last_penance_lecture = -1;

    if (piety_loss <= 0 && penance <= 0)
        return;

    piety_loss = piety_scale(piety_loss);
    penance    = piety_scale(penance);

    if (piety_loss)
    {
        if (last_piety_lecture != you.num_turns)
        {
            // output guilt message:
            mprf("You feel%sguilty.",
                 (piety_loss == 1) ? " a little " :
                 (piety_loss <  5) ? " " :
                 (piety_loss < 10) ? " very "
                                   : " extremely ");
        }

        last_piety_lecture = you.num_turns;
        lose_piety(piety_loss);
    }

    if (you.piety < 1)
        excommunication();
    else if (penance)       // only if still in religion
    {
        if (last_penance_lecture != you.num_turns)
        {
            god_speaks(you.religion,
                       "\"You will pay for your transgression, mortal!\"");
        }
        last_penance_lecture = you.num_turns;
        _inc_penance(penance);
    }
}

// Scales a piety number, applying modifiers (faith).
int piety_scale(int piety)
{
    return piety + (you.faith() * div_rand_round(piety, 4));
}

/** Gain or lose piety to reach a certain value.
 *
 * If the player cannot gain piety (because they worship Xom, Gozag, or
 * no god), their piety will be unchanged.
 *
 * @param piety The new piety value.
 * @pre piety is between 0 and MAX_PIETY, inclusive.
 */
void set_piety(int piety)
{
    ASSERT(piety >= 0);
    ASSERT(piety <= MAX_PIETY);

    // Ru max piety is 6*
    if (you_worship(GOD_RU) && piety > piety_breakpoint(5))
        piety = piety_breakpoint(5);

    // We have to set the exact piety value this way, because diff may
    // be decreased to account for things like penance and gift timeout.
    int diff;
    do
    {
        diff = piety - you.piety;
        if (diff > 0)
        {
            if (!gain_piety(diff, 1, false))
                break;
        }
        else if (diff < 0)
            lose_piety(-diff);
    }
    while (diff != 0);
}

static void _gain_piety_point()
{
    // check to see if we owe anything first
    if (player_under_penance(you.religion))
    {
        dec_penance(1);
        return;
    }
    else if (you.gift_timeout > 0)
    {
        you.gift_timeout--;

        // Slow down piety gain to account for the fact that gifts
        // no longer have a piety cost for getting them.
        // Jiyva is an exception because there's usually a time-out and
        // the gifts aren't that precious.
        // Pakellas is an exception because the gift timeout is exceptionally
        // long and causes extremely slow piety gain above 4* if this isn't
        // here.
        if (!one_chance_in(4) && !you_worship(GOD_JIYVA)
            && !you_worship(GOD_NEMELEX_XOBEH)
            && !you_worship(GOD_PAKELLAS))
        {
#ifdef DEBUG_PIETY
            mprf(MSGCH_DIAGNOSTICS, "Piety slowdown due to gift timeout.");
#endif
            return;
        }
    }

    // slow down gain at upper levels of piety
    if (!you_worship(GOD_SIF_MUNA) && !you_worship(GOD_RU))
    {
        if (you.piety >= MAX_PIETY
            || you.piety >= piety_breakpoint(5) && one_chance_in(3)
            || you.piety >= piety_breakpoint(3) && one_chance_in(3))
        {
            do_god_gift();
            return;
        }
    }
    else if (you_worship(GOD_SIF_MUNA))
    {
        // Sif Muna has a gentler taper off because training becomes
        // naturally slower as the player gains in spell skills.
        if (you.piety >= MAX_PIETY
            || you.piety >= piety_breakpoint(5) && one_chance_in(5))
        {
            do_god_gift();
            return;
        }
    }
    else
    {
      // Ru piety doesn't modulate or taper and Ru doesn't give gifts.
      // Ru max piety is 160 (6*)
      if (you.piety >= piety_breakpoint(5))
          return;
    }

    int old_piety = you.piety;
    // Apply hysteresis.
    // piety_hysteresis is the amount of _loss_ stored up, so this
    // may look backwards.
    if (you.piety_hysteresis)
        you.piety_hysteresis--;
    else if (you.piety < MAX_PIETY)
        you.piety++;

    if (piety_rank() > piety_rank(old_piety))
    {
        // Redraw piety display and, in case the best skill is Invocations,
        // redraw the god title.
        you.redraw_title = true;

        const int rank = piety_rank();
        take_note(Note(NOTE_PIETY_RANK, you.religion, rank));
        for (const auto& power : get_god_powers(you.religion))
        {
            if (power.rank == rank
                || power.rank == 7 && can_do_capstone_ability(you.religion))
            {
                power.display(true, "You can now %s.");
#ifdef USE_TILE_LOCAL
                tiles.layout_statcol();
                redraw_screen();
#endif
                learned_something_new(HINT_NEW_ABILITY_GOD);
                // Preserve the old hotkey
                if (power.abil == ABIL_YRED_ANIMATE_DEAD)
                {
                    replace(begin(you.ability_letter_table),
                            end(you.ability_letter_table),
                            ABIL_YRED_ANIMATE_REMAINS, ABIL_YRED_ANIMATE_DEAD);
                }
            }
        }
        if (rank == rank_for_passive(passive_t::halo))
            mprf(MSGCH_GOD, "A divine halo surrounds you!");
        if (rank == rank_for_passive(passive_t::umbra))
            mprf(MSGCH_GOD, "You are shrouded in an aura of darkness!");
        if (rank == rank_for_passive(passive_t::sinv))
            autotoggle_autopickup(false);
        if (rank == rank_for_passive(passive_t::clarity))
        {
            // Inconsistent with donning amulets, but matches the
            // message better and is not abusable.
            you.duration[DUR_CONF] = 0;
        }
        if (rank >= rank_for_passive(passive_t::identify_items))
            auto_id_inventory();

        if (you_worship(GOD_JIYVA) && can_do_capstone_ability(you.religion))
        {
            simple_god_message(" will now unseal the treasures of the "
                               "Slime Pits.");
            dlua.callfn("dgn_set_persistent_var", "sb",
                        "fix_slime_vaults", true);
            // If we're on Slime:6, pretend we just entered the level
            // in order to bring down the vault walls.
            if (level_id::current() == level_id(BRANCH_SLIME, 6))
                dungeon_events.fire_event(DET_ENTERED_LEVEL);

            you.one_time_ability_used.set(you.religion);
        }
        if (you_worship(GOD_HEPLIAKLQANA))
        {
            if (rank == 2 && !you.props.exists(HEPLIAKLQANA_ALLY_TYPE_KEY))
            {
                god_speaks(you.religion,
                           "You may now remember your ancestor's life.");
            }
            if (rank == 6 && !you.props.exists(HEPLIAKLQANA_ALLY_DEATH_KEY))
            {
                hepliaklqana_pick_death_types();
                god_speaks(you.religion,
                           "You may now remember your ancestor's death.");
            }
        }
    }

    // Every piety level change also affects AC.
    if (will_have_passive(passive_t::bonus_ac))
        you.redraw_armour_class = true;

    // The player's symbol depends on Beogh piety.
    if (you_worship(GOD_BEOGH))
        update_player_symbol();

    if (have_passive(passive_t::stat_boost)
        && chei_stat_boost(old_piety) < chei_stat_boost())
    {
        string msg = " raises the support of your attributes";
        if (have_passive(passive_t::slowed))
            msg += " as your movement slows";
        msg += ".";
        simple_god_message(msg.c_str());
        notify_stat_change();
    }

    if (you_worship(GOD_QAZLAL)
        && qazlal_sh_boost(old_piety) != qazlal_sh_boost())
    {
        you.redraw_armour_class = true;
    }

    if (have_passive(passive_t::halo) || have_passive(passive_t::umbra))
    {
        // Piety change affects halo / umbra radius.
        invalidate_agrid(true);
    }

    do_god_gift();
}

/**
 * Gain an amount of piety.
 *
 * @param original_gain The numerator of the nominal piety gain.
 * @param denominator The denominator of the nominal piety gain.
 * @param should_scale_piety Should the piety gain be scaled by faith,
 *   forlorn, and Sprint?
 * @return True if something happened, or if another call with the same
 *   arguments might cause something to happen (because of random number
 *   rolls).
 */
bool gain_piety(int original_gain, int denominator, bool should_scale_piety)
{
    if (original_gain <= 0)
        return false;

    // Xom uses piety differently; Gozag doesn't at all.
    if (you_worship(GOD_NO_GOD)
        || you_worship(GOD_XOM)
        || you_worship(GOD_GOZAG))
    {
        return false;
    }

    int pgn = should_scale_piety? piety_scale(original_gain) : original_gain;

    if (crawl_state.game_is_sprint() && should_scale_piety)
        pgn = sprint_modify_piety(pgn);

    pgn = div_rand_round(pgn, denominator);
    while (pgn-- > 0)
        _gain_piety_point();
    if (you.piety > you.piety_max[you.religion])
    {
        if (you.piety >= piety_breakpoint(5)
            && you.piety_max[you.religion] < piety_breakpoint(5))
        {
            mark_milestone("god.maxpiety", "became the Champion of "
                           + god_name(you.religion) + ".");
        }
        you.piety_max[you.religion] = you.piety;
    }
    return true;
}

/** Reduce piety and handle side-effects.
 *
 * Appropriate for cases where the player has not sinned, but must lose piety
 * anyway, such as costs for abilities.
 *
 * @param pgn The precise amount of piety lost.
 */
void lose_piety(int pgn)
{
    if (pgn <= 0)
        return;

    const int old_piety = you.piety;

    // Apply hysteresis.
    const int old_hysteresis = you.piety_hysteresis;
    you.piety_hysteresis = min<int>(PIETY_HYSTERESIS_LIMIT,
                                    you.piety_hysteresis + pgn);
    const int pgn_borrowed = (you.piety_hysteresis - old_hysteresis);
    pgn -= pgn_borrowed;
#ifdef DEBUG_PIETY
    mprf(MSGCH_DIAGNOSTICS,
         "Piety decreasing by %d (and %d added to hysteresis)",
         pgn, pgn_borrowed);
#endif

    if (you.piety - pgn < 0)
        you.piety = 0;
    else
        you.piety -= pgn;

    // Don't bother printing out these messages if you're under
    // penance, you wouldn't notice since all these abilities
    // are withheld.
    if (!player_under_penance()
        && piety_rank(old_piety) > piety_rank())
    {
        // Redraw piety display and, in case the best skill is Invocations,
        // redraw the god title.
        you.redraw_title = true;

        const int old_rank = piety_rank(old_piety);

        for (const auto& power : get_god_powers(you.religion))
        {
            if (power.rank == old_rank
                || power.rank == 7 && old_rank == 6
                   && !you.one_time_ability_used[you.religion])
            {
                power.display(false, "You can no longer %s.");
                // Preserve the old hotkey
                if (power.abil == ABIL_YRED_ANIMATE_DEAD)
                {
                    replace(begin(you.ability_letter_table),
                            end(you.ability_letter_table),
                            ABIL_YRED_ANIMATE_DEAD, ABIL_YRED_ANIMATE_REMAINS);
                }
            }
        }
#ifdef USE_TILE_LOCAL
        tiles.layout_statcol();
        redraw_screen();
#endif
    }

    if (you.piety > 0 && you.piety <= 5)
        learned_something_new(HINT_GOD_DISPLEASED);

    // Every piety level change also affects AC.
    if (will_have_passive(passive_t::bonus_ac))
        you.redraw_armour_class = true;

    if (will_have_passive(passive_t::water_walk) && _need_water_walking()
        && !have_passive(passive_t::water_walk))
    {
        fall_into_a_pool(grd(you.pos()));
    }
    if (will_have_passive(passive_t::stat_boost)
        && chei_stat_boost(old_piety) > chei_stat_boost())
    {
        string msg = " lowers the support of your attributes";
        if (will_have_passive(passive_t::slowed))
            msg += " as your movement quickens";
        msg += ".";
        simple_god_message(msg.c_str());
        notify_stat_change();
    }

    if (you_worship(GOD_QAZLAL)
        && qazlal_sh_boost(old_piety) != qazlal_sh_boost())
    {
        you.redraw_armour_class = true;
    }

    if (will_have_passive(passive_t::halo)
        || will_have_passive(passive_t::umbra))
    {
        // Piety change affects halo / umbra radius.
        invalidate_agrid(true);
    }
}

// Fedhas worshipers are on the hook for most plants and fungi
//
// If fedhas worshipers kill a protected monster they lose piety,
// if they attack a friendly one they get penance,
// if a friendly one dies they lose piety.
static bool _fedhas_protects_species(monster_type mc)
{
    return mons_class_is_plant(mc)
           && mons_class_holiness(mc) & MH_PLANT
           && mc != MONS_GIANT_SPORE
           && mc != MONS_SNAPLASHER_VINE
           && mc != MONS_SNAPLASHER_VINE_SEGMENT;
}

bool fedhas_protects(const monster* target)
{
    return target && _fedhas_protects_species(mons_base_type(target));
}

// Fedhas neutralises most plants and fungi
bool fedhas_neutralises(const monster* target)
{
    return target && mons_is_plant(target)
           && target->holiness() & MH_PLANT
           && target->type != MONS_SNAPLASHER_VINE
           && target->type != MONS_SNAPLASHER_VINE_SEGMENT;
}

static string _god_hates_your_god_reaction(god_type god, god_type your_god)
{
    if (god_hates_your_god(god, your_god))
    {
        // Non-good gods always hate your current god.
        if (!is_good_god(god))
            return "";

        // Zin hates chaotic gods.
        if (god == GOD_ZIN && is_chaotic_god(your_god))
            return " for chaos";

        if (is_evil_god(your_god))
            return " for evil";
    }

    return "";
}

void excommunication(bool voluntary, god_type new_god)
{
    const god_type old_god = you.religion;
    ASSERT(old_god != new_god);
    ASSERT(old_god != GOD_NO_GOD);

    const bool had_halo       = have_passive(passive_t::halo);
    const bool had_umbra      = have_passive(passive_t::umbra);
    const bool had_water_walk = have_passive(passive_t::water_walk);
    const bool had_stat_boost = have_passive(passive_t::stat_boost);
    const int  old_piety      = you.piety;

    god_acting gdact(old_god, true);

    take_note(Note(NOTE_LOSE_GOD, old_god));

    you.duration[DUR_PIETY_POOL] = 0; // your loss
    you.duration[DUR_RECITE] = 0;
    you.piety = 0;
    you.piety_hysteresis = 0;

    // so that the player isn't punished for "switching" between good gods via aX
    if (is_good_god(old_god) && voluntary)
    {
        you.saved_good_god_piety = old_piety;
        you.previous_good_god = old_god;
    }
    else
    {
        you.saved_good_god_piety = 0;
        you.previous_good_god = GOD_NO_GOD;
    }

    if (old_god == GOD_ASHENZARI)
        ash_init_bondage(&you);

    you.num_current_gifts[old_god] = 0;

    you.religion = GOD_NO_GOD;

    you.redraw_title = true;

    // Renouncing may have changed the conducts on our wielded or
    // quivered weapons, so refresh the display.
    you.wield_change = true;
    you.redraw_quiver = true;

    mpr("You have lost your religion!");
    // included in default force_more_message

    if (old_god == GOD_BEOGH)
    {
        // The player's symbol depends on Beogh worship.
        update_player_symbol();
    }

    mark_milestone("god.renounce", "abandoned " + god_name(old_god) + ".");
#ifdef DGL_WHEREIS
    whereis_record();
#endif

    if (god_hates_your_god(old_god, new_god))
    {
        simple_god_message(
            make_stringf(" does not appreciate desertion%s!",
                         _god_hates_your_god_reaction(old_god, new_god).c_str()).c_str(),
            old_god);
    }

    if (had_halo)
    {
        mprf(MSGCH_GOD, old_god, "Your divine halo fades away.");
        invalidate_agrid(true);
    }
    if (had_umbra)
    {
        mprf(MSGCH_GOD, old_god, "Your aura of darkness fades away.");
        invalidate_agrid(true);
    }
    // You might have lost water walking at a bad time...
    if (had_water_walk && _need_water_walking())
        fall_into_a_pool(grd(you.pos()));
    if (had_stat_boost)
    {
        redraw_screen();
        notify_stat_change();
    }

    switch (old_god)
    {
    case GOD_XOM:
        _set_penance(old_god, 50);
        break;

    case GOD_KIKUBAAQUDGHA:
        mprf(MSGCH_GOD, old_god, "You sense decay."); // in the state of Denmark
        add_daction(DACT_ROT_CORPSES);
        _set_penance(old_god, 30);
        break;

    case GOD_YREDELEMNUL:
        you.duration[DUR_MIRROR_DAMAGE] = 0;
        if (query_daction_counter(DACT_ALLY_YRED_SLAVE))
        {
            simple_god_message(" reclaims all of your granted undead slaves!",
                               GOD_YREDELEMNUL);
            add_daction(DACT_ALLY_YRED_SLAVE);
            remove_all_companions(GOD_YREDELEMNUL);
        }
        _set_penance(old_god, 30);
        break;

    case GOD_VEHUMET:
        you.vehumet_gifts.clear();
        you.duration[DUR_VEHUMET_GIFT] = 0;
        _set_penance(old_god, 25);
        break;

    case GOD_MAKHLEB:
        _set_penance(old_god, 25);
        add_daction(DACT_ALLY_MAKHLEB);
        break;

    case GOD_TROG:
        if (you.duration[DUR_TROGS_HAND])
            trog_remove_trogs_hand();

        add_daction(DACT_ALLY_TROG);

        _set_penance(old_god, 50);
        break;

    case GOD_BEOGH:
        if (query_daction_counter(DACT_ALLY_BEOGH))
        {
            simple_god_message("'s voice booms out, \"Who do you think you "
                               "are?\"", GOD_BEOGH);
            mprf(MSGCH_MONSTER_ENCHANT, "All of your followers decide to abandon you.");
            add_daction(DACT_ALLY_BEOGH);
            remove_all_companions(GOD_BEOGH);
        }

        env.level_state |= LSTATE_BEOGH;

        _set_penance(old_god, 50);
        break;

    case GOD_SIF_MUNA:
        _set_penance(old_god, 50);
        break;

    case GOD_NEMELEX_XOBEH:
        nemelex_shuffle_decks();
        _set_penance(old_god, 150); // Nemelex penance is special
        break;

    case GOD_LUGONU:
        _set_penance(old_god, 50);
        break;

    case GOD_SHINING_ONE:
        if (you.duration[DUR_DIVINE_SHIELD])
            tso_remove_divine_shield();

        // Leaving TSO for a non-good god will make all your followers
        // abandon you. Leaving him for a good god will make your holy
        // followers (daeva and angel servants) indifferent.
        if (!is_good_god(new_god))
            add_daction(DACT_ALLY_HOLY);
        else
            add_daction(DACT_HOLY_PETS_GO_NEUTRAL);

        _set_penance(old_god, 30);
        break;

    case GOD_ZIN:
        if (you.duration[DUR_DIVINE_STAMINA])
            zin_remove_divine_stamina();

        if (env.sanctuary_time)
            remove_sanctuary();

        // Leaving Zin for a non-good god will make neutral holies
        // (originally from TSO) abandon you.
        if (!is_good_god(new_god))
            add_daction(DACT_ALLY_HOLY);

        _set_penance(old_god, 25);
        break;

    case GOD_ELYVILON:
        you.duration[DUR_LIFESAVING] = 0;
        if (you.duration[DUR_DIVINE_VIGOUR])
            elyvilon_remove_divine_vigour();

        // Leaving Elyvilon for a non-good god will make neutral holies
        // (originally from TSO) abandon you.
        if (!is_good_god(new_god))
            add_daction(DACT_ALLY_HOLY);

        _set_penance(old_god, 30);
        break;

    case GOD_JIYVA:
        if (you.duration[DUR_SLIMIFY])
            you.duration[DUR_SLIMIFY] = 0;

        if (query_daction_counter(DACT_ALLY_SLIME))
        {
            mprf(MSGCH_MONSTER_ENCHANT, "All of your fellow slimes turn on you.");
            add_daction(DACT_ALLY_SLIME);
        }

        _set_penance(old_god, 30);
        break;

    case GOD_FEDHAS:
        if (query_daction_counter(DACT_ALLY_PLANT))
        {
            mprf(MSGCH_MONSTER_ENCHANT, "The plants of the dungeon turn on you.");
            add_daction(DACT_ALLY_PLANT);
        }
        _set_penance(old_god, 30);
        break;

    case GOD_ASHENZARI:
        if (you.transfer_skill_points > 0)
            ashenzari_end_transfer(false, true);
        you.duration[DUR_SCRYING] = 0;
        you.exp_docked[old_god] = exp_needed(min<int>(you.max_level, 27) + 1)
                                  - exp_needed(min<int>(you.max_level, 27));
        you.exp_docked_total[old_god] = you.exp_docked[old_god];
        _set_penance(old_god, 50);
        break;

    case GOD_DITHMENOS:
        if (you.form == TRAN_SHADOW)
            untransform();
        _set_penance(old_god, 25);
        break;

    case GOD_GOZAG:
        if (you.attribute[ATTR_GOZAG_SHOPS_CURRENT])
        {
            mprf(MSGCH_GOD, old_god, "Your funded stores close, unable to pay "
                                     "their debts without your funds.");
            you.attribute[ATTR_GOZAG_SHOPS_CURRENT] = 0;
        }
        you.duration[DUR_GOZAG_GOLD_AURA] = 0;
        for (branch_iterator it; it; ++it)
            branch_bribe[it->id] = 0;
        add_daction(DACT_BRIBE_TIMEOUT);
        add_daction(DACT_REMOVE_GOZAG_SHOPS);
        shopping_list.remove_dead_shops();
        you.exp_docked[old_god] = exp_needed(min<int>(you.max_level, 27) + 1)
                                  - exp_needed(min<int>(you.max_level, 27));
        you.exp_docked_total[old_god] = you.exp_docked[old_god];
        _set_penance(old_god, 50);
        break;

    case GOD_QAZLAL:
        if (old_piety >= piety_breakpoint(0))
        {
            mprf(MSGCH_GOD, old_god, "Your storm instantly dissipates.");
            you.redraw_armour_class = true;
        }
        if (you.duration[DUR_QAZLAL_FIRE_RES])
        {
            mprf(MSGCH_DURATION, "Your resistance to fire fades away.");
            you.duration[DUR_QAZLAL_FIRE_RES] = 0;
        }
        if (you.duration[DUR_QAZLAL_COLD_RES])
        {
            mprf(MSGCH_DURATION, "Your resistance to cold fades away.");
            you.duration[DUR_QAZLAL_COLD_RES] = 0;
        }
        if (you.duration[DUR_QAZLAL_ELEC_RES])
        {
            mprf(MSGCH_DURATION,
                 "Your resistance to electricity fades away.");
            you.duration[DUR_QAZLAL_ELEC_RES] = 0;
        }
        if (you.duration[DUR_QAZLAL_AC])
        {
            mprf(MSGCH_DURATION,
                 "Your resistance to physical damage fades away.");
            you.duration[DUR_QAZLAL_AC] = 0;
            you.redraw_armour_class = true;
        }
        _set_penance(old_god, 25);
        break;

    case GOD_PAKELLAS:
        simple_god_message(" continues to block your magic from regenerating.",
                           old_god);
        if (you.duration[DUR_DEVICE_SURGE])
            you.duration[DUR_DEVICE_SURGE] = 0;
        _set_penance(old_god, 25);
        break;

    case GOD_CHEIBRIADOS:
        simple_god_message(" continues to slow your movements.", old_god);
        _set_penance(old_god, 25);
        break;

    case GOD_HEPLIAKLQANA:
        add_daction(DACT_ALLY_HEPLIAKLQANA);
        remove_all_companions(GOD_HEPLIAKLQANA);

        you.exp_docked[old_god] = exp_needed(min<int>(you.max_level, 27) + 1)
                                    - exp_needed(min<int>(you.max_level, 27));
        you.exp_docked_total[old_god] = you.exp_docked[old_god];
        _set_penance(old_god, 50);
        break;
    default:
        _set_penance(old_god, 25);
        break;
    }

    // When you start worshipping a non-good god, or no god, you make
    // all non-hostile holy beings that worship a good god hostile.
    if (!is_good_god(new_god) && query_daction_counter(DACT_ALLY_HOLY))
    {
        mprf(MSGCH_MONSTER_ENCHANT, "The divine host forsakes you.");
        add_daction(DACT_ALLY_HOLY);
    }

#ifdef USE_TILE_LOCAL
    tiles.layout_statcol();
    redraw_screen();
#endif

    // Evil hack.
    learned_something_new(HINT_EXCOMMUNICATE,
                          coord_def((int)new_god, old_piety));

    for (ability_type abil : get_god_abilities())
        you.stop_train.insert(abil_skill(abil));

    update_can_train();
    you.can_train.set(SK_INVOCATIONS, false);
    reset_training();

    // Perhaps we abandoned Trog with everything but Spellcasting maxed out.
    check_selected_skills();
}

static void _erase_between(string& s, const string &left, const string &right)
{
    string::size_type left_pos;
    string::size_type right_pos;

    while ((left_pos = s.find(left)) != string::npos
           && (right_pos = s.find(right, left_pos + left.size())) != string::npos)
    {
        s.erase(s.begin() + left_pos, s.begin() + right_pos + right.size());
    }
}

static string _sacrifice_message(string msg, const string& itname, bool glowing,
                                 bool plural, piety_gain_t piety_gain)
{
    if (glowing)
    {
        msg = replace_all(msg, "[", "");
        msg = replace_all(msg, "]", "");
    }
    else
        _erase_between(msg, "[", "]");
    msg = replace_all(msg, "%", (plural ? "" : "s"));
    msg = replace_all(msg, "&", conjugate_verb("be", plural));

    const char *tag_start, *tag_end;
    switch (piety_gain)
    {
    case PIETY_NONE:
        tag_start = "<lightgrey>";
        tag_end = "</lightgrey>";
        break;
    default:
    case PIETY_SOME:
        tag_start = tag_end = "";
        break;
    case PIETY_LOTS:
        tag_start = "<white>";
        tag_end = "</white>";
        break;
    }

    msg.insert(0, itname);
    msg = tag_start + msg + tag_end;

    return msg;
}

void print_sacrifice_message(god_type god, const item_def &item,
                             piety_gain_t piety_gain, bool your)
{
    if (god == GOD_ELYVILON && get_weapon_brand(item) == SPWPN_HOLY_WRATH)
    {
        // Weapons blessed by TSO don't get destroyed but are instead
        // returned whence they came. (jpeg)
        simple_god_message(
            make_stringf(" %sreclaims %s.",
                         piety_gain ? "gladly " : "",
                         item.name(DESC_THE).c_str()).c_str(),
            GOD_SHINING_ONE);
        return;
    }
    const string itname = item.name(your ? DESC_YOUR : DESC_THE);
    mprf(MSGCH_GOD, god, "%s",
         _sacrifice_message(_Sacrifice_Messages[god][piety_gain], itname,
                           itname.find("glowing") != string::npos,
                           item.quantity > 1,
                           piety_gain).c_str());
}

static string nemelex_death_glow_message(int piety_gain)
{
    static const char *messages[NUM_PIETY_GAIN] =
    {
        " disappear% without a[dditional] glow.",
        " glow% slightly [brighter ]and disappear%.",
        " glow% with a rainbow of weird colours and disappear%.",
    };

    return messages[piety_gain];
}

void nemelex_death_message()
{
    const piety_gain_t piety_gain = static_cast<piety_gain_t>
            (min(random2(you.piety) / 30, (int)PIETY_LOTS));

    mpr(_sacrifice_message(nemelex_death_glow_message(piety_gain),
                           "Your body", you.backlit(), false, piety_gain));
}

bool god_hates_attacking_friend(god_type god, const monster *fr)
{
    if (!fr || fr->kill_alignment() != KC_FRIENDLY)
        return false;

    monster_type species = fr->mons_species();

    if (mons_is_object(species))
        return false;
    switch (god)
    {
        case GOD_ZIN:
        case GOD_SHINING_ONE:
        case GOD_ELYVILON:
        case GOD_OKAWARU:
            return true;
        case GOD_BEOGH: // added penance to avoid killings for loot
            return mons_genus(species) == MONS_ORC;
        case GOD_JIYVA:
            return mons_class_is_slime(species);
        case GOD_FEDHAS:
            return _fedhas_protects_species(species);
        default:
            return false;
    }
}

/**
 * Does this god accept items for sacrifice?
 *
 * @param god The god.
 * @param greedy_explore If true, the return value is based on whether
 *                       we should make explore greedy for items under
 *                       this god.
 * @return  True if the god accepts items for sacrifice, false otherwise.
*/
bool god_likes_items(god_type god, bool greedy_explore)
{
    if (greedy_explore && !(Options.explore_stop & ES_GREEDY_SACRIFICEABLE))
        return false;

    switch (god)
    {
    case GOD_BEOGH:
        return true;

    case NUM_GODS: case GOD_RANDOM: case GOD_NAMELESS:
        die("Bad god for item sacrifice check: %d", static_cast<int>(god));

    default:
        return false;
    }
}

/**
 * Does a god like a particular item for sacrifice?
 *
 * @param god The god.
 * @param item The item.
 * @return  True if the god likes the item, false otherwise.
*/
bool god_likes_item(god_type god, const item_def& item)
{
    if (!god_likes_items(god))
        return false;

    switch (god)
    {
    case GOD_BEOGH:
        return item.base_type == OBJ_CORPSES
               && mons_genus(item.mon_type) == MONS_ORC;

    default:
        return false;
    }
}

static bool _transformed_player_can_join_god(god_type which_god)
{
    if (which_god == GOD_ZIN && you.form != TRAN_NONE)
        return false; // zin hates everything
    // all these clauses are written with a ! in front of them, so that
    // the stuff to the right of that is uniformly "gods that hate this form"
    switch (you.form) {
    case TRAN_LICH:
        return !(is_good_god(which_god) || which_god == GOD_FEDHAS);
    case TRAN_SHADOW:
        return !is_good_god(which_god);
    case TRAN_STATUE:
        return !(which_god == GOD_YREDELEMNUL);
    default:
        return true;
    }
}

int gozag_service_fee()
{
    if (you.char_class == JOB_MONK && had_gods() == 0)
        return 0;

    const int gold = you.attribute[ATTR_GOLD_GENERATED];
    int fee = (int)(gold - gold / log10(gold + 10.0))/2;

    dprf("found %d gold, fee %d", gold, fee);
    return fee;
}

bool player_can_join_god(god_type which_god)
{
    if (you.species == SP_DEMIGOD)
        return false;

    if (is_good_god(which_god) && you.undead_or_demonic())
        return false;

    if (which_god == GOD_YREDELEMNUL && you.is_artificial())
        return false;

    if (which_god == GOD_BEOGH && !species_is_orcish(you.species))
        return false;

    if (which_god == GOD_HEPLIAKLQANA && you.species == SP_FELID)
        return false;

    // Fedhas hates undead, but will accept demonspawn.
    if (which_god == GOD_FEDHAS && you.holiness() & MH_UNDEAD)
        return false;

#if TAG_MAJOR_VERSION == 34
    // Dithmenos hates fiery species.
    if (which_god == GOD_DITHMENOS
        && (you.species == SP_DJINNI
            || you.species == SP_LAVA_ORC))
    {
        return false;
    }
#endif

    if (which_god == GOD_GOZAG && you.gold < gozag_service_fee())
        return false;

    if (player_mutation_level(MUT_NO_LOVE)
        && (which_god == GOD_BEOGH
            ||  which_god == GOD_JIYVA
            ||  which_god == GOD_ELYVILON
            ||  which_god == GOD_HEPLIAKLQANA))
    {
        return false;
    }

    if (player_mutation_level(MUT_NO_ARTIFICE)
        && which_god == GOD_NEMELEX_XOBEH)
    {
      return false;
    }

    return _transformed_player_can_join_god(which_god);
}

// Handle messaging and identification for items/equipment on conversion.
static void _god_welcome_handle_gear()
{
    // Check for amulets of faith.
    item_def *amulet = you.slot_item(EQ_AMULET, false);
    if (amulet && amulet->sub_type == AMU_FAITH && !is_useless_item(*amulet))
    {
        mprf(MSGCH_GOD, "Your amulet flashes!");
        flash_view_delay(UA_PLAYER, god_colour(you.religion), 300);
    }

    if (you_worship(GOD_ASHENZARI))
    {
        if (!item_type_known(OBJ_SCROLLS, SCR_REMOVE_CURSE))
        {
            set_ident_type(OBJ_SCROLLS, SCR_REMOVE_CURSE, true);
            pack_item_identify_message(OBJ_SCROLLS, SCR_REMOVE_CURSE);
        }
    }

    if (have_passive(passive_t::identify_items))
    {
        // Seemingly redundant with auto_id_inventory(), but we don't want to
        // announce items where the only new information is their cursedness.
        for (auto &item : you.inv)
            if (item.defined())
                item.flags |= ISFLAG_KNOW_CURSE;

        auto_id_inventory();
    }

    if (have_passive(passive_t::detect_portals))
        ash_detect_portals(true);

    // Give a reminder to remove any disallowed equipment.
    for (int i = EQ_MIN_ARMOUR; i < EQ_MAX_ARMOUR; i++)
    {
        const item_def* item = you.slot_item(static_cast<equipment_type>(i));
        if (item && god_hates_item(*item))
        {
            mprf(MSGCH_GOD, "%s warns you to remove %s.",
                 uppercase_first(god_name(you.religion)).c_str(),
                 item->name(DESC_YOUR, false, false, false).c_str());
        }
    }

    // Refresh wielded/quivered weapons in case we have a new conduct
    // on them.
    you.wield_change = true;
    you.redraw_quiver = true;
}

/* Make a CrawlStoreValue an empty vector with the requested item type.
 * It is an error if the value already had a different type.
 */
static void _make_empty_vec(CrawlStoreValue &v, store_val_type vectype)
{
    const store_val_type currtype = v.get_type();
    ASSERT(currtype == SV_NONE || currtype == SV_VEC);

    if (currtype == SV_NONE)
        v.new_vector(vectype);
    else
    {
        CrawlVector &vec = v.get_vector();
        const store_val_type old_vectype = vec.get_type();
        ASSERT(old_vectype == SV_NONE || old_vectype == vectype);
        vec.clear();
    }
}

// Note: we're trying for a behaviour where the player gets
// to keep their assigned invocation slots if they get excommunicated
// and then rejoin (but if they spend time with another god we consider
// the old invocation slots void and erase them). We also try to
// protect any bindings the character might have made into the
// traditional invocation slots (a-e and X). -- bwr
void set_god_ability_slots()
{
    ASSERT(!you_worship(GOD_NO_GOD));

    if (find(begin(you.ability_letter_table), end(you.ability_letter_table),
             ABIL_RENOUNCE_RELIGION) == end(you.ability_letter_table)
        && you.ability_letter_table[letter_to_index('X')] == ABIL_NON_ABILITY)
    {
        you.ability_letter_table[letter_to_index('X')] = ABIL_RENOUNCE_RELIGION;
    }

    // Clear out other god invocations.
    for (ability_type& slot : you.ability_letter_table)
    {
        for (god_iterator it; it; ++it)
        {
            if (slot == ABIL_NON_ABILITY)
                break;
            if (*it == you.religion)
                continue;
            for (const god_power& power : god_powers[*it])
                if (slot == power.abil)
                    slot = ABIL_NON_ABILITY;
        }
    }

    int num = letter_to_index('a');
    // Not using get_god_powers, so that hotkeys remain stable across games
    // even if you can't use a particular ability in a given game.
    for (const god_power& power : god_powers[you.religion])
    {
        if (power.abil != ABIL_NON_ABILITY
            // Animate Dead doesn't have its own hotkey; it steals
            // Animate Remains'
            && power.abil != ABIL_YRED_ANIMATE_DEAD
            && find(begin(you.ability_letter_table),
                    end(you.ability_letter_table), power.abil)
               == end(you.ability_letter_table)
            && you.ability_letter_table[num] == ABIL_NON_ABILITY)
        {
            // Assign sequentially: first ability 'a', third ability 'c',
            // etc., even if one is remapped.
            if (find_ability_slot(power.abil, index_to_letter(num)) < 0)
            {
                you.ability_letter_table[num] = power.abil;
                auto_assign_ability_slot(num);
            }
            ++num;
        }
    }
}

/// Check if the monk's joining bonus should be given. (Except Gozag's.)
static void _apply_monk_bonus()
{
    if (you.char_class != JOB_MONK || had_gods() > 0)
        return;

    // monks get bonus piety for first god
    if (you_worship(GOD_RU))
        you.props[RU_SACRIFICE_PROGRESS_KEY] = 9999;
    else
        gain_piety(35, 1, false);
}

/// Transfer some piety from an old good god to a new one, if applicable.
static void _transfer_good_god_piety()
{
    if (!is_good_god(you.religion))
        return;

    const god_type old_god = you.previous_good_god;
    const uint8_t old_piety = you.saved_good_god_piety;

    if (!is_good_god(old_god))
        return;

    if (you.religion != old_god)
    {
        static const map<god_type, const char*> farewell_messages = {
            { GOD_ELYVILON, "aid the meek" },
            { GOD_SHINING_ONE, "vanquish evil" },
            { GOD_ZIN, "enforce order" },
        };

        // Some feedback that piety moved over.
        simple_god_message(make_stringf(" says: Farewell. Go and %s with %s.",
                                        lookup(farewell_messages, old_god,
                                               "become a bug"),
                                        god_name(you.religion).c_str()).c_str(),

                           old_god);
    }

    // Give a piety bonus when switching between good gods, or back to the
    // same good god.
    if (old_piety > piety_breakpoint(0))
        gain_piety(old_piety - piety_breakpoint(0), 2, false);
}


/**
 * Give an appropriate message for the given good god to give in response to
 * the player joining a god that brings down their wrath.
 *
 * @param good_god    The good god in question.
 */
static string _good_god_wrath_message(god_type good_god)
{
    switch (good_god)
    {
        case GOD_ELYVILON:
            return "Your evil deeds will not go unpunished";
        case GOD_SHINING_ONE:
            return "You will pay for your evil ways, mortal";
        case GOD_ZIN:
            return make_stringf("You will suffer for embracing such %s",
                                is_chaotic_god(you.religion) ? "chaos"
                                                             : "evil");
        default:
            return "You will be buggily punished for this";
    }
}

/**
 * Check if joining the current god will cause wrath for any previously-
 * worshipped good gods. If so, message & set penance timeouts.
 *
 * @param old_god    The previous god worshipped; may be GOD_NO_GOD.
 */
static void _check_good_god_wrath(god_type old_god)
{
    for (god_type good_god : { GOD_ELYVILON, GOD_SHINING_ONE, GOD_ZIN })
    {
        if (old_god == good_god || !you.penance[good_god]
            || !god_hates_your_god(good_god, you.religion))
        {
            continue;
        }

        const string wrath_message
            = make_stringf(" says: %s!",
                           _good_god_wrath_message(good_god).c_str());
        simple_god_message(wrath_message.c_str(), good_god);
        set_penance_xp_timeout();
    }
}

/// Handle basic god piety & related setup for a new-joined god.
static void _set_initial_god_piety()
{
    // Currently, penance is just zeroed. This could be much more
    // interesting.
    you.penance[you.religion] = 0;

    switch (you.religion)
    {
    case GOD_XOM:
        // Xom uses piety and gift_timeout differently.
        you.piety = HALF_MAX_PIETY;
        you.gift_timeout = random2(40) + random2(40);
        break;

    case GOD_RU:
        you.piety = 10; // one moderate sacrifice should get you to *.
        you.piety_hysteresis = 0;
        you.gift_timeout = 0;

        // I'd rather this be in on_join(), but then it overrides the
        // monk bonus...
        you.props[RU_SACRIFICE_PROGRESS_KEY] = 0;
        // offer the first sacrifice faster than normal
    {
        int delay = 50;
        if (crawl_state.game_is_sprint())
            delay /= SPRINT_MULTIPLIER;
        you.props[RU_SACRIFICE_DELAY_KEY] = delay;
    }
        you.props[RU_SACRIFICE_PENALTY_KEY] = 0;
        break;

    default:
        you.piety = 15; // to prevent near instant excommunication
        if (you.piety_max[you.religion] < 15)
            you.piety_max[you.religion] = 15;
        you.piety_hysteresis = 0;
        you.gift_timeout = 0;
        break;
    }

    // Tutorial needs berserk usable.
    if (crawl_state.game_is_tutorial())
        gain_piety(30, 1, false);

    _apply_monk_bonus();
    _transfer_good_god_piety();
}

/// Setup when joining the greedy magnates of Gozag.
static void _join_gozag()
{
    // Handle the fee.
    const int fee = gozag_service_fee();
    if (fee > 0)
    {
        ASSERT(you.gold >= fee);
        mprf(MSGCH_GOD, "You pay a service fee of %d gold.", fee);
        you.gold -= fee;
        you.attribute[ATTR_GOZAG_GOLD_USED] += fee;
    }
    else
        simple_god_message(" waives the service fee.");

    // Note relevant powers.
    bool needs_redraw = false;
    for (const auto& power : get_god_powers(you.religion))
    {
        if (power.abil == ABIL_GOZAG_POTION_PETITION
            && !you.attribute[ATTR_GOZAG_FIRST_POTION])
        {
            simple_god_message(" offers you a free set of potion effects!");
            needs_redraw = true;
            continue;
        }
        if (you.gold >= get_gold_cost(power.abil))
        {
            power.display(true, "You have enough gold to %s.");
            needs_redraw = true;
        }
    }

    if (needs_redraw)
    {
#ifdef USE_TILE_LOCAL
        tiles.layout_statcol();
        redraw_screen();
#else
        ;
#endif
    }

    // Move gold to top of piles.
    add_daction(DACT_GOLD_ON_TOP);
}

/**
 * Choose an antique name for a Hepliaklqana-granted ancestor.
 *
 * @param female    Whether the ancestor is female or male.
 * @return          An appropriate name; e.g. Hrodulf, Citali, Aat.
 */
static string _make_ancestor_name(bool female)
{
    const string gender_name = female ? "female" : "male";
    const string suffix = " " + gender_name + " name";
    const string name = getRandNameString("ancestor", suffix);
    return name.empty() ? make_name() : name;
}

/// Setup when joining the devoted followers of Hepliaklqana.
static void _join_hepliaklqana()
{
    // initial setup.
    if (!you.props.exists(HEPLIAKLQANA_ALLY_NAME_KEY))
    {
        const bool female = coinflip();
        you.props[HEPLIAKLQANA_ALLY_NAME_KEY] = _make_ancestor_name(female);
        you.props[HEPLIAKLQANA_ALLY_GENDER_KEY] = female ? GENDER_FEMALE
                                                         : GENDER_MALE;
    }

    // Complimentary ancestor upon joining.
    const mgen_data mg = hepliaklqana_ancestor_gen_data();
    delayed_monster(mg);
    simple_god_message(make_stringf(" brings forth the memory of your ancestor,"
                                    " %s!",
                                    mg.mname.c_str()).c_str());
}

/// Setup when joining the gelatinous groupies of Jiyva.
static void _join_jiyva()
{
    // Complimentary jelly upon joining.
    if (_has_jelly())
        return;

    mgen_data mg(MONS_JELLY, BEH_STRICT_NEUTRAL, &you, 0, 0, you.pos(),
                 MHITNOT, MG_NONE, GOD_JIYVA);

    delayed_monster(mg);
    simple_god_message(" grants you a jelly!");
}

/// Setup when joining the sacred cult of Ru.
static void _join_ru()
{
    _make_empty_vec(you.props[AVAILABLE_SAC_KEY], SV_INT);
    _make_empty_vec(you.props[HEALTH_SAC_KEY], SV_INT);
    _make_empty_vec(you.props[ESSENCE_SAC_KEY], SV_INT);
    _make_empty_vec(you.props[PURITY_SAC_KEY], SV_INT);
    _make_empty_vec(you.props[ARCANA_SAC_KEY], SV_INT);
}

/// Setup for joining the furious barbarians of Trog.
static void _join_trog()
{
    for (int sk = SK_SPELLCASTING; sk <= SK_LAST_MAGIC; ++sk)
        if (you.skills[sk])
            you.train[sk] = 0;

    // When you start worshipping Trog, you make all non-hostile magic
    // users hostile.
    if (query_daction_counter(DACT_ALLY_SPELLCASTER))
    {
        add_daction(DACT_ALLY_SPELLCASTER);
        mprf(MSGCH_MONSTER_ENCHANT, "Your magic-using allies forsake you.");
    }
}

// Setup for joining the orderly ascetics of Zin.
static void _join_zin()
{
    // When you start worshipping Zin, you make all non-hostile unclean and
    // chaotic beings hostile.
    if (query_daction_counter(DACT_ALLY_UNCLEAN_CHAOTIC))
    {
        add_daction(DACT_ALLY_UNCLEAN_CHAOTIC);
        mprf(MSGCH_MONSTER_ENCHANT, "Your unclean and chaotic allies forsake you.");
    }

    // Need to pay St. Peters.
    if (you.attribute[ATTR_DONATIONS] * 9 < you.gold)
    {
        item_def lucre;
        lucre.base_type = OBJ_GOLD;
        // If you worshipped Zin before, the already tithed for part is fine.
        lucre.quantity = you.gold - you.attribute[ATTR_DONATIONS] * 9;
        // Use the harsh acquirement pricing -- with a cap at +50 piety.
        // We don't want you get max piety at start just because you're filthy
        // rich. In that case, you have to donate again more... That the poor
        // widow is not spared doesn't mean the rich can't be milked for more.
        lucre.props[ACQUIRE_KEY] = 0;
        you.gold -= zin_tithe(lucre, lucre.quantity, false, true);
    }
}

// Setup when becoming an overworked assistant to Pakellas.
static void _join_pakellas()
{
    mprf(MSGCH_GOD, "You stop regenerating magic.");
    mprf(MSGCH_GOD, "You can now gain magical power from killing.");
    pakellas_id_device_charges();
    you.attribute[ATTR_PAKELLAS_EXTRA_MP] = POT_MAGIC_MP;
}

/// What special things happen when you join a god?
static const map<god_type, function<void ()>> on_join = {
    { GOD_ASHENZARI, []() { ash_check_bondage(); }},
    { GOD_BEOGH, update_player_symbol },
    { GOD_CHEIBRIADOS, []() {
        simple_god_message(" begins to support your attributes as your "
                           "movement slows.");
        notify_stat_change();
    }},
    { GOD_FEDHAS, []() {
        mprf(MSGCH_MONSTER_ENCHANT, "The plants of the dungeon cease their "
             "hostilities.");
        if (env.forest_awoken_until)
            for (monster_iterator mi; mi; ++mi)
                mi->del_ench(ENCH_AWAKEN_FOREST);
    }},
    { GOD_GOZAG, _join_gozag },
    { GOD_JIYVA, _join_jiyva },
    { GOD_HEPLIAKLQANA, _join_hepliaklqana },
    { GOD_LUGONU, []() {
        if (you.worshipped[GOD_LUGONU] == 0)
            gain_piety(20, 1, false);  // allow instant access to first power
    }},
    { GOD_PAKELLAS, _join_pakellas },
    { GOD_RU, _join_ru },
    { GOD_TROG, _join_trog },
    { GOD_ZIN, _join_zin },
};

void join_religion(god_type which_god)
{
    ASSERT(which_god != GOD_NO_GOD);
    ASSERT(which_god != GOD_ECUMENICAL);
    ASSERT(you.species != SP_DEMIGOD);

    redraw_screen();

    const god_type old_god = you.religion;
    if (you.previous_good_god == GOD_NO_GOD)
    {
        you.previous_good_god = old_god;
        you.saved_good_god_piety = you.piety;
        // doesn't matter if old_god isn't actually a good god; we check later
        // and then wipe it at the end of the function regardless
    }

    // Leave your prior religion first.
    if (!you_worship(GOD_NO_GOD))
        excommunication(true, which_god);

    // Welcome to the fold!
    you.religion = static_cast<god_type>(which_god);

    mark_milestone("god.worship", "became a worshipper of "
                   + god_name(you.religion) + ".");
    take_note(Note(NOTE_GET_GOD, you.religion));
    simple_god_message(
        make_stringf(" welcomes you%s!",
                     you.worshipped[which_god] ? " back" : "").c_str());
    // included in default force_more_message
#ifdef DGL_WHEREIS
    whereis_record();
#endif

    _set_initial_god_piety();

    set_god_ability_slots();    // remove old god's slots, reserve new god's
    _god_welcome_handle_gear();

    // When you start worshipping a good god, you make all non-hostile
    // unholy and evil beings hostile.
    if (is_good_god(you.religion)
        && query_daction_counter(DACT_ALLY_UNHOLY_EVIL))
    {
        add_daction(DACT_ALLY_UNHOLY_EVIL);
        mprf(MSGCH_MONSTER_ENCHANT, "Your unholy and evil allies forsake you.");
    }

    // Chei worshippers start their stat gain immediately.
    if (have_passive(passive_t::stat_boost))
    {
        string msg = " begins to support your attributes";
        if (have_passive(passive_t::slowed))
            msg += " as your movement slows";
        msg += ".";
        simple_god_message(msg.c_str());
        notify_stat_change();
    }

    // Move gold to top of piles with Gozag.
    if (have_passive(passive_t::detect_gold))
        add_daction(DACT_GOLD_ON_TOP);

    const function<void ()> *join_effect = map_find(on_join, you.religion);
    if (join_effect != nullptr)
        (*join_effect)();

    // after join_effect() so that gozag's service fee is right for monks
    if (you.worshipped[you.religion] < 100)
        you.worshipped[you.religion]++;

    // Warn if a good god is starting wrath now.
    _check_good_god_wrath(old_god);

    if (!you_worship(GOD_GOZAG))
        for (const auto& power : get_god_powers(you.religion))
            if (power.rank <= 0)
                power.display(true, "You can now %s.");

    // Allow training all divine ability skills immediately.
    vector<ability_type> abilities = get_god_abilities();
    for (ability_type abil : abilities)
        you.start_train.insert(abil_skill(abil));
    update_can_train();

    // now that you have a god, you can't save any piety from your prev god
    you.previous_good_god = GOD_NO_GOD;
    you.saved_good_god_piety = 0;

    you.redraw_title = true;

#ifdef USE_TILE_LOCAL
    tiles.layout_statcol();
    redraw_screen();
#endif

    learned_something_new(HINT_CONVERT);
}

void god_pitch(god_type which_god)
{
    if (which_god == GOD_BEOGH && grd(you.pos()) != DNGN_ALTAR_BEOGH)
        mpr("You bow before the missionary of Beogh.");
    else
    {
        mprf("You %s the altar of %s.",
             get_form()->player_prayer_action().c_str(),
             god_name(which_god).c_str());
    }
    // these are included in default force_more_message

    const int fee = (which_god == GOD_GOZAG) ? gozag_service_fee() : 0;

    // Note: using worship we could make some gods not allow followers to
    // return, or not allow worshippers from other religions. - bwr

    // Gods can be racist...
    if (!player_can_join_god(which_god))
    {
        you.turn_is_over = false;
        if (which_god == GOD_GOZAG)
        {
            simple_god_message(" does not accept service from beggars like you!",
                               which_god);
            if (you.gold == 0)
            {
                mprf("The service fee for joining is currently %d gold; you have"
                     " none.", fee);
            }
            else
            {
                mprf("The service fee for joining is currently %d gold; you only"
                     " have %d.", fee, you.gold);
            }
        }
        else if (player_mutation_level(MUT_NO_LOVE)
                 && (which_god == GOD_BEOGH
                 || which_god == GOD_ELYVILON
                 || which_god == GOD_JIYVA
                 || which_god == GOD_HEPLIAKLQANA))
        {
            simple_god_message(" does not accept worship from the loveless!",
                               which_god);
        }
        else if (player_mutation_level(MUT_NO_ARTIFICE)
                 && which_god == GOD_NEMELEX_XOBEH)
        {
            simple_god_message(" does not accept worship for those who cannot "
                              "deal a hand of cards!", which_god);
        } else if (you.species == SP_FELID && which_god == GOD_HEPLIAKLQANA)
        {
            simple_god_message(" does not accept worship from the spawn of "
                               "common housecats!", which_god);
        }
        else if (!_transformed_player_can_join_god(which_god))
        {
            simple_god_message(" says: How dare you come in such a loathsome"
                               " form!",
                               which_god);
        }
        else
        {
            simple_god_message(" does not accept worship from those such as"
                               " you!",
                               which_god);
        }
        return;
    }

    if (which_god == GOD_LUGONU && you.penance[GOD_LUGONU])
    {
        you.turn_is_over = false;
        simple_god_message(" refuses to forgive you so easily!", which_god);
        return;
    }

#ifdef USE_TILE_WEB
    tiles_crt_control show_as_menu(CRT_MENU, "god_pitch");
#endif

    describe_god(which_god, false);

    string service_fee = "";
    if (which_god == GOD_GOZAG)
    {
        if (fee == 0)
        {
            service_fee = string("Gozag will waive the service fee if you ")
                          + (coinflip() ? "act now" : "join today") + "!\n";
        }
        else
        {
            service_fee = make_stringf(
                    "The service fee for joining is currently %d gold; you"
                    " have %d.\n",
                    fee, you.gold);
        }
    }
    const string prompt = make_stringf("%sDo you wish to %sjoin this religion?",
                                       service_fee.c_str(),
                                       (you.worshipped[which_god]) ? "re" : "");

    cgotoxy(1, 21, GOTO_CRT);
    textcolour(channel_to_colour(MSGCH_PROMPT));
    if (!yesno(prompt.c_str(), false, 'n', true, true, false, nullptr, GOTO_CRT))
    {
        you.turn_is_over = false; // Okay, opt out.
        redraw_screen();
        return;
    }

    // OK, so join the new religion.
    join_religion(which_god);
}

/** Ask the user for a god by name.
 *
 * @param def_god The god to use if the user presses just ENTER
 *                (default NUM_GODS).
 * @return the best match for the user's input, or def_god if the user
 *         pressed ENTER without providing input, or NUM_GODS if the
 *         user cancelled or there was no match.
 */
god_type choose_god(god_type def_god)
{
    char specs[80];

    string help = def_god == NUM_GODS ? "by name"
                                      : "default " + god_name(def_god);
    string prompt = make_stringf("Which god (%s)? ", help.c_str());

    if (msgwin_get_line(prompt, specs, sizeof(specs)) != 0)
        return NUM_GODS; // FIXME: distinguish cancellation from no match

    // If they pressed enter with no input.
    if (specs[0] == '\0')
        return def_god;

    string spec = lowercase_string(specs);

    return find_earliest_match(spec, GOD_NO_GOD, NUM_GODS,
                               always_true<god_type>,
                               bind(god_name, placeholders::_1, false));
}

int had_gods()
{
    int count = 0;
    for (god_iterator it; it; ++it)
        count += you.worshipped[*it];
    return count;
}

bool god_likes_your_god(god_type god, god_type your_god)
{
    return is_good_god(god) && is_good_god(your_god);
}

bool god_hates_your_god(god_type god, god_type your_god)
{
    // Ru doesn't care.
    if (god == GOD_RU)
        return false;

    // Gods do not hate themselves.
    if (god == your_god)
        return false;

    // Non-good gods always hate your current god.
    if (!is_good_god(god))
        return true;

    // Zin hates chaotic gods.
    if (god == GOD_ZIN && is_chaotic_god(your_god))
        return true;

    return is_evil_god(your_god);
}

bool god_hates_cannibalism(god_type god)
{
    return is_good_god(god) || god == GOD_BEOGH;
}

bool god_hates_killing(god_type god, const monster* mon)
{
    // Must be at least a creature of sorts. Smacking down an enchanted
    // weapon or disrupting a lightning doesn't count. Technically, this
    // might raise a concern about necromancy but zombies traditionally
    // count as creatures and that's the average person's (even if not ours)
    // intuition.
    if (mons_is_object(mon->type))
        return false;

    // kill as many illusions as you want.
    if (mon->is_illusion())
        return false;

    bool retval = false;
    const mon_holy_type holiness = mon->holiness();

    if (holiness & MH_HOLY)
            retval = (is_good_god(god));
    else if (holiness & MH_NATURAL)
            retval = (god == GOD_ELYVILON);

    if (god == GOD_FEDHAS)
        retval = (fedhas_protects(mon));

    return retval;
}

/**
 * Will the given god object if you eat a monster of this type?
 *
 * @param god       The god in question.
 * @param mc        The monster type to be eaten.
 * @return          Whether eating this monster will incur penance.
 */
bool god_hates_eating(god_type god, monster_type mc)
{
    if (god_hates_cannibalism(god) && is_player_same_genus(mc))
        return true;
    if (is_good_god(you.religion) && mons_class_holiness(mc) & MH_HOLY)
        return true;
    if (you_worship(GOD_ZIN) && mons_class_intel(mc) >= I_HUMAN)
        return true;
    return false;
}

bool god_likes_spell(spell_type spell, god_type god)
{
    switch (god)
    {
    case GOD_VEHUMET:
        return vehumet_supports_spell(spell);
    case GOD_KIKUBAAQUDGHA:
        return spell_typematch(spell, SPTYP_NECROMANCY);
    default: // quash unhandled constants warnings
        return false;
    }
}

bool god_hates_spellcasting(god_type god)
{
    return god == GOD_TROG;
}

bool god_hates_spell(spell_type spell, god_type god, bool rod_spell)
{
    if (god_hates_spellcasting(god))
        return !rod_spell;

    if (god_punishes_spell(spell, god))
        return true;

    spschools_type disciplines = get_spell_disciplines(spell);

    switch (god)
    {
    case GOD_SHINING_ONE:
        // TSO hates using poison, but is fine with curing it.
        if ((disciplines & SPTYP_POISON) && spell != SPELL_CURE_POISON)
            return true;
        break;
    case GOD_CHEIBRIADOS:
        if (is_hasty_spell(spell))
            return true;
        break;
    case GOD_DITHMENOS:
        if (is_fiery_spell(spell))
            return true;
        break;
    case GOD_PAKELLAS:
        if (spell == SPELL_SUBLIMATION_OF_BLOOD)
            return true;
        break;
    default:
        break;
    }
    return false;
}

bool god_loathes_spell(spell_type spell, god_type god)
{
    if (spell == SPELL_NECROMUTATION && is_good_god(god))
        return true;
    if (spell == SPELL_STATUE_FORM && god == GOD_YREDELEMNUL)
        return true;
    return false;
}

bool god_hates_ability(ability_type ability, god_type god)
{
    switch (ability)
    {
        case ABIL_SPIT_POISON:
        case ABIL_BREATHE_POISON:
        case ABIL_BREATHE_MEPHITIC:
            return god == GOD_SHINING_ONE;
        case ABIL_BREATHE_FIRE:
        case ABIL_BREATHE_STICKY_FLAME:
        case ABIL_DELAYED_FIREBALL:
        case ABIL_HELLFIRE:
            return god == GOD_DITHMENOS;
        case ABIL_EVOKE_BERSERK:
            return god == GOD_CHEIBRIADOS;
        default:
            break;
    }
    return false;
}

int elyvilon_lifesaving()
{
    if (!you_worship(GOD_ELYVILON))
        return 0;

    if (you.piety < piety_breakpoint(0))
        return 0;

    return you.piety > 130 ? 2 : 1;
}

bool god_protects_from_harm()
{
    if (you.duration[DUR_LIFESAVING])
    {
        switch (elyvilon_lifesaving())
        {
        case 1:
            if (random2(you.piety) >= piety_breakpoint(0))
                return true;
            break;
        case 2:
            // Reliable lifesaving is costly.
            lose_piety(21 + random2(20));
            return true;
        }
    }

    if (have_passive(passive_t::protect_from_harm)
        && (one_chance_in(10) || x_chance_in_y(you.piety, 1000)))
    {
        return true;
    }

    return false;
}

void handle_god_time(int /*time_delta*/)
{
    if (you.attribute[ATTR_GOD_WRATH_COUNT] > 0)
    {
        vector<god_type> angry_gods;
        // First count the number of gods to whom we owe penance.
        for (god_iterator it; it; ++it)
        {
            if (active_penance(*it))
                angry_gods.push_back(*it);
        }
        if (x_chance_in_y(angry_gods.size(), 20))
        {
            // This should be guaranteed; otherwise the god wouldn't have
            // appeared in the angry_gods list.
            const bool succ = divine_retribution(*random_iterator(angry_gods));
            ASSERT(succ);
        }
        you.attribute[ATTR_GOD_WRATH_COUNT]--;
    }

    // Update the god's opinion of the player.
    if (!you_worship(GOD_NO_GOD))
    {
        int delay;
        int sacrifice_count;
        switch (you.religion)
        {
        case GOD_TROG:
        case GOD_OKAWARU:
        case GOD_MAKHLEB:
        case GOD_BEOGH:
        case GOD_LUGONU:
        case GOD_DITHMENOS:
        case GOD_QAZLAL:
            if (one_chance_in(16))
                lose_piety(1);
            break;

        case GOD_YREDELEMNUL:
        case GOD_KIKUBAAQUDGHA:
        case GOD_VEHUMET:
        case GOD_ZIN:
        case GOD_PAKELLAS:
            if (one_chance_in(17))
                lose_piety(1);
            break;

        case GOD_JIYVA:
            if (one_chance_in(20))
                lose_piety(1);
            break;

        case GOD_ASHENZARI:
            if (one_chance_in(25))
                lose_piety(1);
            break;

        case GOD_SHINING_ONE:
        case GOD_NEMELEX_XOBEH:
            if (one_chance_in(35))
                lose_piety(1);
            break;

        case GOD_ELYVILON:
        case GOD_HEPLIAKLQANA:
            if (one_chance_in(50))
                lose_piety(1);
            break;

        case GOD_SIF_MUNA:
            if (one_chance_in(100))
                lose_piety(1);
            break;

        case GOD_RU:
            ASSERT(you.props.exists(RU_SACRIFICE_PROGRESS_KEY));
            ASSERT(you.props.exists(RU_SACRIFICE_DELAY_KEY));
            ASSERT(you.props.exists(AVAILABLE_SAC_KEY));

            delay = you.props[RU_SACRIFICE_DELAY_KEY].get_int();
            sacrifice_count = you.props[AVAILABLE_SAC_KEY].get_vector().size();

            // 6* is max piety for Ru
            if (sacrifice_count == 0 && you.piety < piety_breakpoint(5)
                && you.props[RU_SACRIFICE_PROGRESS_KEY].get_int() >= delay)
            {
              ru_offer_new_sacrifices();
            }

            break;

        case GOD_FEDHAS:
        case GOD_CHEIBRIADOS:
            // These gods do not lose piety over time.
        case GOD_GOZAG:
        case GOD_XOM:
            // Gods without normal piety do nothing each tick.
            return;

        case GOD_NO_GOD:
        case GOD_RANDOM:
        case GOD_ECUMENICAL:
        case GOD_NAMELESS:
        case NUM_GODS:
            die("Bad god, no bishop!");
            return;

        }

        if (you.piety < 1)
            excommunication();
    }
}

int god_colour(god_type god) // mv - added
{
    switch (god)
    {
    case GOD_ZIN:
    case GOD_SHINING_ONE:
    case GOD_ELYVILON:
    case GOD_OKAWARU:
    case GOD_FEDHAS:
        return CYAN;

    case GOD_YREDELEMNUL:
    case GOD_KIKUBAAQUDGHA:
    case GOD_MAKHLEB:
    case GOD_VEHUMET:
    case GOD_TROG:
    case GOD_BEOGH:
    case GOD_LUGONU:
    case GOD_ASHENZARI:
        return LIGHTRED;

    case GOD_GOZAG:
    case GOD_XOM:
        return YELLOW;

    case GOD_NEMELEX_XOBEH:
        return LIGHTMAGENTA;

    case GOD_SIF_MUNA:
        return LIGHTBLUE;

    case GOD_JIYVA:
        return GREEN;

    case GOD_CHEIBRIADOS:
    case GOD_HEPLIAKLQANA:
        return LIGHTCYAN;

    case GOD_DITHMENOS:
        return MAGENTA;

    case GOD_QAZLAL:
    case GOD_RU:
        return BROWN;

    case GOD_PAKELLAS:
        return LIGHTGREEN;

    case GOD_NO_GOD:
    case NUM_GODS:
    case GOD_RANDOM:
    case GOD_NAMELESS:
    default:
        break;
    }

    return YELLOW;
}

colour_t god_message_altar_colour(god_type god)
{
    int rnd;

    switch (god)
    {
    case GOD_SHINING_ONE:
        return YELLOW;

    case GOD_ZIN:
        return WHITE;

    case GOD_ELYVILON:
        return LIGHTBLUE;     // Really, LIGHTGREY but that's plain text.

    case GOD_OKAWARU:
        return CYAN;

    case GOD_YREDELEMNUL:
        return coinflip() ? DARKGREY : RED;

    case GOD_BEOGH:
        return coinflip() ? BROWN : LIGHTRED;

    case GOD_KIKUBAAQUDGHA:
        return DARKGREY;

    case GOD_FEDHAS:
        return coinflip() ? BROWN : GREEN;

    case GOD_XOM:
        return random2(15) + 1;

    case GOD_VEHUMET:
        rnd = random2(3);
        return (rnd == 0) ? LIGHTMAGENTA :
               (rnd == 1) ? LIGHTRED
                          : LIGHTBLUE;

    case GOD_MAKHLEB:
        rnd = random2(3);
        return (rnd == 0) ? RED :
               (rnd == 1) ? LIGHTRED
                          : YELLOW;

    case GOD_TROG:
        return RED;

    case GOD_NEMELEX_XOBEH:
        return LIGHTMAGENTA;

    case GOD_SIF_MUNA:
        return BLUE;

    case GOD_LUGONU:
        return LIGHTRED;

    case GOD_CHEIBRIADOS:
        return LIGHTCYAN;

    case GOD_JIYVA:
        return coinflip() ? GREEN : LIGHTGREEN;

    case GOD_DITHMENOS:
        return MAGENTA;

    case GOD_GOZAG:
        return coinflip() ? YELLOW : BROWN;

    case GOD_QAZLAL:
    case GOD_RU:
        return BROWN;

    case GOD_PAKELLAS:
        return random_choose(LIGHTMAGENTA, LIGHTGREEN, LIGHTCYAN);

    case GOD_HEPLIAKLQANA:
        return coinflip() ? LIGHTGREEN : LIGHTBLUE;

    default:
        return YELLOW;
    }
}

int piety_rank(int piety)
{
    // XXX: this seems to be used only in dat/database/godspeak.txt?
    if (you_worship(GOD_XOM))
    {
        const int breakpoints[] = { 20, 50, 80, 120, 180, INT_MAX };
        for (unsigned int i = 0; i < ARRAYSZ(breakpoints); ++i)
            if (piety <= breakpoints[i])
                return i + 1;
        die("INT_MAX is no good");
    }

    for (int i = NUM_PIETY_STARS; i >= 1; --i)
        if (piety >= piety_breakpoint(i - 1))
            return i;

    return 0;
}

int piety_breakpoint(int i)
{
    int breakpoints[NUM_PIETY_STARS] = { 30, 50, 75, 100, 120, 160 };
    if (i >= NUM_PIETY_STARS || i < 0)
        return 255;
    else
        return breakpoints[i];
}

// Returns true if the Shining One doesn't mind your using unchivalric
// attacks on this creature.
bool tso_unchivalric_attack_safe_monster(const monster* mon)
{
    const mon_holy_type holiness = mon->holiness();
    return mons_intel(mon) < I_HUMAN
           || mons_is_object(mon->mons_species())
           || mon->undead_or_demonic()
           || mon->is_shapeshifter() && (mon->flags & MF_KNOWN_SHIFTER)
           || !mon->is_holy() && !(holiness & MH_NATURAL);
}

int get_monster_tension(const monster* mons, god_type god)
{
    if (!mons->alive())
        return 0;

    if (you.see_cell(mons->pos()))
    {
        if (!mons_can_hurt_player(mons))
            return 0;
    }

    const mon_attitude_type att = mons_attitude(mons);
    if (att == ATT_GOOD_NEUTRAL || att == ATT_NEUTRAL)
        return 0;

    if (mons->cannot_act() || mons->asleep() || mons_is_fleeing(mons))
        return 0;

    int exper = exper_value(mons);
    if (exper <= 0)
        return 0;

    // Almost dead monsters don't count as much.
    exper *= mons->hit_points;
    exper /= mons->max_hit_points;

    bool gift = false;

    if (god != GOD_NO_GOD)
        gift = mons_is_god_gift(mons, god);

    if (att == ATT_HOSTILE)
    {
        // God is punishing you with a hostile gift, so it doesn't
        // count towards tension.
        if (gift)
            return 0;
    }
    else if (att == ATT_FRIENDLY)
    {
        // Friendly monsters being around to help you reduce
        // tension.
        exper = -exper;

        // If it's a god gift, it reduces tension even more, since
        // the god is already helping you out.
        if (gift)
            exper *= 2;
    }

    if (att != ATT_FRIENDLY)
    {
        if (!you.visible_to(mons))
            exper /= 2;
        if (!mons->visible_to(&you))
            exper *= 2;
    }

    if (mons->confused() || mons->caught())
        exper /= 2;

    if (mons->has_ench(ENCH_SLOW))
    {
        exper *= 2;
        exper /= 3;
    }

    if (mons->has_ench(ENCH_HASTE))
    {
        exper *= 3;
        exper /= 2;
    }

    if (mons->has_ench(ENCH_MIGHT))
    {
        exper *= 5;
        exper /= 4;
    }

    if (mons->berserk_or_insane())
    {
        // in addition to haste and might bonuses above
        exper *= 3;
        exper /= 2;
    }

    return exper;
}

int get_tension(god_type god)
{
    int total = 0;

    bool nearby_monster = false;
    for (radius_iterator ri(you.pos(), LOS_NO_TRANS); ri; ++ri)
    {
        const monster* mon = monster_at(*ri);

        if (mon && mon->alive() && you.can_see(*mon))
        {
            int exper = get_monster_tension(mon, god);

            if (!mon->wont_attack() && !mon->withdrawn())
                nearby_monster = true;

            total += exper;
        }
    }

    // At least one monster has to be nearby, for tension to count.
    if (!nearby_monster)
        return 0;

    const int scale = 1;

    int tension = total;

    // Tension goes up inversely proportional to the percentage of max
    // hp you have.
    tension *= (scale + 1) * you.hp_max;
    tension /= max(you.hp_max + scale * you.hp, 1);

    // Divides by 1 at level 1, 200 at level 27.
    const int exp_lev  = you.get_experience_level();
    const int exp_need = exp_needed(exp_lev);
    const int factor   = (int)ceil(sqrt(exp_need / 30.0));
    const int div      = 1 + factor;

    tension /= div;

    if (player_in_branch(BRANCH_ABYSS))
    {
        if (tension < 2)
            tension = 2;
        else
        {
            tension *= 3;
            tension /= 2;
        }
    }

    if (you.cannot_act())
    {
        tension *= 10;
        tension  = max(1, tension);

        return tension;
    }

    if (you.confused())
        tension *= 2;

    if (you.caught())
        tension *= 2;

    if (you.duration[DUR_SLOW])
    {
        tension *= 3;
        tension /= 2;
    }

    if (you.duration[DUR_HASTE])
    {
        tension *= 2;
        tension /= 3;
    }

    return max(0, tension);
}

int get_fuzzied_monster_difficulty(const monster *mons)
{
    double factor = sqrt(exp_needed(you.experience_level) / 30.0);
    int exp = exper_value(mons) * 100;
    exp = random2(exp) + random2(exp);
    return exp / (1 + factor);
}

/////////////////////////////////////////////////////////////////////////////
// Stuff for placing god gift monsters after the player's turn has ended.
/////////////////////////////////////////////////////////////////////////////

static vector<mgen_data>       _delayed_data;
static deque<delayed_callback> _delayed_callbacks;
static deque<unsigned int>     _delayed_done_trigger_pos;
static deque<delayed_callback> _delayed_done_callbacks;
static deque<string>      _delayed_success;
static deque<string>      _delayed_failure;

void delayed_monster(const mgen_data &mg, delayed_callback callback)
{
    _delayed_data.push_back(mg);
    _delayed_callbacks.push_back(callback);
}

void delayed_monster_done(string success, string failure,
                                  delayed_callback callback)
{
    const unsigned int size = _delayed_data.size();
    ASSERT(size > 0);

    _delayed_done_trigger_pos.push_back(size - 1);
    _delayed_success.push_back(success);
    _delayed_failure.push_back(failure);
    _delayed_done_callbacks.push_back(callback);
}

static void _place_delayed_monsters()
{
    int      placed   = 0;
    god_type prev_god = GOD_NO_GOD;
    for (unsigned int i = 0; i < _delayed_data.size(); i++)
    {
        mgen_data &mg          = _delayed_data[i];
        delayed_callback cback = _delayed_callbacks[i];

        if (prev_god != mg.god)
        {
            placed   = 0;
            prev_god = mg.god;
        }

        monster *mon = create_monster(mg);

        if (cback)
            (*cback)(mg, mon, placed);

        if (mon)
        {
            if (you_worship(GOD_YREDELEMNUL)
                || you_worship(GOD_HEPLIAKLQANA)
                || have_passive(passive_t::convert_orcs))
            {
                add_companion(mon);
            }
            placed++;
        }

        if (!_delayed_done_trigger_pos.empty()
            && _delayed_done_trigger_pos[0] == i)
        {
            cback = _delayed_done_callbacks[0];

            string msg;
            if (placed > 0)
                msg = _delayed_success[0];
            else
                msg = _delayed_failure[0];

            if (placed == 1)
            {
                msg = replace_all(msg, "@a@", "a");
                msg = replace_all(msg, "@an@", "an");
            }
            else
            {
                msg = replace_all(msg, " @a@", "");
                msg = replace_all(msg, " @an@", "");
            }

            if (placed == 1)
                msg = replace_all(msg, "@s@", "");
            else
                msg = replace_all(msg, "@s@", "s");

            prev_god = GOD_NO_GOD;
            _delayed_done_trigger_pos.pop_front();
            _delayed_success.pop_front();
            _delayed_failure.pop_front();
            _delayed_done_callbacks.pop_front();

            if (msg == "")
            {
                if (cback)
                    (*cback)(mg, mon, placed);
                continue;
            }

            // Fake its coming from simple_god_message().
            if (msg[0] == ' ' || msg[0] == '\'')
                msg = uppercase_first(god_name(mg.god)) + msg;

            trim_string(msg);

            god_speaks(mg.god, msg.c_str());

            if (cback)
                (*cback)(mg, mon, placed);
        }
    }

    _delayed_data.clear();
    _delayed_callbacks.clear();
    _delayed_done_trigger_pos.clear();
    _delayed_success.clear();
    _delayed_failure.clear();
}

static bool _is_god(god_type god)
{
    return god > GOD_NO_GOD && god < NUM_GODS;
}

static bool _is_temple_god(god_type god)
{
    if (!_is_god(god))
        return false;

    switch (god)
    {
    case GOD_NO_GOD:
    case GOD_LUGONU:
    case GOD_BEOGH:
    case GOD_JIYVA:
        return false;

    default:
        return true;
    }
}

static bool _is_nontemple_god(god_type god)
{
    return _is_god(god) && !_is_temple_god(god);
}

static bool _cmp_god_by_name(god_type god1, god_type god2)
{
    return god_name(god1, false) < god_name(god2, false);
}

// Vector of temple gods.
// Sorted by name for the benefit of the overview.
vector<god_type> temple_god_list()
{
    vector<god_type> god_list;
    for (god_iterator it; it; ++it)
        if (_is_temple_god(*it))
            god_list.push_back(*it);
    sort(god_list.begin(), god_list.end(), _cmp_god_by_name);
    return god_list;
}

// Vector of non-temple gods.
// Sorted by name for the benefit of the overview.
vector<god_type> nontemple_god_list()
{
    vector<god_type> god_list;
    for (god_iterator it; it; ++it)
        if (_is_nontemple_god(*it))
            god_list.push_back(*it);
    sort(god_list.begin(), god_list.end(), _cmp_god_by_name);
    return god_list;
}
