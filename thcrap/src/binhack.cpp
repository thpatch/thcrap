/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Binary hack handling.
  */

#include "thcrap.h"
#include <math.h>
#include <locale.h>
#include <algorithm>

/*
 * Grumble, grumble, C is garbage and will only do string→float conversion
 * using the decimal separator from the current locale, and OF COURSE it never
 * occured to anyone, not even Microsoft, to provide a neutral strtod() that
 * always looks for a decimal point, and so we have to dynamically allocate
 * (and free) The Neutral Locale instead. C is garbage.
 */
struct LocaleHolder_t {
	_locale_t locale = nullptr;

	LocaleHolder_t(int category, const char* locale_str) {
		locale = _create_locale(category, locale_str);
	}

	~LocaleHolder_t() {
		if (locale) {
			_free_locale(locale);
			locale = nullptr;
		}
	}
};

static LocaleHolder_t lc_neutral(LC_NUMERIC, "C");

void hackpoints_error_function_not_found(const char *func_name)
{
	if (runconfig_msgbox_invalid_func()) {
		if (log_mboxf("Hackpoint error", MB_OKCANCEL | MB_ICONERROR, "ERROR: function '%s' not found! ", func_name) == IDCANCEL) {
			thcrap_ExitProcess(0);
		}
	} else {
		log_printf("ERROR: function '%s' not found! "
#ifdef _DEBUG
			"(implementation not exported or still missing?)"
#else
			"(outdated or corrupt %s installation, maybe?)"
#endif
			, func_name, PROJECT_NAME_SHORT
		);
	}
}

TH_CALLER_DELETEA hackpoint_addr_t* hackpoint_addrs_from_json(json_t* addr_array)
{

	if (!addr_array) {
		return NULL;
	}

	size_t addr_count = 0;

	json_t *it;
	json_flex_array_foreach_scoped(size_t, i, addr_array, it) {
		if (json_is_integer(it) || json_is_string(it)) {
			++addr_count;
		}
	}
	if (!addr_count) {
		return NULL;
	}

	hackpoint_addr_t* ret = (hackpoint_addr_t*)malloc(sizeof(hackpoint_addr_t) * (addr_count + 1));
	ret[addr_count].str = NULL;
	ret[addr_count].raw = 0;
	ret[addr_count].type = END_ADDR;
	ret[addr_count].binhack_source = NULL;

	addr_count = 0;

	json_flex_array_foreach_scoped(size_t, i, addr_array, it) {
		if (json_is_string(it)) {
			ret[addr_count].str = json_string_copy(it);
			ret[addr_count].raw = 0;
			ret[addr_count].type = STR_ADDR;
			ret[addr_count].binhack_source = NULL;
			++addr_count;
		}
		else if (json_is_integer(it)) {
			ret[addr_count].str = NULL;
			ret[addr_count].raw = (uintptr_t)json_integer_value(it);
			ret[addr_count].type = RAW_ADDR;
			ret[addr_count].binhack_source = NULL;
			++addr_count;
		}
	}

	return ret;
}

inline bool eval_hackpoint_addr(hackpoint_addr_t* hackpoint_addr, uintptr_t* out, HMODULE hMod)
{
	if (hackpoint_addr) {
		uintptr_t addr = 0;
		switch (int8_t addr_type = hackpoint_addr->type) {
			case STR_ADDR:
				eval_expr(hackpoint_addr->str, '\0', &addr, NULL, NULL, hMod);
				hackpoint_addr->raw = addr;
				[[fallthrough]];
			case RAW_ADDR:
				addr = hackpoint_addr->raw;
				if (!addr) {
					hackpoint_addr->type = INVALID_ADDR;
				}
				[[fallthrough]];
			case INVALID_ADDR:
				*out = addr;
				return true;
			default:; //case END_ADDR:
		}
	}
	return false;
}

// Returns NULL only if parsing should be aborted.
// Declared noinline since float values aren't used
// frequently and otherwise binhack_calc_size/binhack_render
// waste a whole register storing the address of errno
static TH_NOINLINE const char* consume_float_value(const char *const expr, patch_val_t *const val)
{
	char* expr_next;
	errno = 0;
	double result = _strtod_l(expr, &expr_next, lc_neutral.locale);
	if unexpected(expr == expr_next) {
		// Not actually a floating-point number, keep going though
		val->type = PVT_NONE;
		return expr + 1;
	}
	if unexpected(fabs(result) == HUGE_VAL && errno == ERANGE) {
		log_printf("ERROR: Floating point constant \"%.*s\" out of range!\n", expr_next - expr, expr);
		return NULL;
	}
	switch (expr_next[0] | 0x20) {
		case 'd':
			++expr_next;
		default:
			val->type = PVT_DOUBLE;
			val->d = result;
			return expr_next;
		case 'f':
			val->type = PVT_FLOAT;
			val->f = (float)result;
			return expr_next + 1;
		case 'l':
			val->type = PVT_LONGDOUBLE;
			val->ld = /*(LongDouble80)*/result;
			return expr_next + 1;
	}
}

static TH_NOINLINE double constpool_float_value(const char *const expr, patch_val_t *const val, char end_char, uintptr_t rel_source, HMODULE hMod) {
	char* expr_next;
	double result = _strtod_l(expr, &expr_next, lc_neutral.locale);
	if (expr != expr_next) {
		// Value was a float literal, so just use it directly
		return result;
	}
	// Test for option values
	if (expr[0] == '<') {
		if (get_patch_value(expr, val, NULL, NULL, NULL) != NULL) {
			switch (val->type) {
				case PVT_BYTE: return val->b;
				case PVT_SBYTE: return val->sb;
				case PVT_WORD: return val->w;
				case PVT_SWORD: return val->sw;
				case PVT_DWORD: return val->i;
				case PVT_SDWORD: return val->si;
				case PVT_QWORD: return (double)val->q;
				case PVT_SQWORD: return (double)val->sq;
				case PVT_FLOAT: return val->f;
				case PVT_DOUBLE: return val->d;
				case PVT_LONGDOUBLE: return val->ld;
			}
		}
	}
	// Fallback to the expression parser
	(void)eval_expr(expr, end_char, &val->z, NULL, rel_source, hMod);
	return val->z;
}

// Data to be rendered into a constpool.
struct constpool_input_t {
	union {
		std::string_view text;
		uintptr_t raw_pointer;
	};
	bool is_raw_pointer;

	inline constexpr constpool_input_t(const std::string_view& text) : text(text), is_raw_pointer(false) {}

	inline constexpr constpool_input_t(uintptr_t raw_pointer) : raw_pointer(raw_pointer), is_raw_pointer(true) {}
};

// Fields that are unique to each constpool use.
struct constpool_value_t {
	uintptr_t addr;
	HMODULE source_module;
	size_t render_index;

	inline constexpr constpool_value_t(uintptr_t addr, HMODULE source_module) : addr(addr), source_module(source_module), render_index(0) {}
};

// Fields that can be shared between constpool uses.
struct constpool_key_t {
	uint8_t alignment;
	patch_value_type_t type;
	size_t input_index;

	inline constexpr constpool_key_t(uint8_t alignment, patch_value_type_t type, size_t input_index) : alignment(alignment), type(type), input_index(input_index) {}

