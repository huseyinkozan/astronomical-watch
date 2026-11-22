#include "Portal.h"
#include "EventLog.h"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

#include "dns_server.h"
#include "esp_app_desc.h"
#include "esp_app_format.h"
#include "esp_core_dump.h"
#include "esp_flash.h"
#include "esp_heap_caps.h"
#include "esp_ota_ops.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "lwip/inet.h"
#include "nvs.h"
#include "sdkconfig.h"

static const char *TAG = "portal";
static const char *s_namespace = "portal";
static const char *s_passwordKey = "password";

// The page and the city list, embedded by main/CMakeLists.txt (EMBED_TXTFILES adds a final 0).
// The pages gzipped at build time (main/CMakeLists.txt): the browser unpacks them, the watch sends
// them from flash as they are.
extern const char s_pageStart[] asm( "_binary_index_html_gz_start" );
extern const char s_pageEnd[] asm( "_binary_index_html_gz_end" );
extern const char s_citiesStart[] asm( "_binary_cities_js_gz_start" );
extern const char s_citiesEnd[] asm( "_binary_cities_js_gz_end" );
extern const char s_helpStart[] asm( "_binary_help_html_gz_start" );
extern const char s_helpEnd[] asm( "_binary_help_html_gz_end" );
extern const char s_toolsHelpStart[] asm( "_binary_tools_html_gz_start" );
extern const char s_toolsHelpEnd[] asm( "_binary_tools_html_gz_end" );
extern const char s_updateStart[] asm( "_binary_update_html_gz_start" );
extern const char s_updateEnd[] asm( "_binary_update_html_gz_end" );

// Field names of the save request, one per Settings::Item.
static const char *s_fieldNames[] = { "timeout", "alwaysOn", "devMode",    "autoOff", "usbStayOn", "language", "nightMode",
                                                         "bootCounter",
                                                         "offset",  "lat",      "lon",        "hijri", "isha",
                                                         "face",
                                                         "brightness", "aodDay", "aodNight",
                                                         "chg1",    "aft1",     "chg2",       "aft2",
                                                         // Set by the watch itself; the page never sends them.
                                                         "rtcOffset", "rtcSet",
                                                         "clockOrder", "clockHidden", "infoOrder", "infoHidden",
                                                         "settingsOrder",
                                                         // Set by the watch itself.
                                                         "pwrPullUp", "unplugged",
                                                         "settingsHidden", "mirror", "planetsOrder", "planetsHidden",
                                                         "pagesOrder", "toolsOrder", "toolsHidden", "observingOrder",
                                                         "observingHidden",
                                                         // Set by the watch itself (the counter).
                                                         "counter", "counterTarget", "counterStart",
                                                         "scopeFocal", "scopeAperture", "ep1Focal", "ep1Field",
                                                         "ep2Focal", "ep2Field", "ep3Focal", "ep3Field", "ep4Focal",
                                                         "ep4Field", "camFocal", "camFNumber", "camSensor",
                                                         "baseShutter", "ndStops", "trailMinutes", "trailInterval",
                                                         "satLon" };
// One name per setting: a missing one would be read as a null name.
static_assert( sizeof( s_fieldNames ) / sizeof( s_fieldNames[0] ) == Settings::ItemCount, "a field name per setting" );
// The emergency card's fields (EmergencyCard::Field), in /api/card.
static const char *s_cardNames[EmergencyCard::FieldCount] = { "name",    "blood", "allergies", "medication",
                                                              "contact", "phone", "note" };

static bool s_networkReady = false;
// The 404 handler has no user context: it reaches the running portal through this.
static Portal *s_running = nullptr;

void Portal::init()
{
    uint8_t mac[6] = {};
    esp_read_mac( mac, ESP_MAC_WIFI_SOFTAP );
    snprintf( m_ssid, sizeof( m_ssid ), "AstroWatch-%02X%02X", mac[4], mac[5] );

    nvs_handle_t handle = 0;
    size_t length = sizeof( m_password );
    if ( nvs_open( s_namespace, NVS_READONLY, &handle ) == ESP_OK ) {
        if ( nvs_get_str( handle, s_passwordKey, m_password, &length ) != ESP_OK ) {
            m_password[0] = '\0';
        }
        nvs_close( handle );
    }
    // A password of an earlier form is replaced as well.
    bool pairs = strlen( m_password ) == 8;
    for ( int i = 0; i < 4 && pairs; ++i ) {
        pairs = m_password[2 * i] == m_password[2 * i + 1];
    }
    if ( !pairs ) {
        newPassword();
    }
}

void Portal::newPassword()
{
    if ( isRunning() ) {
        return;
    }
    // Four random digits, each written twice, e.g. 33556688 (WPA2 needs at least eight
    // characters): easy to read off the watch and type on a phone. 10^4 choices are enough for an
    // access point that the wearer opens for a few minutes. esp_random() has full entropy only
    // while the radio runs; the bootloader's early entropy source is enough here.
    for ( int i = 0; i < 4; ++i ) {
        m_password[2 * i] = static_cast<char>( '0' + esp_random() % 10 );
        m_password[2 * i + 1] = m_password[2 * i];
    }
    m_password[8] = '\0';
    nvs_handle_t handle = 0;
    esp_err_t err = nvs_open( s_namespace, NVS_READWRITE, &handle );
    if ( err == ESP_OK ) {
        err = nvs_set_str( handle, s_passwordKey, m_password );
        if ( err == ESP_OK ) {
            err = nvs_commit( handle );
        }
        nvs_close( handle );
    }
    ESP_LOGI( TAG, "New password: %s", esp_err_to_name( err ) );
}

