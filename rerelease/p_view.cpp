// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "g_local.h"
#include "m_player.h"
#include "bots/bot_includes.h"
#include <array>
#include <cmath>
#include <algorithm>
#include "g_thirdperson.h"

static edict_t   *current_player;
static gclient_t *current_client;

static vec3_t forward, right, up;
float		  xyspeed;

float bobmove;
int	  bobcycle, bobcycle_run;	  // odd cycles are right foot going forward
float bobfracsin; // sinf(bobfrac*M_PI)

// Helper: linear interpolation for vectors
static inline vec3_t LerpVec(const vec3_t &a, const vec3_t &b, float t) {
    vec3_t result;
    result.x = a.x + (b.x - a.x) * t;
    result.y = a.y + (b.y - a.y) * t;
    result.z = a.z + (b.z - a.z) * t;
    return result;
}

/*
===============
SkipViewModifiers
===============
*/
inline bool SkipViewModifiers() {
	if ( g_skipViewModifiers->integer && sv_cheats->integer ) {
		return true;
	}
	// don't do bobbing, etc on grapple
	if (current_client->ctf_grapple &&
		 current_client->ctf_grapplestate > CTF_GRAPPLE_STATE_FLY) {
		return true;
	}
	// spectator mode
	if (current_client->resp.spectator || (G_TeamplayEnabled() && current_client->resp.ctf_team == CTF_NOTEAM)) {
		return true;
	}
	return false;
}

/*
===============
SV_CalcRoll

===============
*/
float SV_CalcRoll(const vec3_t &angles, const vec3_t &velocity)
{
	if ( SkipViewModifiers() ) {
		return 0.0f;
	}

	float sign;
	float side;
	float value;

	side = velocity.dot(right);
	sign = side < 0 ? -1.f : 1.f;
	side = fabsf(side);

	value = sv_rollangle->value;

	if (side < sv_rollspeed->value)
		side = side * value / sv_rollspeed->value;
	else
		side = value;

	return side * sign;
}

/*
===============
P_DamageFeedback

Handles color blends and view kicks
===============
*/
void P_DamageFeedback(edict_t *player)
{
	gclient_t		  *client;
	float			 side;
	float			 realcount, count, kick;
	vec3_t			 v;
	int				 l;
	constexpr vec3_t armor_color = { 1.0, 1.0, 1.0 };
	constexpr vec3_t power_color = { 0.0, 1.0, 0.0 };
	constexpr vec3_t bcolor = { 1.0, 0.0, 0.0 };

	client = player->client;

	// flash the backgrounds behind the status numbers
	int16_t want_flashes = 0;

	if (client->damage_blood)
		want_flashes |= 1;
	if (client->damage_armor && !(player->flags & FL_GODMODE) && (client->invincible_time <= level.time))
		want_flashes |= 2;

	if (want_flashes)
	{
		client->flash_time = level.time + 100_ms;
		client->ps.stats[STAT_FLASHES] = want_flashes;
	}
	else if (client->flash_time < level.time)
		client->ps.stats[STAT_FLASHES] = 0;

	// total points of damage shot at the player this frame
	count = (float) (client->damage_blood + client->damage_armor + client->damage_parmor);
	if (count == 0)
		return; // didn't take any damage

	// start a pain animation if still in the player model
	if (client->anim_priority < ANIM_PAIN && player->s.modelindex == MODELINDEX_PLAYER)
	{
		static int i;

		client->anim_priority = ANIM_PAIN;
		if (client->ps.pmove.pm_flags & PMF_DUCKED)
		{
			player->s.frame = FRAME_crpain1 - 1;
			client->anim_end = FRAME_crpain4;
		}
		else
		{
			i = (i + 1) % 3;
			switch (i)
			{
			case 0:
				player->s.frame = FRAME_pain101 - 1;
				client->anim_end = FRAME_pain104;
				break;
			case 1:
				player->s.frame = FRAME_pain201 - 1;
				client->anim_end = FRAME_pain204;
				break;
			case 2:
				player->s.frame = FRAME_pain301 - 1;
				client->anim_end = FRAME_pain304;
				break;
			}
		}

		client->anim_time = 0_ms;
	}

	realcount = count;

	// if we took health damage, do a minimum clamp
	if (client->damage_blood)
	{
		if (count < 10)
			count = 10; // always make a visible effect
	}
	else
	{
		if (count > 2)
			count = 2; // don't go too deep
	}

	// play an appropriate pain sound
	if ((level.time > player->pain_debounce_time) && !(player->flags & FL_GODMODE) && (client->invincible_time <= level.time))
	{
		player->pain_debounce_time = level.time + 700_ms;

		constexpr const char *pain_sounds[] = {
			"*pain25_1.wav",
			"*pain25_2.wav",
			"*pain50_1.wav",
			"*pain50_2.wav",
			"*pain75_1.wav",
			"*pain75_2.wav",
			"*pain100_1.wav",
			"*pain100_2.wav",
		};

		if (player->health < 25)
			l = 0;
		else if (player->health < 50)
			l = 2;
		else if (player->health < 75)
			l = 4;
		else
			l = 6;

		if (brandom())
			l |= 1;

		gi.sound(player, CHAN_VOICE, gi.soundindex(pain_sounds[l]), 1, ATTN_NORM, 0);
		// Paril: pain noises alert monsters
		PlayerNoise(player, player->s.origin, PNOISE_SELF);
	}

	// the total alpha of the blend is always proportional to count
	if (client->damage_alpha < 0)
		client->damage_alpha = 0;

	// [Paril-KEX] tweak the values to rely less on this
	// and more on damage indicators
	if (client->damage_blood || (client->damage_alpha + count * 0.06f) < 0.15f)
	{
		client->damage_alpha += count * 0.06f;

		if (client->damage_alpha < 0.06f)
			client->damage_alpha = 0.06f;
		if (client->damage_alpha > 0.4f)
			client->damage_alpha = 0.4f; // don't go too saturated
	}

	// mix in colors
	v = {};

	if (client->damage_parmor)
		v += power_color * (client->damage_parmor / realcount);
	if (client->damage_blood)
		v += bcolor * max(15.0f, (client->damage_blood / realcount));
	if (client->damage_armor)
		v += armor_color * (client->damage_armor / realcount);
	client->damage_blend = v.normalized();

	//
	// calculate view angle kicks
	//
	kick = (float) abs(client->damage_knockback);
	if (kick && player->health > 0) // kick of 0 means no view adjust at all
	{
		kick = kick * 100 / player->health;

		if (kick < count * 0.5f)
			kick = count * 0.5f;
		if (kick > 50)
			kick = 50;

		v = client->damage_from - player->s.origin;
		v.normalize();

		side = v.dot(right);
		client->v_dmg_roll = kick * side * 0.3f;

		side = -v.dot(forward);
		client->v_dmg_pitch = kick * side * 0.3f;

		client->v_dmg_time = level.time + DAMAGE_TIME();
	}

	// [Paril-KEX] send view indicators
	if (client->num_damage_indicators)
	{
		gi.WriteByte(svc_damage);
		gi.WriteByte(client->num_damage_indicators);

		for (uint8_t i = 0; i < client->num_damage_indicators; i++)
		{
			auto &indicator = client->damage_indicators[i];

			// encode total damage into 5 bits
			uint8_t encoded = std::clamp((indicator.health + indicator.power + indicator.armor) / 3, 1, 0x1F);

			// encode types in the latter 3 bits
			if (indicator.health)
				encoded |= 0x20;
			if (indicator.armor)
				encoded |= 0x40;
			if (indicator.power)
				encoded |= 0x80;

			gi.WriteByte(encoded);
			gi.WriteDir((player->s.origin - indicator.from).normalized());
		}

		gi.unicast(player, false);
	}

	//
	// clear totals
	//
	client->damage_blood = 0;
	client->damage_armor = 0;
	client->damage_parmor = 0;
	client->damage_knockback = 0;
	client->num_damage_indicators = 0;
}

