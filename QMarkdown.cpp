#include "QMarkdown.h"

#include <QRegularExpressionMatch>
#include <QTextCursor>
#include <QTextList>
#include <QDebug>

// code borrowed from qiterator.h
class QStringIterator
{
	typedef typename QString::const_iterator const_iterator;
	QString c;
	const_iterator i;
public:
	inline QStringIterator(const QString &container)
		: c(container), i(c.constBegin()) {} \
	inline QStringIterator &operator=(const QString &container)
	{ c = container; i = c.constBegin(); return *this; }
	inline void toFront() { i = c.constBegin(); }
	inline void toBack() { i = c.constEnd(); }
	inline bool hasNext() const { return i != c.constEnd(); }
	inline const QChar next() { return *i++; }
	inline const QChar peekNext() const { return *i; }
	inline bool hasPrevious() const { return i != c.constBegin(); }
	inline const QChar previous() { return *--i; }
	inline const QChar peekPrevious() const { const_iterator p = i; return *--p; }
	inline bool findNext(const QChar &t)
	{ while (i != c.constEnd()) if (*i++ == t) return true; return false; }
	inline bool findPrevious(const QChar &t)
	{ while (i != c.constBegin()) if (*(--i) == t) return true;
		return false;  }
};

class QGithubMarkdown : public QAbstractMarkdown
{
public:
	QGithubMarkdown()
	{
		if (sizeMap.isEmpty())
		{
			sizeMap[1] = 26;
			sizeMap[2] = 24;
			sizeMap[3] = 20;
			sizeMap[4] = 16;
			sizeMap[5] = 14;
			sizeMap[6] = 13;
		}
	}

	void read(const QByteArray &markdown, QTextDocument *target) override;
	QByteArray write(QTextDocument *source) override;

	struct Token
	{
		enum Type
		{
			Character,
			NewLine,

			HeadingStart,
			QuoteStart,
			CodeDelimiter,
			InlineCodeDelimiter,
			Bold,
			Italic,

			ImageStart,
			LinkStart,
			LinkMiddle,
			LinkEnd,

			UnorderedListStart,
			OrderedListStart,

			HtmlTagOpen,
			HtmlTagClose,

			Invalid,

			EOD // EndOfDocument
		} type = Invalid;
		QVariant content;
		QString source;

		Token() {}
		explicit Token(const Type type, const QVariant &content)
			: type(type), content(content) {}

		// constructor required for comparing without explicitly creating a Token
		Token(const Type type) : type(type) {}
		bool operator==(const Token &other)
		{
			if (type == other.type && type == Character)
			{
				return content == other.content;
			}
			else
			{
				return type == other.type;
			}
		}
	};
	struct Paragraph
	{
		enum Type
		{
			// numbers have to match QGithubMarkdown::sizeMap
			Heading1 = 1,
			Heading2 = 2,
			Heading3 = 3,
			Heading4 = 4,
			Heading5 = 5,
			Heading6 = 6,

			Normal,
			Quote,
			Code,

			UnorderedList,
			OrderedList,

			FirstHeading = Heading1,
			LastHeading = Heading6
		} type = Normal;
		QList<Token> tokens;
		QList<Token> indentTokens;
	};
	struct List
	{
		QList<Paragraph> paragraphs;
		int indent = -1;
		bool ordered = true;
	};

private:
	/// Parses the markdown into tokens
	QList<Token> tokenize(const QString &string);
	/// Parses the list of tokens into paragraphs
	QList<Paragraph> paragraphize(const QList<Token> &tokens);
	/// Parses the list of paragraphs into paragraphs and lists
	QList<QPair<Paragraph, List>> listize(const QList<Paragraph> &paragraphs);

