// EUPHORIA MOD - not part of the original game. Loaded with +set fs_mods euphoria (stacks with horde / nightmare / zinfo:
// "horde euphoria zinfo"), +set euphoria_level 0..1 (tools/euphoria_flags.js). Registered in the fs_mods base hooks
// (mods/_stack/maps/_modstack.gsc). The stagger itself is in the engine (src/game_sp/actor_sp_stagger.cpp): this script
// only sets each zombie's self.drunk.

register()
{
	// Five's own "fix for the day" hook (start of maps\zombie_pentagon::main())
	maps\_modstack::add( "zombie_pentagon_ffotd::main_start", maps\euphoria\_euphoria::main_start );
	// every map (Five included): the shared hook at the start of maps\_zombiemode::main(); main_start runs once
	maps\_modstack::add( "_zombiemode_ffotd::main_start", maps\euphoria\_euphoria::main_start );
}
