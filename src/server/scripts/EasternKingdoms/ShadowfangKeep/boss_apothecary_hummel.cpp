/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "ScriptMgr.h"
#include "Containers.h"
#include "GridNotifiersImpl.h"
#include "Group.h"
#include "InstanceScript.h"
#include "LFGMgr.h"
#include "Map.h"
#include "MotionMaster.h"
#include "Player.h"
#include "ScriptedCreature.h"
#include "ScriptedGossip.h"
#include "shadowfang_keep.h"
#include "SpellScript.h"

enum ApothecarySpells
{
    SPELL_ALLURING_PERFUME       = 68589,
    SPELL_PERFUME_SPRAY          = 68607,
    SPELL_CHAIN_REACTION         = 68821,
    SPELL_SUMMON_TABLE           = 69218,
    SPELL_PERMANENT_FEIGN_DEATH  = 29266,
    SPELL_QUIET_SUICIDE          = 3617,
    SPELL_COLOGNE_SPRAY          = 68948,
    SPELL_VALIDATE_AREA          = 68644,
    SPELL_THROW_COLOGNE          = 68841,
    SPELL_BUNNY_LOCKDOWN         = 69039,
    SPELL_THROW_PERFUME          = 68799,
    SPELL_PERFUME_SPILL          = 68798,
    SPELL_COLOGNE_SPILL          = 68614,
    SPELL_PERFUME_SPILL_DAMAGE   = 68927,
    SPELL_COLOGNE_SPILL_DAMAGE   = 68934
};

enum ApothecarySays
{
    SAY_INTRO_0      = 0,
    SAY_INTRO_1      = 1,
    SAY_INTRO_2      = 2,
    SAY_CALL_BAXTER  = 3,
    SAY_CALL_FRYE    = 4,
    SAY_HUMMEL_DEATH = 5,
    SAY_SUMMON_ADDS  = 6,
    SAY_BAXTER_DEATH = 0,
    SAY_FRYE_DEATH   = 0
};

enum ApothecaryEvents
{
    EVENT_HUMMEL_SAY_0 = 1,
    EVENT_HUMMEL_SAY_1,
    EVENT_HUMMEL_SAY_2,
    EVENT_START_FIGHT,
    EVENT_CHAIN_REACTION,
    EVENT_PERFUME_SPRAY,
    EVENT_COLOGNE_SPRAY,
    EVENT_CALL_BAXTER,
    EVENT_CALL_FRYE
};

enum ApothecaryMisc
{
    ACTION_START_EVENT          = 1,
    ACTION_START_FIGHT          = 2,
    GOSSIP_OPTION_START         = 0,
    GOSSIP_MENU_HUMMEL          = 10847,
    QUEST_YOUVE_BEEN_SERVED     = 14488,
    NPC_APOTHECARY_FRYE         = 36272,
    NPC_APOTHECARY_BAXTER       = 36565,
    NPC_VIAL_BUNNY              = 36530,
    NPC_CROWN_APOTHECARY        = 36885,
    PHASE_ALL                   = 0,
    PHASE_INTRO                 = 1
};

Position const BaxterMovePos = { -221.4115f, 2206.825f, 79.93151f, 0.0f };
Position const FryeMovePos = { -196.2483f, 2197.224f, 79.9315f, 0.0f };

struct boss_apothecary_hummel : public BossAI
{
    boss_apothecary_hummel(Creature* creature) : BossAI(creature, BOSS_APOTHECARY_HUMMEL), _deadCount(0), _isDead(false) { }

    bool OnGossipSelect(Player* player, uint32 menuId, uint32 gossipListId) override
    {
        if (menuId == GOSSIP_MENU_HUMMEL && gossipListId == GOSSIP_OPTION_START)
        {
            me->RemoveNpcFlag(UNIT_NPC_FLAG_GOSSIP);
            CloseGossipMenuFor(player);
            DoAction(ACTION_START_EVENT);
        }
        return false;
    }

    void Reset() override
    {
        _Reset();
        _deadCount = 0;
        _isDead = false;
        events.SetPhase(PHASE_ALL);
        me->SetFaction(FACTION_FRIENDLY);
        me->SummonCreatureGroup(1);
    }