	static QMap<int, int> sizeMap;
	QList<QString> codeSections;
	QList<QString> htmlSections;
	QTextCursor cursor;
	QTextDocument *doc;
	void readInlineText(const QString &data)
	{
		QString line = data;
		// remove all but one space on the front
		{
			bool startedWithSpace = line.startsWith(' ');
			line = line.remove(QRegularExpression("^ *"));
			if (startedWithSpace)
			{
				line = " " + line;
			}
		}
		// inline code
		{
			QRegularExpression exp("`(.*)`");
			QRegularExpressionMatch match = exp.match(line);
			while (match.hasMatch())
			{
				codeSections.append(match.captured(1));
				line.replace(match.capturedStart(), match.capturedLength(), QString("$${{%1}}$$").arg(codeSections.size() - 1));
				match = exp.match(line);
			}
		}
		// link and image
		{
			QRegularExpression exp("(\\!?)\\[(.*)\\]\\((.*)\\)");
			QRegularExpressionMatch match = exp.match(line);
			while (match.hasMatch())
			{
				if (match.captured(1) == "!")
				{
					// FIXME this no work
					htmlSections.append(QString("<img src=\"%1\" alt=\"%2\"/>").arg(match.captured(3), match.captured(2)));
				}
				else
				{
					htmlSections.append(QString("<a href=\"%1\">%2</a>").arg(match.captured(3), match.captured(2)));
				}
				line.replace(match.capturedStart(), match.capturedLength(), QString("$$[[%1]]$$").arg(htmlSections.size() - 1));
				match = exp.match(line);
			}
		}
		QTextCharFormat fmt;
		int numStarsOrUnderscores = 0;
		QChar last = 0;
		for (const QChar &c : line)
		{
			if (last != c)
			{
				// bold
				if (numStarsOrUnderscores == 2)
				{
					if (fmt.fontWeight() == QFont::Normal)
					{
						fmt.setFontWeight(QFont::Bold);
					}
					else
					{
						fmt.setFontWeight(QFont::Normal);
					}
				}
				// italic
				else if (numStarsOrUnderscores == 1)
				{
					fmt.setFontItalic(!fmt.fontItalic());
				}
				numStarsOrUnderscores = 0;
				cursor.setCharFormat(fmt);
			}
			last = c;
			if (c == '*' || c == '_')
			{
				numStarsOrUnderscores++;
			}
			else
			{
				cursor.insertText(c);
			}
		}
	}
	QTextList *tryReadList(QTextList *list, const QString &line)
	{
		QTextList *listOut = list;
		QRegularExpression exp("^( *)(\\d+\\.|\\*) (.*)$");
		QRegularExpressionMatch match = exp.match(line);
		if (match.hasMatch())
		{
			const int indent = match.captured(1).size() / 2 + 1;
			const QString contents = match.captured(3);
			const bool ordered = match.captured(2) != "*";
			QTextListFormat fmt;
			fmt.setStyle(ordered ? QTextListFormat::ListDecimal : QTextListFormat::ListDisc);
			fmt.setIndent(indent);
			if (!listOut || fmt != listOut->format())
			{
				listOut = cursor.insertList(fmt);
			}
			else
			{
				cursor.insertBlock();
			}
			readInlineText(contents);
			listOut->add(cursor.block());
			return listOut;
		}
		else
		{
			return 0;
		}
	}
	QString clean(const QString &in)
	{
		QString data = in;
		data = data.replace("\r\n", "\n").replace('\r', '\n');
		data = data.replace("\t", "    ");
		return data;
	}
	void finish()
	{
		for (int i = 0; i < codeSections.size(); ++i)
		{
			QTextCharFormat fmt;
			fmt.setFontFamily("Monospace");
			doc->find(QString("$${{%1}}$$").arg(i)).insertText(codeSections.at(i), fmt);
		}
		for (int i = 0; i < htmlSections.size(); ++i)
		{
			doc->find(QString("$$[[%1]]$$").arg(i)).insertHtml(htmlSections.at(i));
		}
	}
};
QMap<int, int> QGithubMarkdown::sizeMap;

QDebug operator<<(QDebug dbg, QGithubMarkdown::Token::Type type);
QDebug operator<<(QDebug dbg, QGithubMarkdown::Token token);
QDebug operator<<(QDebug dbg, QGithubMarkdown::Paragraph::Type type);
QDebug operator<<(QDebug dbg, QGithubMarkdown::Paragraph paragraph);

