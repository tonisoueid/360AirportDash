#include "SiteRegistry.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>

namespace {
constexpr int kMinSites = 1;
constexpr int kMaxSites = 60;
constexpr int kMinPollSecs = 60;
constexpr int kMaxPollSecs = 86400;

bool isIcao(const QString &c)
{
    if (c.size() != 4)
        return false;
    for (const QChar ch : c)
        if (ch < QLatin1Char('A') || ch > QLatin1Char('Z'))
            return false;
    return true;
}
}

SiteRegistry::SiteRegistry(QObject *parent)
    : QObject(parent), m_sites(siteList())
{
}

QString SiteRegistry::configPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)
           + QStringLiteral("/sites.json");
}

Site SiteRegistry::normalized(const Site &s)
{
    Site n = s;
    n.name = s.name.trimmed().toUpper();
    n.code = s.code.trimmed().toUpper();
    n.region = s.region.trimmed().toUpper();
    n.endpoint = s.endpoint.trimmed();   // URLs are case-sensitive: trim only
    return n;
}

QString SiteRegistry::validateList(const QVector<Site> &list)
{
    if (list.size() < kMinSites)
        return QStringLiteral("at least %1 site required").arg(kMinSites);
    if (list.size() > kMaxSites)
        return QStringLiteral("roster capped at %1 sites").arg(kMaxSites);

    QSet<QString> seenCodes;
    for (const Site &raw : list) {
        const Site s = normalized(raw);
        if (s.name.isEmpty())
            return QStringLiteral("site with empty name");
        if (!isIcao(s.code))
            return QStringLiteral("%1: ICAO code must be exactly 4 letters A-Z").arg(s.name);
        if (s.pollIntervalSecs < kMinPollSecs || s.pollIntervalSecs > kMaxPollSecs)
            return QStringLiteral("%1: poll interval must be %2-%3 s")
                .arg(s.code).arg(kMinPollSecs).arg(kMaxPollSecs);
        if (seenCodes.contains(s.code))
            return QStringLiteral("duplicate ICAO %1").arg(s.code);
        seenCodes.insert(s.code);
    }
    return QString();
}

SiteRegistry::LoadResult SiteRegistry::load()
{
    QFile f(configPath());
    if (!f.exists())
        return LoadResult::Defaults;
    if (!f.open(QIODevice::ReadOnly))
        return LoadResult::Corrupt;

    const QJsonObject root = QJsonDocument::fromJson(f.readAll()).object();
    const QJsonArray arr = root.value("sites").toArray();
    if (arr.isEmpty())
        return LoadResult::Corrupt;

    QVector<Site> parsed;
    parsed.reserve(arr.size());
    for (const QJsonValue &v : arr) {
        const QJsonObject o = v.toObject();
        Site s;
        s.name = o.value("name").toString();
        s.code = o.value("code").toString();
        s.region = o.value("region").toString();
        s.endpoint = o.value("endpoint").toString();
        // Fields added later: absent in older files -> defaults.
        s.enabled = o.value("enabled").toBool(true);
        s.pinned = o.value("pinned").toBool(false);
        s.pollIntervalSecs = o.value("pollIntervalSecs").toInt(kDefaultPollSecs);
        parsed.push_back(s);
    }
    if (!validateList(parsed).isEmpty())
        return LoadResult::Corrupt;

    m_sites.clear();
    for (const Site &s : parsed)
        m_sites.push_back(normalized(s));
    return LoadResult::File;
}

bool SiteRegistry::save() const
{
    QJsonArray arr;
    for (const Site &s : m_sites) {
        QJsonObject o;
        o.insert("name", s.name);
        o.insert("code", s.code);
        o.insert("region", s.region);
        o.insert("endpoint", s.endpoint);
        o.insert("enabled", s.enabled);
        o.insert("pinned", s.pinned);
        o.insert("pollIntervalSecs", s.pollIntervalSecs);
        arr.append(o);
    }
    QJsonObject root;
    root.insert("version", 2);
    root.insert("sites", arr);

    const QString path = configPath();
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        return false;
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return f.commit();
}

bool SiteRegistry::setSites(const QVector<Site> &list)
{
    if (!validateList(list).isEmpty())
        return false;
    m_sites.clear();
    m_sites.reserve(list.size());
    for (const Site &s : list)
        m_sites.push_back(normalized(s));
    if (!save())
        return false;
    emit sitesChanged();
    return true;
}

void SiteRegistry::resetToDefaults()
{
    setSites(siteList());
}
