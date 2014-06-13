/**
  * Touhou Community Reliant Automatic Patcher
  * Update plugin
  *
  * ----
  *
  * Update notifications for thcrap and the game itself.
  */

#include <thcrap.h>
#include "self.h"

static const char *update_url_message =
	"La nouvelle version peut-être trouvée à\n"
	"\n"
	"\t";
static const char *mbox_copy_message =
	"\n"
	"\n"
	"(Faites CTRL+C pour copier le texte et l'URL de cette boîte de dialogue dans le presse-papiers)";

/// Self-updating messages
/// ----------------------
static UINT self_msg_type[] = {
	MB_ICONINFORMATION, // SELF_OK
	MB_ICONINFORMATION, // SELF_NO_PUBLIC_KEY
	MB_ICONEXCLAMATION, // SELF_SERVER_ERROR
	MB_ICONEXCLAMATION, // SELF_DISK_ERROR
	MB_ICONERROR, // SELF_NO_SIG
	MB_ICONERROR, // SELF_SIG_FAIL
	MB_ICONINFORMATION, // SELF_REPLACE_ERROR
};

static const char *self_header_failure =
	"Une nouvelle version (${build}) de ${project} est disponible.\n"
	"\n";

static const char *self_body[] = {
	// SELF_OK
	"Le ${project} est passé à la version ${build} avec succès.\n"
	"\n"
	"Pour plus d'informations sur cette nouvelle version, rendez-vous sur\n"
	"\n"
	"\t${desc_url}",
	// SELF_NO_PUBLIC_KEY
	"Une mise à jour automatique n'a pas été tentée, suite à l'absence "
	"de signature numérique dans le patcheur en cours d'exécution.\n"
	"\n"
	"Vous pouvez télécharger l'archive manuellement sur\n"
	"\n"
	"\t${desc_url}\n"
	"\n"
	"Cependant, nous ne pouvons pas vérifier son authenticité. Soyez "
	"vigilant !",
	// SELF_SERVER_ERROR
	"Une mise à jour automatique a été tentée, mais aucun serveur de "
	"téléchargement n'a pu être atteint.\n"
	"\n"
	"La nouvelle version peut être trouvée sur\n"
	"\n"
	"\t${desc_url}\n"
	"\n"
	"En revanche, plutôt que télécharger l'archive manuellement, nous "
	"conseillons de réessayer la mise à jour automatique plus tard, "
	"puisque ce processus vérifie aussi l'authenticitié de l'archive par "
	"sa signature numérique.",
	// SELF_DISK_ERROR
	"Une mise à jour automatique a été tentée, mais l'archive n'a pas pu "
	"être sauvergardée sur le disque. Ceci est peut-être dû à l'absence "
	"de permissions d'écriture dans le répertoire ${project_short} "
	"(${thcrap_dir}).\n"
	"\n"
	"Vous pouvez télécharger l'archive manuellement sur\n"
	"\n"
	"\t${desc_url}",
	// SELF_NO_SIG
	"Une mise à jour automatique a été tentée, mais le serveur n'a pas "
	"fourni de  signature numérique pour prouver l'authenticité de "
	"l'archive.\n"
	"\n"
	"De plus, elle pourrait avoir été altérée intentionnellement.",
	// SELF_SIG_FAIL
	"Une mise à jour automatique a été tentée, mais la signature "
	"numérique de l'archive n'a pas pu être comparée à la clé publique du "
	"patcheur actuellement en cours d'exécution.\n"
	"\n"
	"Cela signifie que la mise à jour a été altérée intentionnellement.",
	// SELF_REPLACE_ERROR
	"Une mise à jour automatique a été tentée, mais la version actuelle "
	"n'a pas pu être remplacée par la nouvelle. Ceci est peut-être dû à "
	"l'absence de permissions d'écriture.\n"
	"\n"
	"Veuillez extraire manuellement la nouvelle version depuis l'archive "
	"qui a été sauvegardée dans votre répertoire ${project_short} "
	"(${thcrap_dir}${arc_fn}). "
	"Sa signature numérique a déjà été vérifiée, l'archive est "
	"authentique.\n"
	"\n"
	"Pour plus d'informations sur cette nouvelle version, rendez-vous "
	"sur\n"
	"\n"
	"\t${desc_url}"
};

