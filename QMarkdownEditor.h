#pragma once

#include <QWidget>

class QMarkdownViewer;
class QToolBar;
class QAction;

class QMarkdownEditor : public QWidget
{
	Q_OBJECT
public:
	QMarkdownEditor(QWidget *parent = 0);

	void setMarkdown(const QString &flavour, const QByteArray &data);
	QByteArray getMarkdown(const QString &flavour);

private:
	QMarkdownViewer *m_viewer;
	QToolBar *m_toolBar;
};