/*
===============
SV_CalcViewOffset

Auto pitching on slopes?

  fall from 128: 400 = 160000
  fall from 256: 580 = 336400
  fall from 384: 720 = 518400
  fall from 512: 800 = 640000
  fall from 640: 960 =

  damage = deltavelocity*deltavelocity  * 0.0001

===============
*/
void SV_CalcViewOffset(edict_t *ent) {
    if (sv_thirdperson && sv_thirdperson->integer &&
        ent->health > 0 && !ent->client->resp.spectator) {
        // Third-person camera is processed in G_SetThirdPersonView;
        // skip first-person view offset modifications.
        return;
    }
    
    // --- First-Person View Offset Calculations ---
    vec3_t baseOffset = { 0, 0, 0 };

    // Calculate bobbing offset (with a maximum limit)
    float bob = bobfracsin * xyspeed * bob_up->value;
    bob = std::min(bob, 6.0f);
    baseOffset[2] += bob;

    // Add kick offset (damage/fall kicks)
    // (Assuming P_CurrentKickOrigin returns a vec3_t representing kick offsets)
    vec3_t kickOffset = P_CurrentKickOrigin(ent);
    baseOffset.x += kickOffset.x;
    baseOffset.y += kickOffset.y;
    baseOffset.z += kickOffset.z;
    
    // Clamp the final offset so the view never leaves the player bounds
    baseOffset[0] = std::clamp(baseOffset[0], -14.0f, 14.0f);
    baseOffset[1] = std::clamp(baseOffset[1], -14.0f, 14.0f);
    baseOffset[2] = std::clamp(baseOffset[2], -22.0f, 30.0f);

    // Optionally, smooth the transition from the previous frame’s offset
    ent->client->ps.viewoffset = LerpVec(ent->client->ps.viewoffset, baseOffset, 0.5f);
}

/*
==============
SV_CalcGunOffset
==============
*/
void SV_CalcGunOffset(edict_t *ent)
{
    int i;

    // --- Third-Person Mode Check ---
    // If third-person mode is active, hide the weapon model and reset gun offsets.
    if (sv_thirdperson && sv_thirdperson->integer &&
        ent->health > 0 && !ent->client->resp.spectator)
    {
        ent->client->ps.gunindex = 0;  // Hide weapon
        for (i = 0; i < 3; i++)
            ent->client->ps.gunoffset[i] = 0;
        return;
    }

    // --- First-Person Gun Offset Processing ---
    // Check if we have a valid weapon that isn’t one of the exceptions (e.g., plasma beam or grapple while firing)
    // and if view modifiers (like bobbing) should be applied.
    if (ent->client->pers.weapon &&
        !((ent->client->pers.weapon->id == IT_WEAPON_PLASMABEAM ||
           ent->client->pers.weapon->id == IT_WEAPON_GRAPPLE) &&
          ent->client->weaponstate == WEAPON_FIRING) &&
        !SkipViewModifiers())
    {
        // Gun angles derived from bobbing.
        ent->client->ps.gunangles[ROLL] = xyspeed * bobfracsin * 0.005f;
        ent->client->ps.gunangles[YAW]  = xyspeed * bobfracsin * 0.01f;
        if (bobcycle & 1)
        {
            ent->client->ps.gunangles[ROLL] = -ent->client->ps.gunangles[ROLL];
            ent->client->ps.gunangles[YAW]  = -ent->client->ps.gunangles[YAW];
        }
        ent->client->ps.gunangles[PITCH] = xyspeed * bobfracsin * 0.005f;

        // Compute the change in view angles between the current and previous frames.
        vec3_t viewangles_delta = ent->client->oldviewangles - ent->client->ps.viewangles;

        // Accumulate delta into a slower-adjusting vector (for smoother transitions).
        for (i = 0; i < 3; i++)
            ent->client->slow_view_angles[i] += viewangles_delta[i];

        // Adjust gun angles based on the accumulated slow view angles.
        for (i = 0; i < 3; i++)
        {
            float &d = ent->client->slow_view_angles[i];

            if (!d)
                continue;

            if (d > 180)
                d -= 360;
            if (d < -180)
                d += 360;
            if (d > 45)
                d = 45;
            if (d < -45)
                d = -45;

            // Apply only half the delta to keep the weapon from feeling detached.
            if (i == ROLL)
                ent->client->ps.gunangles[i] += (0.1f * d) * 0.5f;
            else
                ent->client->ps.gunangles[i] += (0.2f * d) * 0.5f;

            // Gradually reduce the accumulated delta over time.
            float reduction_factor = viewangles_delta[i] ? 0.05f : 0.15f;
            if (d > 0)
                d = max(0.f, d - gi.frame_time_ms * reduction_factor);
            else if (d < 0)
                d = min(0.f, d + gi.frame_time_ms * reduction_factor);
        }

        // [Paril-KEX] cl_rollhack: Invert roll so the weapon appears correctly.
        ent->client->ps.gunangles[ROLL] = -ent->client->ps.gunangles[ROLL];
    }
    else
    {
        // If no valid weapon or if view modifiers are skipped, clear the gun angles.
        for (i = 0; i < 3; i++)
            ent->client->ps.gunangles[i] = 0;
    }

    // --- Gun Offset Calculation ---
    // Reset gun offset, then apply developer-defined adjustments based on forward/right/up vectors.
    ent->client->ps.gunoffset = {};
    for (i = 0; i < 3; i++)
    {
        ent->client->ps.gunoffset[i] += forward[i] * (gun_y->value);
        ent->client->ps.gunoffset[i] += right[i]  * gun_x->value;
        ent->client->ps.gunoffset[i] += up[i]     * (-gun_z->value);
    }
}


