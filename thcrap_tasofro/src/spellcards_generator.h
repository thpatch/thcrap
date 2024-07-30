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

json_t* spell_story_generator(const jsonvfs_map& in_data, std::string_view out_fn, size_t& out_size);
json_t* spell_player_generator(const jsonvfs_map& in_data, std::string_view out_fn, size_t& out_size);
json_t* spell_char_select_generator(const jsonvfs_map& in_data, std::string_view out_fn, size_t& out_size);
