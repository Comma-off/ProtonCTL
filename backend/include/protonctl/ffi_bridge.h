#ifndef PROTONCTL_FFI_BRIDGE_H_
#define PROTONCTL_FFI_BRIDGE_H_

// C ABI surface consumed by both frontends (Kotlin via JNA, Flutter via
// `dart:ffi`). Every call that returns data hands back a heap-allocated,
// null-terminated UTF-8 JSON string that MUST be released with
// `protonctl_free_string`. Functions that only report success/failure
// return an int (0 = ok, non-zero = error; use `protonctl_last_error` to
// retrieve the message).

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32)
#define PROTONCTL_API __declspec(dllexport)
#else
#define PROTONCTL_API __attribute__((visibility("default")))
#endif

// --- lifecycle -------------------------------------------------------------

PROTONCTL_API void protonctl_free_string(char* ptr);
PROTONCTL_API const char* protonctl_last_error(void);

// --- first-start wizard / config -------------------------------------------

// Returns JSON array of SteamCandidate.
PROTONCTL_API char* protonctl_detect_steam_candidates(void);
// Validates an arbitrary path; returns a single SteamCandidate JSON object.
PROTONCTL_API char* protonctl_validate_steam_path(const char* path);

PROTONCTL_API char* protonctl_load_config(void);
// `config_json` is a full AppConfig JSON object. Returns 0 on success.
PROTONCTL_API int protonctl_save_config(const char* config_json);

// --- repository management --------------------------------------------------

PROTONCTL_API char* protonctl_list_repositories(void);
// Returns the newly added ProtonRepository JSON object, or NULL on error.
PROTONCTL_API char* protonctl_add_repository(const char* owner, const char* repo);
PROTONCTL_API int protonctl_remove_repository(const char* owner, const char* repo);

// Returns JSON array of GitHubRelease.
PROTONCTL_API char* protonctl_fetch_releases(const char* owner, const char* repo);

// Returns the latest GitHubRelease JSON object if it's newer than
// `installed_version_tag`, or the JSON literal `null` if already up to date
// (or no releases/unknown repo). NULL (not the JSON string "null") means an
// actual error - see protonctl_last_error.
PROTONCTL_API char* protonctl_check_for_update(const char* owner, const char* repo,
                                                const char* installed_version_tag);

// `repo_json` = ProtonRepository, `release_json` = GitHubRelease.
// Returns the InstalledProtonVersion JSON object. Synchronous - blocks for
// however long the download takes, with no progress feedback. Kept for
// existing callers; `protonctl_enqueue_install` below is the one to use
// for anything with a UI, since it reports real byte-level progress
// instead of leaving the caller blocked and guessing whether it's hung.
PROTONCTL_API char* protonctl_install_release(const char* repo_json, const char* release_json,
                                         const char* compat_tools_dir);

PROTONCTL_API char* protonctl_list_installed(const char* compat_tools_dir);
PROTONCTL_API int protonctl_remove_installed(const char* compat_tools_dir, const char* name);

// --- compilation pipeline ----------------------------------------------------

// Returns a plain (non-JSON) job id string.
PROTONCTL_API char* protonctl_enqueue_build(const char* repo_json, const char* ref,
                                       const char* compat_tools_dir);
// Returns the BuildJob JSON object for `job_id`, or NULL if unknown.
PROTONCTL_API char* protonctl_get_build_status(const char* job_id);
// Returns JSON array of all known BuildJob objects (queue view).
PROTONCTL_API char* protonctl_list_build_jobs(void);
PROTONCTL_API int protonctl_cancel_build(const char* job_id);

// --- async release install (real progress, doesn't block the caller) --------

// Returns a plain (non-JSON) job id string.
PROTONCTL_API char* protonctl_enqueue_install(const char* repo_json, const char* release_json,
                                         const char* compat_tools_dir);
// Returns the InstallJob JSON object for `job_id`, or NULL if unknown.
PROTONCTL_API char* protonctl_get_install_status(const char* job_id);
// Returns JSON array of all known InstallJob objects.
PROTONCTL_API char* protonctl_list_install_jobs(void);
PROTONCTL_API int protonctl_cancel_install(const char* job_id);

// --- backup / repair ----------------------------------------------------------

// `manifest_json` = PrtBakManifest (partial is fine; fields default).
PROTONCTL_API int protonctl_create_backup(const char* proton_dir, const char* prefix_dir,
                                     const char* manifest_json, const char* out_file,
                                     int compression_level);
PROTONCTL_API char* protonctl_read_manifest(const char* prtbak_file);
PROTONCTL_API int protonctl_restore_backup(const char* prtbak_file, const char* restore_root);

// Whole-library backup: every installed Proton version under
// compatibilitytools.d in one archive, for moving a whole collection to
// another instance rather than one version (+ prefix) at a time.
PROTONCTL_API int protonctl_create_library_backup(const char* compat_tools_dir, const char* out_file,
                                                    int compression_level);
PROTONCTL_API char* protonctl_read_library_manifest(const char* prtbak_file);
PROTONCTL_API int protonctl_restore_library_backup(const char* prtbak_file, const char* compat_tools_dir);

// Returns JSON array of RepairIssue.
PROTONCTL_API char* protonctl_scan_repair(const char* compat_tools_dir, const char* compat_data_dir);
PROTONCTL_API int protonctl_fix_issue(const char* issue_json);

#ifdef __cplusplus
}
#endif

#endif  // PROTONCTL_FFI_BRIDGE_H_
