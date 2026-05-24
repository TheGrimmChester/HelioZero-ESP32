#include <gtest/gtest.h>

#include "helio_http_response_logic.h"

TEST(HelioHttpResponseLogic, StatusOkFor2xx) {
  EXPECT_TRUE(helio_http_response_status_ok("HTTP/1.0 200 OK\r\n"));
  EXPECT_TRUE(helio_http_response_status_ok("HTTP/1.1 204 No Content\r\n"));
  EXPECT_FALSE(helio_http_response_status_ok("HTTP/1.1 301 Moved\r\n"));
  EXPECT_FALSE(helio_http_response_status_ok("not http"));
}

TEST(HelioHttpResponseLogic, ExtractHttp10JsonBody) {
  const std::string wire =
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: application/json\r\n"
      "Connection: close\r\n"
      "\r\n"
      R"({"dateJour":"2026-05-21","codeJour":1,"libCouleur":"Bleu"})";
  const std::string body = helio_http_response_extract_body(wire);
  EXPECT_EQ(body, R"({"dateJour":"2026-05-21","codeJour":1,"libCouleur":"Bleu"})");
}

TEST(HelioHttpResponseLogic, ExtractChunkedBody) {
  const std::string wire =
      "HTTP/1.1 200 OK\r\n"
      "Transfer-Encoding: chunked\r\n"
      "\r\n"
      "26\r\n"
      R"({"dateJour":"2026-05-21","codeJour":1})"
      "\r\n"
      "0\r\n"
      "\r\n";
  const std::string body = helio_http_response_extract_body(wire);
  EXPECT_EQ(body, R"({"dateJour":"2026-05-21","codeJour":1})");
}

TEST(HelioHttpResponseLogic, RejectsRedirectAndEmpty) {
  const std::string redirect =
      "HTTP/1.1 301 Moved\r\nLocation: https://example/\r\n\r\n";
  EXPECT_TRUE(helio_http_response_extract_body(redirect).empty());
  const std::string empty_ok = "HTTP/1.0 200 OK\r\n\r\n";
  EXPECT_TRUE(helio_http_response_extract_body(empty_ok).empty());
}
