#include "OpenSkyAuth.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardPaths>
#include <QTimer>
#include <QUrlQuery>

OpenSkyAuth::OpenSkyAuth(QNetworkAccessManager *net, QObject *parent)
    : QObject(parent), m_net(net), m_tokenUrl(defaultTokenUrl())
{
    loadCredentials();
}

QString OpenSkyAuth::configPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)
           + QStringLiteral("/opensky.json");
}

QUrl OpenSkyAuth::defaultTokenUrl()
{
    return QUrl(QStringLiteral(
        "https://auth.opensky-network.org/auth/realms/opensky-network/protocol/openid-connect/token"));
}

qint64 OpenSkyAuth::now() const
{
    return m_clock ? m_clock() : QDateTime::currentMSecsSinceEpoch();
}

void OpenSkyAuth::loadCredentials()
{
    QString id = qEnvironmentVariable("OPENSKY_CLIENT_ID").trimmed();
    QString secret = qEnvironmentVariable("OPENSKY_CLIENT_SECRET").trimmed();

    if (id.isEmpty() || secret.isEmpty()) {
        id.clear();
        secret.clear();
        QFile f(configPath());
        if (f.open(QIODevice::ReadOnly)) {
            const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
            if (doc.isObject()) {
                id = doc.object().value("client_id").toString().trimmed();
                secret = doc.object().value("client_secret").toString().trimmed();
            }
        }
        if (id.isEmpty() || secret.isEmpty()) {
            id.clear();
            secret.clear();
        }
    }

    if (id != m_clientId || secret != m_clientSecret) {
        m_token.clear();
        m_expiresAtMs = 0;
    }
    m_clientId = id;
    m_clientSecret = secret;
}

void OpenSkyAuth::withToken(TokenCallback cb)
{
    if (!cb)
        return;

    if (!hasCredentials()) {
        QTimer::singleShot(0, this, [cb = std::move(cb)]() { cb(QString(), QString()); });
        return;
    }

    if (!m_token.isEmpty() && now() < m_expiresAtMs - kRefreshMarginSecs * 1000LL) {
        const QString tok = m_token;
        QTimer::singleShot(0, this, [cb = std::move(cb), tok]() { cb(tok, QString()); });
        return;
    }

    m_waiters.append(std::move(cb));
    if (!m_inFlight)
        fetchToken();
}

void OpenSkyAuth::fetchToken()
{
    m_inFlight = true;

    QNetworkRequest req(m_tokenUrl);
    req.setHeader(QNetworkRequest::ContentTypeHeader,
                  QStringLiteral("application/x-www-form-urlencoded"));
    QUrlQuery body;
    body.addQueryItem("grant_type", "client_credentials");
    body.addQueryItem("client_id", QUrl::toPercentEncoding(m_clientId));
    body.addQueryItem("client_secret", QUrl::toPercentEncoding(m_clientSecret));

    QNetworkReply *reply = m_net->post(req, body.toString(QUrl::FullyEncoded).toUtf8());
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        // Error strings carry the HTTP status only: Qt's errorString() can
        // echo request details, so it is deliberately not used.
        if (status != 200) {
            finishFetch(QString(),
                        status > 0 ? QStringLiteral("token request failed (HTTP %1)").arg(status)
                                   : QStringLiteral("token request failed (network error)"));
            return;
        }
        const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        const QString tok = doc.isObject() ? doc.object().value("access_token").toString()
                                           : QString();
        if (tok.isEmpty()) {
            finishFetch(QString(), QStringLiteral("token response invalid (HTTP 200)"));
            return;
        }
        int life = doc.object().value("expires_in").toInt(kDefaultLifetimeSecs);
        if (life <= 0)
            life = kDefaultLifetimeSecs;
        m_token = tok;
        m_expiresAtMs = now() + life * 1000LL;
        finishFetch(tok, QString());
    });
}

void OpenSkyAuth::finishFetch(const QString &token, const QString &error)
{
    m_inFlight = false;
    if (!error.isEmpty()) {
        m_token.clear();
        m_expiresAtMs = 0;
    }
    // Take the waiters first: a callback may call withToken() again.
    const QList<TokenCallback> waiters = std::exchange(m_waiters, {});
    for (const TokenCallback &w : waiters)
        w(token, error);
}
