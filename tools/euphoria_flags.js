// The euphoria mod's + commands (mods/euphoria/README.md), in ONE place: tools/mod-launcher.hta loads this file (<script src>).
// Plain ES3 (the HTA's JScript and node both run it). euphoriaFlags(level, a): level 0 = off (a unchanged), 0..1 = how drunk
// the zombies are (euphoria_level); a = the command line built so far (with or without the horde / nightmare flags).
function euphoriaFlags(level, a) {
  var l = parseFloat(level);
  if (isNaN(l) || l <= 0) return a;
  if (l > 1) l = 1;
  a = a || "";
  // fs_mods: euphoria after horde / nightmare (their hooks first), before zinfo/noperks; fs_game keeps the first mod's
  if (/\+set fs_mods horde nightmare /.test(a)) a = a.replace("+set fs_mods horde nightmare ", "+set fs_mods horde nightmare euphoria ");
  else if (/\+set fs_mods horde /.test(a)) a = a.replace("+set fs_mods horde ", "+set fs_mods horde euphoria ");
  else if (/\+set fs_mods nightmare /.test(a)) a = a.replace("+set fs_mods nightmare ", "+set fs_mods nightmare euphoria ");
  else if (/\+set fs_mods nightmare$/.test(a)) a += " euphoria";
  else if (/\+set fs_mods /.test(a)) a = a.replace("+set fs_mods ", "+set fs_mods euphoria ");
  else a += " +set fs_mods euphoria";
  if (!/\+set fs_game /.test(a)) a += " +set fs_game mods/euphoria";
  a += " +set euphoria_level " + l;
  return a.replace(/^ /, "");
}
if (typeof module != "undefined") module.exports = euphoriaFlags;
