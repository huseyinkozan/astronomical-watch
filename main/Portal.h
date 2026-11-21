#pragma once

#include <cstdint>

#include "Settings.h"
#include "AlarmStore.h"
#include "DayCounters.h"
#include "EmergencyCard.h"
#include "FaceConfig.h"
#include "WorldClock.h"
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
        // The world clock and the own zone's IANA name; citiesValid: the page sent them.
        bool citiesValid;
        int cityCount;
        WorldClock::City cities[WorldClock::MaxCities];
        char homeZoneName[WorldClock::ZoneNameLength];
        bool placeValid;
        char place[WorldClock::NameLength];
        // The day counters; countersValid: the page sent them.
        bool countersValid;
        int counterCount;
        DayCounters::Counter counters[DayCounters::MaxCounters];
        // The alarms; alarmsValid: the page sent them (invalid entries are left out).
        bool alarmsValid;
        AlarmStore::List alarms;
        // The face pages; facesValid: the page sent them and they decoded.
        bool facesValid;
        FaceConfig::Pages faces;
        // "Show on the watch": the face page to show now (-1: none), and the portal stays on.
        int faceShow;
        bool keepRunning;
    };

    // Reads the stored password, or creates one. NVS must be initialised (Settings::load()).
    void init();

    // Starts the soft AP, DHCP, DNS and HTTP servers. The page reads the current values from
    // `settings` while the portal runs.
    esp_err_t start( const Settings &settings, const WorldClock &worldClock, const DayCounters &counters,
                     const FaceConfig::Pages &faces, const AlarmStore::List &alarms, const EmergencyCard &card );

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

    // True once for each emergency card from the page (its own request, /api/card, before the
    // save: the save's form has no room left for it).
    bool takeCard( EmergencyCard &card );

    // The battery, for the update's condition (USB power or at least UpdateMinPercent).
    static const int UpdateMinPercent = 30;
    void setBattery( int percent, bool usb );

    // True once after a new image was written and chosen for the next start: the main task
    // stops the portal and restarts.
    bool takeUpdated();

private:
    static esp_err_t onPage( httpd_req_t *request );
    static esp_err_t onCities( httpd_req_t *request );
    static esp_err_t onHelp( httpd_req_t *request );
    static esp_err_t onCard( httpd_req_t *request );
    static esp_err_t onCardSave( httpd_req_t *request );
    static esp_err_t onUpdatePage( httpd_req_t *request );
    static esp_err_t onState( httpd_req_t *request );
    static esp_err_t onPing( httpd_req_t *request );
    static esp_err_t onSave( httpd_req_t *request );
    // A new application image from the page (the request body is the .bin file).
    static esp_err_t onUpdate( httpd_req_t *request );
    // Downloads for a report: the event log as text, the core dump of the last crash.
    static esp_err_t onEvents( httpd_req_t *request );
    static esp_err_t onCoreDump( httpd_req_t *request );
    static esp_err_t onNotFound( httpd_req_t *request, httpd_err_code_t error );
    void touch();

    const Settings *m_settings = nullptr;
    const WorldClock *m_worldClock = nullptr;
    const DayCounters *m_counters = nullptr;
    const AlarmStore::List *m_alarms = nullptr;
    const FaceConfig::Pages *m_faces = nullptr;
    const EmergencyCard *m_card = nullptr;
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
    bool m_hasCard = false;
    EmergencyCard m_receivedCard = {};
    int m_batteryPercent = -1;
    bool m_usb = false;
    bool m_updated = false;
    // A crash dump in flash, checked once at the start: the page offers it only then.
    bool m_hasCoreDump = false;
};
