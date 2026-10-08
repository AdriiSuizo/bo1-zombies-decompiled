// The euphoria mod's + commands (mods/euphoria/README.md), in ONE place: tools/mod-launcher.hta loads this file (<script src>).
// Plain ES3 (the HTA's JScript and node both run it). euphoriaFlags(mode, a): mode is the launcher's option:
//   "0" off (a unchanged); "p" active-ragdoll physics; "p0.4" / "p0.75" / "p1" physics plus the procedural drunk weave at
//   that level; "d0.4" / "d0.75" / "d1" the drunk weave only (the mod's first version, an approximation).
// a = the command line built so far (with or without the horde / nightmare flags).
function euphoriaFlags(mode, a) {
  var m = String(mode || "0");
  if (m == "0" || m == "") return a;
  var physics = m.charAt(0) == "p" ? 1 : 0;
  var drunk = parseFloat(m.substring(1));
  if (isNaN(drunk) || drunk < 0) drunk = 0;
  if (drunk > 1) drunk = 1;
  a = a || "";
  // fs_mods: euphoria after horde / nightmare (their hooks first), before zinfo/noperks; fs_game keeps the first mod's
  if (/\+set fs_mods horde nightmare /.test(a)) a = a.replace("+set fs_mods horde nightmare ", "+set fs_mods horde nightmare euphoria ");
  else if (/\+set fs_mods horde /.test(a)) a = a.replace("+set fs_mods horde ", "+set fs_mods horde euphoria ");
  else if (/\+set fs_mods nightmare /.test(a)) a = a.replace("+set fs_mods nightmare ", "+set fs_mods nightmare euphoria ");
  else if (/\+set fs_mods nightmare$/.test(a)) a += " euphoria";
  else if (/\+set fs_mods /.test(a)) a = a.replace("+set fs_mods ", "+set fs_mods euphoria ");
  else a += " +set fs_mods euphoria";
  if (!/\+set fs_game /.test(a)) a += " +set fs_game mods/euphoria";
  a += " +set euphoria_physics " + physics + " +set euphoria_level " + drunk;
  return a.replace(/^ /, "");
}
if (typeof module != "undefined") module.exports = euphoriaFlags;
