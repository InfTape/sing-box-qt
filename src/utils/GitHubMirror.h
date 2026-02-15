#ifndef GITHUBMIRROR_H
#define GITHUBMIRROR_H

#include <QString>
#include <QStringList>

namespace GitHubMirror {
inline QStringList prefixes() {
  return {
      // ghproxy.net is currently the most stable in our observed environments.
      "https://ghproxy.net/",
      // Keep GitHub origin as secondary fallback (use raw URL directly).
      "",
      // Additional mirrors as tertiary fallbacks.
      "https://ghproxy.com/",
      "https://mirror.ghproxy.com/",
  };
}

inline QStringList buildUrls(const QString& rawUrl) {
  if (rawUrl.isEmpty()) {
    return {};
  }
  const QString url = rawUrl.trimmed();
  QStringList urls;
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
