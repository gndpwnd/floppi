/*
 * WiFi Certificates (Enterprise)
 * Only included when USE_WIFI_CERTS is defined in config.h
 * (legacy alias WIFI_USE_CERTS also accepted).
 *
 * Replace the placeholder strings with your actual PEM certificates.
 * Get these from your IT department or network admin.
 *
 * Which fields each EAP method needs:
 *   WIFI_EAP_METHOD_PEAP:  CA cert only (validates RADIUS server; optional)
 *   WIFI_EAP_METHOD_TTLS:  CA cert only (validates RADIUS server; optional)
 *   WIFI_EAP_METHOD_TLS:   CA cert + client cert + client key (mutual auth)
 *
 * This file is also the reserved home for a CA blob that future HTTPS / OTA-
 * over-TLS work can hand to WiFiClientSecure::setCACert() — see
 * docs/features/wifi-configuration.md.
 */

#ifndef WIFI_CERTS_H
#define WIFI_CERTS_H

// CA certificate (root certificate of your network's RADIUS server)
static const char WIFI_CA_CERT[] =
    "-----BEGIN CERTIFICATE-----\n"
    "REPLACE_WITH_YOUR_CA_CERTIFICATE\n"
    "-----END CERTIFICATE-----\n";

// Client certificate (for TLS mutual authentication)
// Not needed for PEAP — leave as empty string if unused
static const char WIFI_CLIENT_CERT[] = "";

// Client private key (for TLS mutual authentication)
// Not needed for PEAP — leave as empty string if unused
static const char WIFI_CLIENT_KEY[] = "";

#endif // WIFI_CERTS_H
