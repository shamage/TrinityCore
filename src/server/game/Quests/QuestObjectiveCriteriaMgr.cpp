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

#include "QuestObjectiveCriteriaMgr.h"
#include "AchievementPackets.h"
#include "DatabaseEnv.h"
#include "DB2Structure.h"
#include "GameTime.h"
#include "Group.h"
#include "Log.h"
#include "Map.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "WorldSession.h"

QuestObjectiveCriteriaMgr::QuestObjectiveCriteriaMgr(Player* owner) : _owner(owner)
{
}

QuestObjectiveCriteriaMgr::~QuestObjectiveCriteriaMgr()
{
}

void QuestObjectiveCriteriaMgr::CheckAllQuestObjectiveCriteria(Player* referencePlayer)
{
    // suppress sending packets
    for (CriteriaType criteriaType : CriteriaMgr::GetRetroactivelyUpdateableCriteriaTypes())
        UpdateCriteria(criteriaType, 0, 0, 0, nullptr, referencePlayer);
}

void QuestObjectiveCriteriaMgr::Reset()
{
    for (auto& criteriaProgres : _criteriaProgress)
        SendCriteriaProgressRemoved(criteriaProgres.first);

    _criteriaProgress.clear();

    DeleteFromDB(_owner->GetGUID());

    // re-fill data
    CheckAllQuestObjectiveCriteria(_owner);
}

void QuestObjectiveCriteriaMgr::DeleteFromDB(ObjectGuid const& guid)
{
    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();

    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHAR_QUESTSTATUS_OBJECTIVES_CRITERIA);
    stmt->setUInt64(0, guid.GetCounter());
    trans->Append(stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHAR_QUESTSTATUS_OBJECTIVES_CRITERIA_PROGRESS);
    stmt->setUInt64(0, guid.GetCounter());
    trans->Append(stmt);

    CharacterDatabase.CommitTransaction(trans);
}

void QuestObjectiveCriteriaMgr::LoadFromDB(PreparedQueryResult objectiveResult, PreparedQueryResult criteriaResult)
{
    if (objectiveResult)
    {
        do
        {
            uint32 objectiveId = (*objectiveResult)[0].GetUInt32();

            QuestObjective const* objective = sObjectMgr->GetQuestObjective(objectiveId);
            if (!objective)
                continue;

            _completedObjectives.insert(objectiveId);

        } while (objectiveResult->NextRow());
    }

    if (criteriaResult)
    {
        time_t now = GameTime::GetGameTime();
        do
        {
            Field* fields = criteriaResult->Fetch();
            uint32 criteriaId = fields[0].GetUInt32();
            uint64 counter = fields[1].GetUInt64();
            time_t date = fields[2].GetInt64();

            Criteria const* criteria = sCriteriaMgr->GetCriteria(criteriaId);
            if (!criteria)
            {
                // Removing non-existing criteria data for all characters
                TC_LOG_ERROR("criteria.quest", "Non-existing quest objective criteria {} data has been removed from the table `character_queststatus_objectives_criteria_progress`.", criteriaId);

                CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_INVALID_QUEST_PROGRESS_CRITERIA);
                stmt->setUInt32(0, criteriaId);
                CharacterDatabase.Execute(stmt);

                continue;
            }

            if (criteria->Entry->StartTimer && time_t(date + criteria->Entry->StartTimer) < now)
                continue;

            CriteriaProgress& progress = _criteriaProgress[criteriaId];
            progress.Counter = counter;
            progress.Date = date;
            progress.Changed = false;
        } while (criteriaResult->NextRow());
    }
}

