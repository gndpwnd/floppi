/*
 * WiFi Certificates (Enterprise)
 * Only included when WIFI_USE_CERTS is defined in wifi_credentials.h.
 *
 * Replace the placeholder strings with your actual PEM certificates.
 * Get these from your IT department or network admin.
 *
 * Not all fields are required for every auth method:
 *   PEAP with CA:  CA cert only (verifies server identity)
 *   TLS:           CA cert + client cert + client key (mutual auth)
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