/*
=============
SV_CalcBlend
=============
*/
void SV_CalcBlend(edict_t *ent)
{
	gtime_t remaining;

	ent->client->ps.damage_blend = ent->client->ps.screen_blend = {};

	// add for powerups
	if (ent->client->quad_time > level.time)
	{
		remaining = ent->client->quad_time - level.time;
		if (remaining.milliseconds() == 3000) // beginning to fade
			gi.sound(ent, CHAN_ITEM, gi.soundindex("items/damage2.wav"), 1, ATTN_NORM, 0);
		if (G_PowerUpExpiringRelative(remaining))
			G_AddBlend(0, 0, 1, 0.08f, ent->client->ps.screen_blend);
	}
	// RAFAEL
	else if (ent->client->quadfire_time > level.time)
	{
		remaining = ent->client->quadfire_time - level.time;
		if (remaining.milliseconds() == 3000) // beginning to fade
			gi.sound(ent, CHAN_ITEM, gi.soundindex("items/quadfire2.wav"), 1, ATTN_NORM, 0);
		if (G_PowerUpExpiringRelative(remaining))
			G_AddBlend(1, 0.2f, 0.5f, 0.08f, ent->client->ps.screen_blend);
	}
	// RAFAEL
	// PMM - double damage
	else if (ent->client->double_time > level.time)
	{
		remaining = ent->client->double_time - level.time;
		if (remaining.milliseconds() == 3000) // beginning to fade
			gi.sound(ent, CHAN_ITEM, gi.soundindex("misc/ddamage2.wav"), 1, ATTN_NORM, 0);
		if (G_PowerUpExpiringRelative(remaining))
			G_AddBlend(0.9f, 0.7f, 0, 0.08f, ent->client->ps.screen_blend);
	}
	// PMM
	else if (ent->client->invincible_time > level.time)
	{
		remaining = ent->client->invincible_time - level.time;
		if (remaining.milliseconds() == 3000) // beginning to fade
			gi.sound(ent, CHAN_ITEM, gi.soundindex("items/protect2.wav"), 1, ATTN_NORM, 0);
		if (G_PowerUpExpiringRelative(remaining))
			G_AddBlend(1, 1, 0, 0.08f, ent->client->ps.screen_blend);
	}
	else if (ent->client->invisible_time > level.time)
	{
		remaining = ent->client->invisible_time - level.time;
		if (remaining.milliseconds() == 3000) // beginning to fade
			gi.sound(ent, CHAN_ITEM, gi.soundindex("items/protect2.wav"), 1, ATTN_NORM, 0);
		if (G_PowerUpExpiringRelative(remaining))
			G_AddBlend(0.8f, 0.8f, 0.8f, 0.08f, ent->client->ps.screen_blend);
	}
	else if (ent->client->enviro_time > level.time)
	{
		remaining = ent->client->enviro_time - level.time;
		if (remaining.milliseconds() == 3000) // beginning to fade
			gi.sound(ent, CHAN_ITEM, gi.soundindex("items/airout.wav"), 1, ATTN_NORM, 0);
		if (G_PowerUpExpiringRelative(remaining))
			G_AddBlend(0, 1, 0, 0.08f, ent->client->ps.screen_blend);
	}
	else if (ent->client->breather_time > level.time)
	{
		remaining = ent->client->breather_time - level.time;
		if (remaining.milliseconds() == 3000) // beginning to fade
			gi.sound(ent, CHAN_ITEM, gi.soundindex("items/airout.wav"), 1, ATTN_NORM, 0);
		if (G_PowerUpExpiringRelative(remaining))
			G_AddBlend(0.4f, 1, 0.4f, 0.04f, ent->client->ps.screen_blend);
	}

	// PGM
	if (ent->client->nuke_time > level.time)
	{
		float brightness = (ent->client->nuke_time - level.time).seconds() / 2.0f;
		G_AddBlend(1, 1, 1, brightness, ent->client->ps.screen_blend);
	}
	if (ent->client->ir_time > level.time)
	{
		remaining = ent->client->ir_time - level.time;
		if (G_PowerUpExpiringRelative(remaining))
		{
			ent->client->ps.rdflags |= RDF_IRGOGGLES;
			G_AddBlend(1, 0, 0, 0.2f, ent->client->ps.screen_blend);
		}
		else
			ent->client->ps.rdflags &= ~RDF_IRGOGGLES;
	}
	else
	{
		ent->client->ps.rdflags &= ~RDF_IRGOGGLES;
	}
	// PGM

	// add for damage
	if (ent->client->damage_alpha > 0)
		G_AddBlend(ent->client->damage_blend[0], ent->client->damage_blend[1], ent->client->damage_blend[2], ent->client->damage_alpha, ent->client->ps.damage_blend);

	// [Paril-KEX] drowning visual indicator
	if (ent->air_finished < level.time + 9_sec)
	{
		constexpr vec3_t drown_color = { 0.1f, 0.1f, 0.2f };
		constexpr float max_drown_alpha = 0.75f;
		float alpha = (ent->air_finished < level.time) ? 1 : (1.f - ((ent->air_finished - level.time).seconds() / 9.0f));
		G_AddBlend(drown_color[0], drown_color[1], drown_color[2], min(alpha, max_drown_alpha), ent->client->ps.damage_blend);
	}

#if 0
	if (ent->client->bonus_alpha > 0)
		G_AddBlend(0.85f, 0.7f, 0.3f, ent->client->bonus_alpha, ent->client->ps.damage_blend);
#endif

	// drop the damage value
	ent->client->damage_alpha -= gi.frame_time_s * 0.6f;
	if (ent->client->damage_alpha < 0)
		ent->client->damage_alpha = 0;

	// drop the bonus value
	ent->client->bonus_alpha -= gi.frame_time_s;
	if (ent->client->bonus_alpha < 0)
		ent->client->bonus_alpha = 0;
}

