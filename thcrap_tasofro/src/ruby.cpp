/**
  * Touhou Community Reliant Automatic Patcher
  * Tasogare Frontier support plugin
  *
  * ----
  *
  * Ruby text patching utilities
  */

#include <thcrap.h>
#include "thcrap_tasofro.h"

std::string parse_ruby(const std::string& in, const char *out_ruby_tag)
{
	std::string out;

	for (size_t i = 0; i < in.size(); i++) {
		if (in.compare(i, 7, "{{ruby|") == 0) {
			i += 7;
			out += out_ruby_tag;
			while (i < in.size() && in.compare(i, 2, "}}") != 0) {
				out += in[i];
				i++;
			}
			out += ']';
			i++; // Actually i += 2, because it will be incremented once again in the for loop.
		}
		else {
			out += in[i];
		}
	}

	return out;
}
