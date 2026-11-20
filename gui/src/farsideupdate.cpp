// See farsideupdate.h.
#include <farsideupdate.h>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QVersionNumber>

namespace Farside {

Update pickUpdate(const QByteArray &json, const QString &current, const QString &asset)
{
	const QJsonObject release = QJsonDocument::fromJson(json).object();
	if(release.value(QStringLiteral("draft")).toBool() || release.value(QStringLiteral("prerelease")).toBool())
		return {};
	QString tag = release.value(QStringLiteral("tag_name")).toString();
	if(tag.startsWith(QLatin1Char('v')))
		tag.remove(0, 1);
	qsizetype read = 0;
	const QVersionNumber found = QVersionNumber::fromString(tag, &read);
	if(found.isNull() || read != tag.size() || QVersionNumber::compare(found, QVersionNumber::fromString(current)) <= 0)
		return {};
	const QString releases = QStringLiteral("https://github.com/%1/releases/download/").arg(QLatin1String(update_repo));
	for(const QJsonValue &entry : release.value(QStringLiteral("assets")).toArray())
	{
		const QJsonObject file = entry.toObject();
		if(file.value(QStringLiteral("name")).toString() != asset)
			continue;
		// "sha256:" and 64 hex digits.
		const QString digest = file.value(QStringLiteral("digest")).toString();
		Update u { found.toString(), file.value(QStringLiteral("browser_download_url")).toString(),
			QByteArray::fromHex(digest.mid(7).toLatin1()), file.value(QStringLiteral("size")).toInteger() };
		if(!digest.startsWith(QLatin1String("sha256:")) || digest.size() != 71 || u.sha256.size() != 32 || u.size <= 0 || !u.url.startsWith(releases))
			return {};
		return u;
	}
	return {};
}

int selfTestUpdate()
{
	const QString hex = QStringLiteral("9c73c3bae7ed48d44112a0f48e66742c00090bdb5bef71d9d3c056c66e97b732");
	const QString good_url = QStringLiteral("https://github.com/%1/releases/download/v0.2.0/FarsideClient-Setup.exe").arg(QLatin1String(update_repo));
	// A release as GitHub describes it, with one thing changed per check.
	auto release = [&](const QString &tag, const QString &digest, const QString &url, bool pre = false) {
		const QJsonObject file { { QStringLiteral("name"), QStringLiteral("FarsideClient-Setup.exe") }, { QStringLiteral("size"), 1234 },
			{ QStringLiteral("digest"), digest }, { QStringLiteral("browser_download_url"), url } };
		const QJsonObject other { { QStringLiteral("name"), QStringLiteral("FarsideHost-Setup.exe") }, { QStringLiteral("size"), 99 },
			{ QStringLiteral("digest"), QStringLiteral("sha256:") + QString(64, QLatin1Char('0')) }, { QStringLiteral("browser_download_url"), good_url } };
		return QJsonDocument(QJsonObject { { QStringLiteral("tag_name"), tag }, { QStringLiteral("draft"), false }, { QStringLiteral("prerelease"), pre },
			{ QStringLiteral("assets"), QJsonArray { other, file } } }).toJson();
	};
	const QString name = QStringLiteral("FarsideClient-Setup.exe"), digest = QStringLiteral("sha256:") + hex;

	// 50: a newer release gives this edition's Setup program, with the hash and size GitHub lists.
	const Update u = pickUpdate(release(QStringLiteral("v0.2.0"), digest, good_url), QStringLiteral("0.1.0"), name);
	if(u.version != QLatin1String("0.2.0") || u.url != good_url || u.sha256.toHex() != hex.toLatin1() || u.size != 1234)
		return 50;
	// 51, 52: the same or an older release is no update.
	if(!pickUpdate(release(QStringLiteral("v0.2.0"), digest, good_url), QStringLiteral("0.2.0"), name).version.isEmpty())
		return 51;
	if(!pickUpdate(release(QStringLiteral("v0.2.0"), digest, good_url), QStringLiteral("0.3.0"), name).version.isEmpty())
		return 52;
	// 53: without a hash to check the download against, there is no update.
	if(!pickUpdate(release(QStringLiteral("v0.2.0"), QString(), good_url), QStringLiteral("0.1.0"), name).version.isEmpty()
		|| !pickUpdate(release(QStringLiteral("v0.2.0"), QStringLiteral("sha256:abcd"), good_url), QStringLiteral("0.1.0"), name).version.isEmpty())
		return 53;
	// 54: a download address outside the releases of update_repo is refused.
	if(!pickUpdate(release(QStringLiteral("v0.2.0"), digest, QStringLiteral("https://example.com/FarsideClient-Setup.exe")), QStringLiteral("0.1.0"), name).version.isEmpty()
		|| !pickUpdate(release(QStringLiteral("v0.2.0"), digest, QStringLiteral("http://github.com/%1/releases/download/v0.2.0/x.exe").arg(QLatin1String(update_repo))), QStringLiteral("0.1.0"), name).version.isEmpty())
		return 54;
	// 55: a pre-release is not offered.
	if(!pickUpdate(release(QStringLiteral("v0.2.0"), digest, good_url, true), QStringLiteral("0.1.0"), name).version.isEmpty())
		return 55;
	// 56: an answer that is not a release (an error text, a tag that is no version) is no update.
	if(!pickUpdate("{\"message\":\"Not Found\"}", QStringLiteral("0.1.0"), name).version.isEmpty()
		|| !pickUpdate("not json", QStringLiteral("0.1.0"), name).version.isEmpty()
		|| !pickUpdate(release(QStringLiteral("nightly"), digest, good_url), QStringLiteral("0.1.0"), name).version.isEmpty())
		return 56;
	// 57: versions compare as numbers (0.10.0 is newer than 0.9.0), with or without the "v".
	if(pickUpdate(release(QStringLiteral("0.10.0"), digest, good_url), QStringLiteral("0.9.0"), name).version != QLatin1String("0.10.0"))
		return 57;
	return 0;
}

} // namespace Farside
