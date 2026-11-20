// The Farside pairing code as a QR picture, and back: drawn on the Host, read on the Client from a
// pasted screenshot or from the screen. The picture carries the code in capitals (QR codes store
// capitals and digits in fewer squares, so the squares come out bigger); parseCode() takes them back.
#pragma once

#include <QImage>
#include <QString>

namespace Farside {

// `pixels` wide and high: dark squares on white, the Farside mark on its dark tile in the middle.
// A null image when the text does not fit a QR code or `pixels` is too small for it.
QImage qrDraw(const QString &code, int pixels);
// The Farside pairing code found in a picture (a QR picture, a screenshot around one, a whole
// desktop), in the form parseCode() accepts. Empty when there is none.
QString qrRead(const QImage &picture);

// Checks 201 to 205 of the self-test (0 = pass). Needs a QCoreApplication (for the JPEG plugin).
int selfTestQr();

} // namespace Farside
