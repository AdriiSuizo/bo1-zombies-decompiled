#include maps\_utility;
#include common_scripts\utility;
#include maps\_zombiemode_utility;

// EUPHORIA MOD - not part of the original game (mods/euphoria/README.md). Zombies keep their own walk / run / attack
// animations and get a physical body on top: an active ragdoll (14 segments, joints, motors that track the animation, a
// balance controller) that reacts to every bullet, stumbles, tries to stay up and falls when it cannot
// (engine: src/euphoria, src/game_sp/actor_sp_euphoria.cpp, src/cgame_mp/cg_euphoria.cpp). This script only switches
// it on per zombie through the actor field self.euphoria (0 = retail, 1 = on; the value also scales the hit impulses)
// and keeps the older procedural "drunk" layer (self.drunk, src/game_sp/actor_sp_stagger.cpp) as an option.
//
// Dvars (set on the command line, +set):
//   euphoria_physics   0..2, default 1: the active-ragdoll body and its hit reactions (0 = off)
//   euphoria_level     0..1, default 0: the procedural drunk weave / sway on top (the first version of the mod; it is
//                      an approximation, kept for the GTA IV "drunk companion" look, off by default)
//   euphoria_variance  default 0.25: each zombie's amounts are value * (1 +- variance)
//   euphoria_specials  0/1, default 0: 1 also applies to the specials (napalm, shrieker, George, astronaut, thief, dogs)
//   euphoria_selftest  1: for the headless checks: hurts zombies itself (DoDamage from in front of them, their health
//                      raised first so nothing dies) and logs "euphoria: ..." lines in games_mp.log: PASS when a
//                      stumble, a fall and a get-up were seen, FAIL after 150 s without.
// Crawlers (has_legs false) switch everything off. Notifies from the engine on the zombie: "euphoria_stumble",
// "euphoria_fall", "euphoria_getup". Read-only fields: self.euphoriastate (0 standing 1 stumbling 2 fallen 3 getting
// up), self.euphoriafalls, self.drunkstumbles.

