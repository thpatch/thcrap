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

json_t* bgm_generator(std::unordered_map<std::string_view, json_t*>& in_data, std::string_view out_fn, size_t& out_size);
