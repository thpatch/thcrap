/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Breakpoint handling.
  */

#include "thcrap.h"

static const char BREAKPOINTS[] = "breakpoints";

// Static global variables
// -----------------------
static BYTE *BP_CodeCave = NULL;

#define BP_Offset 32
#define CALL_LEN (sizeof(void*) + 1)
static const size_t BP_CodeCave_Limits[2] = {CALL_LEN, (BP_Offset - CALL_LEN)};
static json_t *BP_Object = NULL;
// -----------------------

#define EAX 0x00786165
#define ECX 0x00786365
#define EDX 0x00786465
#define EBX 0x00786265
#define ESP 0x00707365
#define EBP 0x00706265
#define ESI 0x00697365
#define EDI 0x00696465

size_t* reg(x86_reg_t *regs, const char *regname)
{
	char cmp[4];

	if(!regname) {
		return NULL;
	}
	memcpy(cmp, regname, 3);
	cmp[3] = 0;
	strlwr(cmp);

	switch(*(DWORD*)(&cmp)) {
		case EAX: return &regs->eax;
		case ECX: return &regs->ecx;
		case EDX: return &regs->edx;
		case EBX: return &regs->ebx;
		case ESP: return &regs->esp;
		case EBP: return &regs->ebp;
		case ESI: return &regs->esi;
		case EDI: return &regs->edi;
	}
	return NULL;
}

size_t* json_object_get_register(json_t *object, x86_reg_t *regs, const char *key)
{
	const char *regname = NULL;

	if(!regs) {
		return NULL;
	}
	regname = json_object_get_string(object, key);
	if(!regname) {
		return NULL;
	}
	return reg(regs, regname);
}

BreakpointFunc_t breakpoint_func_get(const char *key)
{
	if(!key) {
		return NULL;
	}
	{
		BreakpointFunc_t ret = NULL;
		STRLEN_DEC(key);
		VLA(char, bp_key, key_len + strlen("BP_") + 1);

		// Multi-slot support
		const char *slot = strchr(key, '#');
		if(slot) {
			key_len = slot - key;
		}

		strcpy(bp_key, "BP_");
		strncat(bp_key, key, key_len);
		ret = (BreakpointFunc_t)runconfig_func_get(bp_key);
		VLA_FREE(bp_key);
		return ret;
	}
}

__declspec(naked) void breakpoint_process()
{
	json_t *bp;
	const char *key;
	x86_reg_t *regs;

	// Breakpoint counter, required for codecave offset
	size_t i;
	int cave_exec;
	BreakpointFunc_t bp_function;

	// POPAD ignores the ESP register, so we have to implement our own mechanism
	// to be able to manipulate it.
	size_t esp_prev;

	__asm {
		// Save registers, allocate local variables
		pushad	// Needs to be first to save ESP correctly
		pushfd
		mov ebp, esp
		sub esp, __LOCAL_SIZE
		mov regs, ebp
	}

	// Initialize (needs to be here because of __declspec(naked))
	bp = NULL;
	i = 0;
	cave_exec = 1;
	esp_prev = regs->esp;

	json_object_foreach(BP_Object, key, bp) {
		if(json_object_get_hex(bp, "addr") == (regs->retaddr - CALL_LEN)) {
			break;
		}
		i++;
	}

	bp_function = (BreakpointFunc_t)breakpoint_func_get(key);
	if(bp_function) {
		cave_exec = bp_function(regs, bp);
	}
	if(cave_exec) {
		// Point return address to codecave.
		// We need to use RETN here to ensure stack consistency
		// inside the codecave - it will jump back on its own
		regs->retaddr = (size_t)(BP_CodeCave) + (i * BP_Offset);
	}
	if(esp_prev != regs->esp) {
		// ESP change requested.
		// Shift down the regs structure by the requested amount
		size_t esp_diff = regs->esp - esp_prev;
		memmove((BYTE*)(regs) + esp_diff, regs, sizeof(x86_reg_t));
		__asm {
			add ebp, esp_diff
		}
	}
	__asm {
		mov esp, ebp
		popfd
		popad
		retn
	}
}

