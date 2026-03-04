#ifndef GITHUBMIRROR_H
#define GITHUBMIRROR_H

#include <QString>
#include <QStringList>

namespace GitHubMirror {
inline QStringList prefixes() {
  return {
      // Legacy mirrors as tertiary fallbacks.
      "https://ghproxy.net/",
      // gh-proxy.org main endpoint (recommended by upstream mirror provider).
      "https://gh-proxy.org/",
      // Additional mirrors with different CDN/region coverage, as secondary
      // fallbacks.
      "https://v6.gh-proxy.org/",
      // Additional gh-proxy.org CDN/region mirrors.
      "https://hk.gh-proxy.org/",
      "https://cdn.gh-proxy.org/",
      "https://edgeone.gh-proxy.org/"};
}

inline QStringList buildUrls(const QString& rawUrl) {
  if (rawUrl.isEmpty()) {
    return {};
  }
  const QString url = rawUrl.trimmed();
  QStringList   urls;
  // Try direct GitHub URL first, then fallback to mirrors.
  urls.append(url);
  for (const QString& prefix : prefixes()) {
    if (prefix.isEmpty()) {
      urls.append(url);
    } else {
      urls.append(prefix + url);
    }
  }
  urls.removeDuplicates();
  return urls;
}

inline QString withMirror(const QString& rawUrl, int mirrorIndex = 0) {
  const QStringList urls = buildUrls(rawUrl);
  if (urls.isEmpty()) {
    return {};
  }
  const int index =
      (mirrorIndex >= 0 && mirrorIndex < urls.size()) ? mirrorIndex : 0;
  return urls[index];
}
}  // namespace GitHubMirror

#endif  // GITHUBMIRROR_H
