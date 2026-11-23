#include "CycleStore.h"

#include "esp_log.h"
#include "nvs.h"

static const char *TAG = "cycle";
static const char *s_namespace = "cycle";
static const char *s_key = "log";
static bool s_held = false;

void CycleStore::load( CycleLog &log )
{
    log.clear();
    nvs_handle_t handle = 0;
    if ( nvs_open( s_namespace, NVS_READONLY, &handle ) != ESP_OK ) {
        return;
    }
    size_t size = sizeof( log );
    if ( nvs_get_blob( handle, s_key, &log, &size ) != ESP_OK || size != sizeof( log ) || log.version != CycleLog::Version ) {
        log.clear();
    }
    nvs_close( handle );
}

void CycleStore::holdSaves()
{
    s_held = true;
}

esp_err_t CycleStore::save( const CycleLog &log )
{
    if ( s_held ) {
        return ESP_OK;
    }
    nvs_handle_t handle = 0;
    esp_err_t err = nvs_open( s_namespace, NVS_READWRITE, &handle );
    if ( err == ESP_OK ) {
        err = nvs_set_blob( handle, s_key, &log, sizeof( log ) );
        if ( err == ESP_OK ) {
            err = nvs_commit( handle );
        }
        nvs_close( handle );
    }
    if ( err != ESP_OK ) {
        ESP_LOGE( TAG, "Save failed: %s", esp_err_to_name( err ) );
    }
    return err;
}

esp_err_t CycleStore::erase()
{
    nvs_handle_t handle = 0;
    esp_err_t err = nvs_open( s_namespace, NVS_READWRITE, &handle );
    if ( err == ESP_OK ) {
        err = nvs_erase_all( handle );
        if ( err == ESP_OK ) {
            err = nvs_commit( handle );
        }
        nvs_close( handle );
    }
    ESP_LOGW( TAG, "Cycle data erased: %s", esp_err_to_name( err ) );
    return err;
}