	inline bool operator<(const constpool_key_t& rhs) const {
		return this->alignment > rhs.alignment;
	}
};

static std::vector<constpool_input_t> constpool_inputs;
static std::multimap<constpool_key_t, constpool_value_t> constpool_prerenders;

void constpool_reset() {
	constpool_inputs.clear();
	constpool_prerenders.clear();
}

void add_constpool(const char* data, size_t data_length, patch_value_type_t type, uint8_t alignment, uintptr_t addr, HMODULE source_module) {
	std::string_view data_view = std::string_view(data, data_length);
	size_t input_index = constpool_inputs.size();
	for (const auto& constpool_input : constpool_inputs) {
		if (!constpool_input.is_raw_pointer && constpool_input.text == data_view) {
			input_index = &constpool_input - constpool_inputs.data();
			goto has_str;
		}
	}
	constpool_inputs.emplace_back(data_view);
has_str:
	constpool_prerenders.emplace(std::piecewise_construct,
		 /* constpool_key_t */   std::forward_as_tuple(alignment, type, input_index),
		 /* constpool_value_t */ std::forward_as_tuple(addr, source_module)
	);
}

void add_constpool_raw_pointer(uintptr_t data, uintptr_t addr) {
	size_t input_index = constpool_inputs.size();
	for (const auto& constpool_input : constpool_inputs) {
		if (constpool_input.is_raw_pointer && constpool_input.raw_pointer == data) {
			input_index = &constpool_input - constpool_inputs.data();
			goto has_pointer;
		}
	}
	constpool_inputs.emplace_back(data);
has_pointer:
	constpool_prerenders.emplace(std::piecewise_construct,
		 /* constpool_key_t */   std::forward_as_tuple(alignof(void*), PVT_POINTER, input_index),
		 /* constpool_value_t */ std::forward_as_tuple(addr, (HMODULE)NULL)
	);
}

// String/code types aren't usable in constpools
// and don't have single defined sizes anyway,
// so they aren't included in this list
static constexpr size_t patch_val_sizes[] = {
	0,
	sizeof(uint8_t), sizeof(int8_t),
	sizeof(uint16_t), sizeof(int16_t),
	sizeof(uint32_t), sizeof(int32_t),
	sizeof(uint64_t), sizeof(int64_t),
	sizeof(float),
	sizeof(double),
	AlignUpToMultipleOf2(sizeof(long double), 16) // Long double is slightly over aligned
};

