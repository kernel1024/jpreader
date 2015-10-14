#ifndef __HTML_PARSER_NODE_H
#define __HTML_PARSER_NODE_H

//#include <map>
#include <QMap>
#include <QString>
#include <string>
#include <utility>

namespace htmlcxx {
	namespace HTML {
		class Node {

			public:
                Node();
				~Node() {}

				inline void text(const std::string& text) { this->mText = text; }
				inline const std::string& text() const { return this->mText; }

				inline void closingText(const std::string &text) { this->mClosingText = text; }
				inline const std::string& closingText() const { return mClosingText; }

				inline void offset(unsigned int offset) { this->mOffset = offset; }
				inline unsigned int offset() const { return this->mOffset; }

				inline void length(unsigned int length) { this->mLength = length; }
				inline unsigned int length() const { return this->mLength; }

				inline void tagName(const std::string& tagname) { this->mTagName = tagname; }
				inline const std::string& tagName() const { return this->mTagName; }

				bool isTag() const { return this->mIsHtmlTag; }
				void isTag(bool is_html_tag){ this->mIsHtmlTag = is_html_tag; }

				bool isComment() const { return this->mComment; }
				void isComment(bool comment){ this->mComment = comment; }

				operator std::string() const;
				std::ostream &operator<<(std::ostream &stream) const;

                const QMap<QString,QString>& attributes() const { return this->mAttributes; }
				void parseAttributes();

				bool operator==(const Node &rhs) const;

			protected:

				std::string mText;
				std::string mClosingText;
				unsigned int mOffset;
				unsigned int mLength;
				std::string mTagName;
                QMap<QString,QString> mAttributes;
				bool mIsHtmlTag;
				bool mComment;
		};
	}
}

#endif
