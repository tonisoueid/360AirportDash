#pragma once

#include <QDateTime>
#include <QList>
#include <QObject>
#include <QString>
#include <QUrl>
#include <functional>

class QNetworkAccessManager;
class QNetworkReply;

// OAuth2 client-credentials token manager for the OpenSky API.
//
// Credentials come from OPENSKY_CLIENT_ID / OPENSKY_CLIENT_SECRET (both must
// be set; env wins) or, failing that, from `opensky.json` in the app config
// dir: {"client_id": "...", "client_secret": "..."}. With neither, the
// manager is in anonymous mode and withToken() yields an empty token.
//
// SECRETS POLICY: the client id, secret and access token are never written to
// any log or error string. Errors name the HTTP status only.
class OpenSkyAuth : public QObject
{
    Q_OBJECT
public:
    // `token` is a bearer token, or empty in anonymous mode. `error` is empty
    // on success; on failure `token` is empty and `error` is a secret-free
    // description (e.g. "token request failed (HTTP 401)").
    using TokenCallback = std::function<void(const QString &token, const QString &error)>;

    static constexpr int kRefreshMarginSecs = 30;
    static constexpr int kDefaultLifetimeSecs = 1800;

    explicit OpenSkyAuth(QNetworkAccessManager *net, QObject *parent = nullptr);

    static QString configPath();
    static QUrl defaultTokenUrl();

    // (Re)read credentials: env first, then the config file. Drops any
    // cached token when the credentials change.
    void loadCredentials();
    bool hasCredentials() const { return !m_clientId.isEmpty() && !m_clientSecret.isEmpty(); }

    // Test seam: point the grant at another endpoint.
    void setTokenUrl(const QUrl &url) { m_tokenUrl = url; }
    // Test seam: injectable clock (ms since epoch).
    void setClock(std::function<qint64()> clock) { m_clock = std::move(clock); }

    // Async. The callback is always invoked from the event loop, never
    // re-entrantly. Concurrent callers share one in-flight token fetch.
    void withToken(TokenCallback cb);

private:
    void fetchToken();
    void finishFetch(const QString &token, const QString &error);
    qint64 now() const;

    QNetworkAccessManager *m_net;
    QUrl m_tokenUrl;
    QString m_clientId;
    QString m_clientSecret;
    QString m_token;
    qint64 m_expiresAtMs = 0;
    bool m_inFlight = false;
    QList<TokenCallback> m_waiters;
    std::function<qint64()> m_clock;
};