    void EnterEvadeMode(EvadeReason /*why*/) override
    {
        summons.DespawnAll();
        _EnterEvadeMode();
        _DespawnAtEvade(Seconds(10));
    }

    void DoAction(int32 action) override
    {
        if (action == ACTION_START_EVENT && events.IsInPhase(PHASE_ALL))
        {
            events.SetPhase(PHASE_INTRO);
            events.ScheduleEvent(EVENT_HUMMEL_SAY_0, Milliseconds(1));

            me->SetImmuneToPC(true);
            me->SetFaction(FACTION_MONSTER);
            DummyEntryCheckPredicate pred;
            summons.DoAction(ACTION_START_EVENT, pred);
        }
    }

    void DamageTaken(Unit* /*attacker*/, uint32& damage, DamageEffectType /*damageType*/, SpellInfo const* /*spellInfo = nullptr*/) override
    {
        if (damage >= me->GetHealth())
            if (_deadCount < 2)
            {
                damage = me->GetHealth() - 1;
                if (!_isDead)
                {
                    _isDead = true;
                    me->RemoveAurasDueToSpell(SPELL_ALLURING_PERFUME);
                    DoCastSelf(SPELL_PERMANENT_FEIGN_DEATH, true);
                    me->SetUninteractible(true);
                    Talk(SAY_HUMMEL_DEATH);
                }
            }
    }

    void SummonedCreatureDies(Creature* summon, Unit* /*killer*/) override
    {
        if (summon->GetEntry() == NPC_APOTHECARY_FRYE || summon->GetEntry() == NPC_APOTHECARY_BAXTER)
            _deadCount++;

        if (me->HasAura(SPELL_PERMANENT_FEIGN_DEATH) && _deadCount == 2)
            DoCastSelf(SPELL_QUIET_SUICIDE, true);
    }

    void JustDied(Unit* /*killer*/) override
    {
        if (!_isDead)
            Talk(SAY_HUMMEL_DEATH);

        events.Reset();
        me->SetUninteractible(false);
        instance->SetBossState(BOSS_APOTHECARY_HUMMEL, DONE);

        Map::PlayerList const& players = me->GetMap()->GetPlayers();
        if (!players.empty())
        {
            if (Group* group = players.begin()->GetSource()->GetGroup())
                if (group->isLFGGroup())
                    sLFGMgr->FinishDungeon(group->GetGUID(), 288, me->GetMap());
        }
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim() && !events.IsInPhase(PHASE_INTRO))
            return;

        events.Update(diff);

        if (me->HasUnitState(UNIT_STATE_CASTING))
            return;

