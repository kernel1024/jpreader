#ifndef __HTML_PARSER_DOM_H__
#define __HTML_PARSER_DOM_H__

#include "ParserSax.h"
#include "tree.h"

namespace htmlcxx
{
	namespace HTML
	{
		class ParserDom : public ParserSax
		{
            public:
				ParserDom() {}
                ParserDom(const ParserDom& other) = delete;
                ~ParserDom() override = default;
                ParserDom &operator=(const ParserDom& other) = delete;

                const tree<Node> &parseTree(const QString &html);
				const tree<Node> &getTree()
				{ return mHtmlTree; }

			protected:
                void beginParsing() override;

                void foundTag(const Node &node, bool isEnd) override;
                void foundText(const Node &node) override;
                void foundComment(const Node &node) override;

                void endParsing() override;
				
				tree<Node> mHtmlTree;
				tree<Node>::iterator mCurrentState;
		};

        std::ostream &operator<<(std::ostream &stream, const tree<HTML::Node> &tr);

	} //namespace HTML
} //namespace htmlcxx

#endif