void QuestObjectiveCriteriaMgr::SaveToDB(CharacterDatabaseTransaction trans)
{
    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHAR_QUESTSTATUS_OBJECTIVES_CRITERIA);
    stmt->setUInt64(0, _owner->GetGUID().GetCounter());
    trans->Append(stmt);

    if (!_completedObjectives.empty())
    {
        for (uint32 completedObjectiveId : _completedObjectives)
        {
            stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_CHAR_QUESTSTATUS_OBJECTIVES_CRITERIA);
            stmt->setUInt64(0, _owner->GetGUID().GetCounter());
            stmt->setUInt32(1, completedObjectiveId);
            trans->Append(stmt);
        }
    }

    if (!_criteriaProgress.empty())
    {
        for (auto& criteriaProgres : _criteriaProgress)
        {
            if (!criteriaProgres.second.Changed)
                continue;

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHAR_QUESTSTATUS_OBJECTIVES_CRITERIA_PROGRESS_BY_CRITERIA);
            stmt->setUInt64(0, _owner->GetGUID().GetCounter());
            stmt->setUInt32(1, criteriaProgres.first);
            trans->Append(stmt);

            if (criteriaProgres.second.Counter)
            {
                stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_CHAR_QUESTSTATUS_OBJECTIVES_CRITERIA_PROGRESS);
                stmt->setUInt64(0, _owner->GetGUID().GetCounter());
                stmt->setUInt32(1, criteriaProgres.first);
                stmt->setUInt64(2, criteriaProgres.second.Counter);
                stmt->setInt64(3, criteriaProgres.second.Date);
                trans->Append(stmt);
            }

            criteriaProgres.second.Changed = false;
        }
    }
}

void QuestObjectiveCriteriaMgr::ResetCriteriaTree(QuestObjective const* questObjective)
{
    _completedObjectives.erase(questObjective->ID);

    CriteriaTree const* tree = sCriteriaMgr->GetCriteriaTree(questObjective->ObjectID);
    if (!tree)
        return;

    CriteriaMgr::WalkCriteriaTree(tree, [this](CriteriaTree const* criteriaTree)
    {
        RemoveCriteriaProgress(criteriaTree->Criteria);
    });

    CriteriaMgr::WalkCriteriaTree(tree, [this](CriteriaTree const* criteriaTree)
    {
        if (criteriaTree->Criteria && advstd::ranges::contains(CriteriaMgr::GetRetroactivelyUpdateableCriteriaTypes(), CriteriaType(criteriaTree->Criteria->Entry->Type)))
            UpdateCriteria(criteriaTree->Criteria, 0, 0, 0, nullptr, _owner);
    });
}

void QuestObjectiveCriteriaMgr::SendAllData(Player const* /*receiver*/) const
{
    for (const auto& criteriaProgres : _criteriaProgress)
    {
        WorldPackets::Achievement::CriteriaUpdate criteriaUpdate;

        criteriaUpdate.CriteriaID = criteriaProgres.first;
        criteriaUpdate.Quantity = criteriaProgres.second.Counter;
        criteriaUpdate.PlayerGUID = _owner->GetGUID();
        criteriaUpdate.Flags = 0;

        criteriaUpdate.CurrentTime.SetUtcTimeFromUnixTime(criteriaProgres.second.Date);
        criteriaUpdate.CurrentTime += _owner->GetSession()->GetTimezoneOffset();
        criteriaUpdate.CreationTime = 0;

        SendPacket(criteriaUpdate.Write());
    }
}

void QuestObjectiveCriteriaMgr::CompletedObjective(QuestObjective const* questObjective, Player* /*referencePlayer*/)
{
    if (HasCompletedObjective(questObjective))
        return;

    _owner->KillCreditCriteriaTreeObjective(*questObjective);

    TC_LOG_INFO("criteria.quest", "QuestObjectiveCriteriaMgr::CompletedObjective({}). {}", questObjective->ID, GetOwnerInfo());

    _completedObjectives.insert(questObjective->ID);
}

bool QuestObjectiveCriteriaMgr::HasCompletedObjective(QuestObjective const* questObjective) const
{
    return _completedObjectives.contains(questObjective->ID);
}

void QuestObjectiveCriteriaMgr::SendCriteriaUpdate(Criteria const* criteria, CriteriaProgress const* progress, Seconds timeElapsed, bool timedCompleted) const
{
    WorldPackets::Achievement::CriteriaUpdate criteriaUpdate;

    criteriaUpdate.CriteriaID = criteria->ID;
    criteriaUpdate.Quantity = progress->Counter;
    criteriaUpdate.PlayerGUID = _owner->GetGUID();
    criteriaUpdate.Flags = 0;
    if (criteria->Entry->StartTimer)
        criteriaUpdate.Flags = timedCompleted ? 1 : 0; // 1 is for keeping the counter at 0 in client

    criteriaUpdate.CurrentTime.SetUtcTimeFromUnixTime(progress->Date);
    criteriaUpdate.CurrentTime += _owner->GetSession()->GetTimezoneOffset();
    criteriaUpdate.ElapsedTime = timeElapsed;
    criteriaUpdate.CreationTime = 0;

    SendPacket(criteriaUpdate.Write());
}

