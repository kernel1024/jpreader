#ifndef __HTML_PARSER_SAX_H__
#define __HTML_PARSER_SAX_H__

#include <QString>

#include "Node.h"

namespace htmlcxx
{
	namespace HTML
	{
		class ParserSax
		{
			public:
				ParserSax() : mpLiteral(0), mCdata(false) {}
				virtual ~ParserSax() {}

				/** Parse the html code */
                void parse(const QString &html);

				template <typename _Iterator>
				void parse(_Iterator begin, _Iterator end);

			protected:
				// Redefine this if you want to do some initialization before
				// the parsing
				virtual void beginParsing() {}

                virtual void foundTag(Node node, bool isEnd) { Q_UNUSED(node); Q_UNUSED(isEnd); }
                virtual void foundText(Node node) { Q_UNUSED(node); }
                virtual void foundComment(Node node) { Q_UNUSED(node); }

				virtual void endParsing() {}


				template <typename _Iterator>
				void parse(_Iterator &begin, _Iterator &end,
						std::forward_iterator_tag);

				template <typename _Iterator>
				void parseHtmlTag(_Iterator b, _Iterator c);

				template <typename _Iterator>
				void parseContent(_Iterator b, _Iterator c);

				template <typename _Iterator>
				void parseComment(_Iterator b, _Iterator c);

				template <typename _Iterator>
				_Iterator skipHtmlTag(_Iterator ptr, _Iterator end);
				
				template <typename _Iterator>
				_Iterator skipHtmlComment(_Iterator ptr, _Iterator end);

				unsigned long mCurrentOffset;
                QChar *mpLiteral;
				bool mCdata;
		};

	}//namespace HTML
}//namespace htmlcxx

#include "ParserSaxTcc.h"

#endif