/*
=============
P_WorldEffects
=============
*/
void P_WorldEffects()
{
	bool		  breather;
	bool		  envirosuit;
	water_level_t waterlevel, old_waterlevel;

	if (current_player->movetype == MOVETYPE_NOCLIP)
	{
		current_player->air_finished = level.time + 12_sec; // don't need air
		return;
	}

	waterlevel = current_player->waterlevel;
	old_waterlevel = current_client->old_waterlevel;
	current_client->old_waterlevel = waterlevel;

	breather = current_client->breather_time > level.time;
	envirosuit = current_client->enviro_time > level.time;

	//
	// if just entered a water volume, play a sound
	//
	if (!old_waterlevel && waterlevel)
	{
		PlayerNoise(current_player, current_player->s.origin, PNOISE_SELF);
		if (current_player->watertype & CONTENTS_LAVA)
			gi.sound(current_player, CHAN_BODY, gi.soundindex("player/lava_in.wav"), 1, ATTN_NORM, 0);
		else if (current_player->watertype & CONTENTS_SLIME)
			gi.sound(current_player, CHAN_BODY, gi.soundindex("player/watr_in.wav"), 1, ATTN_NORM, 0);
		else if (current_player->watertype & CONTENTS_WATER)
			gi.sound(current_player, CHAN_BODY, gi.soundindex("player/watr_in.wav"), 1, ATTN_NORM, 0);
		current_player->flags |= FL_INWATER;

		// clear damage_debounce, so the pain sound will play immediately
		current_player->damage_debounce_time = level.time - 1_sec;
	}

	//
	// if just completely exited a water volume, play a sound
	//
	if (old_waterlevel && !waterlevel)
	{
		PlayerNoise(current_player, current_player->s.origin, PNOISE_SELF);
		gi.sound(current_player, CHAN_BODY, gi.soundindex("player/watr_out.wav"), 1, ATTN_NORM, 0);
		current_player->flags &= ~FL_INWATER;
	}

	//
	// check for head just going under water
	//
	if (old_waterlevel != WATER_UNDER && waterlevel == WATER_UNDER)
	{
		gi.sound(current_player, CHAN_BODY, gi.soundindex("player/watr_un.wav"), 1, ATTN_NORM, 0);
	}

	//
	// check for head just coming out of water
	//
	if (current_player->health > 0 && old_waterlevel == WATER_UNDER && waterlevel != WATER_UNDER)
	{
		if (current_player->air_finished < level.time)
		{ // gasp for air
			gi.sound(current_player, CHAN_VOICE, gi.soundindex("player/gasp1.wav"), 1, ATTN_NORM, 0);
			PlayerNoise(current_player, current_player->s.origin, PNOISE_SELF);
		}
		else if (current_player->air_finished < level.time + 11_sec)
		{ // just break surface
			gi.sound(current_player, CHAN_VOICE, gi.soundindex("player/gasp2.wav"), 1, ATTN_NORM, 0);
		}
	}

	//
	// check for drowning
	//
	if (waterlevel == WATER_UNDER)
	{
		// breather or envirosuit give air
		if (breather || envirosuit)
		{
			current_player->air_finished = level.time + 10_sec;

			if (((current_client->breather_time - level.time).milliseconds() % 2500) == 0)
			{
				if (!current_client->breather_sound)
					gi.sound(current_player, CHAN_AUTO, gi.soundindex("player/u_breath1.wav"), 1, ATTN_NORM, 0);
				else
					gi.sound(current_player, CHAN_AUTO, gi.soundindex("player/u_breath2.wav"), 1, ATTN_NORM, 0);
				current_client->breather_sound ^= 1;
				PlayerNoise(current_player, current_player->s.origin, PNOISE_SELF);
				// FIXME: release a bubble?
			}
		}

		// if out of air, start drowning
		if (current_player->air_finished < level.time)
		{ // drown!
			if (current_player->client->next_drown_time < level.time && current_player->health > 0)
			{
				current_player->client->next_drown_time = level.time + 1_sec;

				// take more damage the longer underwater
				current_player->dmg += 2;
				if (current_player->dmg > 15)
					current_player->dmg = 15;

				// play a gurp sound instead of a normal pain sound
				if (current_player->health <= current_player->dmg)
					gi.sound(current_player, CHAN_VOICE, gi.soundindex("*drown1.wav"), 1, ATTN_NORM, 0); // [Paril-KEX]
				else if (brandom())
					gi.sound(current_player, CHAN_VOICE, gi.soundindex("*gurp1.wav"), 1, ATTN_NORM, 0);
				else
					gi.sound(current_player, CHAN_VOICE, gi.soundindex("*gurp2.wav"), 1, ATTN_NORM, 0);

				current_player->pain_debounce_time = level.time;

				T_Damage(current_player, world, world, vec3_origin, current_player->s.origin, vec3_origin, current_player->dmg, 0, DAMAGE_NO_ARMOR, MOD_WATER);
			}
		}
		// Paril: almost-drowning sounds
		else if (current_player->air_finished <= level.time + 3_sec)
		{
			if (current_player->client->next_drown_time < level.time)
			{
				gi.sound(current_player, CHAN_VOICE, gi.soundindex(fmt::format("player/wade{}.wav", 1 + ((int32_t) level.time.seconds() % 3)).c_str()), 1, ATTN_NORM, 0);
				current_player->client->next_drown_time = level.time + 1_sec;
			}
		}
	}
	else
	{
		current_player->air_finished = level.time + 12_sec;
		current_player->dmg = 2;
	}

	//
	// check for sizzle damage
	//
	if (waterlevel && (current_player->watertype & (CONTENTS_LAVA | CONTENTS_SLIME)) && current_player->slime_debounce_time <= level.time)
	{
		if (current_player->watertype & CONTENTS_LAVA)
		{
			if (current_player->health > 0 && current_player->pain_debounce_time <= level.time && current_client->invincible_time < level.time)
			{
				if (brandom())
					gi.sound(current_player, CHAN_VOICE, gi.soundindex("player/burn1.wav"), 1, ATTN_NORM, 0);
				else
					gi.sound(current_player, CHAN_VOICE, gi.soundindex("player/burn2.wav"), 1, ATTN_NORM, 0);
				current_player->pain_debounce_time = level.time + 1_sec;
			}

			int dmg = (envirosuit ? 1 : 3) * waterlevel; // take 1/3 damage with envirosuit

			T_Damage(current_player, world, world, vec3_origin, current_player->s.origin, vec3_origin, dmg, 0, DAMAGE_NONE, MOD_LAVA);
			current_player->slime_debounce_time = level.time + 10_hz;
		}

		if (current_player->watertype & CONTENTS_SLIME)
		{
			if (!envirosuit)
			{ // no damage from slime with envirosuit
				T_Damage(current_player, world, world, vec3_origin, current_player->s.origin, vec3_origin, 1 * waterlevel, 0, DAMAGE_NONE, MOD_SLIME);
				current_player->slime_debounce_time = level.time + 10_hz;
			}
		}
	}
}

