#pragma once

#include <cstdint>

#include "Settings.h"
#include "esp_err.h"
#include "esp_http_server.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"

// Setup portal (CLAUDE.md, M9): a WPA2 soft AP with a captive portal. A phone or computer that
// joins it opens the setup page by itself (the DNS server answers every name with the watch's
// address); the page sends the browser's UTC time, the UTC offset, the location and the
// settings. The HTTP handlers run in the server's task; the main task applies what arrived.
class Portal
{
public:
    // What the page sent, taken by the main task.
    struct Received
    {
        bool timeValid;
        int64_t unixMs;     // browser time, estimated for the moment the request arrived
        int64_t atTimerUs;  // esp_timer time of that moment
        bool valueValid[Settings::ItemCount];
        int values[Settings::ItemCount];
    };

    // Reads the stored password, or creates one. NVS must be initialised (Settings::load()).
    void init();

    // Starts the soft AP, DHCP, DNS and HTTP servers. The page reads the current values from
    // `settings` while the portal runs.
    esp_err_t start( const Settings &settings );

    // Stops everything start() started; safe to call when not running.
    void stop();

    bool isRunning() const;

    // Seconds since the last request from a browser (or since the start).
    int idleSeconds() const;

    // A new random password, stored in NVS; only while stopped.
    void newPassword();

    const char *ssid() const;
    const char *password() const;
    const char *address() const;

    // True once for each save from the page.
    bool takeReceived( Received &received );

private:
    static esp_err_t onPage( httpd_req_t *request );
    static esp_err_t onCities( httpd_req_t *request );
    static esp_err_t onState( httpd_req_t *request );
    static esp_err_t onPing( httpd_req_t *request );
    static esp_err_t onSave( httpd_req_t *request );
    static esp_err_t onNotFound( httpd_req_t *request, httpd_err_code_t error );
    void touch();

    const Settings *m_settings = nullptr;
    esp_netif_t *m_netif = nullptr;
    httpd_handle_t m_server = nullptr;
    void *m_dns = nullptr;
    char m_ssid[24] = "";
    char m_password[12] = "";
    char m_address[16] = "";
    int64_t m_lastRequestUs = 0;

    portMUX_TYPE m_lock = portMUX_INITIALIZER_UNLOCKED;
    bool m_hasReceived = false;
    Received m_received = {};
};
