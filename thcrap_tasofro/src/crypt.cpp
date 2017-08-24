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

DWORD CryptTh135::cryptBlock(BYTE* Data, DWORD FileSize, DWORD* Key)
{
	for (DWORD j = 0; j < FileSize / 4; j++)
	{
		*((DWORD*)Data + j) ^= Key[j % 4];
	}

	DWORD remain = FileSize % 4;
	if (remain)
	{
		DWORD tk = Key[FileSize / 4 % 4];
		for (DWORD j = 0; j < remain; j++)
		{
			Data[FileSize - remain + j] ^= tk & 0xFF;
			tk >>= 8;
		}
	}
	return 0;
}

void CryptTh135::uncryptBlock(BYTE* Data, DWORD FileSize, DWORD* Key)
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



CryptTh145::CryptTh145()
	: tempCopy(nullptr), tempCopySize(0)
{}

CryptTh145::~CryptTh145()
{
	if (this->tempCopy) {
		delete[] this->tempCopy;
	}
}

DWORD CryptTh145::cryptBlockInternal(BYTE* Data, DWORD FileSize, DWORD* Key, DWORD Aux)
{
	BYTE *key = (BYTE*)Key;
	BYTE *aux = (BYTE*)&Aux;
	int i;

	for (i = FileSize - 1; i >= 0; i--) {
		BYTE unencByte = Data[i];
		BYTE encByte = aux[i % 4];
		Data[i] = aux[i % 4];
		aux[i % 4] = unencByte ^ encByte ^ key[i % 16];
	}
	return Aux;
}

DWORD CryptTh145::cryptBlock(BYTE* Data, DWORD FileSize, DWORD* Key)
{
	if (FileSize > this->tempCopySize) {
		if (this->tempCopy) {
			delete[] this->tempCopy;
		}
		this->tempCopy = new BYTE[FileSize];
		this->tempCopySize = FileSize;
	}

	DWORD Aux;
	memcpy(this->tempCopy, Data, FileSize);
	Aux = this->cryptBlockInternal(tempCopy, FileSize, Key, Key[0]); // This call seems to give the correct Aux value.

	return this->cryptBlockInternal(Data, FileSize, Key, Aux);
}

void CryptTh145::uncryptBlock(BYTE* Data, DWORD FileSize, DWORD* Key)
{
	BYTE *key = (BYTE*)Key;
	BYTE aux[4];
	DWORD i;

	for (i = 0; i < 4; i++) {
		aux[i] = key[i];
	}

	for (i = 0; i < FileSize; i++) {
		BYTE tmp = Data[i];
		Data[i] = Data[i] ^ key[i % 16] ^ aux[i % 4];
		aux[i % 4] = tmp;
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
