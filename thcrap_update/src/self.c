/**
  * Touhou Community Reliant Automatic Patcher
  * Update plugin
  *
  * ----
  *
  * Digitally signed, automatic updates of thcrap itself.
  */

#include <thcrap.h>
#include <WinCrypt.h>
#include "update.h"
#include "self.h"

#define TEMP_FN_LEN 41

const char *ARC_FN = "thcrap.zip";
const char *SIG_FN = "thcrap.zip.sig";
const char *PREFIX_BACKUP = "thcrap_old_%s_";
const char *PREFIX_NEW = "thcrap_new_";

static void* self_download(DWORD *arc_len, const char *arc_fn)
{
	// Yup, hardcoded download URLs. After all, these should be a property
	// of the engine, and now that it is capable of updating itself,
	// there's no need to include these in any patch.
	json_t *self_ddls = json_pack("{s[ss]}", "servers",
		"http://thcrap.thpatch.net/",
		"http://thcrap.nmlgc.net/releases/"
	);
	json_t *self_ddl_servers = ServerInit(self_ddls);
	void *arc_buf = ServerDownloadFile(self_ddl_servers, arc_fn, arc_len, NULL);
	self_ddls = json_decref_safe(self_ddls);
	return arc_buf;
}

// A superior GetTempFileName.
static int self_tempname(char *fn, size_t len, const char *prefix)
{
	int ret = -1;
	size_t prefix_len = prefix ? strlen(prefix) : 0;
	if(fn && len > 8 && prefix && prefix_len < len - 1) {
		HCRYPTPROV hCryptProv;
		size_t rnd_num = (len - 1) / 2;
		VLA(BYTE, rnd, rnd_num);
		size_t len_w = len;
		size_t len_conv = 0;
		size_t i = 0;

		ZeroMemory(fn, len);
		ret = W32_ERR_WRAP(CryptAcquireContext(
			&hCryptProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT
		));
		if(!ret) {
			CryptGenRandom(hCryptProv, rnd_num, (BYTE*)rnd);
		} else {
			LARGE_INTEGER t;
			QueryPerformanceCounter(&t);
			t.HighPart ^= GetCurrentProcessId();
			for(i = 0; i < rnd_num / sizeof(t); i++) {
				memcpy(rnd + (i * sizeof(t)), &t, sizeof(t));
			}
		}
		for(i = 0; i < rnd_num; i++) {
			sprintf(fn + (i * 2), "%02x", rnd[i]);
		}
		memcpy(fn, prefix, prefix_len);
		VLA_FREE(rnd);
		CryptReleaseContext(hCryptProv, 0);
	}
	return ret;
}

static int self_pubkey_from_signer(PCCERT_CONTEXT *context)
{
	int ret = -1;
	HMODULE self_mod = NULL;
	HCERTSTORE hStore = NULL;
	HCRYPTMSG hMsg = NULL;
	DWORD msg_type = 0;
	DWORD param_len;
	DWORD signer_num;
	DWORD i;

	if(!context || !GetModuleHandleEx(
		GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT | GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
		(LPTSTR)self_pubkey_from_signer,
		&self_mod
	)) {
		return -1;
	}
	{
		// CryptQueryObject() forces us to use the W version, but only
		// our U version can calculate the length of the string, so...
		size_t self_fn_len = GetModuleFileNameU(self_mod, NULL, 0);
		VLA(wchar_t, self_fn, self_fn_len * UTF8_MUL);
		VLA(char, self_fn_utf8, self_fn_len);
		GetModuleFileNameU(self_mod, self_fn_utf8, self_fn_len);
		GetModuleFileNameW(self_mod, self_fn, self_fn_len);

		log_printf(
			"Retrieving public key from the signer certificate of %s... ", self_fn_utf8
		);

		ret = W32_ERR_WRAP(CryptQueryObject(
			CERT_QUERY_OBJECT_FILE, self_fn,
			CERT_QUERY_CONTENT_FLAG_PKCS7_SIGNED_EMBED,
			CERT_QUERY_FORMAT_FLAG_BINARY,
			0, &msg_type, NULL, NULL, &hStore, &hMsg, NULL
		));
		VLA_FREE(self_fn_utf8);
		VLA_FREE(self_fn);
	}
	if(ret) {
		goto end;
	}

	ret = W32_ERR_WRAP(CryptMsgGetParam(
		hMsg, CMSG_SIGNER_COUNT_PARAM, 0, &signer_num, &param_len
	));
	if(ret) {
		goto end;
	}
	for(i = 0; i < signer_num && !(*context); i++) {
		PCERT_INFO signer_info = NULL;

		ret = W32_ERR_WRAP(CryptMsgGetParam(
			hMsg, CMSG_SIGNER_INFO_PARAM, i, NULL, &param_len
		));
		if(ret) {
			continue;
		}
		signer_info = malloc(param_len);
		// MANDATORY
		ZeroMemory(signer_info, param_len);
		ret = W32_ERR_WRAP(CryptMsgGetParam(
			hMsg, CMSG_SIGNER_CERT_INFO_PARAM, i, signer_info, &param_len
		));
		if(ret) {
			continue;
		}
		*context = CertGetSubjectCertificateFromStore(
			hStore, msg_type, signer_info
		);
		ret = *context ? 0 : GetLastError();
		SAFE_FREE(signer_info);
	}
	log_printf(ret ? "not found\n" : "OK\n");
end:
	CertCloseStore(hStore, 0);
	CryptMsgClose(hMsg);
	return ret;
}

