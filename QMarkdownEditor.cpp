#include "QMarkdownEditor.h"

#include <QToolBar>
#include <QAction>
#include <QVBoxLayout>

#include "QMarkdownViewer.h"

template<typename Slot>
QAction *createAction(QToolBar *bar, const QIcon &icon, const QString &tooltip, QObject *receiver, Slot slot)
{
	QAction *action = bar->addAction(icon, tooltip);
	action->setToolTip(tooltip);
	QObject::connect(action, &QAction::triggered, receiver, slot);
	return action;
}
template<typename Lambda>
QAction *createAction(QToolBar *bar, const QIcon &icon, const QString &tooltip, Lambda lambda)
{
	QAction *action = bar->addAction(icon, tooltip);
	action->setToolTip(tooltip);
	QObject::connect(action, &QAction::triggered, lambda);
	return action;
}

QMarkdownEditor::QMarkdownEditor(QWidget *parent)
	: QWidget(parent), m_viewer(new QMarkdownViewer(this)), m_toolBar(new QToolBar(this))
{
	QVBoxLayout *layout = new QVBoxLayout(this);
	setLayout(layout);
	layout->addWidget(m_toolBar);
	layout->addWidget(m_viewer);

	QIcon::setThemeSearchPaths(QIcon::themeSearchPaths() << "/usr/share/icons");
	QIcon::setThemeName("oxygen");

	createAction(m_toolBar, QIcon::fromTheme("format-text-bold"), tr("Bold"), [](){});
	createAction(m_toolBar, QIcon::fromTheme("format-text-italic"), tr("Italic"), [](){});
	createAction(m_toolBar, QIcon::fromTheme("format-font-size-more"), tr("Bigger"), [](){});
	createAction(m_toolBar, QIcon::fromTheme("format-font-size-less"), tr("Smaller"), [](){});
	m_toolBar->addSeparator();
	createAction(m_toolBar, QIcon::fromTheme("format-list-ordered"), tr("Ordered list"), [](){});
	createAction(m_toolBar, QIcon::fromTheme("format-list-unordered"), tr("Unordered list"), [](){});
	createAction(m_toolBar, QIcon::fromTheme("format-indent-less"), tr("Increase indent"), [](){});
	createAction(m_toolBar, QIcon::fromTheme("format-indent-more"), tr("Decrease indent"), [](){});
}

void QMarkdownEditor::setMarkdown(const QString &flavour, const QByteArray &data)
{
	m_viewer->setMarkdown(flavour, data);
}
QByteArray QMarkdownEditor::getMarkdown(const QString &flavour)
{
	return m_viewer->getMarkdown(flavour);
}
