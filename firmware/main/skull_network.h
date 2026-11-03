#pragma once
namespace skull::catalog { struct Catalog; }
namespace skull::network {
// After SD/config and radio/Matter owners are established. Never reconfigures
// primary Matter Wi-Fi; secondary uses only the existing SD Wi-Fi credentials.
void init(bool primary, catalog::Catalog *catalog);
// Existing diagnostic/control owner only: apply bounded commands and snapshot.
void poll(bool idle);
void status();
bool wifi_started();
// Idle USB maintenance only. URI username password, '-' means empty auth field.
// Writes only broker credentials in a separate NVS namespace; reboot to apply.
bool configure_mqtt(const char *arguments);
}