        while (uint32 eventId = events.ExecuteEvent())
        {
            switch (eventId)
            {
                case EVENT_HUMMEL_SAY_0:
                    Talk(SAY_INTRO_0);
                    events.ScheduleEvent(EVENT_HUMMEL_SAY_1, Seconds(4));
                    break;
                case EVENT_HUMMEL_SAY_1:
                    Talk(SAY_INTRO_1);
                    events.ScheduleEvent(EVENT_HUMMEL_SAY_2, Seconds(4));
                    break;
                case EVENT_HUMMEL_SAY_2:
                    Talk(SAY_INTRO_2);
                    events.ScheduleEvent(EVENT_START_FIGHT, 4s);
                    break;
                case EVENT_START_FIGHT:
                {
                    me->SetImmuneToAll(false);
                    DoZoneInCombat();
                    events.ScheduleEvent(EVENT_CALL_BAXTER, 6s);
                    events.ScheduleEvent(EVENT_CALL_FRYE, 14s);
                    events.ScheduleEvent(EVENT_PERFUME_SPRAY, Milliseconds(3640));
                    events.ScheduleEvent(EVENT_CHAIN_REACTION, 15s);

                    Talk(SAY_SUMMON_ADDS);
                    std::vector<Creature*> trashs;
                    me->GetCreatureListWithEntryInGrid(trashs, NPC_CROWN_APOTHECARY);
                    for (Creature* crea : trashs)
                        crea->DespawnOrUnsummon();

                    break;
                }
                case EVENT_CALL_BAXTER:
                {
                    Talk(SAY_CALL_BAXTER);
                    EntryCheckPredicate pred(NPC_APOTHECARY_BAXTER);
                    summons.DoAction(ACTION_START_FIGHT, pred);
                    summons.DoZoneInCombat(NPC_APOTHECARY_BAXTER);
                    break;
                }
                case EVENT_CALL_FRYE:
                {
                    Talk(SAY_CALL_FRYE);
                    EntryCheckPredicate pred(NPC_APOTHECARY_FRYE);
                    summons.DoAction(ACTION_START_FIGHT, pred);
                    break;
                }
                case EVENT_PERFUME_SPRAY:
                    DoCastVictim(SPELL_PERFUME_SPRAY);
                    events.Repeat(Milliseconds(3640));
                    break;
                case EVENT_CHAIN_REACTION:
                    DoCastVictim(SPELL_SUMMON_TABLE, true);
                    DoCastAOE(SPELL_CHAIN_REACTION);
                    events.Repeat(Seconds(25));
                    break;
                default:
                    break;
            }

            if (me->HasUnitState(UNIT_STATE_CASTING))
                return;
        }
    }

    void OnQuestReward(Player* /*player*/, Quest const* quest, LootItemType /*type*/, uint32 /*opt*/) override
    {
        if (quest->GetQuestId() == QUEST_YOUVE_BEEN_SERVED)
            DoAction(ACTION_START_EVENT);
    }

    private:
        uint8 _deadCount;
        bool _isDead;
};

struct npc_apothecary_genericAI : public ScriptedAI
{
    npc_apothecary_genericAI(Creature* creature, Position pos) : ScriptedAI(creature), _movePos(pos) { }

    void DoAction(int32 action) override
    {
        if (action == ACTION_START_EVENT)
        {
            me->SetImmuneToPC(true);
            me->SetFaction(FACTION_MONSTER);
            me->GetMotionMaster()->MovePoint(1, _movePos);
        }
        else if (action == ACTION_START_FIGHT)
        {
            me->SetImmuneToAll(false);
            DoZoneInCombat();
        }
    }

    void MovementInform(uint32 type, uint32 pointId) override
    {
        if (type == POINT_MOTION_TYPE && pointId == 1)
            me->SetEmoteState(EMOTE_STATE_USE_STANDING);
    }

protected:
    Position _movePos;
};

struct npc_apothecary_frye : public npc_apothecary_genericAI
{
    npc_apothecary_frye(Creature* creature) : npc_apothecary_genericAI(creature, FryeMovePos) { }

    void JustDied(Unit* /*killer*/) override
    {
        Talk(SAY_FRYE_DEATH);
    }
};

struct npc_apothecary_baxter : public npc_apothecary_genericAI
{
    npc_apothecary_baxter(Creature* creature) : npc_apothecary_genericAI(creature, BaxterMovePos) { }

    void Reset() override
    {
        _events.Reset();
        _events.ScheduleEvent(EVENT_COLOGNE_SPRAY, 7s);
        _events.ScheduleEvent(EVENT_CHAIN_REACTION, 12s);
    }

    void JustDied(Unit* /*killer*/) override
    {
        _events.Reset();
        Talk(SAY_BAXTER_DEATH);
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim())
            return;

        _events.Update(diff);

        if (me->HasUnitState(UNIT_STATE_CASTING))
            return;

        while (uint32 eventId = _events.ExecuteEvent())
        {
            switch (eventId)
            {
                case EVENT_COLOGNE_SPRAY:
                    DoCastVictim(SPELL_COLOGNE_SPRAY);
                    _events.Repeat(Seconds(4));
                    break;
                case EVENT_CHAIN_REACTION:
                    DoCastVictim(SPELL_SUMMON_TABLE);
                    DoCastVictim(SPELL_CHAIN_REACTION);
                    _events.Repeat(Seconds(25));
                    break;
                default:
                    break;
            }
        }
    }

private:
    EventMap _events;
};

