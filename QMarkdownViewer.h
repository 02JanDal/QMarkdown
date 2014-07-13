#pragma once

#include <QTextEdit>

class QMarkdownViewer : public QTextEdit
{
	Q_OBJECT
public:
	QMarkdownViewer(QWidget *parent = 0);

	void setMarkdown(const QString &flavour, const QByteArray &data);
	QByteArray getMarkdown(const QString &flavour);
};
