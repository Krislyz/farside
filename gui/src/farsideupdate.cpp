// See farsideupdate.h.
#include <farsideupdate.h>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QVersionNumber>

namespace Farside {

// One release as GitHub describes it: its Setup program called asset, or an empty Update when the
// release is a draft, is no version, or its file is not listed with a hash and a download address here.
static Update readRelease(const QJsonObject &release, const QString &asset, bool *prerelease)
{
	if(release.value(QStringLiteral("draft")).toBool())
		return {};
	*prerelease = release.value(QStringLiteral("prerelease")).toBool();
	QString tag = release.value(QStringLiteral("tag_name")).toString();
	if(tag.startsWith(QLatin1Char('v')))
		tag.remove(0, 1);
	qsizetype read = 0;
	const QVersionNumber found = QVersionNumber::fromString(tag, &read);
	if(found.isNull() || read != tag.size())
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

Update pickUpdate(const QByteArray &json, const QString &current, const QString &asset, bool alpha)
{
	const QJsonDocument answer = QJsonDocument::fromJson(json);
	const QJsonArray releases = answer.isArray() ? answer.array() : QJsonArray { answer.object() };
	// The newest one that counts on this channel; GitHub's order is by date, not by version.
	Update best;
	QVersionNumber newest = QVersionNumber::fromString(current);
	for(const QJsonValue &entry : releases)
	{
		bool prerelease = false;
		const Update u = readRelease(entry.toObject(), asset, &prerelease);
		if(u.version.isEmpty() || (prerelease && !alpha))
			continue;
		const QVersionNumber version = QVersionNumber::fromString(u.version);
		if(QVersionNumber::compare(version, newest) > 0)
		{
			newest = version;
			best = u;
		}
	}
	return best;
}

bool isAlphaRelease(const QByteArray &json, const QString &current)
{
	const QJsonDocument answer = QJsonDocument::fromJson(json);
	const QJsonArray releases = answer.isArray() ? answer.array() : QJsonArray { answer.object() };
	const QVersionNumber own = QVersionNumber::fromString(current);
	for(const QJsonValue &entry : releases)
	{
		const QJsonObject release = entry.toObject();
		QString tag = release.value(QStringLiteral("tag_name")).toString();
		if(tag.startsWith(QLatin1Char('v')))
			tag.remove(0, 1);
		if(tag.isEmpty() || release.value(QStringLiteral("draft")).toBool() || QVersionNumber::fromString(tag) != own)
			continue;
		return release.value(QStringLiteral("prerelease")).toBool();
	}
	return false;
}

bool askChannel(bool running_alpha, const QString &channel, const QString &asked, const QString &current)
{
	return running_alpha && channel != QLatin1String("alpha") && asked != current;
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

	// The list of releases, as the channels see it: 0.2.0 full, 0.3.0 pre-release, 0.4.0 draft.
	auto listed = [&](const QString &tag, bool pre, bool draft) {
		QJsonObject r = QJsonDocument::fromJson(release(tag, digest, good_url, pre)).object();
		r.insert(QStringLiteral("draft"), draft);
		return r;
	};
	const QByteArray list = QJsonDocument(QJsonArray { listed(QStringLiteral("v0.4.0"), false, true), listed(QStringLiteral("v0.3.0"), true, false),
		listed(QStringLiteral("v0.2.0"), false, false), listed(QStringLiteral("v0.1.0"), false, false) }).toJson();
	// 58: the stable channel takes the newest full release and passes over the pre-release.
	if(pickUpdate(list, QStringLiteral("0.1.0"), name).version != QLatin1String("0.2.0"))
		return 58;
	// 59: the alpha channel takes the pre-release when it is the newest.
	if(pickUpdate(list, QStringLiteral("0.1.0"), name, true).version != QLatin1String("0.3.0"))
		return 59;
	// 60: a draft is never offered, on either channel.
	if(pickUpdate(list, QStringLiteral("0.3.0"), name, true).version != QString() || pickUpdate(list, QStringLiteral("0.2.0"), name).version != QString())
		return 60;
	// 61: on alpha, a full release newer than every pre-release is taken too (the order in the list does not matter).
	const QByteArray list2 = QJsonDocument(QJsonArray { listed(QStringLiteral("v0.2.0"), true, false), listed(QStringLiteral("v0.5.0"), false, false) }).toJson();
	if(pickUpdate(list2, QStringLiteral("0.1.0"), name, true).version != QLatin1String("0.5.0"))
		return 61;
	// 62: versions of four numbers: 0.1.5.1 is newer than 0.1.5 and older than 0.1.6.
	if(pickUpdate(release(QStringLiteral("v0.1.5.1"), digest, good_url), QStringLiteral("0.1.5"), name).version != QLatin1String("0.1.5.1")
		|| !pickUpdate(release(QStringLiteral("v0.1.5.1"), digest, good_url), QStringLiteral("0.1.6"), name).version.isEmpty()
		|| pickUpdate(release(QStringLiteral("v0.1.6"), digest, good_url), QStringLiteral("0.1.5.1"), name).version != QLatin1String("0.1.6"))
		return 62;
	// 63: a single pre-release (not a list) is taken on alpha only.
	if(pickUpdate(release(QStringLiteral("v0.2.0"), digest, good_url, true), QStringLiteral("0.1.0"), name, true).version != QLatin1String("0.2.0")
		|| !pickUpdate(release(QStringLiteral("v0.2.0"), digest, good_url, true), QStringLiteral("0.1.0"), name).version.isEmpty())
		return 63;

	// 64: the release of this program's own version is marked pre-release: it is an alpha version.
	if(!isAlphaRelease(list, QStringLiteral("0.3.0")) || !isAlphaRelease(release(QStringLiteral("v0.3.0"), digest, good_url, true), QStringLiteral("0.3.0")))
		return 64;
	// 65: a full release, a draft, a version that is not listed and an unreadable answer are no alpha version.
	const QByteArray draft = QJsonDocument(QJsonArray { listed(QStringLiteral("v0.6.0"), true, true) }).toJson();
	if(isAlphaRelease(list, QStringLiteral("0.2.0")) || isAlphaRelease(draft, QStringLiteral("0.6.0")) || isAlphaRelease(list, QStringLiteral("0.9.0"))
		|| isAlphaRelease("not json", QStringLiteral("0.3.0")) || isAlphaRelease("{\"message\":\"Not Found\"}", QStringLiteral("0.3.0")))
		return 65;
	// 66: an alpha version on the stable channel was installed by hand: ask. On the alpha channel, or
	// with a full release, there is nothing to ask.
	if(!askChannel(true, QStringLiteral("stable"), QString(), QStringLiteral("0.3.0"))
		|| askChannel(true, QStringLiteral("alpha"), QString(), QStringLiteral("0.3.0"))
		|| askChannel(false, QStringLiteral("stable"), QString(), QStringLiteral("0.3.0")))
		return 66;
	// 67: asked once per version: not again for the version already answered, again for a later alpha.
	if(askChannel(true, QStringLiteral("stable"), QStringLiteral("0.3.0"), QStringLiteral("0.3.0"))
		|| !askChannel(true, QStringLiteral("stable"), QStringLiteral("0.2.9"), QStringLiteral("0.3.0")))
		return 67;
	return 0;
}

} // namespace Farside
