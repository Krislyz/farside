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

// Reads GitHub's answer to "latest release" (api.github.com/repos/<update_repo>/releases/latest).
// Gives the Setup program called asset when the release is newer than current, is a full release, and
// GitHub lists the file's SHA-256 and a download address inside update_repo. Anything else: an empty Update.
Update pickUpdate(const QByteArray &json, const QString &current, const QString &asset);

// Checks 50 to 57 of the self-test.
int selfTestUpdate();

} // namespace Farside
