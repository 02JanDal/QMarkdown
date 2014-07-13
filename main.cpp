#include <QApplication>
#include <QDebug>
#include <QTextDocument>
#include <QTextCursor>

#include "QMarkdownEditor.h"

int main(int argc, char **argv)
{
	QApplication app(argc, argv);

	QMarkdownEditor viewer;
	viewer.resize(640, 480);
	viewer.show();

	QTextDocument doc;
	QTextCursor cursor(&doc);
	cursor.insertList(QTextListFormat::ListDisc);
	cursor.insertText("asdf");
	cursor.insertBlock();
	cursor.insertText("ffff");
	cursor.insertBlock();
	cursor.insertText("fd");
	//qDebug() << doc.toHtml(); return 0;

	viewer.setMarkdown("github",
					   /*"# This is a markdown document\n"
					   "\n"
					   "It can contain **bold**, _italic_ and **_bold/italic_** text, as well as `inline code`.\n"
					   "\n"
					   "```\n"
					   "oh, and entire code sections\n"
					   "    that honor intendention\n"
					   "```\n"
					   "\n"
					   "\n"
					   "\n"
					   "* Unordered\n"
					   "* lists\n"
					   "\n"
					   "1. Ordered\n"
					   "2. lists\n"
					   "\n"
					   "> This is a quote\n"
					   "\n"
					   "![Image](http://placekitten.com/100/50)\n"
					   "\n"
					   "## And nested lists\n"*/
					   "\n"
					   "1. Item 1\n"
					   "  1. Subitem 1\n"
					   "  2. Subitem 2\n"
					   "2. Item 2\n"
					   "  * Unordered [subitem](http://example.com/) 1\n"
					   "  * Unordered subitem 2\n"
					   "    1. Stuff here\n"
					   "    2. foo\n"
					   "      1. bar\n"
					   "3. Item 3\n"
					   "   has multiple lines\n"
					   );
	//qDebug() << viewer.toHtml().remove("margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;").remove("margin-top: 0px; margin-bottom: 0px; margin-left: 0px; margin-right: 0px; ").remove(" style=\" \"");
	//qDebug() << viewer.getMarkdown("github");

	return app.exec();
}