void QGithubMarkdown::read(const QByteArray &markdown, QTextDocument *target)
{
	doc = target;
	doc->clear();
	cursor = QTextCursor(doc);
	cursor.beginEditBlock();
	const QList<Token> tokens = tokenize(clean(QString::fromUtf8(markdown)));
	const QList<Paragraph> paragraphs = paragraphize(tokens);
	const auto paralists = listize(paragraphs);
	//std::for_each(paragraphs.begin(), paragraphs.end(), [](const Paragraph &item){qDebug() << item;});
	bool firstBlock = true;
	for (const auto paralist : paralists)
	{
		auto insertTokens = [&](const QList<Token> &tokens, const QTextCharFormat &format, const bool isCode)
		{
			QTextCharFormat fmt(format);
			QTextCharFormat codeFmt(format);
			codeFmt.setFontFamily("Monospace");
			QListIterator<Token> iterator(tokens);
			while (iterator.hasNext())
			{
				const Token token = iterator.next();
				if (isCode)
				{
					cursor.insertText(token.source);
				}
				else
				{
					if (token.type == Token::Bold)
					{
						if (fmt.fontWeight() == QFont::Bold)
						{
							fmt.setFontWeight(QFont::Normal);
						}
						else
						{
							fmt.setFontWeight(QFont::Bold);
						}
					}
					else if (token.type == Token::Italic)
					{
						fmt.setFontItalic(!fmt.fontItalic());
					}
					else if (token.type == Token::InlineCodeDelimiter)
					{
						while (iterator.hasNext())
						{
							const Token next = iterator.next();
							if (next.type == Token::InlineCodeDelimiter)
							{
								break;
							}
							else
							{
								cursor.insertText(token.source, codeFmt);
							}
						}
					}
					else if (token.type == Token::Character)
					{
						cursor.insertText(token.content.toChar(), fmt);
					}
					else
					{
						cursor.insertText(token.source, fmt);
					}
				}
			}
		};

		if (paralist.second.indent == -1)
		{
			const Paragraph paragraph = paralist.first;
			QTextCharFormat charFmt;
			QTextBlockFormat blockFmt;
			blockFmt.setBottomMargin(5.0f);
			if (Paragraph::FirstHeading <= paragraph.type && paragraph.type <= Paragraph::LastHeading)
			{
				charFmt.setFontPointSize(sizeMap[paragraph.type]);
			}
			else if (paragraph.type == Paragraph::Quote)
			{
				blockFmt.setIndent(1);
			}
			else if (paragraph.type == Paragraph::Code)
			{
				blockFmt.setNonBreakableLines(true);
				charFmt.setFontFamily("Monospace");
			}

			if (!firstBlock)
			{
				cursor.insertBlock();
			}
			else
			{
				firstBlock = false;
			}
			cursor.setBlockFormat(blockFmt);
			cursor.block().setUserState(paragraph.type);
			insertTokens(paragraph.tokens, charFmt, paragraph.type == Paragraph::Code);
		}
		else
		{
			const List list = paralist.second;
			qDebug() << "##########################" << list.indent << list.ordered;
			std::for_each(list.paragraphs.begin(), list.paragraphs.end(), [](const Paragraph &item){qDebug() << item;});
			cursor.setBlockFormat(QTextBlockFormat());
			cursor.setBlockCharFormat(QTextCharFormat());
			QTextListFormat listFormat;
			listFormat.setStyle(list.ordered ? QTextListFormat::ListDecimal : QTextListFormat::ListDisc);
			listFormat.setIndent(list.indent);
			QTextList *l = cursor.insertList(listFormat);
			qDebug() << "inserting list" << list.indent;
			bool firstBlock = true;
			for (const Paragraph &paragraph : list.paragraphs)
			{
				if (firstBlock)
				{
					firstBlock = false;
				}
				else
				{
					cursor.insertBlock();
					qDebug() << "inserting block";
				}
				insertTokens(paragraph.tokens, QTextCharFormat(), false);
				qDebug() << l->count();
				l->add(cursor.block());
				qDebug() << l->count();
				qDebug() << "inserting characters";
			}
		}
	}
	cursor.endEditBlock();
	qDebug() << doc->toHtml();
}
QByteArray QGithubMarkdown::write(QTextDocument *source)
{
	QStringList output;
	bool wasInList = false;
	bool inCodeBlock = false;
	auto endCodeBlock = [&]()
	{
		if (inCodeBlock)
		{
			output.append("```\n");
		}
		inCodeBlock = false;
	};
	auto formatForPos = [&](const QTextBlock &block, const int pos) -> QTextCharFormat
	{
		for (const auto fmtRange : block.textFormats())
		{
			if (fmtRange.start <= pos && pos <= (fmtRange.start + fmtRange.length))
			{
				return fmtRange.format;
			}
		}
		Q_ASSERT(false);
		return QTextCharFormat();
	};
	auto blockToMarkdown = [&](const QTextBlock &block, const int offset = 0) -> QString
	{
		QString out;
		bool inBold = false;
		bool inItalic = false;
		QString currentLink;
		for (int i = offset; i < block.text().size(); ++i)
		{
			const QChar c = block.text().at(i);
			const QTextCharFormat fmt = formatForPos(block, i);
			if (fmt.fontItalic() != inItalic)
			{
				out.insert(out.size() - 1, '_');
				inItalic = !inItalic;
			}
			if ((fmt.fontWeight() == QFont::Bold) != inBold)
			{
				out.insert(out.size() - 1, "**");
				inBold = !inBold;
			}
			if (fmt.anchorHref().isEmpty() && !currentLink.isNull())
			{
				out.insert(out.size() - 1, "](" + currentLink + ")");
			}
			else if (!fmt.anchorHref().isEmpty() && currentLink.isNull())
			{
				out.insert(out.size() - 1, "[");
				currentLink = fmt.anchorHref();
			}
			// FIXME images
			out.append(c);
		}
		return out;
	};
	for (QTextBlock block = source->begin(); block != source->end(); block = block.next())
	{
		// heading
		if (block.charFormat().toolTip() == block.text())
		{
			endCodeBlock();
			output.append(QString(sizeMap.key(block.charFormat().fontPointSize()), '#') + " " + block.text() + "\n");
		}
		else
		{
			// list
			if (QTextList *list = block.textList())
			{
				endCodeBlock();
				const QString indent = QString((list->format().indent()-1) * 2, ' ');
				if (list->format().style() == QTextListFormat::ListDisc)
				{
					output.append(indent + "* " + blockToMarkdown(block));
				}
				else
				{
					output.append(indent + QString::number(list->itemNumber(block) + 1) + ". " + blockToMarkdown(block));
				}
				wasInList = true;
			}
			else
			{
				if (wasInList)
				{
					output.append("");
					wasInList = false;
				}
				if (block.charFormat().fontFamily() == "Monospace")
				{
					if (!inCodeBlock)
					{
						inCodeBlock = true;
						output.insert(output.size() - 1, "```");
					}
					output.append(block.text().remove("\n"));
				}
				else
				{
					endCodeBlock();
					output.append(blockToMarkdown(block) + "\n");
				}
			}
		}
	}
	QString string = output.join("\n");
	return string.trimmed().toUtf8();
}