const char *self_sig_error =
	"\n"
	"Nous déconseillons le téléchargement depuis le site d'origine "
	"jusqu'à ce que le problème soit résolu.";
/// ----------------------

int IsLatestBuild(json_t *build, json_t **latest)
{
	json_t *json_latest = json_object_get(runconfig_get(), "latest");
	size_t i;
	if(!json_is_string(build) || !latest || !json_latest) {
		return -1;
	}
	json_flex_array_foreach(json_latest, i, *latest) {
		if(json_equal(build, *latest)) {
			return 1;
		}
	}
	return 0;
}

int update_notify_thcrap(void)
{
	const size_t SELF_MSG_SLOT = (size_t)self_body;
	self_result_t ret = SELF_OK;
	json_t *run_cfg = runconfig_get();
	DWORD min_build = json_object_get_hex(run_cfg, "thcrap_version_min");
	if(min_build > PROJECT_VERSION()) {
		const char *thcrap_dir = json_object_get_string(run_cfg, "thcrap_dir");
		const char *thcrap_url = json_object_get_string(run_cfg, "thcrap_url");
		char *arc_fn = NULL;
		const char *self_msg = NULL;
		char min_build_str[11];
		str_hexdate_format(min_build_str, min_build);
		ret = self_update(thcrap_dir, &arc_fn);
		strings_sprintf(SELF_MSG_SLOT,
			"%s%s",
			ret != SELF_OK ? self_header_failure : "",
			self_body[ret]
		);
		if(ret == SELF_NO_SIG || ret == SELF_SIG_FAIL) {
			strings_strcat(SELF_MSG_SLOT, self_sig_error);
		}
		strings_replace(SELF_MSG_SLOT, "${project}", PROJECT_NAME());
		strings_replace(SELF_MSG_SLOT, "${project_short}", PROJECT_NAME_SHORT());
		strings_replace(SELF_MSG_SLOT, "${build}", min_build_str);
		strings_replace(SELF_MSG_SLOT, "${thcrap_dir}", thcrap_dir);
		strings_replace(SELF_MSG_SLOT, "${desc_url}", thcrap_url);
		self_msg = strings_replace(SELF_MSG_SLOT, "${arc_fn}", arc_fn);
		log_mboxf(NULL, MB_OK | self_msg_type[ret], self_msg);
		SAFE_FREE(arc_fn);
	}
	return ret;
}

int update_notify_game(void)
{
	json_t *run_cfg = runconfig_get();
	const json_t *title = runconfig_title_get();
	json_t *build = json_object_get(run_cfg, "build");
	json_t *latest = NULL;
	int ret;
	
	if(!json_is_string(build) || !json_is_string(title)) {
		return -1;
	}
	ret = IsLatestBuild(build, &latest) == 0 && json_is_string(latest);
	if(ret) {
		const char *url_update = json_object_get_string(run_cfg, "url_update");
		log_mboxf("Version obsolète détectée", MB_OK | MB_ICONINFORMATION,
			"Vous utilisez une version obsolète de %s (%s).\n"
			"\n"
			"Bien qu'il soit confirmé que %s fonctionne sous cette version, nous vous recommandons de mettre à jour"
			"votre jeu vers la version officielle la plus récente (%s).%s%s%s%s",
			json_string_value(title), json_string_value(build), PROJECT_NAME_SHORT(),
			json_string_value(latest),
			url_update ? "\n\n": "",
			url_update ? update_url_message : "",
			url_update ? url_update : "",
			url_update ? mbox_copy_message : ""
		);
	}
	return ret;
}