/*
===============
G_SetClientEffects

Sets visual effects for the player entity including powerup effects,
team colors, and environmental indicators. Contains special handling
for third-person mode to maintain player model visibility.
===============
*/
void G_SetClientEffects(edict_t *ent)
{
    // Early check for third-person mode
    // If active, apply minimal effects and ensure player model visibility
    if (sv_thirdperson && sv_thirdperson->integer && 
        ent->health > 0 && !ent->client->resp.spectator)
    {
        // Set minimal effects to avoid interference with visibility
        ent->s.effects = EF_NONE;
        ent->s.renderfx &= RF_STAIR_STEP;
        ent->s.renderfx |= RF_IR_VISIBLE;
        ent->s.alpha = 1.0;
        
        // Force model visibility (critical for third-person)
        ent->svflags &= ~SVF_NOCLIENT;
        ent->flags &= ~FL_NOVISIBLE;
        
        // Link entity to propagate changes
        gi.linkentity(ent);
        
        // Exit early to prevent other effects from interfering
        return;
    } else {
		// Reset effects for normal first-person processing
		ent->s.effects = EF_NONE;
		ent->s.renderfx &= RF_STAIR_STEP;
		ent->s.renderfx |= RF_IR_VISIBLE;
		ent->s.alpha = 1.0;
	}

    // Skip remaining effects if player is dead or during intermission
    if (ent->health <= 0 || level.intermissiontime)
        return;

    // -- Standard effects processing below --
    
    // Flashlight
    if (ent->flags & FL_FLASHLIGHT)
        ent->s.effects |= EF_FLASHLIGHT;

    // Disguise effect (PGM)
    if (ent->flags & FL_DISGUISED)
        ent->s.renderfx |= RF_USE_DISGUISE;

    // Gamerules-based effects (PGM)
    if (gamerules->integer && DMGame.PlayerEffects)
        DMGame.PlayerEffects(ent);

    // Power armor effects
    if (ent->powerarmor_time > level.time)
    {
        int pa_type = PowerArmorType(ent);
        if (pa_type == IT_ITEM_POWER_SCREEN)
        {
            ent->s.effects |= EF_POWERSCREEN;
        }
        else if (pa_type == IT_ITEM_POWER_SHIELD)
        {
            ent->s.effects |= EF_COLOR_SHELL;
            ent->s.renderfx |= RF_SHELL_GREEN;
        }
    }

    // CTF-specific effects
    CTFEffects(ent);

    // Quad damage effect
    if (ent->client->quad_time > level.time)
    {
        if (G_PowerUpExpiring(ent->client->quad_time))
            CTFSetPowerUpEffect(ent, EF_QUAD);
    }

    // Quad fire effect (RAFAEL)
    if (ent->client->quadfire_time > level.time)
    {
        if (G_PowerUpExpiring(ent->client->quadfire_time))
            CTFSetPowerUpEffect(ent, EF_DUALFIRE);
    }

    // Double damage effect (ROGUE)
    if (ent->client->double_time > level.time)
    {
        if (G_PowerUpExpiring(ent->client->double_time))
            CTFSetPowerUpEffect(ent, EF_DOUBLE);
    }

    // Defender sphere effect (ROGUE)
    if ((ent->client->owned_sphere) && (ent->client->owned_sphere->spawnflags == SPHERE_DEFENDER))
    {
        CTFSetPowerUpEffect(ent, EF_HALF_DAMAGE);
    }

    // Tracker pain effect (ROGUE)
    if (ent->client->tracker_pain_time > level.time)
    {
        ent->s.effects |= EF_TRACKERTRAIL;
    }

    // Invisibility effect (ROGUE)
    if (ent->client->invisible_time > level.time)
    {
        if (ent->client->invisibility_fade_time <= level.time)
            ent->s.alpha = 0.1f;
        else
        {
            float x = (ent->client->invisibility_fade_time - level.time).seconds() / INVISIBILITY_TIME.seconds();
            ent->s.alpha = std::clamp(x, 0.1f, 1.0f);
        }
    }

    // Invincibility effect
    if (ent->client->invincible_time > level.time)
    {
        if (G_PowerUpExpiring(ent->client->invincible_time))
            CTFSetPowerUpEffect(ent, EF_PENT);
    }

    // God mode effect (cheat)
    if (ent->flags & FL_GODMODE)
    {
        ent->s.effects |= EF_COLOR_SHELL;
        ent->s.renderfx |= (RF_SHELL_RED | RF_SHELL_GREEN | RF_SHELL_BLUE);
    }
}

/*
===============
G_SetClientEvent
===============
*/
void G_SetClientEvent(edict_t *ent)
{
	if (ent->s.event)
		return;

	if (ent->client->ps.pmove.pm_flags & PMF_ON_LADDER)
	{
		if (!deathmatch->integer &&
			current_client->last_ladder_sound < level.time &&
			(current_client->last_ladder_pos - ent->s.origin).length() > 48.f)
		{
			ent->s.event = EV_LADDER_STEP;
			current_client->last_ladder_pos = ent->s.origin;
			current_client->last_ladder_sound = level.time + LADDER_SOUND_TIME;
		}
	}
	else if (ent->groundentity && xyspeed > 225)
	{
		if ((int) (current_client->bobtime + bobmove) != bobcycle_run)
			ent->s.event = EV_FOOTSTEP;
	}
}

