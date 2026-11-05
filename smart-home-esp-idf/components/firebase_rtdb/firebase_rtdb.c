#include "firebase_rtdb.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "firebase";

#define SIGNUP_URL_FMT   "https://identitytoolkit.googleapis.com/v1/accounts:signUp?key=%s"
#define REFRESH_URL_FMT  "https://securetoken.googleapis.com/v1/token?key=%s"

#define HTTP_TIMEOUT_MS  10000
#define HTTP_RX_BUF      2048
#define HTTP_TX_BUF      2048  /* the request line carries the ~1 KB ID token */
#define AUTH_RESP_MAX    4096
#define URL_MAX          1792
#define ERR_RESP_MAX     256

typedef struct {
    char *buf;
    size_t cap;
    size_t len;
} rx_ctx_t;

typedef struct {
    char api_key[128];
    char host[128];
    char id_token[1536];
    char refresh_token[512];
    int64_t refresh_at_us;  /* time after which a refresh is due   */
    int64_t expires_at_us;  /* time after which the token is dead  */
    bool has_token;
    esp_http_client_handle_t db_client;
    SemaphoreHandle_t lock;
} firebase_t;

static firebase_t s_fb;
static rx_ctx_t s_db_rx;

/* -------------------------------------------------------------------------- */
/* HTTP helpers                                                               */
/* -------------------------------------------------------------------------- */

static esp_err_t on_http_event(esp_http_client_event_t *evt)
{
    rx_ctx_t *ctx = (rx_ctx_t *)evt->user_data;

    if (evt->event_id == HTTP_EVENT_ON_DATA && ctx && ctx->buf && ctx->cap > 0) {
        const size_t room = ctx->cap - ctx->len - 1;
        const size_t n = ((size_t)evt->data_len < room) ? (size_t)evt->data_len : room;
        memcpy(ctx->buf + ctx->len, evt->data, n);
        ctx->len += n;
        ctx->buf[ctx->len] = '\0';
    }
    return ESP_OK;
}

/* One-shot POST used for the (infrequent) authentication calls. */
static esp_err_t http_post_oneshot(const char *url, const char *content_type,
                                   const char *body, char *resp, size_t resp_size,
                                   int *status)
{
    rx_ctx_t ctx = {.buf = resp, .cap = resp_size, .len = 0};
    if (resp && resp_size) {
        resp[0] = '\0';
    }

    const esp_http_client_config_t cfg = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .event_handler = on_http_event,
        .user_data = &ctx,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = HTTP_TIMEOUT_MS,
        .buffer_size = HTTP_RX_BUF,
        .buffer_size_tx = 1024,
    };

    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (client == NULL) {
        return ESP_ERR_NO_MEM;
    }

    esp_http_client_set_header(client, "Content-Type", content_type);
    esp_http_client_set_post_field(client, body, (int)strlen(body));

    esp_err_t err = esp_http_client_perform(client);
    *status = (err == ESP_OK) ? esp_http_client_get_status_code(client) : -1;

    esp_http_client_cleanup(client);
    return err;
}

static void close_db_client(void)
{
    if (s_fb.db_client) {
        esp_http_client_cleanup(s_fb.db_client);
        s_fb.db_client = NULL;
    }
}

/* -------------------------------------------------------------------------- */
/* Token handling                                                             */
/* -------------------------------------------------------------------------- */

static bool token_valid_locked(void)
{
    return s_fb.has_token && esp_timer_get_time() < s_fb.expires_at_us;
}

static esp_err_t store_tokens(const char *json, const char *id_key,
                              const char *refresh_key, const char *expires_key)
{
    cJSON *root = cJSON_Parse(json);
    if (root == NULL) {
        return ESP_FAIL;
    }

    const cJSON *id = cJSON_GetObjectItemCaseSensitive(root, id_key);
    const cJSON *rt = cJSON_GetObjectItemCaseSensitive(root, refresh_key);
    const cJSON *exp = cJSON_GetObjectItemCaseSensitive(root, expires_key);

    esp_err_t err = ESP_FAIL;

    if (cJSON_IsString(id) && id->valuestring && cJSON_IsString(rt) && rt->valuestring &&
        strlen(id->valuestring) < sizeof(s_fb.id_token) &&
        strlen(rt->valuestring) < sizeof(s_fb.refresh_token)) {

        long expires_in = 3600;
        if (cJSON_IsString(exp) && exp->valuestring) {
            expires_in = atol(exp->valuestring);
        } else if (cJSON_IsNumber(exp)) {
            expires_in = (long)exp->valuedouble;
        }
        if (expires_in <= 0) {
            expires_in = 3600;
        }

        strlcpy(s_fb.id_token, id->valuestring, sizeof(s_fb.id_token));
        strlcpy(s_fb.refresh_token, rt->valuestring, sizeof(s_fb.refresh_token));

        const long refresh_after = (expires_in > 600) ? (expires_in - 300) : (expires_in / 2);
        const int64_t now = esp_timer_get_time();
        s_fb.expires_at_us = now + (int64_t)expires_in * 1000000LL;
        s_fb.refresh_at_us = now + (int64_t)refresh_after * 1000000LL;
        s_fb.has_token = true;

        err = ESP_OK;
    }

    cJSON_Delete(root);
    return err;
}

