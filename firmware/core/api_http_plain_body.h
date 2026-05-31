#pragma once

#include <cstddef>

/** Adopt malloc'd POST plain body from WebServer parser (single active request). */
extern "C" void helio_api_store_plain_body(char *data, size_t len);

void api_http_clear_plain_body(void);
size_t api_http_plain_body_length(void);
const char *api_http_plain_body_data(void);
