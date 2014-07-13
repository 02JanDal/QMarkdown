#pragma once

#include <QTextDocument>

class QAbstractMarkdown
{
public:
	virtual ~QAbstractMarkdown() {}
	virtual void read(const QByteArray &markdown, QTextDocument *target) = 0;
	virtual QByteArray write(QTextDocument *source) = 0;

	static QStringList flavours();
	static QAbstractMarkdown *flavour(const QString &id);

protected:
	QAbstractMarkdown() {}
};
