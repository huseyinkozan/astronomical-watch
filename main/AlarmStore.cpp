#include "AlarmStore.h"

#include "esp_log.h"
#include "nvs.h"

static const char *TAG = "alarms";
static const char *s_namespace = "alarms";
static const char *s_key = "list";

void AlarmStore::load( List &list )
{
    List saved = {};
    size_t size = sizeof( saved );
    nvs_handle_t handle = 0;
    bool read = false;
    if ( nvs_open( s_namespace, NVS_READONLY, &handle ) == ESP_OK ) {
        read = nvs_get_blob( handle, s_key, &saved, &size ) == ESP_OK && size == sizeof( saved );
        nvs_close( handle );
    }
    if ( !read || saved.count < 0 || saved.count > Alarms::MaxAlarms ) {
        saved = {};
    }
    list = saved;
}

void AlarmStore::save( const List &list )
{
    nvs_handle_t handle = 0;
    esp_err_t err = nvs_open( s_namespace, NVS_READWRITE, &handle );
    if ( err == ESP_OK ) {
        err = nvs_set_blob( handle, s_key, &list, sizeof( list ) );
        if ( err == ESP_OK ) {
            err = nvs_commit( handle );
        }
        nvs_close( handle );
    }
    if ( err != ESP_OK ) {
        ESP_LOGE( TAG, "Save failed: %s", esp_err_to_name( err ) );
    }
}
