#include "FaceStore.h"

#include "esp_log.h"
#include "nvs.h"

static const char *TAG = "faces";
static const char *s_namespace = "faces";
static const char *s_key = "pages";

void FaceStore::load( FaceConfig::Pages &pages )
{
    FaceConfig::Pages saved = {};
    size_t size = sizeof( saved );
    nvs_handle_t handle = 0;
    bool read = false;
    if ( nvs_open( s_namespace, NVS_READONLY, &handle ) == ESP_OK ) {
        read = nvs_get_blob( handle, s_key, &saved, &size ) == ESP_OK && size == sizeof( saved );
        nvs_close( handle );
    }
    if ( read ) {
        FaceConfig::clamp( saved );
    }
    if ( !read || !FaceConfig::valid( saved ) ) {
        FaceConfig::defaults( saved );
    }
    pages = saved;
}

void FaceStore::save( const FaceConfig::Pages &pages )
{
    nvs_handle_t handle = 0;
    esp_err_t err = nvs_open( s_namespace, NVS_READWRITE, &handle );
    if ( err == ESP_OK ) {
        err = nvs_set_blob( handle, s_key, &pages, sizeof( pages ) );
        if ( err == ESP_OK ) {
            err = nvs_commit( handle );
        }
        nvs_close( handle );
    }
    if ( err != ESP_OK ) {
        ESP_LOGE( TAG, "Save failed: %s", esp_err_to_name( err ) );
    }
}