#pragma warning(push)
// Intentional overflow/wraparound on some values,
#pragma warning(disable : 4307 4146)
void constpool_apply(HackpointMemoryPage* page_array) {

	if unexpected(constpool_prerenders.empty()) {
		return;
	}

	uint8_t* rendered_values = (uint8_t*)malloc(BINHACK_BUFSIZE_MIN);
	uint8_t* current_value = (uint8_t*)malloc(BINHACK_BUFSIZE_MIN);
	uint8_t* padding_tracking = (uint8_t*)malloc(BINHACK_BUFSIZE_MIN); // Could this be compressed to a bit array instead of bytes?
	{
#if !TH_X86 || _M_IX86_FP >= 2
		__m128i xmm_zero = {};
		__m128i xmm_neg_one = _mm_cmpeq_epi8(xmm_zero, xmm_zero);
		for (size_t i = 0; i < BINHACK_BUFSIZE_MIN / sizeof(__m128i); ++i) {
			((__m128i*)rendered_values)[i] = xmm_zero;
			((__m128i*)padding_tracking)[i] = xmm_neg_one;
		}
#else
		for (size_t i = 0; i < BINHACK_BUFSIZE_MIN / sizeof(uint32_t); ++i) {
			((uint32_t*)rendered_values)[i] = 0u;
			((uint32_t*)padding_tracking)[i] = ~0u;
		}
#endif
	}

	size_t rendered_values_alloc_size = BINHACK_BUFSIZE_MIN;
	size_t current_value_alloc_size = BINHACK_BUFSIZE_MIN;

	size_t constpool_memory_size = 0;
	for (auto& [_key, value] : constpool_prerenders) {
		const auto& key = _key;
		const constpool_input_t& input = constpool_inputs[key.input_index];

		size_t filled_value_size;
		size_t per_element_size;
		size_t element_count;

		// Calculate the value of each element...
		if (!input.is_raw_pointer) {
			// Input is a text string from patch JSON
			const std::string_view& exprs = input.text;

			patch_val_t pool_value = {};

			filled_value_size = 0;
			per_element_size = patch_val_sizes[key.type];
			element_count = 0;

			size_t element_pos = 0;
			do {
				// TODO: Find calls memchr, which seems a bit overkill
				size_t end_element = exprs.find(';', element_pos);
				char end_char;
				if (end_element != std::string_view::npos) {
					// Repeat the previous element if it's just a consecutive ;
					if (element_pos != end_element) {
						end_char = ';';
						// MSVC is too dumb to merge two function calls
						goto parse_constpoool_expr;
					}
				} else {
					if (exprs[element_pos] != '}') {
						end_char = '}';
					parse_constpoool_expr:
						if (key.type < PVT_FLOAT) {
							(void)eval_expr(&exprs[element_pos], end_char, &pool_value.z, NULL, value.addr, value.source_module);
						} else {
							double float_value = constpool_float_value(&exprs[element_pos], &pool_value, end_char, value.addr, value.source_module);
							switch (key.type) {
								default: TH_UNREACHABLE;
								case PVT_FLOAT:
									pool_value.f = (float)float_value;
									break;
								case PVT_DOUBLE:
									pool_value.d = float_value;
									break;
								case PVT_LONGDOUBLE:
									pool_value.ld = float_value;
									break;
							}
						}
					}
				}
				element_pos = end_element + 1;
				if unexpected(filled_value_size + per_element_size >= current_value_alloc_size) {
					current_value = (uint8_t*)realloc(current_value, current_value_alloc_size += BINHACK_BUFSIZE_MIN);
				}
				switch (per_element_size) {
					default:
						TH_UNREACHABLE;
					case 16:
						memcpy(&current_value[filled_value_size], &pool_value.ld, sizeof(LongDouble80));
						break;
					case 8:
						// double forces an XMM QWORD copy
						*(double*)&current_value[filled_value_size] = pool_value.d;
						break;
					case 4:
						*(uint32_t*)&current_value[filled_value_size] = pool_value.i;
						break;
					case 2:
						*(uint16_t*)&current_value[filled_value_size] = pool_value.w;
						break;
					case 1:
						*(uint8_t*)&current_value[filled_value_size] = pool_value.b;
						break;
				}
				filled_value_size += per_element_size;
				++element_count;
			} while (element_pos != std::string_view::npos + 1);
		}
		else {
			// Input is a pointer 
			filled_value_size = sizeof(void*);
			per_element_size = sizeof(void*);
			element_count = 1;

			*(uintptr_t*)current_value = input.raw_pointer;
		}

		size_t aligned_value_size = AlignUpToMultipleOf2(filled_value_size, key.alignment);
		size_t overaligned_value_size;
		TH_ASSUME(element_count > 0);

		size_t render_index;
		if (aligned_value_size <= constpool_memory_size) {
			size_t render_index_end = constpool_memory_size - aligned_value_size;
			render_index = 0;
			switch (per_element_size) {
				default:
					TH_UNREACHABLE;
				case 16:
					// Check if the element array is already present in the rendered values...
					do {
						size_t i = 0;
						do {
							uint64_t* temp = (uint64_t*)&padding_tracking[render_index + i * 16];
							if (temp[0] || *(uint16_t*)&temp[1] ||
								memcmp(&rendered_values[render_index + i * 16], current_value + i * 16, sizeof(LongDouble80))
							) {
								goto continue_outer_match_ld;
							}
						} while (++i < element_count);
						goto match_success;
					continue_outer_match_ld:
						render_index += key.alignment;
					} while (render_index < render_index_end);
					// Check if any of the padding elements can be reused...
					render_index = 0;
					do {
						size_t i = 0;
						do {
							uint64_t* temp = (uint64_t*)&padding_tracking[render_index + i * 16];
							if (temp[0] != 0xFFFFFFFFFFFFFFFFull || *(int16_t*)&temp[1] != -1) { // MSVC hates uint16_t comparisons
								goto continue_outer_padding_ld;
							}
						} while (++i < element_count);
						goto padding_success;
					continue_outer_padding_ld:
						render_index += key.alignment;
					} while (render_index < render_index_end);
					break; // Expand
				case 8:
					// Check if the element array is already present in the rendered values...
					do {
						size_t i = 0;
						do {
							if (((uint64_t*)&padding_tracking[render_index])[i] ||
								((uint64_t*)&rendered_values[render_index])[i] != ((uint64_t*)current_value)[i]
							) {
								goto continue_outer_match_q;
							}
						} while (++i < element_count);
						goto match_success;
					continue_outer_match_q:
						render_index += key.alignment;
					} while (render_index < render_index_end);
					// Check if any of the padding elements can be reused...
					render_index = 0;
					do {
						size_t i = 0;
						do {
							if (((uint64_t*)&padding_tracking[render_index])[i] != 0xFFFFFFFFFFFFFFFFull) {
								goto continue_outer_padding_q;
							}
						} while (++i < element_count);
						goto padding_success;
					continue_outer_padding_q:
						render_index += key.alignment;
					} while (render_index < render_index_end);
					break; // Expand
				case 4:
					// Check if the element array is already present in the rendered values...
					do {
						size_t i = 0;
						do {
							if (((uint32_t*)&padding_tracking[render_index])[i] ||
								((uint32_t*)&rendered_values[render_index])[i] != ((uint32_t*)current_value)[i]
							) {
								goto continue_outer_match_d;
							}
						} while (++i < element_count);
						goto match_success;
					continue_outer_match_d:
						render_index += key.alignment;
					} while (render_index < render_index_end);
					// Check if any of the padding elements can be reused...
					render_index = 0;
					do {
						size_t i = 0;
						do {
							if (((uint32_t*)&padding_tracking[render_index])[i] != 0xFFFFFFFFu) {
								goto continue_outer_padding_d;
							}
						} while (++i < element_count);
						goto padding_success;
					continue_outer_padding_d:
						render_index += key.alignment;
					} while (render_index < render_index_end);
					break; // Expand
				case 2:
					// Check if the element array is already present in the rendered values...
					do {
						size_t i = 0;
						do {
							if (((uint16_t*)&padding_tracking[render_index])[i] ||
								((uint16_t*)&rendered_values[render_index])[i] != ((uint16_t*)current_value)[i]
							) {
								goto continue_outer_match_w;
							}
						} while (++i < element_count);
						goto match_success;
					continue_outer_match_w:
						render_index += key.alignment;
					} while (render_index < render_index_end);
					// Check if any of the padding elements can be reused...
					render_index = 0;
					do {
						size_t i = 0;
						do {
							if (((int16_t*)&padding_tracking[render_index])[i] != -1) { // MSVC hates uint16_t comparisons
								goto continue_outer_padding_w;
							}
						} while (++i < element_count);
						goto padding_success;
					continue_outer_padding_w:
						render_index += key.alignment;
					} while (render_index < render_index_end);
					break; // Expand
				case 1:
					// Check if the element array is already present in the rendered values...
					do {
						size_t i = 0;
						do {
							if (padding_tracking[render_index + i] ||
								rendered_values[render_index + i] != current_value[i]
							) {
								goto continue_outer_match_b;
							}
						} while (++i < element_count);
						goto match_success;
					continue_outer_match_b:
						render_index += key.alignment;
					} while (render_index < render_index_end);
					// Check if any of the padding elements can be reused...
					render_index = 0;
					do {
						size_t i = 0;
						do {
							if (padding_tracking[render_index + i] != 0xFF) {
								goto continue_outer_padding_b;
							}
						} while (++i < element_count);
						goto padding_success;
					continue_outer_padding_b:
						render_index += key.alignment;
					} while (render_index < render_index_end);
					break; // Expand
			}
		} else {
			// Not enough memory, always expand the allocation
			
			// Does this get alignment right?
			render_index = AlignUpToMultipleOf2(constpool_memory_size, key.alignment);
		}
		// These are brand new values, so expand the allocation
		overaligned_value_size = AlignUpToMultipleOf2(aligned_value_size, sizeof(__m128i));
		if unexpected(constpool_memory_size + overaligned_value_size >= rendered_values_alloc_size) {
			size_t new_alloc_size = rendered_values_alloc_size + overaligned_value_size + BINHACK_BUFSIZE_MIN;
			rendered_values = (uint8_t*)realloc(rendered_values, new_alloc_size);
			padding_tracking = (uint8_t*)realloc(padding_tracking, new_alloc_size);
			{
#if !TH_X86 || _M_IX86_FP >= 2
				__m128i xmm_zero = {};
				__m128i xmm_neg_one = _mm_cmpeq_epi8(xmm_zero, xmm_zero);
				size_t i = rendered_values_alloc_size;
				do {
					*(__m128i*)&rendered_values[i] = xmm_zero;
					*(__m128i*)&padding_tracking[i] = xmm_neg_one;
					i += sizeof(__m128i);
				} while (i < new_alloc_size);
#else
				for (size_t i = rendered_values_alloc_size; i < new_alloc_size; i += sizeof(uint32_t)) {
					*(uint32_t*)&rendered_values[i] = 0u;
					*(uint32_t*)&padding_tracking[i] = ~0u;
				}
#endif
			}
			rendered_values_alloc_size = new_alloc_size;
		}
		constpool_memory_size += overaligned_value_size;
	padding_success:
		// Render the new values...
		switch (per_element_size) {
			default:
				TH_UNREACHABLE;
			case 16: {
				size_t i = 0;
				do {
					memset(&padding_tracking[render_index + i * 16], 0, sizeof(LongDouble80));
					memcpy(&rendered_values[render_index + i * 16], current_value + i * 16, sizeof(LongDouble80));
				} while (++i < element_count);
				break;
			}
			case 8: {
				size_t i = 0;
				do {
					// memset forces an XMM XOR/MOV
					memset(&((uint64_t*)&padding_tracking[render_index])[i], 0, sizeof(uint64_t));
					// double forces an XMM QWORD copy
					((double*)&rendered_values[render_index])[i] = ((double*)current_value)[i];
				} while (++i < element_count);
				break;
			}
			case 4: {
				size_t i = 0;
				do {
					((uint32_t*)&padding_tracking[render_index])[i] = 0;
					((uint32_t*)&rendered_values[render_index])[i] = ((uint32_t*)current_value)[i];
				} while (++i < element_count);
				break;
			}
			case 2: {
				size_t i = 0;
				do {
					((uint16_t*)&padding_tracking[render_index])[i] = 0;
					((uint16_t*)&rendered_values[render_index])[i] = ((uint16_t*)current_value)[i];
				} while (++i < element_count);
				break;
			}
			case 1: {
				size_t i = 0;
				do {
					padding_tracking[render_index + i] = 0;
					rendered_values[render_index + i] = current_value[i];
				} while (++i < element_count);
				break;
			}
		}
	match_success:
		value.render_index = render_index;
	}
	page_array[0].size = constpool_memory_size;
	if (constpool_memory_size) {
		uint8_t* constpool = page_array[0].address = (uint8_t*)VirtualAllocLow(0, constpool_memory_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

		log_printf(
			"-------------------------\n"
			"Generating constpool... (0x%zX bytes at 0x%.8X)\n"
			"-------------------------\n",
			constpool_memory_size, (uint32_t)constpool
		);

		memcpy(constpool, rendered_values, constpool_memory_size);

		DWORD idgaf;
		VirtualProtect(constpool, constpool_memory_size, PAGE_READONLY, &idgaf);
		
		for (const auto& [_, value] : constpool_prerenders) {
			uint8_t *__ptr32 __sptr constpool_write = (uint8_t * __ptr32 __sptr)constpool + value.render_index;
			log_printf("fixup at 0x%p (0x%.8X)...\n", value.addr, (uint32_t)constpool_write);
			PatchRegion((void*)value.addr, NULL, &constpool_write, sizeof(uint32_t));
		}
	}
}
#pragma warning(pop)

// TODO: Check if anyone uses single quotes already in code strings
//// Char
//else if (expr[0] == '\'' && (expr_next = (char*)strchr(expr+1, '\''))) {
//  val->type = PVT_SBYTE;
//	if (expr[1] == '\\') {
//		switch (expr[2]) {
//			case '0':  val->sb = '\0'; break;
//			case 'a':  val->sb = '\a'; break;
//			case 'b':  val->sb = '\b'; break;
//			case 'f':  val->sb = '\f'; break;
//			case 'n':  val->sb = '\n'; break;
//			case 'r':  val->sb = '\r'; break;
//			case 't':  val->sb = '\t'; break;
//			case 'v':  val->sb = '\v'; break;
//			case '\\': val->sb = '\\'; break;
//			case '\'': val->sb = '\''; break;
//			case '\"': val->sb = '\"'; break;
//			case '\?': val->sb = '\?'; break;
//			default:   val->sb = expr[2]; break;
//		}
//	} else {
//		val->sb = expr[1];
//	}
//	return expr_next + 1;
//}

// Declared forceinline since the compiler can't
// figure it out otherwise and copy/pasting this
// into two functions would be a pain
static TH_FORCEINLINE const char* check_for_code_string_cast(const char* expr, patch_val_t *const val)
{
	switch (const uint8_t c = expr[0] | 0x20) {
		case 'p':
			if (expr[1] == ':') {
				val->type = PVT_POINTER;
				return expr + 2;
			}
			break;
		case 'i': case 'u': case 'f':
			if (expr[1] && expr[2]) {
				switch (const uint32_t temp = *(uint32_t*)expr | TextInt(0x20, 0x00, 0x00, 0x00)) {
					case TextInt('f', '3', '2', ':'):
						val->type = PVT_FLOAT;
						return expr + 4;
					case TextInt('f', '6', '4', ':'):
						val->type = PVT_DOUBLE;
						return expr + 4;
					case TextInt('f', '8', '0', ':'):
						val->type = PVT_LONGDOUBLE;
						return expr + 4;
					case TextInt('u', '1', '6', ':'):
						val->type = PVT_WORD;
						return expr + 4;
					case TextInt('i', '1', '6', ':'):
						val->type = PVT_SWORD;
						return expr + 4;
					case TextInt('u', '3', '2', ':'):
						val->type = PVT_DWORD;
						return expr + 4;
					case TextInt('i', '3', '2', ':'):
						val->type = PVT_SDWORD;
						return expr + 4;
					case TextInt('u', '6', '4', ':'):
						val->type = PVT_QWORD;
						return expr + 4;
					case TextInt('i', '6', '4', ':'):
						val->type = PVT_SQWORD;
						return expr + 4;
					default:
						switch (temp & TextInt(0xFF, 0xFF, 0xFF, '\0')) {
							case TextInt('u', '8', ':'):
								val->type = PVT_BYTE;
								return expr + 3;
							case TextInt('i', '8', ':'):
								val->type = PVT_SBYTE;
								return expr + 3;
						}
				}
			}
			break;
	}
	//log_printf("WARNING: no code string expression size specified, assuming default...\n");
	val->type = PVT_DEFAULT;
	return expr;
}

static inline const char* check_for_code_string_alignment(const char* expr, uint8_t& align_out) {
	if (uint8_t c = expr[0]) {
		switch (TextInt(c, expr[1]) | TextInt(0x20, 0)) {
			case TextInt('z', '.'): align_out = 64; break;
			case TextInt('y', '.'): align_out = 32; break;
			case TextInt('x', '.'): align_out = 16; break;
#ifdef TH_X64
			case TextInt('p', '.'):
#endif
			case TextInt('q', '.'): align_out = 8; break;
#ifndef TH_X64
			case TextInt('p', '.'):
#endif
			case TextInt('d', '.'): align_out = 4; break;
			case TextInt('w', '.'): align_out = 2; break;
			case TextInt('b', '.'): align_out = 1; break;
			default:
#ifdef TH_X64
				align_out = 8;
#else
				align_out = 4;
#endif
				return expr;
		}
		return expr + 2;
	}
	return NULL;
}

static inline const char* parse_brackets(const char* str, char c) {
	--str;
	int32_t paren_count = 0;
	int32_t square_count = 0;
	//int32_t curly_count = 0;
	do {
		paren_count += (c == '(') - (c == ')');
		square_count += (c == '[') - (c == ']');
		//curly_count += (c == '{') - (c == '}');
		if (const int32_t temp = (paren_count | square_count /*| curly_count*/);
			temp == 0) {
			return str;
		}
		else if (temp < 0) {
			return NULL;
		}
	} while (c = *++str);
	return NULL;
}

size_t code_string_calc_size(const char* code_str) {
	if (!code_str) {
		return 0;
	}
	size_t size = 0;
	patch_val_t val;
	val.type = PVT_NONE;
	while (1) {
		// Parse characters
		switch (uint8_t cur_char = code_str[0]) {
			EndOfString: case '\0': // End of string
				return size;
			case '/': // Comments
				if (uint8_t prev_char = *++code_str; prev_char == '*') {
					while (1) {
						cur_char = *++code_str;
						if (cur_char == '\0') { // Unterminated comment
							goto EndOfString;
						} else if (cur_char == '/' && prev_char == '*') {
							break;
						}
						prev_char = cur_char;
					}
					++code_str;
				}
				continue;
			case '{': // Absolute address of constant value
				while (1) {
					switch (cur_char = *++code_str) {
						default:
							continue;
						case '}':
							val.type = PVT_DWORD; // Always a 32 bit address, even on x64 (bottom 2GB)
							++code_str;
							break;
						case '\0':
							code_str = NULL;
							break;
					}
					break;
				}
				break;
			case '(': case '[':
				++code_str;
				if (cur_char == '[') { // Relative patch value
					val.type = PVT_DWORD; // x64 uses 32 bit relative offsets as well
				}
				else { // Expressions
					code_str = check_for_code_string_cast(code_str, &val);
				}
				code_str = parse_brackets(code_str, cur_char);
				break;
			case '<': // Absolute patch value
				code_str = get_patch_value(code_str, &val, NULL, NULL, NULL);
				break;
			case '?': // Wildcard byte
				++code_str;
				if (code_str[0] != '?') {
					// Not a full wildcard byte, ignore current character
					// and parse the next character from the beginning
					continue;
				}
				// Found a wildcard byte, so just add
				// a byte of size and keep going
				val.type = PVT_BYTE;
				++code_str;
				break;
			case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9':
			case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
			case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
				// Raw byte
				++code_str;
				if (!is_valid_hex(code_str[0])) {
					// Next character doesn't form complete byte, so
					// ignore the current character and parse the
					// next character from the beginning
					continue;
				}
				val.type = PVT_BYTE;
				++code_str;
				break;
			case '+': case '-': // Float
				code_str = consume_float_value(code_str, &val);
				break;
			default: // Skip character
				++code_str;
				continue;
		}

		// Check for errors
		if unexpected(!code_str) {
			// Code string calc size error
			return 0;
		}

		// Add to size
		switch (val.type) {
			case PVT_BYTE: case PVT_SBYTE:
				size += sizeof(int8_t);
				break;
			case PVT_WORD: case PVT_SWORD:
				size += sizeof(int16_t);
				break;
			case PVT_DWORD: case PVT_SDWORD:
				size += sizeof(int32_t);
				break;
			case PVT_QWORD: case PVT_SQWORD:
				size += sizeof(int64_t);
				break;
			case PVT_FLOAT:
				size += sizeof(float);
				break;
			case PVT_DOUBLE:
				size += sizeof(double);
				break;
			case PVT_LONGDOUBLE:
				size += sizeof(LongDouble80);
				break;
			case PVT_STRING:
				size += sizeof(const char*);
				break;
			case PVT_STRING16:
				size += sizeof(const char16_t*);
				break;
			case PVT_STRING32:
				size += sizeof(const char32_t*);
				break;
			case PVT_CODE:
				size += val.code.len * val.code.count;
				break;
			case PVT_NONE:
				break;
			default:
				TH_UNREACHABLE;
		}
	}
}

int code_string_render(uint8_t* output_buffer, uintptr_t target_addr, const char* code_str, HMODULE hMod) {

// Dang compatibility things
#define CodeStringErrorRet		1
#define CodeStringSuccessRet	0

	if (!output_buffer || !code_str) {
		return CodeStringErrorRet;
	}

#define CodeStringRenderError() code_str = NULL;

	patch_val_t val;

	while (1) {
		// Parse characters
		switch (uint8_t cur_char = code_str[0]) {
			EndOfString: case '\0': // End of string
				return CodeStringSuccessRet;
			case '/': // Comments
				if (uint8_t prev_char = *++code_str; prev_char == '*') {
					while (1) {
						cur_char = *++code_str;
						if (cur_char == '\0') { // Unterminated comment
							goto EndOfString;
						}
						else if (cur_char == '/' && prev_char == '*') {
							break;
						}
						prev_char = cur_char;
					}
					++code_str;
				}
				continue;
			case '(': // Expression
				code_str = check_for_code_string_cast(++code_str, &val);
				code_str = eval_expr(code_str, ')', &val.z, NULL, target_addr, hMod);
				if unexpected(!code_str) {
					break; // Error
				}
				switch (val.type) {
					case PVT_FLOAT:
						val.f = (float)val.z;
						break;
					case PVT_DOUBLE:
						val.d = (double)val.z;
						break;
					case PVT_LONGDOUBLE:
						val.ld = /*(LongDouble80)*/val.z;
						break;
				}
				break;
			case '{': // Absolute address of constant value in bottom 2GB
				if (code_str = check_for_code_string_alignment(++code_str, val.alignment)) {
					code_str = check_for_code_string_cast(code_str, &val);
					size_t i = 0;
					while (code_str[i] != '}') ++i;
					if (i) {
						add_constpool(code_str, i, val.type, val.alignment, target_addr, hMod);
						code_str += i;
					}
					++code_str;
					val.i = 0; // Use NULL as a placeholder to avoid silent failure if the constpool breaks
					val.type = PVT_DWORD;
				}
				break;
			case '[': case '<': // Patch value
				code_str = get_patch_value(code_str, &val, NULL, target_addr, hMod);
				break;
			case '?': // Wildcard byte
				++code_str;
				if (code_str[0] != '?') {
					// Not a full wildcard byte, ignore current character
					// and parse the next character from the beginning
					continue;
				}
				/*if (!can_deref_target) {
					// Please just don't use an invalid address for
					// target_addr, it makes this hard to implement
					CodeStringRenderError();
					break;
				}*/
				// Found a wildcard byte, so read the contents
				// of the appropriate address into the buffer.
				val.type = PVT_BYTE;
				val.b = *(unsigned char*)target_addr;
				++code_str;
				break;
			case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9':
			case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
			case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
			{ // Raw byte
				++code_str;
				uint8_t low_nibble = code_str[0] - '0';
				if (low_nibble >= 10) {
					low_nibble &= 0xDF;
					low_nibble -= 17;
					if (low_nibble > 6) {
						// Next character doesn't form complete byte, so
						// ignore the current character and parse the
						// next character from the beginning
						continue;
					}
					low_nibble += 10;
				}
				cur_char -= '0'; // high_nibble
				if (cur_char >= 10) {
					cur_char &= 0xDF;
					cur_char -= 7;
				}
				val.b = cur_char << 4 | low_nibble;
				val.type = PVT_BYTE;
				++code_str;
				break;
			}
			case '+': case '-': // Float
				code_str = consume_float_value(code_str, &val);
				break;
			default: // Skip character
				++code_str;
				continue;
		}

		// Check for errors
		if unexpected(!code_str) {
			log_print("Code string render error!\n");
			return CodeStringErrorRet;
		}

#ifdef EnableCodeStringLogging
#define CodeStringLogging(...) log_printf(__VA_ARGS__)
#else
// Using __noop makes the compiler check the validity of
// the macro contents without actually compiling them.
#define CodeStringLogging(...) TH_EVAL_NOOP(__VA_ARGS__)
#endif

		// Render bytes
		switch (val.type) {
			case PVT_BYTE:
				*(uint8_t*)output_buffer = val.b;
				CodeStringLogging("Code string rendered: %02hhX at %p\n", *(uint8_t*)output_buffer, target_addr);
				output_buffer += sizeof(uint8_t);
				target_addr += sizeof(uint8_t);
				break;
			case PVT_SBYTE:
				*(int8_t*)output_buffer = val.sb;
				CodeStringLogging("Code string rendered: %02hhX at %p\n", *(int8_t*)output_buffer, target_addr);
				output_buffer += sizeof(int8_t);
				target_addr += sizeof(int8_t);
				break;
			case PVT_WORD:
				*(uint16_t*)output_buffer = val.w;
				CodeStringLogging("Code string rendered: %04hX at %p\n", *(uint16_t*)output_buffer, target_addr);
				output_buffer += sizeof(uint16_t);
				target_addr += sizeof(uint16_t);
				break;
			case PVT_SWORD:
				*(int16_t*)output_buffer = val.sw;
				CodeStringLogging("Code string rendered: %04hX at %p\n", *(int16_t*)output_buffer, target_addr);
				output_buffer += sizeof(int16_t);
				target_addr += sizeof(int16_t);
				break;
			case PVT_DWORD:
				*(uint32_t*)output_buffer = val.i;
				CodeStringLogging("Code string rendered: %08X at %p\n", *(uint32_t*)output_buffer, target_addr);
				output_buffer += sizeof(uint32_t);
				target_addr += sizeof(uint32_t);
				break;
			case PVT_SDWORD:
				*(int32_t*)output_buffer = val.si;
				CodeStringLogging("Code string rendered: %08X at %p\n", *(int32_t*)output_buffer, target_addr);
				output_buffer += sizeof(int32_t);
				target_addr += sizeof(int32_t);
				break;
			case PVT_QWORD:
				*(uint64_t*)output_buffer = val.q;
				CodeStringLogging("Code string rendered: %016X at %p\n", *(uint64_t*)output_buffer, target_addr);
				output_buffer += sizeof(uint64_t);
				target_addr += sizeof(uint64_t);
				break;
			case PVT_SQWORD:
				*(int64_t*)output_buffer = val.sq;
				CodeStringLogging("Code string rendered: %016X at %p\n", *(int64_t*)output_buffer, target_addr);
				output_buffer += sizeof(int64_t);
				target_addr += sizeof(int64_t);
				break;
			case PVT_FLOAT:
				*(float*)output_buffer = val.f;
				CodeStringLogging("Code string rendered: %X at %p\n", *(uint32_t*)output_buffer, target_addr);
				output_buffer += sizeof(float);
				target_addr += sizeof(float);
				break;
			case PVT_DOUBLE:
				*(double*)output_buffer = val.d;
				CodeStringLogging("Code string rendered: %llX at %p\n", *(uint64_t*)output_buffer, target_addr);
				output_buffer += sizeof(double);
				target_addr += sizeof(double);
				break;
			case PVT_LONGDOUBLE:
				*(LongDouble80*)output_buffer = val.ld;
				CodeStringLogging("Code string rendered: %llX%hX at %p\n", *(uint64_t*)output_buffer, *(uint16_t*)(output_buffer + 8), target_addr);
				output_buffer += sizeof(LongDouble80);
				target_addr += sizeof(LongDouble80);
				break;
			case PVT_STRING:
				*(const char**)output_buffer = val.str.ptr;
				CodeStringLogging("Code string rendered: %zX at %p\n", (size_t)*(const char**)output_buffer, target_addr);
				output_buffer += sizeof(const char*);
				target_addr += sizeof(const char*);
				break;
			case PVT_STRING16:
				*(const char16_t**)output_buffer = val.str16.ptr;
				CodeStringLogging("Code string rendered: %zX at %p\n", (size_t)*(const char16_t**)output_buffer, target_addr);
				output_buffer += sizeof(const char16_t*);
				target_addr += sizeof(const char16_t*);
				break;
			case PVT_STRING32:
				*(const char32_t**)output_buffer = val.str32.ptr;
				CodeStringLogging("Code string rendered: %zX at %p\n", (size_t)*(const char32_t**)output_buffer, target_addr);
				output_buffer += sizeof(const char32_t*);
				target_addr += sizeof(const char32_t*);
				break;
			case PVT_CODE: {
				while (val.code.count--) {
					if unexpected(code_string_render(output_buffer, target_addr, val.code.ptr, hMod)) {
						return CodeStringErrorRet;
					}
					output_buffer += val.code.len;
					target_addr += val.code.len;
				}
				break;
			}
			case PVT_NONE:
				break;
			default:
				TH_UNREACHABLE;
		}
	}
}

bool binhack_from_json(const char *name, json_t *in, binhack_t *out)
{
	if (!json_is_object(in)) {
		log_printf("binhack %s: not an object\n", name);
		return false;
	}

	if (json_object_get_eval_bool_default(in, "ignore", false, JEVAL_DEFAULT) ||
		!json_object_get_eval_bool_default(in, "enable", true, JEVAL_DEFAULT)) {
		log_printf("binhack %s: ignored\n", name);
		return false;
	}

	const char *code = json_object_get_concat_string_array(in, "code"); // Allocates a string that must be freed if non-null
	if (!code) {
		// Ignore binhacks with missing fields
		// It usually means the binhack doesn't apply for this game or game version.
		return false;
	}

	size_t code_size = code_string_calc_size(code);
	if (!code_size) {
		free((void*)code); // free the code string since it's not needed
		return false;
	}

	hackpoint_addr_t *addrs = hackpoint_addrs_from_json(json_object_get(in, "addr"));
	if (!addrs) {
		// Ignore binhacks with no valid addresses
		free((void*)code); // free the code string since it's not needed
		return false;
	}

	// The binhack is definitely valid now, so start writing the fields

	out->name = strdup(name);
	out->title = json_object_get_string_copy(in, "title");

	out->code = code;
	out->code_size = code_size;

	out->expected = NULL;
	if (const char* expected = json_object_get_concat_string_array(in, "expected")) { // Allocates a string that must be freed if non-null
		size_t expected_size = code_string_calc_size(expected);
		if (expected_size == code_size) {
			out->expected = expected;
		}
		else {
			log_printf("binhack %s: different sizes for expected and code (%zu != %zu), verification will be skipped\n", name, expected_size, code_size);
			free((void*)expected); // Expected won't be used, so it can be freed
		}
	}

	out->addr = addrs;

	return true;
}

// Returns the number of all individual instances of binary hacks in [binhacks].
static size_t binhacks_total_count(const binhack_t *binhacks, size_t binhacks_count)
{
	size_t ret = 0;
	for (size_t i = 0; i < binhacks_count; i++) {
		for (size_t j = 0; binhacks[i].addr[j].type != END_ADDR; j++) {
			ret++;
		}
	}
	return ret;
}

size_t binhacks_apply(const binhack_t *binhacks, size_t binhacks_count, HMODULE hMod, HackpointMemoryPage* page_array)
{
	if (!binhacks_count) {
		log_print("No binary hacks to apply.\n");
		return 0;
	}

	size_t sourcecaves_total_size = 0;
	size_t failed = binhacks_total_count(binhacks, binhacks_count);
	size_t total_valid_addrs = 0;
	size_t largest_cavesize = 0;

	VLA(bool, binhack_is_valid, binhacks_count);

	log_print(
		"------------------------\n"
		"Setting up binary hacks...\n"
		"------------------------"
	);

	for (size_t i = 0; i < binhacks_count; ++i) {
		const binhack_t& cur = binhacks[i];

		binhack_is_valid[i] = false;

		log_printf(
			cur.title ? "\n(%2zu/%2zu) %s (%s)..." : "\n(%2zu/%2zu) %s..."
			, i + 1, binhacks_count, cur.name, cur.title
		);

		size_t cavesize = cur.code_size;

		assert(cavesize == code_string_calc_size(cur.code));

		if (cur.expected) {
			assert(cavesize == code_string_calc_size(cur.expected));
		}

		if (!cavesize) {
			log_print("invalid code string size, skipping...\n");
			continue;
		}

		size_t cur_valid_addrs = 0;

		uintptr_t addr;
		for (hackpoint_addr_t* cur_addr = cur.addr;
			 eval_hackpoint_addr(cur_addr, &addr, hMod);
			 ++cur_addr) {

			if (!addr) {
				// NULL_ADDR
				continue;
			}

			log_printf("\nat 0x%p... ", addr);

			if (!VirtualCheckRegion((const void*)addr, cavesize)) {
				cur_addr->type = INVALID_ADDR;
				log_print("not enough source bytes, skipping...");
				continue;
			}
			++cur_valid_addrs;
			log_print("OK");
			sourcecaves_total_size += cavesize;
		}

		if (cur_valid_addrs) {
			binhack_is_valid[i] = true;
			if (cavesize > largest_cavesize) largest_cavesize = cavesize;
			total_valid_addrs += cur_valid_addrs;
			--failed;
		}
	}

	if (!total_valid_addrs) {
		log_print("No valid binary hacks to render.\n");
		return failed;
	}

	uint8_t* const cave_source = (uint8_t*)VirtualAlloc(0, sourcecaves_total_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	if (page_array) {
		page_array->address = cave_source;
		page_array->size = sourcecaves_total_size;
	}

	log_printf(
		"\n"
		"-------------------------\n"
		"Rendering binary hacks... (source cave at 0x%p)\n"
		"-------------------------",
		cave_source
	);

	uint8_t* sourcecave_p = cave_source;
	uint8_t* sourcecave_end = sourcecave_p + sourcecaves_total_size;

	uint8_t* asm_buf = (uint8_t*)malloc(largest_cavesize * 2);
	uint8_t* exp_buf = asm_buf + largest_cavesize;

	for (size_t i = 0; i < binhacks_count; ++i) {
		if (binhack_is_valid[i]) {
			const binhack_t& cur = binhacks[i];

			size_t cavesize = cur.code_size;

			bool use_expected = (cur.expected != NULL);

			uintptr_t addr;
			for (hackpoint_addr_t* cur_addr = cur.addr;
				 eval_hackpoint_addr(cur_addr, &addr, hMod);
				 ++cur_addr) {

				if (!addr) {
					// NULL_ADDR
					continue;
				}

				log_printf("\nat 0x%p... ", addr);

				if (code_string_render(asm_buf, addr, cur.code, hMod)) {
					log_print("invalid code string, skipping...");
					continue;
				}
				if (use_expected && code_string_render(exp_buf, addr, cur.expected, hMod)) {
					log_print("invalid expected string, skipping verification... ");
					use_expected = false;
				}
				if ((cur_addr->binhack_source = (uint8_t*)PatchRegionCopySrc((void*)addr, (use_expected ? exp_buf : NULL), asm_buf, sourcecave_p, cavesize))) {
					sourcecave_p += cavesize;
					assert(sourcecave_p <= sourcecave_end);
					log_print("OK");
				}
				else {
					log_print("expected bytes not matched, skipping...");
					++failed;
				}
			}
		}
	}
	free(asm_buf);
	VLA_FREE(binhack_is_valid);
	log_print("\n");

	DWORD idgaf;
	VirtualProtect(cave_source, sourcecaves_total_size, PAGE_READONLY, &idgaf);

	return failed;
}

bool codecave_from_json(const char *name, json_t *in, codecave_t *out) {
	// Default properties
	size_t size_val = 0;
	const char* code = NULL;
	bool export_val = false;
	CodecaveAccessType access_val = EXECUTE_READWRITE;
	size_t fill_val = 0;
	size_t align_val = 16;

	if (json_is_object(in)) {
		if (json_object_get_eval_bool_default(in, "ignore", false, JEVAL_DEFAULT) ||
			!json_object_get_eval_bool_default(in, "enable", true, JEVAL_DEFAULT)) {
			log_printf("codecave %s: ignored\n", name);
			return false;
		}

		switch (json_object_get_eval_int(in, "size", &size_val, JEVAL_STRICT)) {
			default:
				log_printf("ERROR: invalid json type for size of codecave %s, must be 32-bit integer or string\n", name);
				return false;
			case JEVAL_SUCCESS: {
				if (!size_val) {
					log_printf("codecave %s with size 0 ignored\n", name);
					return false;
				}
				size_t count_val;
				switch (json_object_get_eval_int(in, "count", &count_val, JEVAL_STRICT)) {
					default:
						log_printf("ERROR: invalid json type specified for count of codecave %s, must be 32-bit integer or string\n", name);
						return false;
					case JEVAL_SUCCESS:
						if (!count_val) {
							log_printf("codecave %s with count 0 ignored\n", name);
							return false;
						}
						size_val *= count_val;
					case JEVAL_NULL_PTR:
						break;
				}
				break;
			}
			case JEVAL_NULL_PTR:
				break;
		}

		(void)json_object_get_eval_bool(in, "export", &export_val, JEVAL_DEFAULT);

		switch (json_object_get_eval_int(in, "fill", &fill_val, JEVAL_STRICT)) {
			default:
				log_printf("ERROR: invalid json type specified for fill value of codecave %s, must be 32-bit integer or string\n", name);
				return false;
			case JEVAL_SUCCESS:
			case JEVAL_NULL_PTR:
				break;
		}

		switch (json_object_get_eval_int(in, "align", &align_val, JEVAL_STRICT)) {
			default:
				log_printf("ERROR: invalid json type specified for align value of codecave %s, must be 32-bit integer or string\n", name);
				return false;
			case JEVAL_SUCCESS:
				// Round the alignment to the next power of 2 (including 1)
				if (unsigned long bit; _BitScanReverse(&bit, align_val - 1)) {
					align_val = 1u << (bit + 1);
				} else {
					align_val = 1u;
				}
				TH_FALLTHROUGH;
			case JEVAL_NULL_PTR:
				break;
		}

		code = json_object_get_concat_string_array(in, "code"); // Potentially allocates a string that must be freed

		if (const char* access = json_object_get_string(in, "access")) {
			uint8_t temp = 0;
			while (char c = *access++ & 0xDF) {
				temp |= (c == 'R');
				temp |= (c == 'W') << 1;
				temp |= ((c == 'E') | (c == 'X')) << 2;
			}
			switch (temp) {
				case 0: /*NOACCESS*/
					log_printf("codecave %s: why would you set a codecave to no access? skipping instead...\n", name);
					if (code) {
						free((void*)code); // free the code string since it's not needed
					}
					return false;
				case 1: /*READONLY*/
					access_val = (CodecaveAccessType)(temp - 1);
					break;
				case 2: /*WRITE*/
					log_printf("codecave %s: write-only codecaves can't be set, using read-write instead...\n", name);
					[[fallthrough]];
				case 3: /*READWRITE*/ case 4: /*EXECUTE*/ case 5: /*EXECUTE-READ*/
					access_val = (CodecaveAccessType)(temp - 2);
					break;
				case 6: /*EXECUTE-WRITE*/
					log_printf("codecave %s: execute-write codecaves can't be set, using execute-read-write instead...\n", name);
					[[fallthrough]];
				case 7: /*EXECUTE-READWRITE*/
					access_val = (CodecaveAccessType)(temp - 3);
					break;
			}
			if (export_val && access_val != EXECUTE && access_val != EXECUTE_READ) {
				log_printf("codecave %s: export can only be applied to execute or execute-read codecaves\n", name);
				export_val = false;
			}
		}
		else {
			if ((code && !size_val) || export_val) {
				access_val = EXECUTE;
			}
			else if (size_val && !code) {
				access_val = READWRITE;
			}
			//else {
			//    access_val = EXECUTE_READWRITE;
			//}
		}
	}
	else if (json_is_string(in) || json_is_array(in)) {
		// size_val = 0;
		code = json_concat_string_array(in, name); // Allocates a string that must be freed
		// export_val = false;
		// access_val = EXECUTE_READWRITE;
		// fill_val = 0;
		// align_val = 16;
	}
	else if (json_is_integer(in)) {
		size_val = (size_t)json_integer_value(in);
		// code = NULL;
		// export_val = false;
		access_val = READWRITE;
		// fill_val = 0;
		// align_val = 16;
	}
	else {
		// Don't print an error, this can be used for comments
		return false;
	}

	// Validate codecave size early
	if (code) {
		DisableCodecaveNotFoundWarning(true);
		const size_t code_size = code_string_calc_size(code);
		DisableCodecaveNotFoundWarning(false);
		if (!code_size && !size_val) {
			log_printf("codecave %s with size 0 ignored\n", name);
			free((void*)code); // free the code string since it's not needed
			return false;
		}
		if (code_size > size_val) {
			size_val = code_size;
		}
	}
	else if (!size_val) {
		log_printf("codecave %s without \"code\" or \"size\" ignored\n", name);
		return false; // No need to free the code string since the pointer must have been NULL to get here
	}

	out->code = code;
	out->name = strdup_cat("codecave:", name);
	out->access_type = access_val;
	out->size = size_val;
	out->fill = (uint8_t)fill_val;
	out->export_codecave = export_val;
	out->align = (uint32_t)align_val;
	out->virtual_address = NULL;

	return true;
}

size_t codecaves_apply(codecave_t *codecaves, size_t codecaves_count, HMODULE hMod, HackpointMemoryPage page_array[5]) {
	if (codecaves_count == 0) {
		return 0;
	}

	log_print(
		"------------------------\n"
		"Applying codecaves...\n"
		"------------------------\n"
	);

	static constexpr size_t codecave_sep_size_min = 1;

	//One size for each valid access flag combo
	size_t codecaves_alloc_size[5] = { 0, 0, 0, 0, 0 };

	size_t codecave_export_count = 0;

	VLA(size_t, codecaves_full_size, codecaves_count);

	// First pass: calc the complete codecave size
	for (size_t i = 0; i < codecaves_count; ++i) {
		if (codecaves[i].export_codecave) {
			++codecave_export_count;
		}
		const size_t size = codecaves[i].size + codecave_sep_size_min;
		// This doesn't make good use of padding bytes
		codecaves_full_size[i] = AlignUpToMultipleOf2(size, (int32_t)codecaves[i].align);
		codecaves_alloc_size[codecaves[i].access_type] += codecaves_full_size[i];
	}

	for (int i = 0; i < 5; ++i) {
		uint8_t* codecave_buf = NULL;
		size_t codecave_size = codecaves_alloc_size[i];
		page_array[i].size = codecave_size;
		if (codecave_size > 0) {
			codecave_buf = (uint8_t*)VirtualAlloc(NULL, codecave_size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
			if (codecave_buf) {
				/*
				*  TODO: Profile whether it's faster to memset the whole block
				*  here or just the extra padding required after each codecave.
				*  Apply the same thing to breakpoint sourcecaves.
				*/
				//memset(codecave_buf[i], 0xCC, codecaves_total_size[i]);
			}
			else {
				//Should probably put an abort error here
			}
		}
		page_array[i].address = codecave_buf;
	}

	// Second pass: Gather the addresses, so that codecaves can refer to each other.
	uint8_t* current_cave[5];
	for (int i = 0; i < 5; ++i) {
		current_cave[i] = page_array[i].address;
	}

	exported_func_t* codecaves_export_table;
	if (codecave_export_count > 0) {
		codecaves_export_table = (exported_func_t*)malloc((codecave_export_count + 1) * sizeof(exported_func_t));
		codecaves_export_table[codecave_export_count].name = NULL;
		codecaves_export_table[codecave_export_count].func = NULL;
	}
	size_t export_index = 0;
	
	for (size_t i = 0; i < codecaves_count; ++i) {
		const CodecaveAccessType access = codecaves[i].access_type;

		log_printf("Recording \"%s\" at 0x%p\n", codecaves[i].name, (uintptr_t)current_cave[access]);
		func_add(codecaves[i].name, (uintptr_t)current_cave[access]);

		if (codecaves[i].export_codecave) {
			codecaves_export_table[export_index].name = codecaves[i].name;
			codecaves_export_table[export_index].func = (uintptr_t)current_cave[access];
			++export_index;
		}

		current_cave[access] += codecaves_full_size[i];
	}

	// Third pass: Write all of the code
	for (int i = 0; i < 5; ++i) {
		current_cave[i] = page_array[i].address;
	}

	for (size_t i = 0; i < codecaves_count; ++i) {
		const char* code = codecaves[i].code;
		const CodecaveAccessType access = codecaves[i].access_type;
		if (codecaves[i].fill) {
			// VirtualAlloc zeroes memory, so don't bother when fill is 0
			memset(current_cave[access], codecaves[i].fill, codecaves[i].size);
		}
		if (code) {
			code_string_render(current_cave[access], (uintptr_t)current_cave[access], code, hMod);
		}
		codecaves[i].virtual_address = current_cave[access];

		current_cave[access] += codecaves[i].size;
		for (size_t j = codecaves[i].size; j < codecaves_full_size[i]; ++j) {
			*(current_cave[access]++) = 0xCC; // x86 INT3
		}
	}

	if (codecave_export_count > 0) {
		patch_func_init(codecaves_export_table);
		free(codecaves_export_table);
	}
	VLA_FREE(codecaves_full_size);

	static constexpr DWORD page_access_type_array[5] = { PAGE_READONLY, PAGE_READWRITE, PAGE_EXECUTE, PAGE_EXECUTE_READ, PAGE_EXECUTE_READWRITE };
	DWORD idgaf;

	for (int i = 0; i < 5; ++i) {
		if (page_array[i].address) {
			VirtualProtect(page_array[i].address, page_array[i].size, page_access_type_array[i], &idgaf);
		}
	}
	return 0;
}
