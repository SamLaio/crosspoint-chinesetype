#include "UrlUtils.h"

#include <vector>

namespace UrlUtils {

namespace {
std::string normalizeUrlPath(const std::string& url) {
  const size_t protocolEnd = url.find("://");
  const size_t hostEnd = protocolEnd == std::string::npos ? 0 : url.find('/', protocolEnd + 3);
  if (hostEnd == std::string::npos) {
    return url;
  }

  const std::string prefix = url.substr(0, hostEnd);
  std::string pathAndQuery = url.substr(hostEnd);
  std::string query;
  const size_t queryPos = pathAndQuery.find('?');
  if (queryPos != std::string::npos) {
    query = pathAndQuery.substr(queryPos);
    pathAndQuery.erase(queryPos);
  }

  std::vector<std::string> parts;
  size_t start = 1;
  while (start <= pathAndQuery.size()) {
    const size_t slash = pathAndQuery.find('/', start);
    const std::string part = pathAndQuery.substr(start, slash == std::string::npos ? std::string::npos : slash - start);
    if (part == "..") {
      if (!parts.empty()) {
        parts.pop_back();
      }
    } else if (!part.empty() && part != ".") {
      parts.push_back(part);
    }
    if (slash == std::string::npos) {
      break;
    }
    start = slash + 1;
  }

  std::string normalized = prefix;
  for (const auto& part : parts) {
    normalized += "/" + part;
  }
  if (!pathAndQuery.empty() && pathAndQuery.back() == '/' && (normalized.empty() || normalized.back() != '/')) {
    normalized += "/";
  }
  return normalized + query;
}

std::string appendQueryHref(std::string baseUrl, const std::string& href) {
  const size_t queryPos = baseUrl.find('?');
  if (queryPos != std::string::npos) {
    baseUrl.erase(queryPos);
  }

  const size_t protocolEnd = baseUrl.find("://");
  const size_t hostStart = protocolEnd == std::string::npos ? 0 : protocolEnd + 3;
  if (baseUrl.find('/', hostStart) == std::string::npos) {
    baseUrl += "/";
  }
  return normalizeUrlPath(baseUrl + href);
}
}  // namespace

bool isHttpsUrl(const std::string& url) { return url.rfind("https://", 0) == 0; }

std::string ensureProtocol(const std::string& url) {
  if (url.find("://") == std::string::npos) {
    return "http://" + url;
  }
  return url;
}

std::string extractHost(const std::string& url) {
  const size_t protocolEnd = url.find("://");
  if (protocolEnd == std::string::npos) {
    // No protocol, find first slash
    const size_t firstSlash = url.find('/');
    return firstSlash == std::string::npos ? url : url.substr(0, firstSlash);
  }
  // Find the first slash after the protocol
  const size_t hostStart = protocolEnd + 3;
  const size_t pathStart = url.find('/', hostStart);
  return pathStart == std::string::npos ? url : url.substr(0, pathStart);
}

std::string buildUrl(const std::string& serverUrl, const std::string& path) {
  const std::string urlWithProtocol = ensureProtocol(serverUrl);
  if (path.empty()) {
    return urlWithProtocol;
  }
  if (path.find("://") != std::string::npos) {
    return ensureProtocol(path);
  }
  if (path[0] == '/') {
    // Absolute path - use just the host
    return extractHost(urlWithProtocol) + path;
  }
  // Relative path - append to server URL
  if (urlWithProtocol.back() == '/') {
    return urlWithProtocol + path;
  }
  return urlWithProtocol + "/" + path;
}

std::string resolveUrl(const std::string& baseUrl, const std::string& href) {
  if (href.empty()) {
    return ensureProtocol(baseUrl);
  }
  if (href.find("://") != std::string::npos) {
    return ensureProtocol(href);
  }

  const std::string base = ensureProtocol(baseUrl);
  if (!href.empty() && href[0] == '/') {
    return normalizeUrlPath(extractHost(base) + href);
  }

  std::string baseWithoutFragment = base;
  const size_t fragmentPos = baseWithoutFragment.find('#');
  if (fragmentPos != std::string::npos) {
    baseWithoutFragment.erase(fragmentPos);
  }

  if (!href.empty() && href[0] == '?') {
    return appendQueryHref(baseWithoutFragment, href);
  }

  const size_t queryPos = baseWithoutFragment.find('?');
  if (queryPos != std::string::npos) {
    baseWithoutFragment.erase(queryPos);
  }

  if (!baseWithoutFragment.empty() && baseWithoutFragment.back() == '/') {
    return normalizeUrlPath(baseWithoutFragment + href);
  }

  return normalizeUrlPath(baseWithoutFragment + "/" + href);
}

}  // namespace UrlUtils
