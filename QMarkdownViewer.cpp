#include "QMarkdownViewer.h"

#include "QMarkdown.h"

QMarkdownViewer::QMarkdownViewer(QWidget *parent)
	: QTextEdit(parent)
{
}

void QMarkdownViewer::setMarkdown(const QString &flavour, const QByteArray &data)
{
	QAbstractMarkdown::flavour(flavour)->read(data, document());
}
QByteArray QMarkdownViewer::getMarkdown(const QString &flavour)
{
	return QAbstractMarkdown::flavour(flavour)->write(document());
}
