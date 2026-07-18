#ifndef RAD_USER_AUTH_H
#define RAD_USER_AUTH_H

#include <stddef.h>
#include <stdint.h>

void rad_auth_sha256_hex(const char *text, char out_hex[65]);
void rad_auth_password_hash(const char *salt, const char *password, char out_hex[65]);

#endif