/*
===============
G_SetClientSound
===============
*/
void G_SetClientSound(edict_t *ent)
{
	// help beep (no more than three times)
	if (ent->client->pers.helpchanged && ent->client->pers.helpchanged <= 3 && ent->client->pers.help_time < level.time)
	{
		if (ent->client->pers.helpchanged == 1) // [KEX] haleyjd: once only
			gi.sound(ent, CHAN_AUTO, gi.soundindex("misc/pc_up.wav"), 1, ATTN_STATIC, 0);
		ent->client->pers.helpchanged++;
		ent->client->pers.help_time = level.time + 5_sec;
	}

	// reset defaults
	ent->s.sound = 0;
	ent->s.loop_attenuation = 0;
	ent->s.loop_volume = 0;

	if (ent->waterlevel && (ent->watertype & (CONTENTS_LAVA | CONTENTS_SLIME)))
	{
		ent->s.sound = snd_fry;
		return;
	}

	if (ent->deadflag || ent->client->resp.spectator)
		return;

	if (ent->client->weapon_sound)
		ent->s.sound = ent->client->weapon_sound;
	else if (ent->client->pers.weapon)
	{
		if (ent->client->pers.weapon->id == IT_WEAPON_RAILGUN)
			ent->s.sound = gi.soundindex("weapons/rg_hum.wav");
		else if (ent->client->pers.weapon->id == IT_WEAPON_BFG)
			ent->s.sound = gi.soundindex("weapons/bfg_hum.wav");
		// RAFAEL
		else if (ent->client->pers.weapon->id == IT_WEAPON_PHALANX)
			ent->s.sound = gi.soundindex("weapons/phaloop.wav");
		// RAFAEL
	}

	// [Paril-KEX] if no other sound is playing, play appropriate grapple sounds
	if (!ent->s.sound && ent->client->ctf_grapple)
	{
		if (ent->client->ctf_grapplestate == CTF_GRAPPLE_STATE_PULL)
			ent->s.sound = gi.soundindex("weapons/grapple/grpull.wav");
		else if (ent->client->ctf_grapplestate == CTF_GRAPPLE_STATE_FLY)
			ent->s.sound = gi.soundindex("weapons/grapple/grfly.wav");
		else if (ent->client->ctf_grapplestate == CTF_GRAPPLE_STATE_HANG)
			ent->s.sound = gi.soundindex("weapons/grapple/grhang.wav");
	}

	// weapon sounds play at a higher attn
	ent->s.loop_attenuation = ATTN_NORM;
}

/*
===============
G_SetClientFrame
===============
*/
void G_SetClientFrame(edict_t *ent)
{
	gclient_t *client;
	bool	   duck, run;

	if (ent->s.modelindex != MODELINDEX_PLAYER)
		return; // not in the player model

	client = ent->client;

	if (client->ps.pmove.pm_flags & PMF_DUCKED)
		duck = true;
	else
		duck = false;
	if (xyspeed)
		run = true;
	else
		run = false;

	// check for stand/duck and stop/go transitions
	if (duck != client->anim_duck && client->anim_priority < ANIM_DEATH)
		goto newanim;
	if (run != client->anim_run && client->anim_priority == ANIM_BASIC)
		goto newanim;
	if (!ent->groundentity && client->anim_priority <= ANIM_WAVE)
		goto newanim;
	
	if (client->anim_time > level.time)
		return;
	else if ((client->anim_priority & ANIM_REVERSED) && (ent->s.frame > client->anim_end))
	{
		if (client->anim_time <= level.time)
		{
			ent->s.frame--;
			client->anim_time = level.time + 10_hz;
		}
		return;
	}
	else if (!(client->anim_priority & ANIM_REVERSED) && (ent->s.frame < client->anim_end))
	{
		// continue an animation
		if (client->anim_time <= level.time)
		{
			ent->s.frame++;
			client->anim_time = level.time + 10_hz;
		}
		return;
	}

	if (client->anim_priority == ANIM_DEATH)
		return; // stay there
	if (client->anim_priority == ANIM_JUMP)
	{
		if (!ent->groundentity)
			return; // stay there
		ent->client->anim_priority = ANIM_WAVE;

		if (duck)
		{
			ent->s.frame = FRAME_jump6;
			ent->client->anim_end = FRAME_jump4;
			ent->client->anim_priority |= ANIM_REVERSED;
		}
		else
		{
			ent->s.frame = FRAME_jump3;
			ent->client->anim_end = FRAME_jump6;
		}
		ent->client->anim_time = level.time + 10_hz;
		return;
	}

newanim:
	// return to either a running or standing frame
	client->anim_priority = ANIM_BASIC;
	client->anim_duck = duck;
	client->anim_run = run;
	client->anim_time = level.time + 10_hz;

	if (!ent->groundentity)
	{
		// ZOID: if on grapple, don't go into jump frame, go into standing
		// frame
		if (client->ctf_grapple)
		{
			if (duck)
			{
				ent->s.frame = FRAME_crstnd01;
				client->anim_end = FRAME_crstnd19;
			}
			else
			{
				ent->s.frame = FRAME_stand01;
				client->anim_end = FRAME_stand40;
			}
		}
		else
		{
			// ZOID
			client->anim_priority = ANIM_JUMP;

			if (duck)
			{
				if (ent->s.frame != FRAME_crwalk2)
					ent->s.frame = FRAME_crwalk1;
				client->anim_end = FRAME_crwalk2;
			}
			else
			{
				if (ent->s.frame != FRAME_jump2)
					ent->s.frame = FRAME_jump1;
				client->anim_end = FRAME_jump2;
			}
		}
	}
	else if (run)
	{ // running
		if (duck)
		{
			ent->s.frame = FRAME_crwalk1;
			client->anim_end = FRAME_crwalk6;
		}
		else
		{
			ent->s.frame = FRAME_run1;
			client->anim_end = FRAME_run6;
		}
	}
	else
	{ // standing
		if (duck)
		{
			ent->s.frame = FRAME_crstnd01;
			client->anim_end = FRAME_crstnd19;
		}
		else
		{
			ent->s.frame = FRAME_stand01;
			client->anim_end = FRAME_stand40;
		}
	}
}