QList<QGithubMarkdown::Token> QGithubMarkdown::tokenize(const QString &string)
{
	bool escapeNextCharacter = false;
	QList<Token> tokens;
	QStringIterator iterator(string);

	auto lastToken = [&]() { return tokens.isEmpty() ? Token() : tokens.last(); };
	auto peekNext = [&]() { return iterator.hasNext() ? iterator.peekNext() : QChar(); };
	auto peekNext2 = [&]()
	{
		const QChar next = peekNext();
		if (next.isNull())
		{
			return QChar();
		}
		iterator.next();
		const QChar ret = peekNext();
		iterator.previous(); // don't forget to undo the call to next
		return ret;
	};
	auto peekPrevious = [&]() { return iterator.hasPrevious() ? iterator.peekPrevious() : QChar(); };
	auto peekPrevious2 = [&]()
	{
		const QChar previous = peekPrevious();
		if (previous.isNull())
		{
			return QChar();
		}
		iterator.previous();
		const QChar ret = peekPrevious();
		iterator.next(); // don't forget to undo the call to next
		return ret;
	};

	auto consumeSpace = [&]()
	{
		QString out;
		while (peekNext() == ' ')
		{
			out += iterator.next();
		}
		return out;
	};
	auto consumeUntilNewline = [&]()
	{
		QString out;
		while (true)
		{
			const QChar next = peekNext();
			if (next == '\n' || next.isNull())
			{
				break;
			}
			out += iterator.next();
		}
		if (iterator.hasNext())
		{
			out += iterator.next(); // the newline itself
		}
		return out;
	};
	auto firstNonSpaceOnLine = [&]()
	{
		int numCharacters = 0;
		while (peekPrevious2() == ' ' && !peekPrevious2().isNull())
		{
			numCharacters++;
			iterator.previous();
		}
		bool ret = peekPrevious2().isNull() || peekPrevious2() == '\n';
		// roll back
		while (numCharacters > 0)
		{
			numCharacters--;
			iterator.next();
		}
		return ret;
	};

	while (iterator.hasNext())
	{
		const QChar c = iterator.next();
		Token token;
		token.source = c;
		if (escapeNextCharacter)
		{
			escapeNextCharacter = false;
			token.type = Token::Character;
			token.source.prepend('\\');
			tokens.append(token);
			continue;
		}
		if (c == '\\' && peekNext() != '\n') // we don't allow escaping newlines
		{
			escapeNextCharacter = true;
			continue;
		}

		const bool startOfParagraph = lastToken().type == Token::NewLine || lastToken().type == Token::Invalid;
		const bool isFirstNonSpaceOnLine = startOfParagraph || firstNonSpaceOnLine();
		if (isFirstNonSpaceOnLine && c == '#')
		{
			int level = 1;
			while (peekNext() == '#')
			{
				level++;
				token.source += iterator.next();
			}
			token.source += consumeSpace();
			token.type = Token::HeadingStart;
			token.content = level;
		}
		else if (isFirstNonSpaceOnLine && c == '>')
		{
			token.source += consumeSpace();
			token.type = Token::QuoteStart;
		}
		else if (startOfParagraph && c == '`' && peekNext() == '`' && peekNext2() == '`')
		{
			token.source += iterator.next();
			token.source += iterator.next();
			token.source += consumeSpace();
			// TODO read the language here and add syntax highlighting?
			token.source += consumeUntilNewline();
			token.type = Token::CodeDelimiter;
		}
		else if (isFirstNonSpaceOnLine && c == '*')
		{
			token.source += consumeSpace();
			token.type = Token::UnorderedListStart;
		}
		// one digit
		else if (isFirstNonSpaceOnLine && c.isDigit() && peekNext() == '.')
		{
			token.content = QString(c).toInt();
			token.source += iterator.next();
			token.source += consumeSpace();
			token.type = Token::OrderedListStart;
		}
		// two digits
		else if (isFirstNonSpaceOnLine && c.isDigit() && peekNext().isDigit() && peekNext2() == '.')
		{
			token.content = QString(QString(c) + QString(peekNext())).toInt();
			token.source += iterator.next();
			token.source += iterator.next();
			token.source += consumeSpace();
			token.type = Token::OrderedListStart;
		}
		// TODO allow for numbers higher than 99?

		else if ((c == '*' || c == '_') && peekNext() == c)
		{
			token.source += iterator.next();
			token.type = Token::Bold;
		}
		else if ((c == '*' || c == '_'))
		{
			token.type = Token::Italic;
		}
		else if (c == '[')
		{
			token.type = Token::LinkStart;
		}
		else if (c == '!' && peekNext() == '[')
		{
			token.source += iterator.next();
			token.type = Token::ImageStart;
		}
		else if (c == ']' && peekNext() == '(')
		{
			token.source += iterator.next();
			token.type = Token::LinkMiddle;
		}
		else if (c == ')')
		{
			token.type = Token::LinkEnd;
		}
		else if (c == '`')
		{
			token.type = Token::InlineCodeDelimiter;
		}
		else if (c == '\n')
		{
			token.type = Token::NewLine;
		}
		else if (c == '<')
		{
			token.type = peekNext() == '/' ? Token::HtmlTagClose : Token::HtmlTagOpen;
			QString tag = "<";
			while (true)
			{
				const QChar c = iterator.next();
				token.source += c;
				tag += c;
				if (c == '>' || c.isNull())
				{
					break;
				}
			}
			token.content = tag;
		}
		else
		{
			token.type = Token::Character;
			token.content = c;
		}

		tokens.append(token);
	}
	tokens.append(Token::EOD);
	return tokens;
}
QList<QGithubMarkdown::Paragraph> QGithubMarkdown::paragraphize(const QList<QGithubMarkdown::Token> &tokens)
{
	QList<Paragraph> out;
	Paragraph currentParagraph;
	QListIterator<Token> iterator(tokens);

	auto peekPreviousInternal = [&]() { return iterator.hasPrevious() ? iterator.peekPrevious() : Token(); };
	auto peekPrevious = [&]()
	{
		const Token previous = peekPreviousInternal();
		if (previous.type == Token::Invalid)
		{
			return Token();
		}
		iterator.previous();
		const Token ret = peekPreviousInternal();
		iterator.next(); // don't forget to undo the call to next
		return ret;
	};
	auto firstNonSpaceOnLine = [&]()
	{
		int numTokens = 0;
		while (peekPrevious().type == Token::Character && peekPrevious().content.toString() == " ")
		{
			numTokens++;
			iterator.previous();
		}
		bool ret = peekPrevious().type == Token::Invalid || peekPrevious().type == Token::NewLine;
		// roll back
		while (numTokens > 0)
		{
			numTokens--;
			iterator.next();
		}
		return ret;
	};
	auto nextParagraph = [&]()
	{
		if (!currentParagraph.tokens.isEmpty())
		{
			if ((currentParagraph.tokens.last().type == Token::Character
					&& currentParagraph.tokens.last().source == "\n")
					|| currentParagraph.tokens.last().type == Token::NewLine)
			{
				currentParagraph.tokens.removeLast();
			}
			if (!currentParagraph.tokens.isEmpty())
			{
				out.append(currentParagraph);
			}
		}
		currentParagraph = Paragraph();
	};

	QList<Token> spaceTokens;

	while (iterator.hasNext())
	{
		const Token token = iterator.next();
		const bool isFirstNonSpace = firstNonSpaceOnLine();

		if (token.type == Token::EOD)
		{
			break;
		}
		if (isFirstNonSpace && token.type == Token::Character && token.content == ' ')
		{
			spaceTokens += token;
			continue;
		}
		if (token.type == Token::NewLine)
		{
			spaceTokens.clear();
		}

		if (token.type == Token::NewLine && peekPrevious().type == Token::NewLine)
		{
			nextParagraph();
		}
		else if (token.type == Token::CodeDelimiter)
		{
			currentParagraph.type = Paragraph::Code;
			while (iterator.hasNext() && iterator.peekNext().type != Token::CodeDelimiter)
			{
				currentParagraph.tokens.append(iterator.next());
			}
			if (iterator.hasNext())
			{
				iterator.next(); // consume code end delimiter
			}
			nextParagraph();
		}
		else if (token.type == Token::QuoteStart && isFirstNonSpace)
		{
			currentParagraph.type = Paragraph::Quote;
		}
		else if (token.type == Token::HeadingStart && isFirstNonSpace)
		{
			currentParagraph.type = (Paragraph::Type)token.content.toInt();
		}
		else if (token.type == Token::UnorderedListStart && isFirstNonSpace)
		{
			nextParagraph();
			currentParagraph.type = Paragraph::UnorderedList;
			currentParagraph.indentTokens = spaceTokens;
		}
		else if (token.type == Token::OrderedListStart && isFirstNonSpace)
		{
			nextParagraph();
			currentParagraph.type = Paragraph::OrderedList;
			currentParagraph.indentTokens = spaceTokens;
		}
		else if (token.type == Token::NewLine)
		{
			Token token;
			token.type = Token::Character;
			token.source = '\n';
			token.content = QChar(' ');
			currentParagraph.tokens += token;
		}
		else
		{
			currentParagraph.tokens += token;
		}
	}

	if (!currentParagraph.tokens.isEmpty())
	{
		out.append(currentParagraph);
	}

	return out;
}
QList<QPair<QGithubMarkdown::Paragraph, QGithubMarkdown::List> > QGithubMarkdown::listize(const QList<QGithubMarkdown::Paragraph> &paragraphs)
{
	QList<QPair<Paragraph, List>> out;
	List currentList;

	auto finishList = [&]()
	{
		if (currentList.paragraphs.size() > 0)
		{
			out.append(qMakePair(Paragraph(), currentList));
		}
		currentList = List();
	};

	for (const Paragraph &paragraph : paragraphs)
	{
		if (paragraph.type != Paragraph::OrderedList && paragraph.type != Paragraph::UnorderedList)
		{
			finishList();
			out.append(qMakePair(paragraph, List()));
		}
		else
		{
			const int indent = (paragraph.indentTokens.size() / 2) + 1;
			if (currentList.indent != indent
					|| currentList.ordered != (paragraph.type == Paragraph::OrderedList))
			{
				finishList();
				currentList.indent = indent;
				currentList.ordered = (paragraph.type == Paragraph::OrderedList);
			}
			currentList.paragraphs.append(paragraph);
		}
	}
	return out;
}

