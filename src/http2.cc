/*
 * nghttp2 - HTTP/2 C Library
 *
 * Copyright (c) 2012 Tatsuhiro Tsujikawa
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#include "http2.h"

#include "util.h"

namespace nghttp2 {

namespace http2 {

std::string get_status_string(unsigned int status_code) {
  switch (status_code) {
  case 100:
    return "100 Continue";
  case 101:
    return "101 Switching Protocols";
  case 200:
    return "200 OK";
  case 201:
    return "201 Created";
  case 202:
    return "202 Accepted";
  case 203:
    return "203 Non-Authoritative Information";
  case 204:
    return "204 No Content";
  case 205:
    return "205 Reset Content";
  case 206:
    return "206 Partial Content";
  case 300:
    return "300 Multiple Choices";
  case 301:
    return "301 Moved Permanently";
  case 302:
    return "302 Found";
  case 303:
    return "303 See Other";
  case 304:
    return "304 Not Modified";
  case 305:
    return "305 Use Proxy";
  // case 306: return "306 (Unused)";
  case 307:
    return "307 Temporary Redirect";
  case 308:
    return "308 Permanent Redirect";
  case 400:
    return "400 Bad Request";
  case 401:
    return "401 Unauthorized";
  case 402:
    return "402 Payment Required";
  case 403:
    return "403 Forbidden";
  case 404:
    return "404 Not Found";
  case 405:
    return "405 Method Not Allowed";
  case 406:
    return "406 Not Acceptable";
  case 407:
    return "407 Proxy Authentication Required";
  case 408:
    return "408 Request Timeout";
  case 409:
    return "409 Conflict";
  case 410:
    return "410 Gone";
  case 411:
    return "411 Length Required";
  case 412:
    return "412 Precondition Failed";
  case 413:
    return "413 Payload Too Large";
  case 414:
    return "414 URI Too Long";
  case 415:
    return "415 Unsupported Media Type";
  case 416:
    return "416 Requested Range Not Satisfiable";
  case 417:
    return "417 Expectation Failed";
  case 421:
    return "421 Misdirected Request";
  case 426:
    return "426 Upgrade Required";
  case 428:
    return "428 Precondition Required";
  case 429:
    return "429 Too Many Requests";
  case 431:
    return "431 Request Header Fields Too Large";
  case 500:
    return "500 Internal Server Error";
  case 501:
    return "501 Not Implemented";
  case 502:
    return "502 Bad Gateway";
  case 503:
    return "503 Service Unavailable";
  case 504:
    return "504 Gateway Timeout";
  case 505:
    return "505 HTTP Version Not Supported";
  case 511:
    return "511 Network Authentication Required";
  default:
    return util::utos(status_code);
  }
}

void capitalize(std::string &s, size_t offset) {
  s[offset] = util::upcase(s[offset]);
  for (size_t i = offset + 1, eoi = s.size(); i < eoi; ++i) {
    if (s[i - 1] == '-') {
      s[i] = util::upcase(s[i]);
    } else {
      s[i] = util::lowcase(s[i]);
    }
  }
}

bool lws(const char *value) {
  for (; *value; ++value) {
    switch (*value) {
    case '\t':
    case ' ':
      continue;
    default:
      return false;
    }
  }
  return true;
}

void copy_url_component(std::string &dest, const http_parser_url *u, int field,
                        const char *url) {
  if (u->field_set & (1 << field)) {
    dest.assign(url + u->field_data[field].off, u->field_data[field].len);
  }
}

Headers::value_type to_header(const uint8_t *name, size_t namelen,
                              const uint8_t *value, size_t valuelen,
                              bool no_index) {
  return Header(std::string(reinterpret_cast<const char *>(name), namelen),
                std::string(reinterpret_cast<const char *>(value), valuelen),
                no_index);
}

void add_header(Headers &nva, const uint8_t *name, size_t namelen,
                const uint8_t *value, size_t valuelen, bool no_index) {
  if (valuelen > 0) {
    size_t i, j;
    for (i = 0; i < valuelen && (value[i] == ' ' || value[i] == '\t'); ++i)
      ;
    for (j = valuelen - 1; j > i && (value[j] == ' ' || value[j] == '\t'); --j)
      ;
    value += i;
    valuelen -= i + (valuelen - j - 1);
  }
  nva.push_back(to_header(name, namelen, value, valuelen, no_index));
}

const Headers::value_type *get_header(const Headers &nva, const char *name) {
  const Headers::value_type *res = nullptr;
  for (auto &nv : nva) {
    if (nv.name == name) {
      res = &nv;
    }
  }
  return res;
}

std::string value_to_str(const Headers::value_type *nv) {
  if (nv) {
    return nv->value;
  }
  return "";
}

bool non_empty_value(const Headers::value_type *nv) {
  return nv && !nv->value.empty();
}

nghttp2_nv make_nv(const std::string &name, const std::string &value,
                   bool no_index) {
  uint8_t flags;

  flags = no_index ? NGHTTP2_NV_FLAG_NO_INDEX : NGHTTP2_NV_FLAG_NONE;

  return {(uint8_t *)name.c_str(), (uint8_t *)value.c_str(), name.size(),
          value.size(), flags};
}

void copy_headers_to_nva(std::vector<nghttp2_nv> &nva, const Headers &headers) {
  for (auto &kv : headers) {
    if (kv.name.empty() || kv.name[0] == ':') {
      continue;
    }
    switch (lookup_token(kv.name)) {
    case HD_COOKIE:
    case HD_CONNECTION:
    case HD_HTTP2_SETTINGS:
    case HD_KEEP_ALIVE:
    case HD_PROXY_CONNECTION:
    case HD_SERVER:
    case HD_TRAILER:
    case HD_TRANSFER_ENCODING:
    case HD_UPGRADE:
    case HD_VIA:
    case HD_X_FORWARDED_FOR:
    case HD_X_FORWARDED_PROTO:
      continue;
    }
    nva.push_back(make_nv(kv.name, kv.value, kv.no_index));
  }
}

void build_http1_headers_from_headers(std::string &hdrs,
                                      const Headers &headers) {
  for (auto &kv : headers) {
    if (kv.name.empty() || kv.name[0] == ':') {
      continue;
    }
    switch (lookup_token(kv.name)) {
    case HD_CONNECTION:
    case HD_COOKIE:
    case HD_HTTP2_SETTINGS:
    case HD_KEEP_ALIVE:
    case HD_PROXY_CONNECTION:
    case HD_SERVER:
    case HD_TRAILER:
    case HD_UPGRADE:
    case HD_VIA:
    case HD_X_FORWARDED_FOR:
    case HD_X_FORWARDED_PROTO:
      continue;
    }
    hdrs += kv.name;
    capitalize(hdrs, hdrs.size() - kv.name.size());
    hdrs += ": ";
    hdrs += kv.value;
    hdrs += "\r\n";
  }
}

int32_t determine_window_update_transmission(nghttp2_session *session,
                                             int32_t stream_id) {
  int32_t recv_length, window_size;
  if (stream_id == 0) {
    recv_length = nghttp2_session_get_effective_recv_data_length(session);
    window_size = nghttp2_session_get_effective_local_window_size(session);
  } else {
    recv_length = nghttp2_session_get_stream_effective_recv_data_length(
        session, stream_id);
    window_size = nghttp2_session_get_stream_effective_local_window_size(
        session, stream_id);
  }
  if (recv_length != -1 && window_size != -1) {
    if (recv_length >= window_size / 2) {
      return recv_length;
    }
  }
  return -1;
}

void dump_nv(FILE *out, const char **nv) {
  for (size_t i = 0; nv[i]; i += 2) {
    fwrite(nv[i], strlen(nv[i]), 1, out);
    fwrite(": ", 2, 1, out);
    fwrite(nv[i + 1], strlen(nv[i + 1]), 1, out);
    fwrite("\n", 1, 1, out);
  }
  fwrite("\n", 1, 1, out);
  fflush(out);
}

void dump_nv(FILE *out, const nghttp2_nv *nva, size_t nvlen) {
  auto end = nva + nvlen;
  for (; nva != end; ++nva) {
    fwrite(nva->name, nva->namelen, 1, out);
    fwrite(": ", 2, 1, out);
    fwrite(nva->value, nva->valuelen, 1, out);
    fwrite("\n", 1, 1, out);
  }
  fwrite("\n", 1, 1, out);
  fflush(out);
}

void dump_nv(FILE *out, const Headers &nva) {
  for (auto &nv : nva) {
    fwrite(nv.name.c_str(), nv.name.size(), 1, out);
    fwrite(": ", 2, 1, out);
    fwrite(nv.value.c_str(), nv.value.size(), 1, out);
    fwrite("\n", 1, 1, out);
  }
  fwrite("\n", 1, 1, out);
  fflush(out);
}

std::string rewrite_location_uri(const std::string &uri,
                                 const http_parser_url &u,
                                 const std::string &request_host,
                                 const std::string &upstream_scheme,
                                 uint16_t upstream_port) {
  // We just rewrite host and optionally port. We don't rewrite https
  // link. Not sure it happens in practice.
  if (u.field_set & (1 << UF_SCHEMA)) {
    auto field = &u.field_data[UF_SCHEMA];
    if (!util::streq("http", &uri[field->off], field->len)) {
      return "";
    }
  }
  if ((u.field_set & (1 << UF_HOST)) == 0) {
    return "";
  }
  auto field = &u.field_data[UF_HOST];
  if (!util::startsWith(std::begin(request_host), std::end(request_host),
                        &uri[field->off], &uri[field->off] + field->len) ||
      (request_host.size() != field->len && request_host[field->len] != ':')) {
    return "";
  }
  std::string res = upstream_scheme;
  res += "://";
  res.append(&uri[field->off], field->len);
  if (upstream_scheme == "http") {
    if (upstream_port != 80) {
      res += ":";
      res += util::utos(upstream_port);
    }
  } else if (upstream_scheme == "https") {
    if (upstream_port != 443) {
      res += ":";
      res += util::utos(upstream_port);
    }
  }
  if (u.field_set & (1 << UF_PATH)) {
    field = &u.field_data[UF_PATH];
    res.append(&uri[field->off], field->len);
  }
  if (u.field_set & (1 << UF_QUERY)) {
    field = &u.field_data[UF_QUERY];
    res += "?";
    res.append(&uri[field->off], field->len);
  }
  if (u.field_set & (1 << UF_FRAGMENT)) {
    field = &u.field_data[UF_FRAGMENT];
    res += "#";
    res.append(&uri[field->off], field->len);
  }
  return res;
}

int check_nv(const uint8_t *name, size_t namelen, const uint8_t *value,
             size_t valuelen) {
  if (!nghttp2_check_header_name(name, namelen)) {
    return 0;
  }
  if (!nghttp2_check_header_value(value, valuelen)) {
    return 0;
  }
  return 1;
}

int parse_http_status_code(const std::string &src) {
  if (src.size() != 3) {
    return -1;
  }

  int status = 0;
  for (auto c : src) {
    if (!isdigit(c)) {
      return -1;
    }
    status *= 10;
    status += c - '0';
  }

  if (status < 100) {
    return -1;
  }

  return status;
}

int lookup_token(const std::string &name) {
  return lookup_token(reinterpret_cast<const uint8_t *>(name.c_str()),
                      name.size());
}

// This function was generated by genheaderfunc.py.  Inspired by h2o
// header lookup.  https://github.com/h2o/h2o
int lookup_token(const uint8_t *name, size_t namelen) {
  switch (namelen) {
  case 2:
    switch (name[namelen - 1]) {
    case 'e':
      if (util::streq("t", name, 1)) {
        return HD_TE;
      }
      break;
    }
    break;
  case 3:
    switch (name[namelen - 1]) {
    case 'a':
      if (util::streq("vi", name, 2)) {
        return HD_VIA;
      }
      break;
    }
    break;
  case 4:
    switch (name[namelen - 1]) {
    case 't':
      if (util::streq("hos", name, 3)) {
        return HD_HOST;
      }
      break;
    }
    break;
  case 5:
    switch (name[namelen - 1]) {
    case 'h':
      if (util::streq(":pat", name, 4)) {
        return HD__PATH;
      }
      break;
    case 't':
      if (util::streq(":hos", name, 4)) {
        return HD__HOST;
      }
      break;
    }
    break;
  case 6:
    switch (name[namelen - 1]) {
    case 'e':
      if (util::streq("cooki", name, 5)) {
        return HD_COOKIE;
      }
      break;
    case 'r':
      if (util::streq("serve", name, 5)) {
        return HD_SERVER;
      }
      break;
    case 't':
      if (util::streq("expec", name, 5)) {
        return HD_EXPECT;
      }
      break;
    }
    break;
  case 7:
    switch (name[namelen - 1]) {
    case 'c':
      if (util::streq("alt-sv", name, 6)) {
        return HD_ALT_SVC;
      }
      break;
    case 'd':
      if (util::streq(":metho", name, 6)) {
        return HD__METHOD;
      }
      break;
    case 'e':
      if (util::streq(":schem", name, 6)) {
        return HD__SCHEME;
      }
      if (util::streq("upgrad", name, 6)) {
        return HD_UPGRADE;
      }
      break;
    case 'r':
      if (util::streq("traile", name, 6)) {
        return HD_TRAILER;
      }
      break;
    case 's':
      if (util::streq(":statu", name, 6)) {
        return HD__STATUS;
      }
      break;
    }
    break;
  case 8:
    switch (name[namelen - 1]) {
    case 'n':
      if (util::streq("locatio", name, 7)) {
        return HD_LOCATION;
      }
      break;
    }
    break;
  case 10:
    switch (name[namelen - 1]) {
    case 'e':
      if (util::streq("keep-aliv", name, 9)) {
        return HD_KEEP_ALIVE;
      }
      break;
    case 'n':
      if (util::streq("connectio", name, 9)) {
        return HD_CONNECTION;
      }
      break;
    case 'y':
      if (util::streq(":authorit", name, 9)) {
        return HD__AUTHORITY;
      }
      break;
    }
    break;
  case 14:
    switch (name[namelen - 1]) {
    case 'h':
      if (util::streq("content-lengt", name, 13)) {
        return HD_CONTENT_LENGTH;
      }
      break;
    case 's':
      if (util::streq("http2-setting", name, 13)) {
        return HD_HTTP2_SETTINGS;
      }
      break;
    }
    break;
  case 15:
    switch (name[namelen - 1]) {
    case 'r':
      if (util::streq("x-forwarded-fo", name, 14)) {
        return HD_X_FORWARDED_FOR;
      }
      break;
    }
    break;
  case 16:
    switch (name[namelen - 1]) {
    case 'n':
      if (util::streq("proxy-connectio", name, 15)) {
        return HD_PROXY_CONNECTION;
      }
      break;
    }
    break;
  case 17:
    switch (name[namelen - 1]) {
    case 'e':
      if (util::streq("if-modified-sinc", name, 16)) {
        return HD_IF_MODIFIED_SINCE;
      }
      break;
    case 'g':
      if (util::streq("transfer-encodin", name, 16)) {
        return HD_TRANSFER_ENCODING;
      }
      break;
    case 'o':
      if (util::streq("x-forwarded-prot", name, 16)) {
        return HD_X_FORWARDED_PROTO;
      }
      break;
    }
    break;
  }
  return -1;
}

void init_hdidx(int *hdidx) { memset(hdidx, -1, sizeof(hdidx[0]) * HD_MAXIDX); }

void index_headers(int *hdidx, const Headers &headers) {
  for (size_t i = 0; i < headers.size(); ++i) {
    auto &kv = headers[i];
    auto token = lookup_token(
        reinterpret_cast<const uint8_t *>(kv.name.c_str()), kv.name.size());
    if (token >= 0) {
      http2::index_header(hdidx, token, i);
    }
  }
}

void index_header(int *hdidx, int token, size_t idx) {
  if (token == -1) {
    return;
  }
  assert(token < HD_MAXIDX);
  hdidx[token] = idx;
}

bool check_http2_request_pseudo_header(const int *hdidx, int token) {
  switch (token) {
  case HD__AUTHORITY:
  case HD__METHOD:
  case HD__PATH:
  case HD__SCHEME:
    return hdidx[token] == -1;
  default:
    return false;
  }
}

bool check_http2_response_pseudo_header(const int *hdidx, int token) {
  switch (token) {
  case HD__STATUS:
    return hdidx[token] == -1;
  default:
    return false;
  }
}

bool http2_header_allowed(int token) {
  switch (token) {
  case HD_CONNECTION:
  case HD_KEEP_ALIVE:
  case HD_PROXY_CONNECTION:
  case HD_TRANSFER_ENCODING:
  case HD_UPGRADE:
    return false;
  default:
    return true;
  }
}

bool http2_mandatory_request_headers_presence(const int *hdidx) {
  if (hdidx[HD__METHOD] == -1 || hdidx[HD__PATH] == -1 ||
      hdidx[HD__SCHEME] == -1 ||
      (hdidx[HD__AUTHORITY] == -1 && hdidx[HD_HOST] == -1)) {
    return false;
  }
  return true;
}

const Headers::value_type *get_header(const int *hdidx, int token,
                                      const Headers &nva) {
  auto i = hdidx[token];
  if (i == -1) {
    return nullptr;
  }
  return &nva[i];
}

} // namespace http2

} // namespace nghttp2