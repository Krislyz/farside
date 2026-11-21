// Updates: which installer of a newer release to fetch. Fetching and starting it is in farsidebackend.cpp.
#pragma once

#include <QByteArray>
#include <QString>

namespace Farside {

// The GitHub repository whose releases are the updates: tag v<version>, one Setup program per edition.
inline constexpr char update_repo[] = "Krislyz/farside";

struct Update
{
	QString version, url; // empty version: nothing newer
	QByteArray sha256;    // of the Setup program, as GitHub lists it
	qint64 size = 0;
};

// Reads GitHub's list of releases (api.github.com/repos/<update_repo>/releases), or one release.
// Gives the Setup program called asset of the newest release that is newer than current, for which
// GitHub lists the file's SHA-256 and a download address inside update_repo. Anything else: an empty Update.
// Channels: "stable" takes full releases only; with alpha set, releases marked "pre-release" on GitHub
// count too. Drafts never count. Versions are numbers separated by dots, of any length (0.1.5.1 is
// newer than 0.1.5 and older than 0.1.6); a tag with anything else in it is not a version.
Update pickUpdate(const QByteArray &json, const QString &current, const QString &asset, bool alpha = false);

// Checks 50 to 63 of the self-test.
int selfTestUpdate();

} // namespace Farside
