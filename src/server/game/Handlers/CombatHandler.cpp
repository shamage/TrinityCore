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

#include "WorldSession.h"
#include "CombatPackets.h"
#include "Common.h"
#include "CreatureAI.h"
#include "DB2Structure.h"
#include "Log.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "Vehicle.h"

void WorldSession::HandleAttackSwingOpcode(WorldPackets::Combat::AttackSwing& packet)
{
    Unit* enemy = ObjectAccessor::GetUnit(*_player, packet.Victim);

    if (!enemy)
    {
        // stop attack state at client
        SendAttackStop(nullptr);
        return;
    }

    if (!_player->IsValidAttackTarget(enemy))
    {
        // stop attack state at client
        SendAttackStop(enemy);
        return;
    }

    //! Client explicitly checks the following before sending CMSG_ATTACK_SWING packet,
    //! so we'll place the same check here. Note that it might be possible to reuse this snippet
    //! in other places as well.
    if (Vehicle* vehicle = _player->GetVehicle())
    {
        VehicleSeatEntry const* seat = vehicle->GetSeatForPassenger(_player);
        ASSERT(seat);
        if (!(seat->Flags & VEHICLE_SEAT_FLAG_CAN_ATTACK))
        {
            SendAttackStop(enemy);
            return;
        }
    }

    _player->Attack(enemy, true);
}

void WorldSession::HandleAttackStopOpcode(WorldPackets::Combat::AttackStop& /*packet*/)
{
    GetPlayer()->AttackStop();
}

void WorldSession::HandleSetSheathedOpcode(WorldPackets::Combat::SetSheathed& packet)
{
    if (packet.CurrentSheathState >= MAX_SHEATH_STATE)
    {
        TC_LOG_ERROR("network", "Unknown sheath state {} ??", packet.CurrentSheathState);
        return;
    }

    GetPlayer()->SetSheath(SheathState(packet.CurrentSheathState));
}

void WorldSession::SendAttackStop(Unit const* enemy)
{
    WorldPackets::Combat::SAttackStop attackStop;
    attackStop.Attacker = _player->GetGUID();
    if (enemy)
    {
        attackStop.Victim = enemy->GetGUID();
        attackStop.NowDead = !enemy->IsAlive();
    }

    SendPacket(attackStop.Write());
}
