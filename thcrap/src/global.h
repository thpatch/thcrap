/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Globals and compile-time constants.
  */

#pragma once

// We only want to read from or write to one file at a time
extern CRITICAL_SECTION cs_file_access;

extern json_t *run_cfg;

// Project stats
const char* PROJECT_NAME();
const char* PROJECT_NAME_SHORT();
const DWORD PROJECT_VERSION();
const char* PROJECT_VERSION_STRING();

json_t* runconfig_get();
void runconfig_set(json_t *new_run_cfg);