// 68965 - [DND] Lingering Fumes Targetting (starter)
class spell_apothecary_lingering_fumes : public SpellScript
{
    void HandleAfterCast()
    {
        Unit* caster = GetCaster();
        if (!caster->IsInCombat() || roll_chance_i(50))
            return;

        std::list<Creature*> triggers;
        caster->GetCreatureListWithEntryInGrid(triggers, NPC_VIAL_BUNNY, 100.0f);
        if (triggers.empty())
            return;

        Creature* trigger = Trinity::Containers::SelectRandomContainerElement(triggers);
        caster->GetMotionMaster()->MovePoint(0, trigger->GetPosition());

    }

    void HandleScript(SpellEffIndex /*effindex*/)
    {
        Unit* caster = GetCaster();
        caster->CastSpell(GetHitUnit(), SPELL_VALIDATE_AREA, true);
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_apothecary_lingering_fumes::HandleAfterCast);
        OnEffectHitTarget += SpellEffectFn(spell_apothecary_lingering_fumes::HandleScript, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 68644 - [DND] Valentine Boss Validate Area
class spell_apothecary_validate_area : public SpellScript
{
    void FilterTargets(std::list<WorldObject*>& targets)
    {
        targets.remove_if(Trinity::UnitAuraCheck(true, SPELL_BUNNY_LOCKDOWN));
        if (targets.empty())
            return;

        WorldObject* target = Trinity::Containers::SelectRandomContainerElement(targets);
        targets.clear();
        targets.push_back(target);
    }

    void HandleScript(SpellEffIndex /*effindex*/)
    {
        GetHitUnit()->CastSpell(GetHitUnit(), SPELL_BUNNY_LOCKDOWN, true);
        GetCaster()->CastSpell(GetHitUnit(), RAND(SPELL_THROW_COLOGNE, SPELL_THROW_PERFUME), true);
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_apothecary_validate_area::FilterTargets, EFFECT_0, TARGET_UNIT_DEST_AREA_ENTRY);
        OnEffectHitTarget += SpellEffectFn(spell_apothecary_validate_area::HandleScript, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 69038 - Throw Cologne
class spell_apothecary_throw_cologne : public SpellScript
{
    void HandleScript(SpellEffIndex /*effindex*/)
    {
        GetHitUnit()->CastSpell(GetHitUnit(), SPELL_COLOGNE_SPILL, true);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_apothecary_throw_cologne::HandleScript, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 68966 - Throw Perfume
class spell_apothecary_throw_perfume : public SpellScript
{
    void HandleScript(SpellEffIndex /*effindex*/)
    {
        GetHitUnit()->CastSpell(GetHitUnit(), SPELL_PERFUME_SPILL, true);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_apothecary_throw_perfume::HandleScript, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 68798 - Concentrated Alluring Perfume Spill
class spell_apothecary_perfume_spill : public AuraScript
{
    void OnPeriodic(AuraEffect const* /*aurEff*/)
    {
        GetTarget()->CastSpell(GetTarget(), SPELL_PERFUME_SPILL_DAMAGE, true);
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_apothecary_perfume_spill::OnPeriodic, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY);
    }
};

// 68614 - Concentrated Irresistible Cologne Spill
class spell_apothecary_cologne_spill : public AuraScript
{
    void OnPeriodic(AuraEffect const* /*aurEff*/)
    {
        GetTarget()->CastSpell(GetTarget(), SPELL_COLOGNE_SPILL_DAMAGE, true);
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_apothecary_cologne_spill::OnPeriodic, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY);
    }
};

void AddSC_boss_apothecary_hummel()
{
    RegisterShadowfangKeepCreatureAI(boss_apothecary_hummel);
    RegisterShadowfangKeepCreatureAI(npc_apothecary_baxter);
    RegisterShadowfangKeepCreatureAI(npc_apothecary_frye);
    RegisterSpellScript(spell_apothecary_lingering_fumes);
    RegisterSpellScript(spell_apothecary_validate_area);
    RegisterSpellScript(spell_apothecary_throw_cologne);
    RegisterSpellScript(spell_apothecary_throw_perfume);
    RegisterSpellScript(spell_apothecary_perfume_spill);
    RegisterSpellScript(spell_apothecary_cologne_spill);
}