esp_err_t Portal::start( const Settings &settings, const WorldClock &worldClock, const DayCounters &counters,
                         const FaceConfig::Pages &faces, const AlarmStore::List &alarms, const EmergencyCard &card )
{
    if ( isRunning() ) {
        return ESP_OK;
    }
    m_settings = &settings;
    m_worldClock = &worldClock;
    m_counters = &counters;
    m_alarms = &alarms;
    m_faces = &faces;
    taskENTER_CRITICAL( &m_lock );
    m_card = card;
    taskEXIT_CRITICAL( &m_lock );
    m_hasCoreDump = esp_core_dump_image_check() == ESP_OK;
    esp_err_t err = ESP_OK;
    if ( !s_networkReady ) {
        err = esp_netif_init();
        if ( err == ESP_OK ) {
            err = esp_event_loop_create_default();
        }
        if ( err != ESP_OK ) {
            ESP_LOGE( TAG, "Network init failed: %s", esp_err_to_name( err ) );
            return err;
        }
        s_networkReady = true;
    }
    m_netif = esp_netif_create_default_wifi_ap();

    wifi_init_config_t initConfig = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init( &initConfig );
    if ( err == ESP_OK ) {
        // The configuration below is set at every start; keeping it in NVS would only wear flash.
        err = esp_wifi_set_storage( WIFI_STORAGE_RAM );
    }
    wifi_config_t config = {};
    strncpy( reinterpret_cast<char *>( config.ap.ssid ), m_ssid, sizeof( config.ap.ssid ) );
    config.ap.ssid_len = static_cast<uint8_t>( strlen( m_ssid ) );
    strncpy( reinterpret_cast<char *>( config.ap.password ), m_password, sizeof( config.ap.password ) );
    config.ap.channel = 1;
    config.ap.max_connection = 2;
    config.ap.authmode = WIFI_AUTH_WPA2_PSK;
    if ( err == ESP_OK ) {
        err = esp_wifi_set_mode( WIFI_MODE_AP );
    }
    if ( err == ESP_OK ) {
        err = esp_wifi_set_config( WIFI_IF_AP, &config );
    }
    if ( err == ESP_OK ) {
        err = esp_wifi_start();
    }
    if ( err != ESP_OK ) {
        ESP_LOGE( TAG, "Wi-Fi start failed: %s", esp_err_to_name( err ) );
        stop();
        return err;
    }

    esp_netif_ip_info_t ipInfo = {};
    esp_netif_get_ip_info( m_netif, &ipInfo );
    inet_ntoa_r( ipInfo.ip.addr, m_address, sizeof( m_address ) );
    // DHCP option 114 (RFC 8910) points newer phones straight at the page.
    static char portalUri[32];
    snprintf( portalUri, sizeof( portalUri ), "http://%s", m_address );
    esp_netif_dhcps_stop( m_netif );
    esp_netif_dhcps_option( m_netif, ESP_NETIF_OP_SET, ESP_NETIF_CAPTIVEPORTAL_URI, portalUri, strlen( portalUri ) );
    esp_netif_dhcps_start( m_netif );

    httpd_config_t serverConfig = HTTPD_DEFAULT_CONFIG();
    serverConfig.lru_purge_enable = true;
    // lwIP has CONFIG_LWIP_MAX_SOCKETS sockets: the server keeps 3 for itself and the captive DNS
    // takes 1. With the default 7 connections two browsers used up the last socket before the
    // server's own count was full, so it never closed an idle connection and accepted nothing
    // more ("error in accept (23)"). One fewer, and the least recently used one is closed.
    serverConfig.max_open_sockets = CONFIG_LWIP_MAX_SOCKETS - 4;
    serverConfig.stack_size = 6144;
    serverConfig.max_uri_handlers = 14;
    // An image upload takes a while between packets on a busy phone.
    serverConfig.recv_wait_timeout = 15;
    err = httpd_start( &m_server, &serverConfig );
    if ( err != ESP_OK ) {
        ESP_LOGE( TAG, "HTTP server start failed: %s", esp_err_to_name( err ) );
        stop();
        return err;
    }
    const httpd_uri_t handlers[14] = {
        { "/", HTTP_GET, onPage, this },
        { "/help", HTTP_GET, onHelp, this },
        { "/tools", HTTP_GET, onToolsHelp, this },
        { "/api/card", HTTP_GET, onCard, this },
        { "/api/card", HTTP_POST, onCardSave, this },
        { "/update", HTTP_GET, onUpdatePage, this },
        { "/api/update", HTTP_POST, onUpdate, this },
        { "/api/events", HTTP_GET, onEvents, this },
        { "/api/coredump", HTTP_GET, onCoreDump, this },
        { "/cities.js", HTTP_GET, onCities, this },
        { "/api/state", HTTP_GET, onState, this },
        { "/api/ping", HTTP_GET, onPing, this },
        { "/api/save", HTTP_POST, onSave, this },
        { "/api/reset", HTTP_POST, onReset, this },
    };
    for ( int i = 0; i < 14; ++i ) {
        httpd_register_uri_handler( m_server, &handlers[i] );
    }
    httpd_register_err_handler( m_server, HTTPD_404_NOT_FOUND, onNotFound );
    // Captive-portal checks of phones and computers ask for unknown pages; their redirects are normal.
    esp_log_level_set( "httpd_uri", ESP_LOG_ERROR );
    esp_log_level_set( "httpd_txrx", ESP_LOG_ERROR );
    esp_log_level_set( "httpd_parse", ESP_LOG_ERROR );

    // Every name ("*") resolves to the access point's address (DNS_SERVER_CONFIG_SINGLE, spelled
    // out: its C designated initialiser leaves a field out, which C++ warns about).
    dns_server_config_t dnsConfig = {};
    dnsConfig.num_of_entries = 1;
    dnsConfig.item[0].name = "*";
    dnsConfig.item[0].if_key = "WIFI_AP_DEF";
    m_dns = start_dns_server( &dnsConfig );

    touch();
    s_running = this;
#if CONFIG_WATCH_DEV_LOGGING
    // For host-side tests (USB console only).
    ESP_LOGI( TAG, "Portal password: %s", m_password );
#endif
    ESP_LOGI( TAG, "Portal on: %s, http://%s, internal heap %u free, %u largest", m_ssid, m_address,
              static_cast<unsigned>( heap_caps_get_free_size( MALLOC_CAP_INTERNAL ) ),
              static_cast<unsigned>( heap_caps_get_largest_free_block( MALLOC_CAP_INTERNAL ) ) );
    return ESP_OK;
}