static int self_verify_buffer(
	const void *file_buf,
	const size_t file_len,
	const json_t *sig,
	HCRYPTPROV hCryptProv,
	HCRYPTKEY hPubKey,
	ALG_ID hash_alg
)
{
	int ret = -1;
	const size_t sig_base64_len = json_string_length(sig);
	const char *sig_base64 = json_string_value(sig);
	DWORD i, j;
	DWORD sig_len = 0;
	BYTE *sig_buf = NULL;
	HCRYPTHASH hHash = 0;

	if(!file_buf || !file_len || !sig_base64 || !sig_base64_len) {
		goto end;
	}
	ret = W32_ERR_WRAP(CryptStringToBinaryA(
		sig_base64, sig_base64_len, CRYPT_STRING_BASE64, NULL, &sig_len, NULL, NULL
	));
	if(!ret) {
		sig_buf = malloc(sig_len);
		ret = W32_ERR_WRAP(CryptStringToBinaryA(
			sig_base64, sig_base64_len, CRYPT_STRING_BASE64, sig_buf, &sig_len, NULL, NULL
		));
	}
	if(ret) {
		log_printf("invalid Base64 string\n");
		goto end;
	}
	// Reverse the signature...
	// (http://dev.unreliabledevice.net/windows-crypto-api-nightmares-rsa-signature-padding-and-byte-order-ramblings/)
	for(i = 0, j = sig_len - 1; i < j; i++, j--) {
		BYTE t = sig_buf[i];
		sig_buf[i] = sig_buf[j];
		sig_buf[j] = t;
	}
	ret = W32_ERR_WRAP(CryptCreateHash(hCryptProv, hash_alg, 0, 0, &hHash));
	if(ret) {
		log_printf("couldn't create hash object\n");
		goto end;
	}
	ret = W32_ERR_WRAP(CryptHashData(hHash, (BYTE*)file_buf, file_len, 0));
	if(ret) {
		log_printf("couldn't hash the file data?!?)\n");
		goto end;
	}
	ret = W32_ERR_WRAP(CryptVerifySignature(
		hHash, sig_buf, sig_len, hPubKey, NULL, 0
	));
	log_printf(ret ? "invalid\n" : "valid\n");
end:
	CryptDestroyHash(hHash);
	SAFE_FREE(sig_buf);
	return ret;
}

static ALG_ID self_alg_from_str(const char *hash)
{
	if(hash) {
		if(!stricmp(hash, "SHA1")) {
			return CALG_SHA1;
		} else if(!stricmp(hash, "SHA256")) {
			return CALG_SHA_256;
		}
	}
	return 0;
}

static self_result_t self_verify(const void *zip_buf, size_t zip_len, json_t *sig, PCCERT_CONTEXT context)
{
	self_result_t ret = SELF_NO_SIG;
	int local_ret;
	const json_t *sig_sig = json_object_get(sig, "sig");
	const char *sig_alg = json_object_get_string(sig, "alg");
	const char *key = NULL;
	json_t *val = NULL;
	ALG_ID hash_alg = self_alg_from_str(sig_alg);
	HCRYPTPROV hCryptProv = 0;
	HCRYPTKEY hPubKey = 0;

	if(!zip_buf || !zip_len || !json_is_string(sig_sig) || !context || !sig_alg) {
		return ret;
	}
	log_printf("Verifying archive signature... ");
	if(!hash_alg) {
		log_func_printf("Unsupported hash algorithm ('%s')!\n", sig_alg);
		goto end;
	}
	ret = SELF_NO_PUBLIC_KEY;
	local_ret = W32_ERR_WRAP(CryptAcquireContext(
		&hCryptProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT
	));
	if(local_ret) {
		goto end;
	}
	local_ret = W32_ERR_WRAP(CryptImportPublicKeyInfo(
		hCryptProv, X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
		&context->pCertInfo->SubjectPublicKeyInfo, &hPubKey
	));
	if(local_ret) {
		log_func_printf("Invalid public key!\n");
		goto end;
	}
	ret = self_verify_buffer(
		zip_buf, zip_len, sig_sig, hCryptProv, hPubKey, hash_alg
	) ? SELF_SIG_FAIL : SELF_OK;
end:
	CryptReleaseContext(hCryptProv, 0);
	return ret;
}

