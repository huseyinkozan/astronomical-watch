#include "EmergencyCard.h"

#include <cstring>

#include "esp_log.h"
#include "nvs.h"

static const char *TAG = "card";
static const char *s_namespace = "card";
static const char *s_key = "card";

bool EmergencyCard::empty() const
{
    for ( int i = 0; i < FieldCount; ++i ) {
        if ( fields[i][0] != '\0' ) {
            return false;
        }
    }
    return true;
}

void EmergencyCard::set( Field field, const char *text )
{
    size_t length = strlen( text );
    if ( length > FieldSize - 1 ) {
        length = FieldSize - 1;
        // Continuation bytes are 10xxxxxx: the cut goes before the letter they belong to.
        while ( length > 0 && ( static_cast<unsigned char>( text[length] ) & 0xC0 ) == 0x80 ) {
            --length;
        }
    }
    char *out = fields[field];
    for ( size_t i = 0; i < length; ++i ) {
        const unsigned char c = static_cast<unsigned char>( text[i] );
        out[i] = c < 0x20 || c == 0x7F || c == '"' || c == '\\' ? ' ' : static_cast<char>( c );
    }
    out[length] = '\0';
}

void EmergencyCard::load()
{
    memset( fields, 0, sizeof( fields ) );
    nvs_handle_t handle = 0;
    if ( nvs_open( s_namespace, NVS_READONLY, &handle ) != ESP_OK ) {
        return;
    }
    size_t size = sizeof( fields );
    if ( nvs_get_blob( handle, s_key, fields, &size ) != ESP_OK || size != sizeof( fields ) ) {
        memset( fields, 0, sizeof( fields ) );
    }
    nvs_close( handle );
    for ( int i = 0; i < FieldCount; ++i ) {
        fields[i][FieldSize - 1] = '\0';
    }
}

void EmergencyCard::save() const
{
    nvs_handle_t handle = 0;
    esp_err_t err = nvs_open( s_namespace, NVS_READWRITE, &handle );
    if ( err == ESP_OK ) {
        err = nvs_set_blob( handle, s_key, fields, sizeof( fields ) );
        if ( err == ESP_OK ) {
            err = nvs_commit( handle );
        }
        nvs_close( handle );
    }
    if ( err != ESP_OK ) {
        ESP_LOGE( TAG, "Save failed: %s", esp_err_to_name( err ) );
    }
}