void Portal::stop()
{
    s_running = nullptr;
    if ( m_dns != nullptr ) {
        stop_dns_server( static_cast<dns_server_handle_t>( m_dns ) );
        m_dns = nullptr;
    }
    if ( m_server != nullptr ) {
        httpd_stop( m_server );
        m_server = nullptr;
    }
    if ( m_netif != nullptr ) {
        esp_wifi_stop();
        esp_wifi_deinit();
        esp_netif_destroy_default_wifi( m_netif );
        m_netif = nullptr;
        m_address[0] = '\0';
        ESP_LOGI( TAG, "Portal off" );
    }
}

bool Portal::isRunning() const
{
    return m_netif != nullptr;
}

int Portal::idleSeconds() const
{
    return static_cast<int>( ( esp_timer_get_time() - m_lastRequestUs ) / 1000000 );
}

const char *Portal::ssid() const
{
    return m_ssid;
}

const char *Portal::password() const
{
    return m_password;
}

const char *Portal::address() const
{
    return m_address;
}

bool Portal::takeCard( EmergencyCard &card )
{
    bool taken = false;
    taskENTER_CRITICAL( &m_lock );
    if ( m_hasCard ) {
        card = m_receivedCard;
        m_hasCard = false;
        taken = true;
    }
    taskEXIT_CRITICAL( &m_lock );
    return taken;
}

bool Portal::takeReset()
{
    taskENTER_CRITICAL( &m_lock );
    const bool wanted = m_resetWanted;
    m_resetWanted = false;
    taskEXIT_CRITICAL( &m_lock );
    return wanted;
}

bool Portal::takeReceived( Received &received )
{
    bool taken = false;
    taskENTER_CRITICAL( &m_lock );
    if ( m_hasReceived ) {
        received = m_received;
        m_hasReceived = false;
        taken = true;
    }
    taskEXIT_CRITICAL( &m_lock );
    return taken;
}

void Portal::touch()
{
    m_lastRequestUs = esp_timer_get_time();
}

static void noCache( httpd_req_t *request )
{
    httpd_resp_set_hdr( request, "Cache-Control", "no-store" );
}

esp_err_t Portal::onPage( httpd_req_t *request )
{
    static_cast<Portal *>( request->user_ctx )->touch();
    ESP_LOGI( TAG, "Setup page served" );
    httpd_resp_set_type( request, "text/html; charset=utf-8" );
    httpd_resp_set_hdr( request, "Content-Encoding", "gzip" );
    noCache( request );
    return httpd_resp_send( request, s_pageStart, s_pageEnd - s_pageStart );
}

esp_err_t Portal::onHelp( httpd_req_t *request )
{
    static_cast<Portal *>( request->user_ctx )->touch();
    httpd_resp_set_type( request, "text/html; charset=utf-8" );
    httpd_resp_set_hdr( request, "Content-Encoding", "gzip" );
    return httpd_resp_send( request, s_helpStart, s_helpEnd - s_helpStart );
}

