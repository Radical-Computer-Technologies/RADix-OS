#ifndef RADIX_USER_AUTH_H
#define RADIX_USER_AUTH_H

#include <stddef.h>
#include <stdint.h>

void radix_auth_sha256_hex(const char *text, char out_hex[65]);
void radix_auth_password_hash(const char *salt, const char *password, char out_hex[65]);

#endif