main_start()
{
	if ( IsDefined( level.euphoria_started ) )
		return;
	level.euphoria_started = true;
	euphoria_default( "euphoria_physics", 1 );
	euphoria_default( "euphoria_level", 0 );
	euphoria_default( "euphoria_variance", 0.25 );
	euphoria_default( "euphoria_specials", 0 );
	level.euphoria_physics = GetDvarFloat( #"euphoria_physics" );
	if ( level.euphoria_physics > 2 )
		level.euphoria_physics = 2;
	level.euphoria_level = GetDvarFloat( #"euphoria_level" );
	if ( level.euphoria_level > 1 )
		level.euphoria_level = 1;
	if ( level.euphoria_physics <= 0 && level.euphoria_level <= 0 )
	{
		LogPrint( "euphoria: off (euphoria_physics 0, euphoria_level 0)\n" );
		return;
	}
	LogPrint( "euphoria: physics " + level.euphoria_physics + " drunk " + level.euphoria_level + " variance " + GetDvarFloat( #"euphoria_variance" ) + " specials " + GetDvarInt( #"euphoria_specials" ) + " engine " + GetDvarInt( #"bo1_mod_euphoria" ) + "/" + GetDvarInt( #"bo1_mod_stagger" ) + "\n" );
	level thread euphoria_watch();
	if ( GetDvarInt( #"euphoria_selftest" ) == 1 )
		level thread euphoria_selftest();
}


euphoria_default( name, value )
{
	if ( GetDvar( name ) == "" )
		SetDvar( name, "" + value );
}


// every zombie alive gets its amounts once (retail spawns them one by one through _zombiemode_spawner; the specials
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
	scale = 1 + RandomFloatRange( -1, 1 ) * variance;
	physics = level.euphoria_physics * scale;
	if ( physics > 2 )
		physics = 2;
	if ( level.euphoria_physics > 0 && physics < 0.2 )
		physics = 0.2;
	drunk = level.euphoria_level * scale;
	if ( drunk > 1 )
		drunk = 1;
	if ( level.euphoria_level > 0 && drunk < 0.05 )
		drunk = 0.05;
	self.euphoria = physics;
	self.drunk = drunk;
	self thread euphoria_log_events();
	// a crawler cannot stagger: everything off when the legs go
	for ( ;; )
	{
		wait 0.5;
		if ( IsDefined( self.has_legs ) && !self.has_legs )
		{
			self.euphoria = 0;
			self.drunk = 0;
			return;
		}
	}
}


// the engine's notifies, counted for the self-test
euphoria_log_events()
{
	self endon( "death" );
	if ( !IsDefined( level.euphoria_events ) )
	{
		level.euphoria_events = [];
		level.euphoria_events["euphoria_stumble"] = 0;
		level.euphoria_events["euphoria_fall"] = 0;
		level.euphoria_events["euphoria_getup"] = 0;
	}
	self thread euphoria_count_event( "euphoria_stumble" );
	self thread euphoria_count_event( "euphoria_fall" );
	self thread euphoria_count_event( "euphoria_getup" );
}


euphoria_count_event( name )
{
	self endon( "death" );
	for ( ;; )
	{
		self waittill( name );
		level.euphoria_events[name]++;
		if ( GetDvarInt( #"euphoria_selftest" ) == 1 )
			LogPrint( "euphoria: event " + name + " ent " + self GetEntityNumber() + " at " + ( GetTime() - level.euphoria_selftest_start ) + " ms state " + self.euphoriastate + " falls " + self.euphoriafalls + "\n" );
	}
}


// headless check: hurt zombies from in front of them (through the real damage path, so Actor_Pain and the balance
// model see a real hit) and watch the engine's state and notifies
euphoria_selftest()
{
	level.euphoria_selftest_start = GetTime();
	level thread euphoria_selftest_hitter();
	for ( ;; )
	{
		wait 5;
		zombies = GetAiSpeciesArray( "axis", "all" );
		on = 0;
		falls = 0;
		down = 0;
		max_roll = 0;
		max_pitch = 0;
		for ( i = 0; i < zombies.size; i++ )
		{
			z = zombies[i];
			if ( !IsDefined( z ) || !IsAlive( z ) || !IsDefined( z.euphoria ) || z.euphoria <= 0 )
				continue;
			on++;
			falls += z.euphoriafalls;
			if ( z.euphoriastate == 2 || z.euphoriastate == 3 )
				down++;
			roll = abs( AngleClamp180( z.angles[2] ) );
			pitch = abs( AngleClamp180( z.angles[0] ) );
			if ( roll > max_roll )
				max_roll = roll;
			if ( pitch > max_pitch )
				max_pitch = pitch;
		}
		ev = level.euphoria_events;
		stumbles = 0;
		fallev = 0;
		getups = 0;
		if ( IsDefined( ev ) )
		{
			stumbles = ev["euphoria_stumble"];
			fallev = ev["euphoria_fall"];
			getups = ev["euphoria_getup"];
		}
		LogPrint( "euphoria: t " + ( GetTime() - level.euphoria_selftest_start ) + " ms alive " + zombies.size + " on " + on + " down " + down + " falls " + falls + " events stumble " + stumbles + " fall " + fallev + " getup " + getups + " max pitch " + max_pitch + " max roll " + max_roll + "\n" );
		if ( stumbles > 0 && fallev > 0 && getups > 0 )
		{
			LogPrint( "euphoria PASS stumbles " + stumbles + " falls " + fallev + " getups " + getups + " after " + ( GetTime() - level.euphoria_selftest_start ) + " ms\n" );
			return;
		}
		if ( GetTime() - level.euphoria_selftest_start > 150000 )
		{
			LogPrint( "euphoria FAIL in 150 s: stumbles " + stumbles + " falls " + fallev + " getups " + getups + " (on " + on + ")\n" );
			return;
		}
	}
}


// three light hits in a row (a stumble that adds up), then a heavy one (a fall), on one zombie at a time
euphoria_selftest_hitter()
{
	for ( ;; )
	{
		wait 3;
		zombies = GetAiSpeciesArray( "axis", "all" );
		target = undefined;
		for ( i = 0; i < zombies.size; i++ )
		{
			z = zombies[i];
			if ( IsDefined( z ) && IsAlive( z ) && IsDefined( z.euphoria ) && z.euphoria > 0 && z.euphoriastate == 0 && is_true( z.completed_emerging_into_playable_area ) )
			{
				target = z;
				break;
			}
		}
		if ( !IsDefined( target ) )
			continue;
		target.health = 50000; // the hits must not kill it (self-test only)
		target.maxhealth = 50000;
		source = target.origin + AnglesToForward( target.angles ) * 60 + ( 0, 0, 40 );
		for ( h = 0; h < 3; h++ )
		{
			if ( !IsAlive( target ) )
				break;
			target DoDamage( 40, source );
			LogPrint( "euphoria: selftest light hit " + h + " ent " + target GetEntityNumber() + " state " + target.euphoriastate + "\n" );
			wait 0.15;
		}
		wait 2.5;
		if ( IsAlive( target ) )
		{
			source = target.origin + AnglesToForward( target.angles ) * 60 + ( 0, 0, 40 );
			target DoDamage( 150, source );
			LogPrint( "euphoria: selftest heavy hit ent " + target GetEntityNumber() + " state " + target.euphoriastate + " falls " + target.euphoriafalls + "\n" );
		}
		wait 4;
	}
}