esp_err_t Portal::onToolsHelp( httpd_req_t *request )
{
    static_cast<Portal *>( request->user_ctx )->touch();
    httpd_resp_set_type( request, "text/html; charset=utf-8" );
    httpd_resp_set_hdr( request, "Content-Encoding", "gzip" );
    return httpd_resp_send( request, s_toolsHelpStart, s_toolsHelpEnd - s_toolsHelpStart );
}

esp_err_t Portal::onUpdatePage( httpd_req_t *request )
{
    static_cast<Portal *>( request->user_ctx )->touch();
    httpd_resp_set_type( request, "text/html; charset=utf-8" );
    httpd_resp_set_hdr( request, "Content-Encoding", "gzip" );
    noCache( request );
    return httpd_resp_send( request, s_updateStart, s_updateEnd - s_updateStart );
}

esp_err_t Portal::onCities( httpd_req_t *request )
{
    static_cast<Portal *>( request->user_ctx )->touch();
    httpd_resp_set_type( request, "text/javascript; charset=utf-8" );
    httpd_resp_set_hdr( request, "Content-Encoding", "gzip" );
    return httpd_resp_send( request, s_citiesStart, s_citiesEnd - s_citiesStart );
}

// One buffer for every request that needs one: the HTTP server serves one request at a time in
// its own task, whose stack is small. Its size holds the setup page's form and state with eight
// face pages (their commas URL-encoded in the form).
static char s_buffer[4096];
// The face pages as FaceConfig::encode writes them, for the state and from the form, where each
// separator takes three characters ("%2C"): 22 numbers of up to three digits a page, then the
// eight numbers of the corner gauges.
static char s_faces[( FaceConfig::MaxPages * 22 + 8 ) * 6 + 16];

esp_err_t Portal::onState( httpd_req_t *request )
{
    Portal *portal = static_cast<Portal *>( request->user_ctx );
    portal->touch();
    const Settings &settings = *portal->m_settings;
    const WorldClock &world = *portal->m_worldClock;
    char *json = s_buffer;
    const size_t jsonSize = sizeof( s_buffer );
    snprintf( json, jsonSize,
              "{\"timeout\":%d,\"alwaysOn\":%d,\"brightness\":%d,\"autoOff\":%d,\"face\":%d,"
              "\"offset\":%d,\"lat\":%.6f,\"lon\":%.6f,\"hijri\":%d,"
              "\"aodDay\":%d,\"aodNight\":%d,\"devMode\":%d,\"usbStayOn\":%d,\"nightMode\":%d,\"isha\":%d,\"language\":%d,"
              "\"clockOrder\":%d,\"clockHidden\":%d,\"infoOrder\":%d,\"infoHidden\":%d,"
              "\"settingsOrder\":%d,\"settingsHidden\":%d,\"mirror\":%d,\"planetsOrder\":%d,\"planetsHidden\":%d,"
              "\"pagesOrder\":%d,\"toolsOrder\":%d,\"toolsHidden\":%d,\"observingOrder\":%d,\"observingHidden\":%d,"
              "\"bootCounter\":%d,"
              "\"version\":\"%s\",\"battery\":%d,\"usb\":%d,\"updateMin\":%d,\"coreDump\":%d,"
              "\"zone\":\"%s\",\"place\":\"%s\",\"cities\":[",
              settings.value( Settings::DisplayTimeout ), settings.value( Settings::AlwaysOnFace ),
              settings.value( Settings::Brightness ), settings.value( Settings::AutoOff ),
              settings.value( Settings::FaceStyle ),
              settings.value( Settings::UtcOffset ), settings.value( Settings::Latitude ) / 1e6,
              settings.value( Settings::Longitude ) / 1e6, settings.value( Settings::HijriMethod ),
              settings.value( Settings::AodDayBrightness ), settings.value( Settings::AodNightBrightness ),
              settings.value( Settings::DevMode ), settings.value( Settings::UsbStayOn ), settings.value( Settings::NightMode ),
              settings.value( Settings::IshaAngle ), settings.value( Settings::Language ),
              settings.value( Settings::ClockOrder ),
              settings.value( Settings::ClockHidden ), settings.value( Settings::InfoOrder ),
              settings.value( Settings::InfoHidden ), settings.value( Settings::SettingsOrder ),
              settings.value( Settings::SettingsHidden ), settings.value( Settings::Mirror ),
              settings.value( Settings::PlanetsOrder ), settings.value( Settings::PlanetsHidden ),
              settings.value( Settings::PagesOrder ), settings.value( Settings::ToolsOrder ),
              settings.value( Settings::ToolsHidden ), settings.value( Settings::ObservingOrder ),
              settings.value( Settings::ObservingHidden ), settings.value( Settings::BootCounter ),
              esp_app_get_description()->version, portal->m_batteryPercent, portal->m_usb ? 1 : 0, UpdateMinPercent,
              portal->m_hasCoreDump ? 1 : 0,
              world.homeZoneName(),
              world.homePlace() );
    // The names come from the page's own city list: no quotes or backslashes.
    for ( int i = 0; i < world.count(); ++i ) {
        const int used = strlen( json );
        snprintf( json + used, jsonSize - used, "%s[\"%s\",\"%s\"]", i == 0 ? "" : ",", world.city( i ).name,
                  world.city( i ).zoneName );
    }
    strlcat( json, "]", jsonSize );
    // The observing page's equipment, by the field names.
    for ( int i = Settings::ScopeFocal; i < Settings::ItemCount; ++i ) {
        const int used = strlen( json );
        snprintf( json + used, jsonSize - used, ",\"%s\":%d", s_fieldNames[i],
                  settings.value( static_cast<Settings::Item>( i ) ) );
    }
    strlcat( json, ",\"days\":[", jsonSize );
    const DayCounters &counters = *portal->m_counters;
    for ( int i = 0; i < counters.count(); ++i ) {
        const time_t when = static_cast<time_t>( counters.counter( i ).day ) * 86400;
        tm fields = {};
        gmtime_r( &when, &fields );
        const int used = strlen( json );
        snprintf( json + used, jsonSize - used, "%s[\"%s\",\"%04d-%02d-%02d\"]", i == 0 ? "" : ",",
                  counters.counter( i ).name, fields.tm_year + 1900, fields.tm_mon + 1, fields.tm_mday );
    }
    strlcat( json, "],\"alarms\":[", jsonSize );
    const AlarmStore::List &alarms = *portal->m_alarms;
    for ( int i = 0; i < alarms.count; ++i ) {
        char text[48];
        if ( Alarms::encode( alarms.alarms[i], text, sizeof( text ) ) ) {
            const int used = strlen( json );
            snprintf( json + used, jsonSize - used, "%s\"%s\"", i == 0 ? "" : ",", text );
        }
    }
    // The page's lists may be longer than this version's catalogues: it offers only these.
    if ( FaceConfig::encode( *portal->m_faces, s_faces, sizeof( s_faces ) ) < 0 ) {
        s_faces[0] = '\0';
    }
    int used = strlen( json );
    snprintf( json + used, jsonSize - used, "],\"faces\":\"%s\",\"faceItems\":[%d,%d]", s_faces,
              static_cast<int>( FaceConfig::TextItemCount ), static_cast<int>( FaceConfig::CornerItemCount ) );
    // The default pages, the same as on a first start, for the page's "Load the default faces".
    FaceConfig::Pages defaults = {};
    FaceConfig::defaults( defaults );
    if ( FaceConfig::encode( defaults, s_faces, sizeof( s_faces ) ) < 0 ) {
        s_faces[0] = '\0';
    }
    used = strlen( json );
    snprintf( json + used, jsonSize - used, ",\"faceDefaults\":\"%s\"}", s_faces );
    httpd_resp_set_type( request, "application/json" );
    noCache( request );
    return httpd_resp_sendstr( request, json );
}

