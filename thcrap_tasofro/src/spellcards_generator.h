/**
  * Touhou Community Reliant Automatic Patcher
  * Tasogare Frontier support plugin
  *
  * ----
  *
  * VFS generators for spellcard files
  */

#pragma once

#include <vfs.h>

json_t* spell_story_generator(std::unordered_map<std::string_view, json_t*>& in_data, std::string_view out_fn, size_t& out_size);
json_t* spell_player_generator(std::unordered_map<std::string_view, json_t*>& in_data, std::string_view out_fn, size_t& out_size);
json_t* spell_char_select_generator(std::unordered_map<std::string_view, json_t*>& in_data, std::string_view out_fn, size_t& out_size);