void cave_fix(BYTE *cave, size_t bp_addr)
{
	/// Fix relative stuff
	/// ------------------

	// #1: Relative far call / jump at the very beginning
	if(cave[0] == 0xe8 || cave[0] == 0xe9)
	{
		size_t dist_old, dist_new;

		dist_old = *((size_t*)(cave + 1));
		dist_new = (dist_old + (bp_addr + CALL_LEN)) - ((size_t)cave + CALL_LEN);

		memcpy(cave + 1, &dist_new, sizeof(dist_new));

		log_printf("fixing rel.addr. 0x%08x to 0x%08x... ", dist_old, dist_new);
	}
	/// ------------------
}

int breakpoints_apply()
{
	json_t *breakpoints;
	const char *key;
	json_t *bp;
	size_t breakpoint_count;
	size_t i = 0;

	breakpoints = json_object_get(run_cfg, BREAKPOINTS);
	if(!breakpoints) {
		return -1;
	}
	breakpoint_count = json_object_size(breakpoints);

	if(!breakpoint_count) {
		log_printf("No breakpoints to set up.\n");
		return 0;
	}
	// Don't set up twice
	if(BP_CodeCave) {
		log_printf("Breakpoints already set up.\n");
		return 0;
	}

	BP_CodeCave = (BYTE*)VirtualAlloc(0, breakpoint_count * BP_Offset, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
	memset(BP_CodeCave, 0xcc, breakpoint_count * BP_Offset);

	log_printf("Setting up breakpoints... (code cave at 0x%08x)\n", BP_CodeCave);
	log_printf("-------------------------\n");

	json_object_foreach(breakpoints, key, bp) {
		size_t addr = json_object_get_hex(bp, "addr");
		size_t cavesize = json_object_get_hex(bp, "cavesize");
		BYTE* cave = NULL;
		size_t cave_dist;
		size_t bp_dist;

		i++;

		if(!addr) {
			breakpoint_count--;
			continue;
		}
		log_printf("(%2d/%2d) 0x%08x %s... ", i, breakpoint_count, addr, key);
		if(!cavesize) {
			log_printf("ERROR: no cavesize specified!\n");
			continue;
		}
		if(cavesize < BP_CodeCave_Limits[0] || cavesize > BP_CodeCave_Limits[1]) {
			log_printf("ERROR: cavesize exceeds limits! (given: %d, min: %d, max: %d)\n",
				cavesize, BP_CodeCave_Limits[0], BP_CodeCave_Limits[1]);
			continue;
		}

		// Calculate values for cave
		cave = BP_CodeCave + ((i - 1) * BP_Offset);
		cave_dist = (addr + cavesize) - ((DWORD)cave + cavesize + CALL_LEN);

		// Copy old code to cave
		memcpy(cave, UlongToPtr(addr), cavesize);
		cave_fix(cave, addr);

		// JMP addr
		cave[cavesize] = 0xe9;
		memcpy(cave + cavesize + 1, &cave_dist, sizeof(cave_dist));

		// Build breakpoint asm
		{
			BYTE bp_asm[BP_Offset];
			memset(bp_asm, 0x90, cavesize);

			bp_dist = (size_t)(breakpoint_process) - ((DWORD)addr + CALL_LEN);

			// CALL breakpoint_process
			bp_asm[0] = 0xe8;
			memcpy(bp_asm + 1, &bp_dist, sizeof(void*));
			PatchRegionNoCheck((void*)addr, bp_asm, cavesize);
		}
		log_printf("OK\n");
	}
	BP_Object = breakpoints;
	log_printf("-------------------------\n");
	return 0;
}

int breakpoints_remove()
{
	json_t *breakpoints;
	size_t breakpoint_count;

	breakpoints = json_object_get(run_cfg, BREAKPOINTS);
	if(!breakpoints) {
		return -1;
	}
	breakpoint_count = json_object_size(breakpoints);
	if(!breakpoint_count) {
		log_printf("No breakpoints to remove.\n");
		return 0;
	}

	log_printf(
		"Removing breakpoints...\n"
		"-----------------------\n"
	);
	// TODO: Implement!

	VirtualFree(BP_CodeCave, 0, MEM_RELEASE);
	return 0;
}