static int self_move_to_dir(const char *dst_dir, const char *fn)
{
	int ret;
	size_t full_fn_len = strlen(dst_dir) + 1 + strlen(fn) + 1;
	VLA(char, full_fn, full_fn_len);
	sprintf(full_fn, "%s/%s", dst_dir, fn);
	dir_create_for_fn(full_fn);
	ret = W32_ERR_WRAP(MoveFile(fn, full_fn));
	RemoveDirectory(fn);
	VLA_FREE(full_fn);
	return ret;
}

static self_result_t self_replace(zip_t *zip)
{
	self_result_t ret = SELF_REPLACE_ERROR;
	if(zip) {
		size_t prefix_backup_len = _scprintf(PREFIX_BACKUP, PROJECT_VERSION_STRING()) + 1;
		VLA(char, prefix_backup, prefix_backup_len);
		char backup_dir[TEMP_FN_LEN];
		const char *fn;
		json_t *val;

		sprintf(prefix_backup, PREFIX_BACKUP, PROJECT_VERSION_STRING());
		self_tempname(backup_dir, sizeof(backup_dir), prefix_backup);
		VLA_FREE(prefix_backup);

		// We don't error-check CreateDirectory(), as that might return
		// ERROR_ALREADY_EXISTS when directories are created recursively.
		// If this fails, writing should fail too.
		log_printf("Replacing engine...\n");
		CreateDirectoryU(backup_dir, NULL);
		json_object_foreach(zip_list(zip), fn, val) {
			int local_ret = self_move_to_dir(backup_dir, fn);
			if(
				local_ret == ERROR_FILE_NOT_FOUND
				|| local_ret == ERROR_PATH_NOT_FOUND
			) {
				local_ret = 0;
			}
			if(local_ret || zip_file_unzip(zip, fn)) {
				goto end;
			}
		}
		ret = SELF_OK;
	}
end:
	return ret;
}

self_result_t self_update(const char *thcrap_dir, char **arc_fn_ptr)
{
	self_result_t ret;
	char arc_fn[TEMP_FN_LEN];
	DWORD arc_len = 0;
	void *arc_buf = NULL;
	zip_t *arc = NULL;
	DWORD sig_len = 0;
	void *sig_buf = NULL;
	json_t *sig = NULL;
	PCCERT_CONTEXT context = NULL;

	size_t cur_dir_len = GetCurrentDirectory(0, NULL) + 1;
	VLA(char, cur_dir, cur_dir_len);
	GetCurrentDirectory(cur_dir_len, cur_dir);
	SetCurrentDirectory(thcrap_dir);

	if(self_pubkey_from_signer(&context)) {
		ret = SELF_NO_PUBLIC_KEY;
		goto end;
	}
	arc_buf = self_download(&arc_len, ARC_FN);
	if(!arc_buf) {
		ret = SELF_SERVER_ERROR;
		goto end;
	}
	sig_buf = self_download(&sig_len, SIG_FN);
	sig = json_loadb_report(sig_buf, sig_len, JSON_DISABLE_EOF_CHECK, SIG_FN);
	if(!sig) {
		ret = SELF_NO_SIG;
		goto end;
	}
	ret = self_verify(arc_buf, arc_len, sig, context);
	if(ret != SELF_OK) {
		goto end;
	}
	self_tempname(arc_fn, sizeof(arc_fn), PREFIX_NEW);
	strcpy(arc_fn + sizeof(arc_fn) - 1 - strlen(".zip"), ".zip");
	if(file_write(arc_fn, arc_buf, arc_len)) {
		ret = SELF_DISK_ERROR;
		goto end;
	}
	arc = zip_open(arc_fn);
	ret = self_replace(arc);
end:
	sig = json_decref_safe(sig);
	SAFE_FREE(sig_buf);
	SAFE_FREE(arc_buf);
	CertFreeCertificateContext(context);
	zip_close(arc);
	if(ret != SELF_REPLACE_ERROR) {
		DeleteFile(arc_fn);
	}
	if(arc_fn_ptr) {
		*arc_fn_ptr = malloc(TEMP_FN_LEN);
		memcpy(*arc_fn_ptr, arc_fn, TEMP_FN_LEN);
	}
	SetCurrentDirectory(cur_dir);
	VLA_FREE(cur_dir);
	return ret;
}
