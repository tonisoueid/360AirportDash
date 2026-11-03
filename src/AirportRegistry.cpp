#include "AirportRegistry.h"
#include "AirportSeed.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>
#include <cmath>

AirportRegistry::AirportRegistry(QObject *parent)
    : QObject(parent), m_airports(airportSeed())
{
}

QString AirportRegistry::configPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)
           + QStringLiteral("/airports.json");
}

bool AirportRegistry::contains(const QString &icaoUpper) const
{
    for (const Airport &a : m_airports)
        if (a.icao.compare(icaoUpper, Qt::CaseInsensitive) == 0)
            return true;
    return false;
}

AirportRegistry::LoadResult AirportRegistry::load()
{
    m_airports = airportSeed();
    m_needsRefill = false;

    QFile f(configPath());
    if (!f.exists())
        return LoadResult::Seed;

    auto corrupt = [this]() {
        m_airports = airportSeed();
        m_needsRefill = true;
        return LoadResult::Corrupt;
    };
    if (!f.open(QIODevice::ReadOnly))
        return corrupt();

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
        return corrupt();
    const QJsonValue av = doc.object().value("airports");
    if (!av.isArray() || av.toArray().isEmpty())
        return corrupt();

    QVector<Airport> parsed;
    QSet<QString> seen;
    for (const QJsonValue &v : av.toArray()) {
        if (!v.isObject())
            return corrupt();
        const QJsonObject o = v.toObject();
        Airport a;
        a.icao = o.value("icao").toString().trimmed().toUpper();
        if (a.icao.isEmpty() || seen.contains(a.icao))
            return corrupt();
        seen.insert(a.icao);
        a.iata = o.value("iata").toString();
        a.name = o.value("name").toString();
        a.city = o.value("city").toString();
        a.country = o.value("country").toString();
        a.timeZone = o.value("timeZone").toString();
        // null / absent stays NaN: unknown is never stored as 0.
        if (o.value("lat").isDouble()) a.lat = o.value("lat").toDouble();
        if (o.value("lon").isDouble()) a.lon = o.value("lon").toDouble();
        a.seeded = o.value("seeded").toBool(false);
        parsed.push_back(a);
    }
    m_airports = parsed;
    return LoadResult::File;
}

bool AirportRegistry::save()
{
    QJsonArray arr;
    for (const Airport &a : m_airports) {
        QJsonObject o;
        o.insert("icao", a.icao);
        o.insert("iata", a.iata);
        o.insert("name", a.name);
        o.insert("city", a.city);
        o.insert("country", a.country);
        o.insert("timeZone", a.timeZone);
        o.insert("lat", std::isnan(a.lat) ? QJsonValue(QJsonValue::Null) : QJsonValue(a.lat));
        o.insert("lon", std::isnan(a.lon) ? QJsonValue(QJsonValue::Null) : QJsonValue(a.lon));
        o.insert("seeded", a.seeded);
        arr.append(o);
    }
    QJsonObject root;
    root.insert("version", 1);
    root.insert("airports", arr);

    const QString path = configPath();
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        return false;
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    if (!f.commit())
        return false;
    return true;
}

bool AirportRegistry::addLearned(const QString &icao)
{
    const QString key = icao.trimmed().toUpper();
    if (key.isEmpty() || contains(key))
        return false;
    Airport a;
    a.icao = key;   // label = ICAO; everything else stays unknown
    m_airports.push_back(a);
    save();
    emit airportsChanged();
    return true;
}