void Portal::setBattery( int percent, bool usb )
{
    m_batteryPercent = percent;
    m_usb = usb;
}

bool Portal::takeUpdated()
{
    taskENTER_CRITICAL( &m_lock );
    const bool updated = m_updated;
    m_updated = false;
    taskEXIT_CRITICAL( &m_lock );
    return updated;
}

// Body: the application image. Written to the slot that does not run, checked by ESP-IDF
// (header, segments, SHA-256), and chosen for the next start; the old image stays until a later
// update, and the bootloader returns to it if the new one fails before it is marked valid.
esp_err_t Portal::onUpdate( httpd_req_t *request )
{
    Portal *portal = static_cast<Portal *>( request->user_ctx );
    portal->touch();
    if ( !portal->m_usb && portal->m_batteryPercent < UpdateMinPercent ) {
        httpd_resp_set_status( request, "403 Forbidden" );
        return httpd_resp_sendstr( request, "battery" );
    }
    const esp_partition_t *target = esp_ota_get_next_update_partition( nullptr );
    esp_ota_handle_t ota = 0;
    if ( target == nullptr || esp_ota_begin( target, OTA_WITH_SEQUENTIAL_WRITES, &ota ) != ESP_OK ) {
        return httpd_resp_send_err( request, HTTPD_500_INTERNAL_SERVER_ERROR, "No update slot" );
    }
    char *buffer = s_buffer;
    int left = request->content_len;
    bool first = true;
    esp_err_t err = ESP_OK;
    while ( left > 0 && err == ESP_OK ) {
        const int part = httpd_req_recv( request, buffer, left < static_cast<int>( sizeof( s_buffer ) ) ? left : sizeof( s_buffer ) );
        if ( part == HTTPD_SOCK_ERR_TIMEOUT ) {
            continue;
        }
        if ( part <= 0 ) {
            err = ESP_FAIL;
            break;
        }
        portal->touch();
        // The first block holds the image header and the application description: only this
        // project's images for this chip.
        const size_t descOffset = sizeof( esp_image_header_t ) + sizeof( esp_image_segment_header_t );
        if ( first ) {
            first = false;
            const esp_image_header_t *header = reinterpret_cast<const esp_image_header_t *>( buffer );
            const esp_app_desc_t *desc = reinterpret_cast<const esp_app_desc_t *>( buffer + descOffset );
            if ( part < static_cast<int>( descOffset + sizeof( esp_app_desc_t ) ) || header->magic != ESP_IMAGE_HEADER_MAGIC ||
                 header->chip_id != ESP_CHIP_ID_ESP32S3 ||
                 strncmp( desc->project_name, esp_app_get_description()->project_name, sizeof( desc->project_name ) ) != 0 ) {
                esp_ota_abort( ota );
                httpd_resp_set_status( request, "415 Unsupported Media Type" );
                return httpd_resp_sendstr( request, "not this watch's software" );
            }
            ESP_LOGI( TAG, "Update to version %s, %d bytes", desc->version, request->content_len );
        }
        err = esp_ota_write( ota, buffer, part );
        left -= part;
    }
    if ( err != ESP_OK ) {
        esp_ota_abort( ota );
        ESP_LOGE( TAG, "Update failed: %s", esp_err_to_name( err ) );
        return httpd_resp_send_err( request, HTTPD_500_INTERNAL_SERVER_ERROR, "Update failed" );
    }
    err = esp_ota_end( ota );
    if ( err == ESP_OK ) {
        err = esp_ota_set_boot_partition( target );
    }
    if ( err != ESP_OK ) {
        ESP_LOGE( TAG, "Update not accepted: %s", esp_err_to_name( err ) );
        return httpd_resp_send_err( request, HTTPD_500_INTERNAL_SERVER_ERROR, "Image not valid" );
    }
    taskENTER_CRITICAL( &portal->m_lock );
    portal->m_updated = true;
    taskEXIT_CRITICAL( &portal->m_lock );
    noCache( request );
    return httpd_resp_sendstr( request, "Updated" );
}