static esp_err_t sign_up_locked(void)
{
    char url[192];
    snprintf(url, sizeof(url), SIGNUP_URL_FMT, s_fb.api_key);

    char *resp = calloc(1, AUTH_RESP_MAX);
    if (resp == NULL) {
        return ESP_ERR_NO_MEM;
    }

    int status = -1;
    esp_err_t err = http_post_oneshot(url, "application/json", "{\"returnSecureToken\":true}",
                                      resp, AUTH_RESP_MAX, &status);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Sign-up request failed: %s", esp_err_to_name(err));
    } else if (status != 200) {
        ESP_LOGE(TAG, "Sign-up rejected (HTTP %d): %.160s", status, resp);
        err = ESP_FAIL;
    } else {
        err = store_tokens(resp, "idToken", "refreshToken", "expiresIn");
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Sign-up response could not be parsed");
        }
    }

    free(resp);
    return err;
}

static esp_err_t refresh_locked(int *status_out)
{
    char url[192];
    snprintf(url, sizeof(url), REFRESH_URL_FMT, s_fb.api_key);

    char body[sizeof(s_fb.refresh_token) + 48];
    snprintf(body, sizeof(body), "grant_type=refresh_token&refresh_token=%s", s_fb.refresh_token);

    char *resp = calloc(1, AUTH_RESP_MAX);
    if (resp == NULL) {
        return ESP_ERR_NO_MEM;
    }

    int status = -1;
    esp_err_t err = http_post_oneshot(url, "application/x-www-form-urlencoded", body,
                                      resp, AUTH_RESP_MAX, &status);
    *status_out = status;

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Token refresh request failed: %s", esp_err_to_name(err));
    } else if (status != 200) {
        ESP_LOGE(TAG, "Token refresh rejected (HTTP %d): %.160s", status, resp);
        err = ESP_FAIL;
    } else {
        err = store_tokens(resp, "id_token", "refresh_token", "expires_in");
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Refresh response could not be parsed");
        }
    }

    free(resp);
    return err;
}

/* -------------------------------------------------------------------------- */
/* Public API                                                                 */
/* -------------------------------------------------------------------------- */

esp_err_t firebase_rtdb_init(const char *api_key, const char *database_url)
{
    if (!api_key || !database_url || api_key[0] == '\0' || database_url[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }

    memset(&s_fb, 0, sizeof(s_fb));

    if (strlen(api_key) >= sizeof(s_fb.api_key)) {
        return ESP_ERR_INVALID_SIZE;
    }
    strlcpy(s_fb.api_key, api_key, sizeof(s_fb.api_key));

    /* Normalise the database URL down to its bare host name. */
    const char *p = database_url;
    if (strncmp(p, "https://", 8) == 0) {
        p += 8;
    } else if (strncmp(p, "http://", 7) == 0) {
        p += 7;
    }
    size_t n = strcspn(p, "/");
    if (n == 0 || n >= sizeof(s_fb.host)) {
        return ESP_ERR_INVALID_ARG;
    }
    memcpy(s_fb.host, p, n);
    s_fb.host[n] = '\0';

    s_fb.lock = xSemaphoreCreateMutex();
    if (s_fb.lock == NULL) {
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "Client initialized (host: %s).", s_fb.host);
    return ESP_OK;
}

esp_err_t firebase_rtdb_authenticate(void)
{
    if (s_fb.lock == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    xSemaphoreTake(s_fb.lock, portMAX_DELAY);

    /* Free the database TLS session before opening another one. */
    close_db_client();

    esp_err_t err = ESP_FAIL;

    if (s_fb.refresh_token[0] != '\0') {
        int status = -1;
        err = refresh_locked(&status);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Token refreshed.");
        } else if (status == 400) {
            /* The refresh token was revoked/invalid: start over. */
            s_fb.refresh_token[0] = '\0';
            s_fb.has_token = false;
        }
    }

    if (err != ESP_OK && s_fb.refresh_token[0] == '\0') {
        ESP_LOGI(TAG, "Attempting anonymous authentication...");
        err = sign_up_locked();
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Authentication successful.");
        } else {
            ESP_LOGE(TAG, "Authentication failed.");
        }
    }

    xSemaphoreGive(s_fb.lock);
    return err;
}

