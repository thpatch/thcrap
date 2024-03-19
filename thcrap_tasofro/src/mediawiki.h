/**
  * Touhou Community Reliant Automatic Patcher
  * Tasogare Frontier support plugin
  *
  * ----
  *
  * Mediawiki syntax support
  */

#pragma once

#ifdef __cplusplus

#include <string>

struct MwDefinition
{
	struct Ruby
	{
		enum class Order
		{
			TopThenBottom,
			BottomThenTop,
		};
		const char *tagBegin;
		const char *tagMiddle;
		const char *tagEnd;
		Order order;
	};

	Ruby ruby;
};

extern const MwDefinition mwdef_nsml;
extern const MwDefinition mwdef_th135;
extern const MwDefinition mwdef_th175_stage_title;

std::string parse_mediawiki(const std::string& in, const MwDefinition& mwsyntax);

#endif
