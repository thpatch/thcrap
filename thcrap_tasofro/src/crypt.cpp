/**
  * Touhou Community Reliant Automatic Patcher
  * Tasogare Frontier support plugin
  *
  * ----
  *
  * XOR-based encryption.
  */

#include <thcrap.h>
#include "thcrap_tasofro.h"
#include "crypt.h"

ICrypt* ICrypt::instance = nullptr;

DWORD CryptTh135::cryptBlock(BYTE *TH_RESTRICT Data, DWORD FileSize, const DWORD *TH_RESTRICT Key)
{
	if (FileSize) {
		size_t i = 0;

		if (FileSize >= 16) {
			do {
				size_t index = i >> 2;
				((DWORD *TH_RESTRICT)Data)[index + 0] ^= Key[0];
				((DWORD *TH_RESTRICT)Data)[index + 1] ^= Key[1];
				((DWORD *TH_RESTRICT)Data)[index + 2] ^= Key[2];
				((DWORD *TH_RESTRICT)Data)[index + 3] ^= Key[3];
			} while ((i += 16) <= FileSize - 16);
		}
		if (size_t remaining = FileSize & 0xF) {
			const BYTE *TH_RESTRICT key_byte_ptr = (const BYTE *TH_RESTRICT)Key;
			do {
				Data[i++] ^= *key_byte_ptr++;
			} while (--remaining);
		}
	}
	return 0;
}

void CryptTh135::uncryptBlock(BYTE *TH_RESTRICT Data, DWORD FileSize, const DWORD *TH_RESTRICT Key)
{
	this->cryptBlock(Data, FileSize, Key);
}

// Normalized Hash
DWORD CryptTh135::SpecialFNVHash(const char *begin, const char *end, DWORD initHash)
{
	DWORD hash; // eax@1
	DWORD ch; // esi@2

	int inMBCS = 0;
	for (hash = initHash; begin != end; hash = ch ^ 0x1000193 * hash)
	{
		ch = *begin++;
		if (!inMBCS && ((unsigned char)ch >= 0x81u && (unsigned char)ch <= 0x9Fu || (unsigned char)ch + 32 <= 0x1Fu)) inMBCS = 2;
		if (!inMBCS)
		{
			ch = tolower(ch);  // bad ass style but WORKS PERFECTLY!
			if (ch == '/') ch = '\\';
		}
		else inMBCS--;
	}
	return hash;
}

void CryptTh135::convertKey(DWORD*)
{}

#define USE_UNROLLED_CRYPT_LOOPS 1

