/**
  * Touhou Community Reliant Automatic Patcher
  * DLL component
  *
  * ----
  *
  * Stuff related to text rendering.
  */

/**
  * This module provides three ways to replace the fonts used by a game.
  *
  * ----
  *
  * The first, and original, two methods operate on the face name string
  * pointer passed to CreateFont(). They replace the pointer by
  * 1) first looking up a hardcoded string translation for the face name,
  * 2) then falling back to the "font" string in the run configuration, if
  *    given.
  * Note that these replacements are not applied for calls to the
  * CreateFontIndirect*() functions, as they use a LOGFONT structure which
  * contains the face name in a local char array. Therefore, they mainly
  * continue to be provided for backwards compatibility with old patch
  * configurations.
  *
  * ----
  *
  * The third and most flexible method is a rule system for matching both the
  * original face name and some of the other members of the LOGFONT structure
  * against a set of replacement rules. This allows a greater degree of
  * customization in font configurations, especially for games that use more
  * than one font.
  *
  * These rules are defined using a JSON object named "fontrules" in the run
  * configuration. A comprehensive example:
  * {
  *		"fontrules": {
  *			"'ＭＳ ゴシック'": "'Linux Biolinum O'",
  *			"'ＭＳ ゴシック' 32": "* * 14 600",
  *			"'ＭＳ 明朝'": "'Linux Libertine O'",
  *			"'メイリオ' ": "'Linux Biolinum O'",
  *			"'メイリオ' 36": "* 24",
  *			"'メイリオ' 42": "* 28",
  *			"'メイリオ' 48": "* 32 14 600",
  *			"'メイリオ' 60": "* 40"
  *		}
  * }
  * This replaces the original Japanese typeface pair on Windows, MS Gothic
  * and MS Mincho, with the typeface pair from the Libertine project, Linux
  * Biolinum and Linux Libertine. Additionally, if the game requested MS
  * Gothic in height 32, the resulting font will also be compressed to a width
  * of 14 and will have a semibold (600 == FW_SEMIBOLD) weight. The generic
  * 'MS Gothic' → 'Linux Biolinum O' rule is carried over, so this case would
  * effectively result in creating a font using 'Linux Biolinum O' with a
  * height of 32, a width of 14 and a weight of 600.
  * Finally, a set of rules for Meiryo, the new default Japanese sans-serif
  * font introduced in Windows Vista, is provided. The game in question uses
  * this font in favor of MS Gothic if it's available on the user's system.
  * However, the game also makes up for Meiryo's smaller character size by
  * increasing the height of each font by a third. The font rules establish
  * compatibility to the MS Gothic rules by undoing the increases for the
  * height values of 36, 42, 48 and 60, and replicating the custom setting for
  * MS Gothic with height 32. In the end, all text will appear identical,
  * regardless of whether the game chooses MS Gothic or Meiryo.
  *
  * ----
  *
  * Due to the nature of Jansson's JSON object implementation, all rules are
  * applied to the original font in random order. However, closer matches are
  * ensured to always take priority over more generic ones.
  *
  * A common string representation of the LOGFONT members is used for both
  * match statements (the JSON keys) and overrides (the JSON values), as well
  * as for logging both the original and final state of the LOGFONT structure
  * eventually passed to CreateFontIndirectEx().
  * The currently supported font parameters are, in order:
  *
  *			"'Linux Biolinum O'  32   14  600 DEFAULT_QUALITY"
  *			                1)   2)   3)   4)              5)
  *
  * 1) Face name (string) or the font's PitchAndFamily value (int).
  *    The latter can be used as an option for games that don't use font
  *    names and instead specify the look of a font via the PitchAndFamily
  *    member of the LOGFONT structure.
  * 2) Height (int)
  * 3) Width (int)
  * 4) Weight (int)
  * 5) Quality (enum). Case-sensitive, can be one of the following,
  *    • DEFAULT_QUALITY
  *    • DRAFT_QUALITY
  *    • PROOF_QUALITY
  *    • NONANTIALIASED_QUALITY
  *    • ANTIALIASED_QUALITY
  *    • CLEARTYPE_QUALITY
  *    • CLEARTYPE_NATURAL_QUALITY
  *
  * The basic rules of the syntax:
  *
  * • All parameters are separated by spaces.
  * • Strings have to be wrapped in single quotes ('') to be recognized as
  *   such.
  * • Parameters with a value of 0 - or, in the case of integers, any non-
  *   digit character - are ignored. In match statements, ignored parameters
  *   match any original value; in overrides, they carry over the respective
  *   parameter unchanged.
  * • The list can stop at any parameter, which will ignore all further ones.
  * • As a convention, a single '*' should be used to skip certain parameters
  *   you're not interested in matching or overriding.
  *
  * Note that rule replacements are applied after the face string replacements
  * done by the original two methods. This means that any face name matches
  * would also have to take these modified face names into account, and
  * possibly provide additional matching rules.
  */

#pragma once

void patch_fonts_load(const json_t *patch_info);

// Parses a font rule string according to the syntax above, fills [lf] with
// the parameters given, and returns the amount of parameters (the "score")
// contained in [str].
THCRAP_API int fontrule_parse(LOGFONTA *lf, const char *str);

void textdisp_mod_init(void);
void textdisp_mod_detour(void);