static void sendLine( const char *line, void *user )
{
    httpd_resp_sendstr_chunk( static_cast<httpd_req_t *>( user ), line );
}

esp_err_t Portal::onEvents( httpd_req_t *request )
{
    Portal *portal = static_cast<Portal *>( request->user_ctx );
    portal->touch();
    httpd_resp_set_type( request, "text/plain; charset=utf-8" );
    httpd_resp_set_hdr( request, "Content-Disposition", "attachment; filename=\"watch-events.txt\"" );
    noCache( request );
    // The format of tools/read_events.py, which also reads a downloaded file.
    EventLog::write( portal->m_settings->value( Settings::UtcOffset ), sendLine, request );
    return httpd_resp_sendstr_chunk( request, nullptr );
}

esp_err_t Portal::onCoreDump( httpd_req_t *request )
{
    static_cast<Portal *>( request->user_ctx )->touch();
    size_t address = 0;
    size_t size = 0;
    if ( esp_core_dump_image_check() != ESP_OK || esp_core_dump_image_get( &address, &size ) != ESP_OK ) {
        httpd_resp_set_status( request, "404 Not Found" );
        return httpd_resp_sendstr( request, "No crash dump" );
    }
    httpd_resp_set_type( request, "application/octet-stream" );
    httpd_resp_set_hdr( request, "Content-Disposition", "attachment; filename=\"watch-coredump.bin\"" );
    noCache( request );
    for ( size_t done = 0; done < size; ) {
        const size_t part = size - done < sizeof( s_buffer ) ? size - done : sizeof( s_buffer );
        if ( esp_flash_read( nullptr, s_buffer, address + done, part ) != ESP_OK ||
             httpd_resp_send_chunk( request, s_buffer, part ) != ESP_OK ) {
            break;
        }
        done += part;
    }
    return httpd_resp_send_chunk( request, nullptr, 0 );
}

// The page asked twice (a confirmation in the browser); the main task erases and restarts.
esp_err_t Portal::onReset( httpd_req_t *request )
{
    Portal *portal = static_cast<Portal *>( request->user_ctx );
    portal->touch();
    taskENTER_CRITICAL( &portal->m_lock );
    portal->m_resetWanted = true;
    taskEXIT_CRITICAL( &portal->m_lock );
    noCache( request );
    return httpd_resp_sendstr( request, "ok" );
}

esp_err_t Portal::onPing( httpd_req_t *request )
{
    static_cast<Portal *>( request->user_ctx )->touch();
    noCache( request );
    return httpd_resp_sendstr( request, "pong" );
}

// The card's texts have no quotes, backslashes or control characters (EmergencyCard::set).
esp_err_t Portal::onCard( httpd_req_t *request )
{
    Portal *portal = static_cast<Portal *>( request->user_ctx );
    portal->touch();
    static EmergencyCard card;
    taskENTER_CRITICAL( &portal->m_lock );
    card = portal->m_card;
    taskEXIT_CRITICAL( &portal->m_lock );
    char *json = s_buffer;
    strlcpy( json, "{", sizeof( s_buffer ) );
    for ( int i = 0; i < EmergencyCard::FieldCount; ++i ) {
        const int used = strlen( json );
        snprintf( json + used, sizeof( s_buffer ) - used, "%s\"%s\":\"%s\"", i == 0 ? "" : ",", s_cardNames[i],
                  card.fields[i] );
    }
    strlcat( json, "}", sizeof( s_buffer ) );
    httpd_resp_set_type( request, "application/json" );
    noCache( request );
    return httpd_resp_sendstr( request, json );
}

