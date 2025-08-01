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

#include "BattlegroundPackets.h"
#include "PacketOperators.h"

namespace WorldPackets::Battleground
{
WorldPacket const* SeasonInfo::Write()
{
    _worldPacket << int32(MythicPlusDisplaySeasonID);
    _worldPacket << int32(MythicPlusMilestoneSeasonID);
    _worldPacket << int32(CurrentArenaSeason);
    _worldPacket << int32(PreviousArenaSeason);
    _worldPacket << int32(ConquestWeeklyProgressCurrencyID);
    _worldPacket << int32(PvpSeasonID);
    _worldPacket << int32(Unknown1027_1);
    _worldPacket << Bits<1>(WeeklyRewardChestsEnabled);
    _worldPacket << Bits<1>(CurrentArenaSeasonUsesTeams);
    _worldPacket << Bits<1>(PreviousArenaSeasonUsesTeams);
    _worldPacket.FlushBits();

    return &_worldPacket;
}

void AreaSpiritHealerQuery::Read()
{
    _worldPacket >> HealerGuid;
}

void AreaSpiritHealerQueue::Read()
{
    _worldPacket >> HealerGuid;
}

WorldPacket const* AreaSpiritHealerTime::Write()
{
    _worldPacket << HealerGuid;
    _worldPacket << int32(TimeLeft);

    return &_worldPacket;
}

ByteBuffer& operator<<(ByteBuffer& data, PVPMatchStatistics::RatingData const& ratingData)
{
    for (std::size_t i = 0; i < 2; ++i)
    {
        data << int32(ratingData.Prematch[i]);
        data << int32(ratingData.Postmatch[i]);
        data << int32(ratingData.PrematchMMR[i]);
    }

    return data;
}

ByteBuffer& operator<<(ByteBuffer& data, PVPMatchStatistics::HonorData const& honorData)
{
    data << uint32(honorData.HonorKills);
    data << uint32(honorData.Deaths);
    data << uint32(honorData.ContributionPoints);

    return data;
}

ByteBuffer& operator<<(ByteBuffer& data, PVPMatchStatistics::PVPMatchPlayerPVPStat const& pvpStat)
{
    data << int32(pvpStat.PvpStatID);
    data << int32(pvpStat.PvpStatValue);

    return data;
}

ByteBuffer& operator<<(ByteBuffer& data, PVPMatchStatistics::PVPMatchPlayerStatistics const& playerData)
{
    data << playerData.PlayerGUID;
    data << uint32(playerData.Kills);
    data << int32(playerData.Faction);
    data << uint32(playerData.DamageDone);
    data << uint32(playerData.HealingDone);
    data << Size<uint32>(playerData.Stats);
    data << int32(playerData.PrimaryTalentTree);
    data << int8(playerData.Sex);
    data << int8(playerData.Race);
    data << int8(playerData.Class);
    data << int32(playerData.CreatureID);
    data << int32(playerData.HonorLevel);
    data << int32(playerData.Role);
    for (PVPMatchStatistics::PVPMatchPlayerPVPStat const& pvpStat : playerData.Stats)
        data << pvpStat;

    data << Bits<1>(playerData.IsInWorld);
    data << OptionalInit(playerData.Honor);
    data << OptionalInit(playerData.PreMatchRating);
    data << OptionalInit(playerData.RatingChange);
    data << OptionalInit(playerData.PreMatchMMR);
    data << OptionalInit(playerData.MmrChange);
    data << OptionalInit(playerData.PostMatchMMR);
    data.FlushBits();

    if (playerData.Honor)
        data << *playerData.Honor;

    if (playerData.PreMatchRating)
        data << uint32(*playerData.PreMatchRating);

    if (playerData.RatingChange)
        data << int32(*playerData.RatingChange);

    if (playerData.PreMatchMMR)
        data << uint32(*playerData.PreMatchMMR);

    if (playerData.MmrChange)
        data << int32(*playerData.MmrChange);

    if (playerData.PostMatchMMR)
        data << uint32(*playerData.PostMatchMMR);

    return data;
}

ByteBuffer& operator<<(ByteBuffer& data, PVPMatchStatistics const& pvpLogData)
{
    data << OptionalInit(pvpLogData.Ratings);
    data << Size<uint32>(pvpLogData.Statistics);
    data.append(pvpLogData.PlayerCount.data(), pvpLogData.PlayerCount.size());

    if (pvpLogData.Ratings)
        data << *pvpLogData.Ratings;

    for (PVPMatchStatistics::PVPMatchPlayerStatistics const& player : pvpLogData.Statistics)
        data << player;

    return data;
}

WorldPacket const* PVPMatchStatisticsMessage::Write()
{
    _worldPacket.reserve(Data.Statistics.size() * sizeof(PVPMatchStatistics::PVPMatchPlayerStatistics) + sizeof(PVPMatchStatistics));

    _worldPacket << Data;

    return &_worldPacket;
}

void BattlemasterJoin::Read()
{
    _worldPacket >> Size<uint32>(QueueIDs);
    _worldPacket >> Roles;
    for (int32& blacklistMap : BlacklistMap)
        _worldPacket >> blacklistMap;

    for (uint64& queueId : QueueIDs)
        _worldPacket >> queueId;
}

void BattlemasterJoinArena::Read()
{
    _worldPacket >> TeamSizeIndex;
    _worldPacket >> Roles;
}

ByteBuffer& operator<<(ByteBuffer& data, BattlefieldStatusHeader const& header)
{
    data << header.Ticket;
    data << Size<uint32>(header.QueueID);
    data << uint8(header.RangeMin);
    data << uint8(header.RangeMax);
    data << uint8(header.TeamSize);
    data << uint32(header.InstanceID);
    for (uint64 queueID : header.QueueID)
        data << uint64(queueID);

    data << Bits<1>(header.RegisteredMatch);
    data << Bits<1>(header.TournamentRules);
    data.FlushBits();

    return data;
}

WorldPacket const* BattlefieldStatusNone::Write()
{
    _worldPacket << Ticket;

    return &_worldPacket;
}

WorldPacket const* BattlefieldStatusNeedConfirmation::Write()
{
    _worldPacket << Hdr;
    _worldPacket << uint32(Mapid);
    _worldPacket << uint32(Timeout);
    _worldPacket << uint8(Role);

    return &_worldPacket;
}

WorldPacket const* BattlefieldStatusActive::Write()
{
    _worldPacket << Hdr;
    _worldPacket << uint32(Mapid);
    _worldPacket << uint32(ShutdownTimer);
    _worldPacket << uint32(StartTimer);
    _worldPacket << int8(ArenaFaction);
    _worldPacket << Bits<1>(LeftEarly);
    _worldPacket << Bits<1>(Brawl);
    _worldPacket.FlushBits();

    return &_worldPacket;
}

WorldPacket const* BattlefieldStatusQueued::Write()
{
    _worldPacket << Hdr;
    _worldPacket << uint32(AverageWaitTime);
    _worldPacket << uint32(WaitTime);
    _worldPacket << int32(SpecSelected);
    _worldPacket << Bits<1>(AsGroup);
    _worldPacket << Bits<1>(EligibleForMatchmaking);
    _worldPacket << Bits<1>(SuspendedQueue);
    _worldPacket.FlushBits();

    return &_worldPacket;
}

WorldPacket const* BattlefieldStatusFailed::Write()
{
    _worldPacket << Ticket;
    _worldPacket << uint64(QueueID);
    _worldPacket << uint32(Reason);
    _worldPacket << ClientID;

    return &_worldPacket;
}

void BattlefieldPort::Read()
{
    _worldPacket >> Ticket;
    _worldPacket >> Bits<1>(AcceptedInvite);
}

void BattlefieldListRequest::Read()
{
    _worldPacket >> ListID;
}

WorldPacket const* BattlefieldList::Write()
{
    _worldPacket << BattlemasterGuid;
    _worldPacket << int32(BattlemasterListID);
    _worldPacket << uint8(MinLevel);
    _worldPacket << uint8(MaxLevel);
    _worldPacket << Size<uint32>(Battlefields);
    if (!Battlefields.empty())
        _worldPacket.append(Battlefields.data(), Battlefields.size());

    _worldPacket << Bits<1>(PvpAnywhere);
    _worldPacket << Bits<1>(HasRandomWinToday);
    _worldPacket.FlushBits();

    return &_worldPacket;
}

WorldPacket const* PVPOptionsEnabled::Write()
{
    _worldPacket << Bits<1>(RatedBattlegrounds);
    _worldPacket << Bits<1>(PugBattlegrounds);
    _worldPacket << Bits<1>(WargameBattlegrounds);
    _worldPacket << Bits<1>(WargameArenas);
    _worldPacket << Bits<1>(RatedArenas);
    _worldPacket << Bits<1>(ArenaSkirmish);
    _worldPacket << Bits<1>(SoloShuffle);
    _worldPacket << Bits<1>(RatedSoloShuffle);
    _worldPacket << Bits<1>(BattlegroundBlitz);
    _worldPacket << Bits<1>(RatedBattlegroundBlitz);
    _worldPacket.FlushBits();

    return &_worldPacket;
}

void ReportPvPPlayerAFK::Read()
{
    _worldPacket >> Offender;
}

WorldPacket const* ReportPvPPlayerAFKResult::Write()
{
    _worldPacket << Offender;
    _worldPacket << uint8(Result);
    _worldPacket << uint8(NumBlackMarksOnOffender);
    _worldPacket << uint8(NumPlayersIHaveReported);

    return &_worldPacket;
}

ByteBuffer& operator<<(ByteBuffer& data, BattlegroundPlayerPosition const& playerPosition)
{
    data << playerPosition.Guid;
    data << playerPosition.Pos;
    data << int8(playerPosition.IconID);
    data << int8(playerPosition.ArenaSlot);

    return data;
}

WorldPacket const* BattlegroundPlayerPositions::Write()
{
    _worldPacket << Size<uint32>(FlagCarriers);
    for (BattlegroundPlayerPosition const& pos : FlagCarriers)
        _worldPacket << pos;

    return &_worldPacket;
}

WorldPacket const* BattlegroundPlayerJoined::Write()
{
    _worldPacket << Guid;

    return &_worldPacket;
}

WorldPacket const* BattlegroundPlayerLeft::Write()
{
    _worldPacket << Guid;

    return &_worldPacket;
}

WorldPacket const* DestroyArenaUnit::Write()
{
    _worldPacket << Guid;

    return &_worldPacket;
}

ByteBuffer& operator<<(ByteBuffer& data, RatedPvpInfo::BracketInfo const& bracketInfo)
{
    data << int32(bracketInfo.PersonalRating);
    data << int32(bracketInfo.Ranking);
    data << int32(bracketInfo.SeasonPlayed);
    data << int32(bracketInfo.SeasonWon);
    data << int32(bracketInfo.SeasonFactionPlayed);
    data << int32(bracketInfo.SeasonFactionWon);
    data << int32(bracketInfo.WeeklyPlayed);
    data << int32(bracketInfo.WeeklyWon);
    data << int32(bracketInfo.RoundsSeasonPlayed);
    data << int32(bracketInfo.RoundsSeasonWon);
    data << int32(bracketInfo.RoundsWeeklyPlayed);
    data << int32(bracketInfo.RoundsWeeklyWon);
    data << int32(bracketInfo.BestWeeklyRating);
    data << int32(bracketInfo.LastWeeksBestRating);
    data << int32(bracketInfo.BestSeasonRating);
    data << int32(bracketInfo.PvpTierID);
    data << int32(bracketInfo.SeasonPvpTier);
    data << int32(bracketInfo.BestWeeklyPvpTier);
    data << uint8(bracketInfo.BestSeasonPvpTierEnum);
    data << Bits<1>(bracketInfo.Disqualified);
    data.FlushBits();

    return data;
}

WorldPacket const* RatedPvpInfo::Write()
{
    for (BracketInfo const& bracket : Bracket)
        _worldPacket << bracket;

    return &_worldPacket;
}

ByteBuffer& operator<<(ByteBuffer& data, RatedMatchDeserterPenalty const& ratedMatchDeserterPenalty)
{
    data << int32(ratedMatchDeserterPenalty.PersonalRatingChange);
    data << int32(ratedMatchDeserterPenalty.QueuePenaltySpellID);
    data << ratedMatchDeserterPenalty.QueuePenaltyDuration;

    return data;
}

WorldPacket const* PVPMatchInitialize::Write()
{
    _worldPacket << uint32(MapID);
    _worldPacket << uint8(State);
    _worldPacket << StartTime;
    _worldPacket << Duration;
    _worldPacket << uint8(ArenaFaction);
    _worldPacket << uint32(BattlemasterListID);
    _worldPacket << Bits<1>(Registered);
    _worldPacket << Bits<1>(AffectsRating);
    _worldPacket << OptionalInit(DeserterPenalty);
    _worldPacket.FlushBits();

    if (DeserterPenalty)
        _worldPacket << *DeserterPenalty;

    return &_worldPacket;
}

WorldPacket const* PVPMatchSetState::Write()
{
    _worldPacket << uint8(State);

    return &_worldPacket;
}

WorldPacket const* PVPMatchComplete::Write()
{
    _worldPacket << int32(Winner);
    _worldPacket << Duration;
    _worldPacket << OptionalInit(LogData);
    _worldPacket << Bits<2>(SoloShuffleStatus);
    _worldPacket.FlushBits();

    if (LogData)
        _worldPacket << *LogData;

    return &_worldPacket;
}

ByteBuffer& operator<<(ByteBuffer& data, BattlegroundCapturePointInfo const& battlegroundCapturePointInfo)
{
    data << battlegroundCapturePointInfo.Guid;
    data << battlegroundCapturePointInfo.Pos;
    data << int8(battlegroundCapturePointInfo.State);

    if (battlegroundCapturePointInfo.State == BattlegroundCapturePointState::ContestedHorde || battlegroundCapturePointInfo.State == BattlegroundCapturePointState::ContestedAlliance)
    {
        data << battlegroundCapturePointInfo.CaptureTime;
        data << battlegroundCapturePointInfo.CaptureTotalDuration;
    }

    return data;
}

WorldPacket const* UpdateCapturePoint::Write()
{
    _worldPacket << CapturePointInfo;

    return &_worldPacket;
}

WorldPacket const* CapturePointRemoved::Write()
{
    _worldPacket << CapturePointGUID;

    return &_worldPacket;
}
}
