/**
 * @file firebase_rtdb.h
 * @brief Minimal Firebase Realtime Database client for ESP-IDF.
 *
 * Talks to Firebase over its public REST APIs (TLS, certificate bundle):
 *  - anonymous sign-up and token refresh via the Identity Toolkit / Secure Token APIs,
 *  - reads and partial updates (PATCH) against the Realtime Database.
 *
 * One persistent HTTPS connection to the database host is kept alive between
 * requests, so a sync cycle costs a single TLS handshake.
 *
 * All functions are serialised by an internal mutex.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialise the client.
 *
 * @param api_key       Firebase Web API key.
 * @param database_url  Realtime Database URL, with or without scheme and
 *                      trailing slash (e.g. "https://x-default-rtdb.firebaseio.com/").
 */
esp_err_t firebase_rtdb_init(const char *api_key, const char *database_url);

/**
 * @brief Obtain a fresh ID token.
 *
 * Refreshes the existing token when a refresh token is available, otherwise
 * performs an anonymous sign-up. Requires network connectivity.
 */
esp_err_t firebase_rtdb_authenticate(void);

/** @brief True while a non-expired ID token is held. */
bool firebase_rtdb_is_ready(void);

/** @brief True when there is no token yet, or it is close to expiring. */
bool firebase_rtdb_auth_due(void);

/**
 * @brief Merge a JSON object into the node at `path` (HTTP PATCH).
 *
 * @param path  Node path without ".json", e.g. "Sensor". Must be URL-safe.
 * @param json  Serialised JSON object.
 */
esp_err_t firebase_rtdb_patch(const char *path, const char *json);

/**
 * @brief Read the node at `path` as raw JSON text.
 *
 * @param out       Destination buffer (NUL-terminated on success).
 * @param out_size  Size of `out` in bytes.
 */
esp_err_t firebase_rtdb_get(const char *path, char *out, size_t out_size);

#ifdef __cplusplus
}
#endif
