/**
  * Touhou Community Reliant Automatic Patcher
  * Tasogare Frontier support plugin
  *
  * ----
  *
  * Ruby text patching utilities
  */

#include <thcrap.h>
#include "mediawiki.h"

std::string parse_mediawiki(const std::string& in, const MwDefinition& mwsyntax)
{
	std::string out;

	for (size_t i = 0; i < in.size(); ) {
		if (in.compare(i, 7, "{{ruby|") == 0) {
			i += 7;

			size_t middle = in.find('|', i);
			size_t end    = in.find("}}", middle);
			auto top = std::string_view(in.data() + middle + 1, end - middle - 1);
			auto bot = std::string_view(in.data() + i, middle - i);

			out += mwsyntax.ruby.tagBegin;
			out += mwsyntax.ruby.order == MwDefinition::Ruby::Order::TopThenBottom ? top : bot;
			out += mwsyntax.ruby.tagMiddle;
			out += mwsyntax.ruby.order == MwDefinition::Ruby::Order::TopThenBottom ? bot : top;
			out += mwsyntax.ruby.tagEnd;

			i = end + 2;
		}
		else if (in.compare(i, 9, "{{tlnote|") == 0) {
			// tlnote isn't supported, ignore it
			i = in.find("}}", i) + 2;
		}
		else {
			out += in[i];
			i++;
		}
	}

	return out;
}