// Decodes a form value in place: "+" is a space, "%XX" a byte.
static void urlDecode( char *text )
{
    char *out = text;
    for ( const char *in = text; *in != '\0'; ++in ) {
        if ( *in == '+' ) {
            *out++ = ' ';
        } else if ( *in == '%' && isxdigit( static_cast<unsigned char>( in[1] ) ) &&
                    isxdigit( static_cast<unsigned char>( in[2] ) ) ) {
            const char hex[3] = { in[1], in[2], '\0' };
            *out++ = static_cast<char>( strtol( hex, nullptr, 16 ) );
            in += 2;
        } else {
            *out++ = *in;
        }
    }
    *out = '\0';
}

// Body: "time=<ms since 1970>&rtt=<ms>&timeout=10&...&lat=41.0082&lon=28.9784", form-encoded.
esp_err_t Portal::onSave( httpd_req_t *request )
{
    const int64_t arrivedUs = esp_timer_get_time();
    Portal *portal = static_cast<Portal *>( request->user_ctx );
    portal->touch();
    char *body = s_buffer;
    const int length = request->content_len < sizeof( s_buffer ) - 1 ? request->content_len : sizeof( s_buffer ) - 1;
    int read = 0;
    while ( read < length ) {
        const int part = httpd_req_recv( request, body + read, length - read );
        if ( part <= 0 ) {
            return httpd_resp_send_err( request, HTTPD_400_BAD_REQUEST, "No data" );
        }
        read += part;
    }
    body[read] = '\0';

    static Received received;
    received = {};
    received.faceShow = -1;
    char value[24];
    if ( httpd_query_key_value( body, "time", value, sizeof( value ) ) == ESP_OK ) {
        char rtt[12] = "0";
        httpd_query_key_value( body, "rtt", rtt, sizeof( rtt ) );
        // The page sent its time when the request left; half the measured round trip later it
        // arrived here.
        received.unixMs = strtoll( value, nullptr, 10 ) + strtol( rtt, nullptr, 10 ) / 2;
        received.atTimerUs = arrivedUs;
        received.timeValid = received.unixMs > 1700000000000LL;
    }
    for ( int i = 0; i < Settings::ItemCount; ++i ) {
        if ( httpd_query_key_value( body, s_fieldNames[i], value, sizeof( value ) ) != ESP_OK ) {
            continue;
        }
        if ( i == Settings::Latitude || i == Settings::Longitude ) {
            received.values[i] = static_cast<int>( lround( strtod( value, nullptr ) * 1e6 ) );
        } else {
            received.values[i] = static_cast<int>( strtol( value, nullptr, 10 ) );
        }
        received.valueValid[i] = true;
    }
    // The world clock: "c0".."c3" = "name|zone|offset|change1|after1|change2|after2", URL-encoded.
    if ( httpd_query_key_value( body, "cities", value, sizeof( value ) ) == ESP_OK ) {
        received.citiesValid = true;
        char text[128];
        if ( httpd_query_key_value( body, "zone", text, sizeof( text ) ) == ESP_OK ) {
            urlDecode( text );
            strlcpy( received.homeZoneName, text, sizeof( received.homeZoneName ) );
        }
        const int count = static_cast<int>( strtol( value, nullptr, 10 ) );
        for ( int i = 0; i < count && i < WorldClock::MaxCities; ++i ) {
            char key[4] = { 'c', static_cast<char>( '0' + i ), '\0', '\0' };
            if ( httpd_query_key_value( body, key, text, sizeof( text ) ) != ESP_OK ) {
                continue;
            }
            urlDecode( text );
            WorldClock::City &city = received.cities[received.cityCount];
            char *fields[7] = {};
            char *rest = text;
            for ( int f = 0; f < 7 && rest != nullptr; ++f ) {
                fields[f] = rest;
                rest = strchr( rest, '|' );
                if ( rest != nullptr ) {
                    *rest++ = '\0';
                }
            }
            if ( fields[6] == nullptr ) {
                continue;
            }
            strlcpy( city.name, fields[0], sizeof( city.name ) );
            strlcpy( city.zoneName, fields[1], sizeof( city.zoneName ) );
            city.zone.offset = static_cast<int>( strtol( fields[2], nullptr, 10 ) );
            city.zone.changeAt[0] = static_cast<int>( strtol( fields[3], nullptr, 10 ) );
            city.zone.offsetAfter[0] = static_cast<int>( strtol( fields[4], nullptr, 10 ) );
            city.zone.changeAt[1] = static_cast<int>( strtol( fields[5], nullptr, 10 ) );
            city.zone.offsetAfter[1] = static_cast<int>( strtol( fields[6], nullptr, 10 ) );
            ++received.cityCount;
        }
    }
    {
        char text[64];
        if ( httpd_query_key_value( body, "place", text, sizeof( text ) ) == ESP_OK ) {
            urlDecode( text );
            strlcpy( received.place, text, sizeof( received.place ) );
            received.placeValid = true;
        }
    }
    // The day counters: "d0".."d3" = "name|YYYY-MM-DD", URL-encoded.
    if ( httpd_query_key_value( body, "days", value, sizeof( value ) ) == ESP_OK ) {
        received.countersValid = true;
        const int count = static_cast<int>( strtol( value, nullptr, 10 ) );
        for ( int i = 0; i < count && i < DayCounters::MaxCounters; ++i ) {
            char key[4] = { 'd', static_cast<char>( '0' + i ), '\0', '\0' };
            char text[96];
            if ( httpd_query_key_value( body, key, text, sizeof( text ) ) != ESP_OK ) {
                continue;
            }
            urlDecode( text );
            char *date = strrchr( text, '|' );
            int year = 0;
            int month = 0;
            int day = 0;
            if ( date == nullptr || sscanf( date + 1, "%d-%d-%d", &year, &month, &day ) != 3 ) {
                continue;
            }
            *date = '\0';
            tm fields = {};
            fields.tm_year = year - 1900;
            fields.tm_mon = month - 1;
            fields.tm_mday = day;
            DayCounters::Counter &counter = received.counters[received.counterCount++];
            strlcpy( counter.name, text, sizeof( counter.name ) );
            // The firmware never sets TZ, so mktime() works in UTC.
            counter.day = static_cast<int>( mktime( &fields ) / 86400 );
        }
    }
    // The alarms: "a0".."a3" = Alarms::encode's text, URL-encoded.
    if ( httpd_query_key_value( body, "alarms", value, sizeof( value ) ) == ESP_OK ) {
        received.alarmsValid = true;
        const int count = static_cast<int>( strtol( value, nullptr, 10 ) );
        for ( int i = 0; i < count && i < Alarms::MaxAlarms; ++i ) {
            char key[4] = { 'a', static_cast<char>( '0' + i ), '\0', '\0' };
            char text[48];
            if ( httpd_query_key_value( body, key, text, sizeof( text ) ) != ESP_OK ) {
                continue;
            }
            urlDecode( text );
            if ( Alarms::decode( text, received.alarms.alarms[received.alarms.count] ) ) {
                ++received.alarms.count;
            } else {
                ESP_LOGW( TAG, "Setup page: alarm refused: %s", text );
            }
        }
    }
    // The face pages: FaceConfig::encode's text, URL-encoded.
    if ( httpd_query_key_value( body, "faces", s_faces, sizeof( s_faces ) ) == ESP_OK ) {
        urlDecode( s_faces );
        received.facesValid = FaceConfig::decode( s_faces, received.faces );
        if ( !received.facesValid ) {
            ESP_LOGW( TAG, "Setup page: face pages refused: %s", s_faces );
        }
    }
    if ( httpd_query_key_value( body, "show", value, sizeof( value ) ) == ESP_OK ) {
        received.faceShow = static_cast<int>( strtol( value, nullptr, 10 ) );
    }
    received.keepRunning = httpd_query_key_value( body, "keep", value, sizeof( value ) ) == ESP_OK;
    ESP_LOGI( TAG, "Setup page saved: %s", body );
    taskENTER_CRITICAL( &portal->m_lock );
    portal->m_received = received;
    portal->m_hasReceived = true;
    taskEXIT_CRITICAL( &portal->m_lock );
    noCache( request );
    return httpd_resp_sendstr( request, "Saved" );
}

