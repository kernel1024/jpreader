#ifndef __HTML_PARSER_NODE_H
#define __HTML_PARSER_NODE_H

#include <string>
#include <QHash>
#include <QString>
#include <QStringList>

namespace htmlcxx {
	namespace HTML {
		class Node {
            public:
                Node();
				~Node() {}

                inline void text(const QString& text) { this->mText = text; }
                inline const QString& text() const { return this->mText; }

                inline void closingText(const QString &text) { this->mClosingText = text; }
                inline const QString& closingText() const { return mClosingText; }

				inline void offset(unsigned int offset) { this->mOffset = offset; }
				inline unsigned int offset() const { return this->mOffset; }

				inline void length(unsigned int length) { this->mLength = length; }
				inline unsigned int length() const { return this->mLength; }

                inline void tagName(const QString& tagname) { this->mTagName = tagname; }
                inline const QString& tagName() const { return this->mTagName; }

				bool isTag() const { return this->mIsHtmlTag; }
				void isTag(bool is_html_tag){ this->mIsHtmlTag = is_html_tag; }

				bool isComment() const { return this->mComment; }
				void isComment(bool comment){ this->mComment = comment; }

                operator QString() const;
                operator std::string() const;
                std::ostream &operator<<(std::ostream &stream) const;

                const QHash<QString,QString>& attributes() const { return this->mAttributes; }
                const QStringList& attributesOrder() const { return this->mAttributesOrder; }
                void parseAttributes();

				bool operator==(const Node &rhs) const;

			protected:

                QString mText;
                QString mClosingText;
				unsigned int mOffset;
				unsigned int mLength;
                QString mTagName;
                QHash<QString,QString> mAttributes;
                QStringList mAttributesOrder;
				bool mIsHtmlTag;
				bool mComment;
		};
	}
}

#endif
