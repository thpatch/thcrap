/**
  * Touhou Community Reliant Automatic Patcher
  * Tasogare Frontier support plugin
  *
  * ----
  *
  * VFS generators for music names and comments
  */

#pragma once

#include <thcrap.h>
#include <jansson.h>
#include <vfs.h>

json_t* bgm_generator(const jsonvfs_map& in_data, std::string_view out_fn, size_t& out_size);