// Body: "name=...&blood=...&..." (every field of the card), form-encoded.
esp_err_t Portal::onCardSave( httpd_req_t *request )
{
    Portal *portal = static_cast<Portal *>( request->user_ctx );
    portal->touch();
    char *body = s_buffer;
    const int length = request->content_len < sizeof( s_buffer ) - 1 ? request->content_len : sizeof( s_buffer ) - 1;
    int read = 0;
    while ( read < length ) {
        const int part = httpd_req_recv( request, body + read, length - read );
        if ( part <= 0 ) {
            return httpd_resp_send_err( request, HTTPD_400_BAD_REQUEST, "No data" );
        }
        read += part;
    }
    body[read] = '\0';
    static EmergencyCard card;
    card = {};
    for ( int i = 0; i < EmergencyCard::FieldCount; ++i ) {
        // A field three times as long as it may be: every byte "%XX".
        char text[EmergencyCard::FieldSize * 3 + 8];
        if ( httpd_query_key_value( body, s_cardNames[i], text, sizeof( text ) ) == ESP_OK ) {
            urlDecode( text );
            card.set( static_cast<EmergencyCard::Field>( i ), text );
        }
    }
    taskENTER_CRITICAL( &portal->m_lock );
    portal->m_receivedCard = card;
    portal->m_hasCard = true;
    portal->m_card = card;
    taskEXIT_CRITICAL( &portal->m_lock );
    noCache( request );
    return httpd_resp_sendstr( request, "Saved" );
}

// Every unknown address leads to the page: phones and computers test for a captive portal
// with their own addresses and open the page when they are redirected.
esp_err_t Portal::onNotFound( httpd_req_t *request, httpd_err_code_t error )
{
    char location[32];
    snprintf( location, sizeof( location ), "http://%s/", s_running != nullptr ? s_running->address() : "192.168.4.1" );
    httpd_resp_set_status( request, "302 Found" );
    httpd_resp_set_hdr( request, "Location", location );
    // iOS needs a body to detect the portal.
    return httpd_resp_sendstr( request, "Redirect to the setup page" );
}
