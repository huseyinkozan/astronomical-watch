#include "EventLog.h"

#include <cstdio>
#include <ctime>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"

static const char *TAG = "events";
static const char *s_namespace = "events";
// 128 entries of 32 bytes: in development mode a few hours of always-on summaries plus the
// sessions of a day or more fit.
static const int s_capacity = 128;

static_assert( sizeof( EventLog::Entry ) == 32, "keep the NVS entries small" );

static void key( char *text, size_t size, int index )
{
    snprintf( text, size, "e%03d", index );
}

void EventLog::add( Entry entry, uint32_t utcTime )
{
    entry.time = utcTime;
    if ( utcTime == 0 ) {
        entry.flags |= TimeInvalid;
    }
    entry.heapMinimumKb = static_cast<uint16_t>( heap_caps_get_minimum_free_size( MALLOC_CAP_INTERNAL ) / 1024 );
    entry.mainStackFree = static_cast<uint16_t>( uxTaskGetStackHighWaterMark( nullptr ) );
    TaskHandle_t lvgl = xTaskGetHandle( "taskLVGL" );
    entry.lvglStackFree = lvgl != nullptr ? static_cast<uint16_t>( uxTaskGetStackHighWaterMark( lvgl ) ) : 0;

    // A timer wake that powers the watch off has not initialised NVS yet.
    esp_err_t err = nvs_flash_init();
    nvs_handle_t handle = 0;
    if ( err == ESP_OK ) {
        err = nvs_open( s_namespace, NVS_READWRITE, &handle );
    }
    if ( err != ESP_OK ) {
        ESP_LOGE( TAG, "Event log open failed: %s", esp_err_to_name( err ) );
        return;
    }
    uint16_t next = 0;
    nvs_get_u16( handle, "next", &next );
    char name[8];
    key( name, sizeof( name ), next % s_capacity );
    err = nvs_set_blob( handle, name, &entry, sizeof( entry ) );
    if ( err == ESP_OK ) {
        err = nvs_set_u16( handle, "next", static_cast<uint16_t>( ( next + 1 ) % ( 2 * s_capacity ) ) );
    }
    if ( err == ESP_OK ) {
        err = nvs_commit( handle );
    }
    nvs_close( handle );
    if ( err != ESP_OK ) {
        ESP_LOGE( TAG, "Event log write failed: %s", esp_err_to_name( err ) );
    }
}

void EventLog::print( int utcOffsetMinutes )
{
    nvs_handle_t handle = 0;
    printf( "EVENTS BEGIN\n" );
    if ( nvs_open( s_namespace, NVS_READONLY, &handle ) == ESP_OK ) {
        uint16_t next = 0;
        nvs_get_u16( handle, "next", &next );
        // `next` counts to twice the capacity, so that a full ring is told from an empty one:
        // the oldest entry is the one after the last written, when it exists.
        for ( int i = 0; i < s_capacity; ++i ) {
            char name[8];
            key( name, sizeof( name ), ( next + i ) % s_capacity );
            Entry entry = {};
            size_t size = sizeof( entry );
            if ( nvs_get_blob( handle, name, &entry, &size ) != ESP_OK || size != sizeof( entry ) ) {
                continue;
            }
            char when[24] = "no time";
            if ( !( entry.flags & TimeInvalid ) ) {
                const time_t local = static_cast<time_t>( entry.time ) + utcOffsetMinutes * 60;
                tm fields = {};
                gmtime_r( &local, &fields );
                strftime( when, sizeof( when ), "%Y-%m-%d %H:%M:%S", &fields );
            }
            printf( "EVENT %s type=%u code=%u flags=0x%02X battery=%u%%/%umV duration=%u touches=%u "
                    "touch=0x%02X/0x%02X heapmin=%uK mainstack=%u lvglstack=%u wakes=%u longest=%ums pmu=0x%02X/0x%02X\n",
                    when, entry.type, entry.code, entry.flags, entry.batteryPercent, entry.batteryMillivolts,
                    entry.duration, entry.touches, entry.touchModeStart, entry.touchModeEnd, entry.heapMinimumKb,
                    entry.mainStackFree, entry.lvglStackFree, entry.wakes, entry.longestWakeMs, entry.pmuPowerOn,
                    entry.pmuPowerOff );
        }
        nvs_close( handle );
    }
    printf( "EVENTS END\n" );
    fflush( stdout );
}