static constexpr int8_t sse_mask_array[] = {
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

DWORD cryptBlockCalcAux(const BYTE *TH_RESTRICT Data, DWORD FileSize, const DWORD *TH_RESTRICT Key)
{
	DWORD aux = Key[0];
	if (FileSize) {
		size_t i = 0;
		__m128 aux_wide = _mm_setzero_ps();
		__m128 key_wide = _mm_loadu_ps((float*)Key);
		if (FileSize >= 16) {
			do {
				aux_wide = _mm_xor_ps(aux_wide, _mm_loadu_ps((float*)(Data + i)));
			} while ((i += 16) <= FileSize - 16);
			if (FileSize / 16 & 1) {
				aux_wide = _mm_xor_ps(aux_wide, key_wide);
			}
		}
		aux_wide = _mm_xor_ps(aux_wide, _mm_and_ps(key_wide, _mm_loadu_ps((float*)&sse_mask_array[16 - (FileSize & 0xF)])));
		__m128i aux_widei = *(__m128i*)&aux_wide;
		aux_widei = _mm_xor_si128(aux_widei, _mm_srli_si128(aux_widei, 8));
		aux_widei = _mm_xor_si128(aux_widei, _mm_srli_si128(aux_widei, 4));
		aux ^= _mm_cvtsi128_si32(aux_widei);
#if USE_UNROLLED_CRYPT_LOOPS
		if (size_t dwords_width_remaining = FileSize & 0b1100) {
			i += dwords_width_remaining;
			switch (dwords_width_remaining) {
				default:
					TH_UNREACHABLE;
				case 0b1100:
					aux ^= ((DWORD *TH_RESTRICT)(Data + i))[-3];
				case 0b1000:
					aux ^= ((DWORD *TH_RESTRICT)(Data + i))[-2];
				case 0b0100:
					aux ^= ((DWORD *TH_RESTRICT)(Data + i))[-1];
			}
		}
		if (size_t bytes_remaining = FileSize & 0b11) {
			DWORD temp = 0;
			switch (bytes_remaining) {
				default:
					TH_UNREACHABLE;
				case 3:
					temp = Data[i + 2];
					temp <<= 8;
				case 2:
					temp |= Data[i + 1];
					temp <<= 8;
				case 1:
					temp |= Data[i];
			}
			aux ^= temp;
		}
#else
		if (size_t remaining = FileSize & 0xF) {
			size_t j = 0;
			do {
				aux ^= Data[i++] << (j & 31);
				j += 8;
			} while (--remaining);
		}
#endif
	}
	return aux;
}

DWORD cryptBlockInternal(BYTE *TH_RESTRICT Data, DWORD FileSize, const DWORD *TH_RESTRICT Key, DWORD Aux)
{
	if (FileSize) {
		const BYTE *TH_RESTRICT key_byte_ptr = (const BYTE *TH_RESTRICT)Key;
		BYTE *TH_RESTRICT cur_data = Data + FileSize;

		key_byte_ptr += FileSize & 0xF;

		if (size_t bytes_remaining = FileSize & 0b11) {
			cur_data -= bytes_remaining;
			key_byte_ptr -= bytes_remaining;
			DWORD temp;
			switch (bytes_remaining) {
				default:
					TH_UNREACHABLE;
				case 3:
					temp = cur_data[2];
					cur_data[2] = Aux >> 16;
					Aux ^= (temp ^ key_byte_ptr[2]) << 16;
				case 2:
					temp = cur_data[1];
					cur_data[1] = Aux >> 8;
					Aux ^= (temp ^ key_byte_ptr[1]) << 8;
				case 1:
					temp = cur_data[0];
					cur_data[0] = Aux;
					Aux ^= temp ^ key_byte_ptr[0];
			}
		}

		if (size_t dwords_remaining = FileSize >> 2) {

			DWORD keyA = Key[dwords_remaining - 1 & 0b11];
			DWORD keyB = Key[dwords_remaining - 2 & 0b11];
			DWORD keyC = Key[dwords_remaining - 3 & 0b11];
			DWORD keyD = Key[dwords_remaining - 4 & 0b11];
			DWORD temp;
			do {
				cur_data -= 4;

				temp = *(DWORD *TH_RESTRICT)cur_data;
				*(DWORD *TH_RESTRICT)cur_data = Aux;
				Aux ^= temp ^ keyA;

				temp = keyA;
				keyA = keyB;
				keyB = keyC;
				keyC = keyD;
				keyD = temp;
			} while (cur_data != Data);
		}
	}
	return Aux;
}

DWORD CryptTh145::cryptBlock(BYTE *TH_RESTRICT Data, DWORD FileSize, const DWORD *TH_RESTRICT Key)
{
	DWORD Aux = cryptBlockCalcAux(Data, FileSize, Key); // This call seems to give the correct Aux value.
	return cryptBlockInternal(Data, FileSize, Key, Aux);
}

void CryptTh145::uncryptBlock(BYTE *TH_RESTRICT Data, DWORD FileSize, const DWORD *TH_RESTRICT Key)
{
	
	if (FileSize) {
		DWORD aux = Key[0];
		size_t i = 0;
		if (FileSize >= 16) {
			__m128 key_wide = _mm_loadu_ps((float*)Key);
			__m128 aux_wide = key_wide;
			BYTE *TH_RESTRICT data_end = Data + FileSize - 16;
			do {
				__m128 temp = _mm_loadu_ps((float*)Data);
				aux_wide = _mm_shuffle_ps(_mm_movelh_ps(aux_wide, temp), temp, 0x98);
				__m128 xor_data = key_wide;
				_mm_storeu_ps((float*)Data, _mm_xor_ps(_mm_xor_ps(xor_data, temp), aux_wide));
				*(__m128i*)&aux_wide = _mm_srli_si128(*(__m128i*)&temp, 12);
			} while ((Data += 16) <= data_end);
			aux = _mm_cvtsi128_si32(*(__m128i*)&aux_wide);
		}
#if USE_UNROLLED_CRYPT_LOOPS
		const BYTE *TH_RESTRICT key_byte_ptr = (const BYTE *TH_RESTRICT)Key;
		if (size_t dwords_width_remaining = FileSize & 0b1100) {
			Data += dwords_width_remaining;
			key_byte_ptr += dwords_width_remaining;
			DWORD temp;
			switch (dwords_width_remaining) {
				default:
					TH_UNREACHABLE;
				case 0b1100:
					temp = ((DWORD *TH_RESTRICT)Data)[-3];
					((DWORD *TH_RESTRICT)Data)[-3] ^= aux ^ ((DWORD *TH_RESTRICT)key_byte_ptr)[-3];
					aux = temp;
				case 0b1000:
					temp = ((DWORD *TH_RESTRICT)Data)[-2];
					((DWORD *TH_RESTRICT)Data)[-2] ^= aux ^ ((DWORD *TH_RESTRICT)key_byte_ptr)[-2];
					aux = temp;
				case 0b0100:
					temp = ((DWORD *TH_RESTRICT)Data)[-1];
					((DWORD *TH_RESTRICT)Data)[-1] ^= aux ^ ((DWORD *TH_RESTRICT)key_byte_ptr)[-1];
					aux = temp;
			}
		}
		if (size_t bytes_remaining = FileSize & 0b11) {
			Data += bytes_remaining;
			key_byte_ptr += bytes_remaining;
			switch (bytes_remaining) {
				default:
					TH_UNREACHABLE;
				case 3:
					Data[-3] ^= (uint8_t)aux ^ key_byte_ptr[-3];
					aux >>= 8;
				case 2:
					Data[-2] ^= (uint8_t)aux ^ key_byte_ptr[-2];
					aux >>= 8;
				case 1:
					Data[-1] ^= (uint8_t)aux ^ key_byte_ptr[-1];
			}
		}
#else
		if (size_t remaining = FileSize & 0xF) {
			const BYTE *TH_RESTRICT key_byte_ptr = (const BYTE *TH_RESTRICT)Key;
			do {
				uint8_t temp = *Data;
				*Data++ = temp ^ (uint8_t)aux ^ *key_byte_ptr++;
				aux >>= 8;
				aux |= temp << 24;
			} while (--remaining);
		}
#endif
	}
}

// Normalized Hash
DWORD CryptTh145::SpecialFNVHash(const char *begin, const char *end, DWORD initHash)
{
	DWORD hash; // eax@1
	DWORD ch; // esi@2

	int inMBCS = 0;
	for (hash = initHash; begin != end; hash = (hash ^ ch) * 0x1000193)
	{
		ch = *begin++;
		if (!inMBCS && ((unsigned char)ch >= 0x81u && (unsigned char)ch <= 0x9Fu || (unsigned char)ch + 32 <= 0x1Fu)) inMBCS = 2;
		if (!inMBCS)
		{
			ch = tolower(ch);  // bad ass style but WORKS PERFECTLY!
			if (ch == '/') ch = '\\';
		}
		else inMBCS--;
	}
	return hash * -1;
}

void CryptTh145::convertKey(DWORD* key)
{
	key[0] = key[0] *-1;
	key[1] = key[1] *-1;
	key[2] = key[2] *-1;
	key[3] = key[3] *-1;
}

DWORD CryptTh175::cryptBlock(BYTE *TH_RESTRICT, DWORD, const DWORD *TH_RESTRICT)
{
	// For now, we work directly with the uncrypted files
	return 0;
}

void CryptTh175::uncryptBlock(BYTE *TH_RESTRICT, DWORD, const DWORD *TH_RESTRICT)
{
	// For now, we work directly with the uncrypted files
}

DWORD CryptTh175::SpecialFNVHash(const char *filename, const char *, DWORD)
{
	if (strcmp(filename, "game.exe") == 0) {
		// I don't know the filename for this hash, but I need to have one, so I arbitrarily decide it's "game.exe".
		return 0x1f47c0c8;
	}

	uint64_t hash = 0x811C9DC5;
	for (int i = 0; filename[i]; i++) {
		char c = filename[i];
		if (c == '\\') {
			c = '/';
		}
		hash = ((hash ^ c) * 0x1000193) & 0xFFFFFFFF;
	}
	return (uint32_t)hash;
}

void CryptTh175::convertKey(DWORD*)
{
	// Nothing to do for th175
}
