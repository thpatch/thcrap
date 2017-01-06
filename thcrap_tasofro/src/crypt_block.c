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

void uncrypt_block(BYTE* Data, DWORD FileSize, DWORD* Key)
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

DWORD crypt_block_internal(BYTE* Data, DWORD FileSize, DWORD* Key, DWORD Aux)
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

DWORD crypt_block(BYTE* Data, DWORD FileSize, DWORD* Key)
{
	DWORD Aux;
	VLA(BYTE, tempCopy, FileSize);
	memcpy(tempCopy, Data, FileSize);
	Aux = crypt_block_internal(tempCopy, FileSize, Key, Key[0]); // This call seems to give the correct Aux value.
	VLA_FREE(tempCopy);

	return crypt_block_internal(Data, FileSize, Key, Aux);
}