// [Paril-KEX]
static void P_RunMegaHealth(edict_t *ent)
{
	if (!ent->client->pers.megahealth_time)
		return;
	else if (ent->health <= ent->max_health)
	{
		ent->client->pers.megahealth_time = 0_ms;
		return;
	}

	ent->client->pers.megahealth_time -= FRAME_TIME_S;

	if (ent->client->pers.megahealth_time <= 0_ms)
	{
		ent->health--;

		if (ent->health > ent->max_health)
			ent->client->pers.megahealth_time = 1000_ms;
		else
			ent->client->pers.megahealth_time = 0_ms;
	}
}

// [Paril-KEX] push all players' origins back to match their lag compensation
void G_LagCompensate(edict_t *from_player, const vec3_t &start, const vec3_t &dir)
{
	uint32_t current_frame = gi.ServerFrame();

	// if you need this to fight monsters, you need help
	if (!deathmatch->integer)
		return;
	else if (!g_lag_compensation->integer)
		return;
	// don't need this
	else if (from_player->client->cmd.server_frame >= current_frame ||
		(from_player->svflags & SVF_BOT))
		return;

	int32_t frame_delta = (current_frame - from_player->client->cmd.server_frame) + 1;

	for (auto player : active_players())
	{
		// we aren't gonna hit ourselves
		if (player == from_player)
			continue;

		// not enough data, spare them
		if (player->client->num_lag_origins < frame_delta)
			continue;

		// if they're way outside of cone of vision, they won't be captured in this
		if ((player->s.origin - start).normalized().dot(dir) < 0.75f)
			continue;

		int32_t lag_id = (player->client->next_lag_origin - 1) - (frame_delta - 1);

		if (lag_id < 0)
			lag_id = game.max_lag_origins + lag_id;

		if (lag_id < 0 || lag_id >= player->client->num_lag_origins)
		{
			gi.Com_Print("lag compensation error\n");
			G_UnLagCompensate();
			return;
		}

		const vec3_t &lag_origin = (game.lag_origins + ((player->s.number - 1) * game.max_lag_origins))[lag_id];

		// no way they'd be hit if they aren't in the PVS
		if (!gi.inPVS(lag_origin, start, false))
			continue;

		// only back up once
		if (!player->client->is_lag_compensated)
		{
			player->client->is_lag_compensated = true;
			player->client->lag_restore_origin = player->s.origin;
		}
			
		player->s.origin = lag_origin;

		gi.linkentity(player);
	}
}

// [Paril-KEX] pop everybody's lag compensation values
void G_UnLagCompensate()
{
	for (auto player : active_players())
	{
		if (player->client->is_lag_compensated)
		{
			player->client->is_lag_compensated = false;
			player->s.origin = player->client->lag_restore_origin;
			gi.linkentity(player);
		}
	}
}

// [Paril-KEX] save the current lag compensation value
static void G_SaveLagCompensation(edict_t *ent)
{
	(game.lag_origins + ((ent->s.number - 1) * game.max_lag_origins))[ent->client->next_lag_origin] = ent->s.origin;
	ent->client->next_lag_origin = (ent->client->next_lag_origin + 1) % game.max_lag_origins;

	if (ent->client->num_lag_origins < game.max_lag_origins)
		ent->client->num_lag_origins++;
}

/**
 * @brief End-of-frame processing for client entities
 * 
 * This function handles all per-frame processing for player entities including:
 * - Camera and view management
 * - Third-person mode handling
 * - Player model visibility
 * - Effect and sound updates
 * - Animation processing
 * 
 * Called once per frame for each connected client.
 * 
 * @param ent The player entity being processed
 */
