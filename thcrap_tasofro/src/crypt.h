/**
  * Touhou Community Reliant Automatic Patcher
  * Tasogare Frontier support plugin
  *
  * ----
  *
  * Encryption and hashing.
  */

#pragma once

#include <thcrap.h>
#include "thcrap_tasofro.h"

class ICrypt
{
public:
	virtual ~ICrypt() {}
	virtual DWORD cryptBlock(BYTE* Data, DWORD FileSize, const DWORD* Key) = 0;
	virtual void uncryptBlock(BYTE* Data, DWORD FileSize, const DWORD* Key) = 0;
	virtual DWORD SpecialFNVHash(const char *begin, const char *end, DWORD initHash = 0x811C9DC5u) = 0;
	virtual void convertKey(DWORD *key) = 0;

	static ICrypt *instance;
};

class CryptTh135 : public ICrypt
{
public:
	virtual DWORD cryptBlock(BYTE* Data, DWORD FileSize, const DWORD* Key);
	virtual void uncryptBlock(BYTE* Data, DWORD FileSize, const DWORD* Key);
	virtual DWORD SpecialFNVHash(const char *begin, const char *end, DWORD initHash = 0x811C9DC5u);
	virtual void convertKey(DWORD *key);
};

class CryptTh145 : public ICrypt
{
private:
	BYTE *tempCopy;
	DWORD tempCopySize;
	DWORD cryptBlockInternal(BYTE* Data, DWORD FileSize, const DWORD* Key, DWORD Aux);

public:
	CryptTh145();
	~CryptTh145();
	virtual DWORD cryptBlock(BYTE* Data, DWORD FileSize, const DWORD* Key);
	virtual void uncryptBlock(BYTE* Data, DWORD FileSize, const DWORD* Key);
	virtual DWORD SpecialFNVHash(const char *begin, const char *end, DWORD initHash = 0x811C9DC5u);
	virtual void convertKey(DWORD *key);
};