void QuestObjectiveCriteriaMgr::SendCriteriaProgressRemoved(uint32 criteriaId)
{
    WorldPackets::Achievement::CriteriaDeleted criteriaDeleted;
    criteriaDeleted.CriteriaID = criteriaId;
    SendPacket(criteriaDeleted.Write());
}

bool QuestObjectiveCriteriaMgr::CanUpdateCriteriaTree(Criteria const* criteria, CriteriaTree const* tree, Player* referencePlayer) const
{
    QuestObjective const* objective = tree->QuestObjective;
    if (!objective)
        return false;

    if (HasCompletedObjective(objective))
    {
        TC_LOG_TRACE("criteria.quest", "QuestObjectiveCriteriaMgr::CanUpdateCriteriaTree: (Id: {} Type {} Quest Objective {}) Objective already completed",
            criteria->ID, CriteriaMgr::GetCriteriaTypeString(criteria->Entry->Type), objective->ID);
        return false;
    }

    if (_owner->GetQuestStatus(objective->QuestID) != QUEST_STATUS_INCOMPLETE)
    {
        TC_LOG_TRACE("criteria.quest", "QuestObjectiveCriteriaMgr::CanUpdateCriteriaTree: (Id: {} Type {} Quest Objective {}) Not on quest",
            criteria->ID, CriteriaMgr::GetCriteriaTypeString(criteria->Entry->Type), objective->ID);
        return false;
    }

    Quest const* quest = ASSERT_NOTNULL(sObjectMgr->GetQuestTemplate(objective->QuestID));
    if (_owner->GetGroup() && _owner->GetGroup()->isRaidGroup() && !quest->IsAllowedInRaid(referencePlayer->GetMap()->GetDifficultyID()))
    {
        TC_LOG_TRACE("criteria.quest", "QuestObjectiveCriteriaMgr::CanUpdateCriteriaTree: (Id: {} Type {} Quest Objective {}) Quest cannot be completed in raid group",
            criteria->ID, CriteriaMgr::GetCriteriaTypeString(criteria->Entry->Type), objective->ID);
        return false;
    }

    uint16 slot = _owner->FindQuestSlot(objective->QuestID);
    if (slot >= MAX_QUEST_LOG_SIZE || !_owner->IsQuestObjectiveCompletable(slot, quest, *objective))
    {
        TC_LOG_TRACE("criteria.quest", "QuestObjectiveCriteriaMgr::CanUpdateCriteriaTree: (Id: {} Type {} Quest Objective {}) Objective not completable",
            criteria->ID, CriteriaMgr::GetCriteriaTypeString(criteria->Entry->Type), objective->ID);
        return false;
    }

    return CriteriaHandler::CanUpdateCriteriaTree(criteria, tree, referencePlayer);
}

bool QuestObjectiveCriteriaMgr::CanCompleteCriteriaTree(CriteriaTree const* tree)
{
    QuestObjective const* objective = tree->QuestObjective;
    if (!objective)
        return false;

    return CriteriaHandler::CanCompleteCriteriaTree(tree);
}

void QuestObjectiveCriteriaMgr::CompletedCriteriaTree(CriteriaTree const* tree, Player* referencePlayer)
{
    QuestObjective const* objective = tree->QuestObjective;
    if (!objective)
        return;

    CriteriaTree const* entireObjectiveTree = sCriteriaMgr->GetCriteriaTree(objective->ObjectID);
    if (IsCompletedCriteriaTree(entireObjectiveTree))
        CompletedObjective(objective, referencePlayer);
}

void QuestObjectiveCriteriaMgr::SendPacket(WorldPacket const* data) const
{
    _owner->SendDirectMessage(data);
}

std::string QuestObjectiveCriteriaMgr::GetOwnerInfo() const
{
    return Trinity::StringFormat("{} {}", _owner->GetGUID().ToString(), _owner->GetName());
}

CriteriaList const& QuestObjectiveCriteriaMgr::GetCriteriaByType(CriteriaType type, uint32 /*asset*/) const
{
    return sCriteriaMgr->GetQuestObjectiveCriteriaByType(type);
}

bool QuestObjectiveCriteriaMgr::RequiredAchievementSatisfied(uint32 achievementId) const
{
    return _owner->HasAchieved(achievementId);
}
