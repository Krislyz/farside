// See farsideqr.h.
#include <farsideqr.h>
#include <farsidelink.h>

#include <QBuffer>
#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>

#include <ZXing/BitMatrix.h>
#include <ZXing/MultiFormatWriter.h>
#include <ZXing/ReadBarcode.h>

#include <exception>

namespace Farside {

// The Farside mark (brand/farside-mark.svg) on its dark rounded tile, filling `tile`.
static void drawLogo(QPainter &p, const QRectF &tile)
{
	p.setPen(Qt::NoPen);
	p.setBrush(QColor(0x10, 0x12, 0x1b));
	p.drawRoundedRect(tile, tile.width() * 0.22, tile.width() * 0.22);
	// The mark's own box is 860 x 860 with its corner at (238, 197).
	const double pad = tile.width() * 0.12, scale = (tile.width() - 2 * pad) / 860.0;
	p.save();
	p.translate(tile.left() + pad, tile.top() + pad);
	p.scale(scale, scale);
	p.translate(-238, -197);
	auto shape = [&p](std::initializer_list<QPointF> points, const QBrush &fill) {
		p.setBrush(fill);
		p.drawPolygon(QPolygonF(QList<QPointF>(points)));
	};
	QLinearGradient fold(505, 500, 893, 538);
	fold.setColorAt(0, QColor(0xff, 0x3e, 0x91));
	fold.setColorAt(1, QColor(0x9e, 0x1f, 0x5b));
	shape({ { 272, 760 }, { 517, 405 }, { 497, 597 } }, QColor(0xff, 0x53, 0xa9));
	shape({ { 517, 405 }, { 1063, 290 }, { 893, 538 } }, QColor(0xfd, 0x4f, 0xa4));
	shape({ { 517, 405 }, { 893, 538 }, { 497, 597 } }, fold);
	shape({ { 320, 963 }, { 552, 623 }, { 872, 604 }, { 802, 714 }, { 623, 719 } }, QColor(0xe9, 0xee, 0xf6));
	shape({ { 623, 719 }, { 802, 714 }, { 758, 782 }, { 624, 820 } }, QColor(0x3f, 0x54, 0x6f));
	shape({ { 623, 719 }, { 624, 820 }, { 320, 963 } }, QColor(0x4c, 0x65, 0x81));
	p.restore();
}

QImage qrDraw(const QString &code, int pixels)
{
	ZXing::BitMatrix squares;
	try
	{
// The newer CreateBarcode() needs the zint library; this writer is part of zxing-cpp itself.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
		// Level 8 = the highest error correction (30 %): the logo covers squares, and screenshots get blurred.
		squares = ZXing::MultiFormatWriter(ZXing::BarcodeFormat::QRCode).setEccLevel(8).setMargin(0).encode(code.toUpper().toStdString(), 0, 0);
#pragma GCC diagnostic pop
	}
	catch(const std::exception &)
	{
		return QImage();
	}
	// Four squares of white all round, as the QR standard asks.
	const int count = squares.width(), cell = pixels / (count + 8);
	if(count <= 0 || cell < 1)
		return QImage();
	const int origin = (pixels - cell * count) / 2;
	QImage picture(pixels, pixels, QImage::Format_RGB32);
	picture.fill(Qt::white);
	QPainter p(&picture);
	for(int y = 0; y < count; y++)
		for(int x = 0; x < count; x++)
			if(squares.get(x, y))
				p.fillRect(origin + x * cell, origin + y * cell, cell, cell, QColor(0x0b, 0x0b, 0x10));
	// The logo sits on whole squares: its width in squares is odd when the code's is, so it is centred.
	int logo = qRound(count * 0.22);
	if((logo & 1) != (count & 1))
		logo++;
	p.setRenderHint(QPainter::Antialiasing);
	const double at = origin + (count - logo) / 2 * cell;
	drawLogo(p, QRectF(at, at, logo * cell, logo * cell));
	return picture;
}

static QString readGray(const QImage &gray)
{
	ZXing::ReaderOptions options;
	options.setFormats(ZXing::BarcodeFormat::QRCode);
	options.setTryHarder(true);
	options.setTryDownscale(true);
	QString ticket;
	QByteArray secret;
	try
	{
		for(const ZXing::Barcode &found : ZXing::ReadBarcodes(ZXing::ImageView(gray.constBits(), gray.width(), gray.height(), ZXing::ImageFormat::Lum, int(gray.bytesPerLine())), options))
		{
			const QString text = QString::fromStdString(found.text()).trimmed().toLower();
			if(found.isValid() && parseCode(text, &ticket, &secret))
				return text;
		}
	}
	catch(const std::exception &)
	{
	}
	return QString();
}

QString qrRead(const QImage &picture)
{
	if(picture.isNull())
		return QString();
	// See-through parts count as white, as they look on most backgrounds.
	QImage flat(picture.size(), QImage::Format_RGB32);
	flat.fill(Qt::white);
	{
		QPainter p(&flat);
		p.drawImage(0, 0, picture);
	}
	const QImage gray = flat.convertToFormat(QImage::Format_Grayscale8);
	QString code = readGray(gray);
	// A small or blurred code (a screenshot of a video call) often reads once it is enlarged.
	// ponytail: enlarging is skipped above 3 megapixels to keep a whole-desktop scan quick.
	const bool small = qint64(gray.width()) * gray.height() <= 3000000;
	for(int factor = 2; code.isEmpty() && small && factor <= 3; factor++)
		code = readGray(gray.scaled(gray.width() * factor, gray.height() * factor, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
	return code;
}

int selfTestQr()
{
	// A code as long as a real one: the ticket dumbpipe prints is 122 characters.
	const QString code = makeCode(QStringLiteral("endpointabc234").repeated(9).left(122), QByteArray::fromHex("000102030405060708090a0b0c0d0e0f"));

	const QImage drawn = qrDraw(code, 480);
	if(drawn.isNull() || drawn.width() != 480 || drawn.height() != 480 || qrRead(drawn) != code) return 201;

	// Through a screen share: half the size, then JPEG at low quality.
	QByteArray jpeg;
	{
		QBuffer out(&jpeg);
		out.open(QIODevice::WriteOnly);
		if(!drawn.scaled(240, 240, Qt::IgnoreAspectRatio, Qt::SmoothTransformation).save(&out, "JPG", 20)) return 202;
	}
	if(qrRead(QImage::fromData(jpeg, "JPG")) != code) return 202;

	// Somewhere on a whole desktop.
	QImage desktop(1920, 1080, QImage::Format_RGB32);
	{
		QPainter p(&desktop);
		QLinearGradient sky(0, 0, 1920, 1080);
		sky.setColorAt(0, QColor(0x0b, 0x0b, 0x10));
		sky.setColorAt(1, QColor(0x3a, 0x6e, 0xa5));
		p.fillRect(desktop.rect(), sky);
		p.fillRect(100, 80, 700, 500, Qt::white);
		p.drawImage(1280, 560, qrDraw(code, 300));
	}
	if(qrRead(desktop) != code) return 203;

	QImage plain(640, 480, QImage::Format_RGB32);
	plain.fill(QColor(0x14, 0x14, 0x1d));
	if(!qrRead(plain).isEmpty() || !qrRead(QImage()).isEmpty()) return 204;

	// A QR code that is not a Farside code is nothing to pair with.
	const QImage other = qrDraw(QStringLiteral("https://example.com/not-a-farside-code"), 300);
	if(other.isNull() || !qrRead(other).isEmpty()) return 205;
	return 0;
}

} // namespace Farside