void ClientEndServerFrame(edict_t *ent)
{
    // Skip processing for unspawned entities
    if (!ent->client->pers.spawned)
        return;

    // Set up frame processing context
    current_player = ent;
    current_client = ent->client;
    float bobtime, bobtime_run;

    // Check for special game states that bypass normal processing
    if (level.intermissiontime || ent->client->awaiting_respawn) {
        // Handle intermission states
        if (ent->client->awaiting_respawn || (level.intermission_eou || level.is_n64 || 
            (deathmatch->integer && level.intermissiontime))) {
            current_client->ps.screen_blend[3] = current_client->ps.damage_blend[3] = 0;
            current_client->ps.fov = 90;
            current_client->ps.gunindex = 0;
        }
        
        // Update stats even during intermission
        G_SetStats(ent);
        G_SetCoopStats(ent);

        // Update scoreboards if needed
        if (deathmatch->integer && ent->client->showscores && ent->client->menutime) {
            DeathmatchScoreboardMessage(ent, ent->enemy);
            gi.unicast(ent, false);
            ent->client->menutime = 0_ms;
        }

        return;
    }

    // Update fog effects
    P_ForceFogTransition(ent, false);

    // Update goal notification
    G_PlayerNotifyGoal(ent);

    // Run health effects
    P_RunMegaHealth(ent);

    // Synchronize physics state
    current_client->ps.pmove.origin = ent->s.origin;
    current_client->ps.pmove.velocity = ent->velocity;

    // ===== THIRD-PERSON MODE PROCESSING =====
    // Detect third-person mode and apply appropriate handling
    bool thirdPersonActive = (sv_thirdperson && sv_thirdperson->integer && 
                             ent->health > 0 && !ent->client->resp.spectator);
    
    // Apply special third-person processing if active
    if (thirdPersonActive) {
        // PHASE 1: Force model visibility and correct entity state
        ent->svflags &= ~SVF_NOCLIENT;  // Make player model visible
        ent->flags &= ~FL_NOVISIBLE;    // Clear invisibility flags
        ent->solid = SOLID_BBOX;        // Ensure proper collision
        ent->s.modelindex = MODELINDEX_PLAYER;        // Force correct model index
        
        // PHASE 2: Apply CTF effects if needed
        CTFApplyRegeneration(ent);
        
        // PHASE 3: Calculate view vectors for the frame
        AngleVectors(ent->client->v_angle, forward, right, up);
        
        // PHASE 4: Process world environmental effects
        P_WorldEffects();
        
        // PHASE 5: Set up third-person camera
        G_SetThirdPersonView(ent);
        
        // PHASE 6: Process essential visual effects 
        // (subset of normal processing appropriate for third-person)
        P_DamageFeedback(ent);
        SV_CalcViewOffset(ent);
        SV_CalcBlend(ent);
        G_SetStats(ent);
        G_CheckChaseStats(ent);
        G_SetCoopStats(ent);
        G_SetClientSound(ent);
        G_SetClientFrame(ent);
        
        // PHASE 7: Store state for next frame
        ent->client->oldvelocity = ent->velocity;
        ent->client->oldviewangles = ent->client->ps.viewangles;
        ent->client->oldgroundentity = ent->groundentity;
        
        // PHASE 8: Final updates for third-person mode
        P_AssignClientSkinnum(ent);
        
        if (deathmatch->integer)
            G_SaveLagCompensation(ent);
            
        Compass_Update(ent, false);
        
        // PHASE 9: Final visibility enforcement
        ent->svflags &= ~SVF_NOCLIENT;
        gi.linkentity(ent);
        
        // Handle player collision in coop mode
        if (coop->integer && G_ShouldPlayersCollide(false) && 
            !(ent->clipmask & CONTENTS_PLAYER) && ent->takedamage) {
            
            bool clipped_player = false;
            
            for (auto player : active_players()) {
                if (player == ent)
                    continue;
                
                trace_t clip = gi.clip(player, ent->s.origin, ent->mins, ent->maxs, 
                                      ent->s.origin, CONTENTS_MONSTER | CONTENTS_PLAYER);
                
                if (clip.startsolid || clip.allsolid) {
                    clipped_player = true;
                    break;
                }
            }
            
            // Safe to enable player collision
            if (!clipped_player)
                ent->clipmask |= CONTENTS_PLAYER;
        }
        
        // Skip standard processing for third-person mode
        return;
    }

    // ===== STANDARD FIRST-PERSON PROCESSING =====
    // Only executed for first-person mode

    // Apply team-specific regeneration effects
    CTFApplyRegeneration(ent);

    // Calculate view vectors
    AngleVectors(ent->client->v_angle, forward, right, up);

    // Process environmental effects
    P_WorldEffects();

    // Calculate model angles from view direction
    if (ent->client->v_angle[PITCH] > 180)
        ent->s.angles[PITCH] = (-360 + ent->client->v_angle[PITCH]) / 3;
    else
        ent->s.angles[PITCH] = ent->client->v_angle[PITCH] / 3;
    
    ent->s.angles[YAW] = ent->client->v_angle[YAW];
    ent->s.angles[ROLL] = 0;
    ent->s.angles[ROLL] = -SV_CalcRoll(ent->s.angles, ent->velocity) * 4;

    // Calculate movement parameters
    xyspeed = sqrt(ent->velocity[0] * ent->velocity[0] + ent->velocity[1] * ent->velocity[1]);

    // Handle bobbing calculation
    if (xyspeed < 5) {
        bobmove = 0;
        current_client->bobtime = 0;
    } else if (ent->groundentity) {
        if (xyspeed > 210)
            bobmove = gi.frame_time_ms / 400.f;
        else if (xyspeed > 100)
            bobmove = gi.frame_time_ms / 800.f;
        else
            bobmove = gi.frame_time_ms / 1600.f;
    }

    // Calculate bob timing
    bobtime = (current_client->bobtime += bobmove);
    bobtime_run = bobtime;

    if ((current_client->ps.pmove.pm_flags & PMF_DUCKED) && ent->groundentity)
        bobtime *= 4;

    bobcycle = (int) bobtime;
    bobcycle_run = (int) bobtime_run;
    bobfracsin = fabsf(sinf(bobtime * PIf));

    // Process standard player visual effects
    P_DamageFeedback(ent);
    SV_CalcViewOffset(ent);
    SV_CalcGunOffset(ent);
    SV_CalcBlend(ent);

    // Handle spectator and stats
    if (ent->client->resp.spectator)
        G_SetSpectatorStats(ent);
    else
        G_SetStats(ent);

    G_CheckChaseStats(ent);
    G_SetCoopStats(ent);

    // Process visual and audio events
    G_SetClientEvent(ent);
    G_SetClientEffects(ent);
    G_SetClientSound(ent);
    G_SetClientFrame(ent);

    // Store state for next frame
    ent->client->oldvelocity = ent->velocity;
    ent->client->oldviewangles = ent->client->ps.viewangles;
    ent->client->oldgroundentity = ent->groundentity;

    // Handle menu updates
    if (ent->client->menudirty && ent->client->menutime <= level.time) {
        if (ent->client->menu) {
            PMenu_Do_Update(ent);
            gi.unicast(ent, true);
        }
        ent->client->menutime = level.time;
        ent->client->menudirty = false;
    }

    // Update scoreboards
    if (ent->client->showscores && ent->client->menutime <= level.time) {
        if (ent->client->menu) {
            PMenu_Do_Update(ent);
            ent->client->menudirty = false;
        } else {
            DeathmatchScoreboardMessage(ent, ent->enemy);
        }
        gi.unicast(ent, false);
        ent->client->menutime = level.time + 3_sec;
    }

    // Handle bot processing
    if ((ent->svflags & SVF_BOT) != 0) {
        Bot_EndFrame(ent);
    }

    // Update visual appearance
    P_AssignClientSkinnum(ent);

    // Handle network lag compensation
    if (deathmatch->integer)
        G_SaveLagCompensation(ent);

    // Update compass
    Compass_Update(ent, false);

    // Handle player collision in coop mode
    if (coop->integer && G_ShouldPlayersCollide(false) && 
        !(ent->clipmask & CONTENTS_PLAYER) && ent->takedamage) {
        
        bool clipped_player = false;
        
        for (auto player : active_players()) {
            if (player == ent)
                continue;
            
            trace_t clip = gi.clip(player, ent->s.origin, ent->mins, ent->maxs, 
                                  ent->s.origin, CONTENTS_MONSTER | CONTENTS_PLAYER);
            
            if (clip.startsolid || clip.allsolid) {
                clipped_player = true;
                break;
            }
        }
        
        // Safe to enable player collision
        if (!clipped_player)
            ent->clipmask |= CONTENTS_PLAYER;
    }
}