QStringList QAbstractMarkdown::flavours()
{
	return QStringList() << "github";
}
QAbstractMarkdown *QAbstractMarkdown::flavour(const QString &id)
{
	if (id == "github")
	{
		return new QGithubMarkdown;
	}
	Q_ASSERT(false);
	return 0;
}

QDebug operator<<(QDebug dbg, QGithubMarkdown::Token::Type type)
{
	switch (type)
	{
	case QGithubMarkdown::Token::Character: dbg.nospace() << "Character"; break;
	case QGithubMarkdown::Token::NewLine: dbg.nospace() << "NewLine"; break;
	case QGithubMarkdown::Token::HeadingStart: dbg.nospace() << "HeadingStart"; break;
	case QGithubMarkdown::Token::QuoteStart: dbg.nospace() << "QuoteStart"; break;
	case QGithubMarkdown::Token::CodeDelimiter: dbg.nospace() << "CodeDelimiter"; break;
	case QGithubMarkdown::Token::InlineCodeDelimiter: dbg.nospace() << "InlineCodeDelimiter"; break;
	case QGithubMarkdown::Token::Bold: dbg.nospace() << "Bold"; break;
	case QGithubMarkdown::Token::Italic: dbg.nospace() << "Italic"; break;
	case QGithubMarkdown::Token::ImageStart: dbg.nospace() << "ImageStart"; break;
	case QGithubMarkdown::Token::LinkStart: dbg.nospace() << "LinkStart"; break;
	case QGithubMarkdown::Token::LinkMiddle: dbg.nospace() << "LinkMiddle"; break;
	case QGithubMarkdown::Token::LinkEnd: dbg.nospace() << "LinkEnd"; break;
	case QGithubMarkdown::Token::UnorderedListStart: dbg.nospace() << "UnorderedListStart"; break;
	case QGithubMarkdown::Token::OrderedListStart: dbg.nospace() << "OrderedListStart"; break;
	case QGithubMarkdown::Token::HtmlTagOpen: dbg.nospace() << "HtmlTagOpen"; break;
	case QGithubMarkdown::Token::HtmlTagClose: dbg.nospace() << "HtmlTagClose"; break;
	case QGithubMarkdown::Token::Invalid: dbg.nospace() << "Invalid"; break;
	case QGithubMarkdown::Token::EOD: dbg.nospace() << "EOD"; break;
	}
	return dbg.maybeSpace();
}
QDebug operator<<(QDebug dbg, QGithubMarkdown::Token token)
{
	dbg.nospace() << "Token(type=" << token.type << " content=" << token.content << " source=" << token.source << ")";
	return dbg.maybeSpace();
}
QDebug operator<<(QDebug dbg, QGithubMarkdown::Paragraph::Type type)
{
	switch (type)
	{
	case QGithubMarkdown::Paragraph::Heading1: dbg.nospace() << "Heading1"; break;
	case QGithubMarkdown::Paragraph::Heading2: dbg.nospace() << "Heading2"; break;
	case QGithubMarkdown::Paragraph::Heading3: dbg.nospace() << "Heading3"; break;
	case QGithubMarkdown::Paragraph::Heading4: dbg.nospace() << "Heading4"; break;
	case QGithubMarkdown::Paragraph::Heading5: dbg.nospace() << "Heading5"; break;
	case QGithubMarkdown::Paragraph::Heading6: dbg.nospace() << "Heading6"; break;
	case QGithubMarkdown::Paragraph::Normal: dbg.nospace() << "Normal"; break;
	case QGithubMarkdown::Paragraph::Quote: dbg.nospace() << "Quote"; break;
	case QGithubMarkdown::Paragraph::Code: dbg.nospace() << "Code"; break;
	case QGithubMarkdown::Paragraph::UnorderedList: dbg.nospace() << "UnorderedList"; break;
	case QGithubMarkdown::Paragraph::OrderedList: dbg.nospace() << "OrderedList"; break;
	}
	return dbg.maybeSpace();
}
QDebug operator<<(QDebug dbg, QGithubMarkdown::Paragraph paragraph)
{
	dbg.nospace() << "Paragraph(type=" << paragraph.type << "\n"
				  << "          tokens=\n";
	for (const QGithubMarkdown::Token &token : paragraph.tokens)
	{
		dbg.nospace() << "              " << token << "\n";
	}
	dbg.nospace() << "          indentTokens=\n";
	for (const QGithubMarkdown::Token &token : paragraph.indentTokens)
	{
		dbg.nospace() << "              " << token << "\n";
	}
	dbg.nospace() << ")";
	return dbg.maybeSpace();
}
