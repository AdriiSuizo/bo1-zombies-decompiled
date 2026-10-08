#include maps\_utility;
#include common_scripts\utility;
#include maps\_zombiemode_utility;

// EUPHORIA MOD - not part of the original game (mods/euphoria/README.md). Zombies walk and run with their own animations
// but stagger like the drunk companions of GTA IV: they weave off the path, lurch sideways, lean into it and catch
// themselves, and now and then nearly fall. The movement is procedural and lives in the engine
// (src/game_sp/actor_sp_stagger.cpp, bo1_mod_stagger* dvars): it bends the direction and the stride the run animation
// produces and tilts the body (pitch and roll of the entity, retail keeps them at 0). This script gives every zombie its
// amount through the actor field self.drunk (0 = retail, 1 = full; read-only self.drunkstumbles counts its stumbles).
//
// Dvars (set on the command line, +set):
//   euphoria_level     0..1, default 0.75: how drunk the zombies are (0 = the mod only adds files)
//   euphoria_variance  default 0.25: each zombie's amount is level * (1 +- variance), so a crowd staggers out of step
//   euphoria_specials  0/1, default 0: 1 also staggers the specials (napalm, shrieker, George, astronaut, thief, dogs)
//   euphoria_selftest  1: for the headless checks: every 5 s one "euphoria: ..." line in games_mp.log with the drunk
//                      zombies alive, the largest body roll seen and the stumbles so far; "euphoria PASS" once a zombie
//                      has stumbled and leaned, "euphoria FAIL" after 180 s without.
// Crawlers (has_legs false) sober up: nothing to stumble on. A zombie in a scripted animation (window climb, attack,
// traversal) is left alone by the engine, so doors, windows and grabs still line up.

main_start()
{
	if ( IsDefined( level.euphoria_started ) )
		return;
	level.euphoria_started = true;
	euphoria_default( "euphoria_level", 0.75 );
	euphoria_default( "euphoria_variance", 0.25 );
	euphoria_default( "euphoria_specials", 0 );
	level.euphoria_level = GetDvarFloat( #"euphoria_level" );
	if ( level.euphoria_level > 1 )
		level.euphoria_level = 1;
	if ( level.euphoria_level <= 0 )
	{
		LogPrint( "euphoria: off (euphoria_level 0)\n" );
		return;
	}
	LogPrint( "euphoria: level " + level.euphoria_level + " variance " + GetDvarFloat( #"euphoria_variance" ) + " specials " + GetDvarInt( #"euphoria_specials" ) + " engine " + GetDvarInt( #"bo1_mod_stagger" ) + "\n" );
	level thread euphoria_watch();
	if ( GetDvarInt( #"euphoria_selftest" ) == 1 )
		level thread euphoria_selftest();
}


euphoria_default( name, value )
{
	if ( GetDvar( name ) == "" )
		SetDvar( name, "" + value );
}


// every zombie alive gets its amount once (retail spawns them one by one through _zombiemode_spawner; the specials
// and the mods' enemies come through the same AI list)
euphoria_watch()
{
	for ( ;; )
	{
		zombies = GetAiSpeciesArray( "axis", "all" );
		for ( i = 0; i < zombies.size; i++ )
		{
			z = zombies[i];
			if ( !IsDefined( z ) || !IsAlive( z ) || IsDefined( z.euphoria_seen ) )
				continue;
			z.euphoria_seen = true;
			z thread euphoria_zombie();
		}
		wait 0.25;
	}
}


euphoria_zombie()
{
	self endon( "death" );
	// a special has no animname in its first frames (mods/_stack/maps/_modenemy.gsc): decide after it is set up
	wait 0.5;
	if ( !GetDvarInt( #"euphoria_specials" ) )
	{
		if ( IsDefined( self.animname ) && self.animname != "zombie" )
			return;
		if ( is_true( self.isdog ) )
			return;
	}
	variance = GetDvarFloat( #"euphoria_variance" );
	amount = level.euphoria_level * ( 1 + RandomFloatRange( -1, 1 ) * variance );
	if ( amount > 1 )
		amount = 1;
	if ( amount < 0.05 )
		amount = 0.05;
	self.drunk = amount;
	// a crawler cannot stagger: it sobers up when the legs go
	for ( ;; )
	{
		wait 0.5;
		if ( IsDefined( self.has_legs ) && !self.has_legs )
		{
			self.drunk = 0;
			return;
		}
	}
}


euphoria_selftest()
{
	start = GetTime();
	for ( ;; )
	{
		wait 5;
		zombies = GetAiSpeciesArray( "axis", "all" );
		drunk = 0;
		max_roll = 0;
		max_pitch = 0;
		stumbles = 0;
		for ( i = 0; i < zombies.size; i++ )
		{
			z = zombies[i];
			if ( !IsDefined( z ) || !IsAlive( z ) || !IsDefined( z.drunk ) || z.drunk <= 0 )
				continue;
			drunk++;
			roll = abs( AngleClamp180( z.angles[2] ) );
			pitch = abs( AngleClamp180( z.angles[0] ) );
			if ( roll > max_roll )
				max_roll = roll;
			if ( pitch > max_pitch )
				max_pitch = pitch;
			stumbles += z.drunkstumbles;
		}
		LogPrint( "euphoria: t " + ( GetTime() - start ) + " ms alive " + zombies.size + " drunk " + drunk + " max roll " + max_roll + " max pitch " + max_pitch + " stumbles " + stumbles + "\n" );
		if ( stumbles > 0 && max_roll > 2 )
		{
			LogPrint( "euphoria PASS stumbles " + stumbles + " max roll " + max_roll + " after " + ( GetTime() - start ) + " ms\n" );
			return;
		}
		if ( GetTime() - start > 180000 )
		{
			LogPrint( "euphoria FAIL no stumble with a lean in 180 s (drunk " + drunk + " stumbles " + stumbles + " max roll " + max_roll + ")\n" );
			return;
		}
	}
}
