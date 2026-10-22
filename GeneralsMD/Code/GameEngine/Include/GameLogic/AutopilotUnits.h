/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 TheSuperHackers
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

// GeneralsX @feature Claude 21/10/2026 The autopilot's vocabulary of unit roles. A ThingTemplate cannot report what a unit can do (only a built
// object can), so roles are recognised by the unit name inside the template name, taken from the game's object INIs. General variants carry a
// prefix (Lazr_, Nuke_, Infa_, ...), so the match is on the name inside. One place to extend when a faction's unit is missing.

#pragma once

#include "Common/GameCommon.h"

class ThingTemplate;

namespace AutopilotUnits
{
	/// Carries the capture-building special power once the "Capture Building" upgrade is bought: Ranger, Red Guard, Rebel, and the Infantry
	/// General's Minigunner (he has no Red Guard).
	Bool isCaptureCapableInfantry(const ThingTemplate *t);

	/// The gun infantry of a squad: the basic rifleman of each faction.
	Bool isRifleInfantry(const ThingTemplate *t);

	/// The "tank person" of a squad: Tank Hunter, Missile Defender, the GLA RPG trooper (Tunnel Defender).
	Bool isAntiVehicleInfantry(const ThingTemplate *t);

	/// A structure whose special power is an area superweapon the autopilot knows how to fire: Particle Cannon / Uplink, Nuclear (Neutron) Missile,
	/// Scud Storm. Some structures are flagged as superweapons without carrying one (the Laser General's laser cannon is a turret).
	Bool isSuperweaponStructure(const ThingTemplate *t);

	/// China's Hacker: hacks the Internet for cash, faster inside an Internet Center (not the Black Lotus).
	Bool isHacker(const ThingTemplate *t);
}
