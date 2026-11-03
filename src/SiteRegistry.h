#pragma once

#include "SiteData.h"

#include <QObject>
#include <QString>
#include <QVector>

// ----------------------------------------------------------------------------
// The administrable watched-site roster. Seeded from the built-in defaults in
// SiteData.h, then overridden by a JSON file in the user's config directory
// (~/.config/<appname>/ on Linux, %APPDATA% on Windows, ~/Library/Application
// Support on macOS). The admin dialog commits edits through setSites(),
// which normalizes, validates, persists, and announces the new list.
// ----------------------------------------------------------------------------
class SiteRegistry : public QObject
{
    Q_OBJECT
public:
    explicit SiteRegistry(QObject *parent = nullptr);

    enum class LoadResult { Defaults, File, Corrupt };
    LoadResult load();                                  // apply saved overrides at startup

    bool setSites(const QVector<Site> &list);           // validate + save + emit
    void resetToDefaults();                             // restore built-in roster (persists)

    const QVector<Site> &sites() const { return m_sites; }

    static QString configPath();
    static Site normalized(const Site &s);              // trim (+ upper-case identity fields)
    static QString validateList(const QVector<Site> &list);   // "" when ok

signals:
    void sitesChanged();

private:
    bool save() const;

    QVector<Site> m_sites;   // starts as siteList() defaults
};
