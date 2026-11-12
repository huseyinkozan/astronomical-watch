#include "Portal.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "dns_server.h"
#include "esp_heap_caps.h"
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
extern const char s_pageStart[] asm( "_binary_index_html_start" );
extern const char s_pageEnd[] asm( "_binary_index_html_end" );
extern const char s_citiesStart[] asm( "_binary_cities_js_start" );
extern const char s_citiesEnd[] asm( "_binary_cities_js_end" );

// Field names of the save request, one per Settings::Item.
static const char *s_fieldNames[Settings::ItemCount] = { "timeout", "alwaysOn", "brightness", "autoOff",
                                                         "offset",  "lat",      "lon",        "hijri",
                                                         "face",    "aod",      "faceItems",  "aodItems" };

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

esp_err_t Portal::start( const Settings &settings )
{
    if ( isRunning() ) {
        return ESP_OK;
    }
    m_settings = &settings;
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
    serverConfig.stack_size = 6144;
    err = httpd_start( &m_server, &serverConfig );
    if ( err != ESP_OK ) {
        ESP_LOGE( TAG, "HTTP server start failed: %s", esp_err_to_name( err ) );
        stop();
        return err;
    }
    const httpd_uri_t handlers[5] = {
        { "/", HTTP_GET, onPage, this },
        { "/cities.js", HTTP_GET, onCities, this },
        { "/api/state", HTTP_GET, onState, this },
        { "/api/ping", HTTP_GET, onPing, this },
        { "/api/save", HTTP_POST, onSave, this },
    };
    for ( int i = 0; i < 5; ++i ) {
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
    noCache( request );
    return httpd_resp_send( request, s_pageStart, s_pageEnd - s_pageStart - 1 );
}

esp_err_t Portal::onCities( httpd_req_t *request )
{
    static_cast<Portal *>( request->user_ctx )->touch();
    httpd_resp_set_type( request, "text/javascript; charset=utf-8" );
    return httpd_resp_send( request, s_citiesStart, s_citiesEnd - s_citiesStart - 1 );
}

esp_err_t Portal::onState( httpd_req_t *request )
{
    Portal *portal = static_cast<Portal *>( request->user_ctx );
    portal->touch();
    const Settings &settings = *portal->m_settings;
    char json[320];
    snprintf( json, sizeof( json ),
              "{\"timeout\":%d,\"alwaysOn\":%d,\"brightness\":%d,\"autoOff\":%d,\"face\":%d,\"aod\":%d,"
              "\"offset\":%d,\"lat\":%.6f,\"lon\":%.6f,\"hijri\":%d,\"faceItems\":%d,\"aodItems\":%d}",
              settings.value( Settings::DisplayTimeout ), settings.value( Settings::AlwaysOnFace ),
              settings.value( Settings::Brightness ), settings.value( Settings::AutoOff ),
              settings.value( Settings::FaceStyle ), settings.value( Settings::AodStyle ),
              settings.value( Settings::UtcOffset ), settings.value( Settings::Latitude ) / 1e6,
              settings.value( Settings::Longitude ) / 1e6, settings.value( Settings::HijriMethod ),
              settings.value( Settings::FaceItems ), settings.value( Settings::AodItems ) );
    httpd_resp_set_type( request, "application/json" );
    noCache( request );
    return httpd_resp_sendstr( request, json );
}

esp_err_t Portal::onPing( httpd_req_t *request )
{
    static_cast<Portal *>( request->user_ctx )->touch();
    noCache( request );
    return httpd_resp_sendstr( request, "pong" );
}

// Body: "time=<ms since 1970>&rtt=<ms>&timeout=10&...&lat=41.0082&lon=28.9784", form-encoded.
esp_err_t Portal::onSave( httpd_req_t *request )
{
    const int64_t arrivedUs = esp_timer_get_time();
    Portal *portal = static_cast<Portal *>( request->user_ctx );
    portal->touch();
    char body[384];
    const int length = request->content_len < sizeof( body ) - 1 ? request->content_len : sizeof( body ) - 1;
    int read = 0;
    while ( read < length ) {
        const int part = httpd_req_recv( request, body + read, length - read );
        if ( part <= 0 ) {
            return httpd_resp_send_err( request, HTTPD_400_BAD_REQUEST, "No data" );
        }
        read += part;
    }
    body[read] = '\0';

    Received received = {};
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
    ESP_LOGI( TAG, "Setup page saved: %s", body );
    taskENTER_CRITICAL( &portal->m_lock );
    portal->m_received = received;
    portal->m_hasReceived = true;
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