bool firebase_rtdb_is_ready(void)
{
    if (s_fb.lock == NULL) {
        return false;
    }
    xSemaphoreTake(s_fb.lock, portMAX_DELAY);
    const bool ready = token_valid_locked();
    xSemaphoreGive(s_fb.lock);
    return ready;
}

bool firebase_rtdb_auth_due(void)
{
    if (s_fb.lock == NULL) {
        return false;
    }
    xSemaphoreTake(s_fb.lock, portMAX_DELAY);
    const bool due = !s_fb.has_token || esp_timer_get_time() >= s_fb.refresh_at_us;
    xSemaphoreGive(s_fb.lock);
    return due;
}

/* Performs one request on the persistent database connection. */
static esp_err_t db_request(esp_http_client_method_t method, const char *path,
                            const char *body, char *resp, size_t resp_size)
{
    if (s_fb.lock == NULL || path == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    xSemaphoreTake(s_fb.lock, portMAX_DELAY);

    esp_err_t err = ESP_FAIL;
    char *url = NULL;

    if (!token_valid_locked()) {
        err = ESP_ERR_INVALID_STATE;
        goto out;
    }

    url = malloc(URL_MAX);
    if (url == NULL) {
        err = ESP_ERR_NO_MEM;
        goto out;
    }

    while (*path == '/') {
        path++;
    }

    const int url_len = snprintf(url, URL_MAX, "https://%s/%s.json?auth=%s",
                                 s_fb.host, path, s_fb.id_token);
    if (url_len < 0 || url_len >= URL_MAX) {
        err = ESP_ERR_INVALID_SIZE;
        goto out;
    }

    s_db_rx = (rx_ctx_t){.buf = resp, .cap = resp_size, .len = 0};
    if (resp && resp_size) {
        resp[0] = '\0';
    }

    if (s_fb.db_client == NULL) {
        const esp_http_client_config_t cfg = {
            .url = url,
            .event_handler = on_http_event,
            .user_data = &s_db_rx,
            .crt_bundle_attach = esp_crt_bundle_attach,
            .timeout_ms = HTTP_TIMEOUT_MS,
            .buffer_size = HTTP_RX_BUF,
            .buffer_size_tx = HTTP_TX_BUF,
        };
        s_fb.db_client = esp_http_client_init(&cfg);
        if (s_fb.db_client == NULL) {
            err = ESP_ERR_NO_MEM;
            goto out;
        }
    } else {
        esp_http_client_set_url(s_fb.db_client, url);
    }

    esp_http_client_set_method(s_fb.db_client, method);
    esp_http_client_set_header(s_fb.db_client, "Content-Type", "application/json");
    esp_http_client_set_post_field(s_fb.db_client, body, body ? (int)strlen(body) : 0);

    err = esp_http_client_perform(s_fb.db_client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP request failed: %s", esp_err_to_name(err));
        close_db_client(); /* force a fresh connection next time */
        goto out;
    }

    const int status = esp_http_client_get_status_code(s_fb.db_client);
    if (status == 200) {
        err = ESP_OK;
    } else {
        ESP_LOGE(TAG, "Database returned HTTP %d: %.120s", status,
                 (resp && resp_size) ? resp : "");
        if (status == 401) {
            s_fb.refresh_at_us = 0; /* token rejected: ask for a refresh */
        }
        err = ESP_FAIL;
    }

out:
    free(url);
    xSemaphoreGive(s_fb.lock);
    return err;
}

esp_err_t firebase_rtdb_patch(const char *path, const char *json)
{
    if (json == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    char resp[ERR_RESP_MAX];
    return db_request(HTTP_METHOD_PATCH, path, json, resp, sizeof(resp));
}

esp_err_t firebase_rtdb_get(const char *path, char *out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    return db_request(HTTP_METHOD_GET, path, NULL, out, out_size);
}
