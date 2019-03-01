#include "ParserDom.h"

#include <iostream>
#include <vector>

#include <QDebug>

#define TAG_NAME_MAX 10

using namespace std;
using namespace htmlcxx; 
using namespace HTML; 
using namespace kp;

const tree<HTML::Node>& ParserDom::parseTree(const QString &html)
{
    parse(html);
    return getTree();
}

void ParserDom::beginParsing()
{
	mHtmlTree.clear();
	tree<HTML::Node>::iterator top = mHtmlTree.begin();
	HTML::Node lambda_node;
	lambda_node.offset(0);
	lambda_node.length(0);
	lambda_node.isTag(true);
	lambda_node.isComment(false);
	mCurrentState = mHtmlTree.insert(top,lambda_node);
}

void ParserDom::endParsing()
{
	tree<HTML::Node>::iterator top = mHtmlTree.begin();
	top->length(mCurrentOffset);
}

void ParserDom::foundComment(Node node)
{
	//Add child content node, but do not update current state
	mHtmlTree.append_child(mCurrentState, node);
}

void ParserDom::foundText(Node node)
{
	//Add child content node, but do not update current state
	mHtmlTree.append_child(mCurrentState, node);
}

void ParserDom::foundTag(Node node, bool isEnd)
{
	if (!isEnd) 
	{
		//append to current tree node
		tree<HTML::Node>::iterator next_state;
		next_state = mHtmlTree.append_child(mCurrentState, node);
		mCurrentState = next_state;
	} 
	else 
	{
		//Look if there is a pending open tag with that same name upwards
		//If mCurrentState tag isn't matching tag, maybe a some of its parents
		// matches
        std::vector< tree<HTML::Node>::iterator > path;
		tree<HTML::Node>::iterator i = mCurrentState;
		bool found_open = false;
		while (i != mHtmlTree.begin())
		{
			assert(i->isTag());
            assert(!i->tagName().isEmpty());

            bool equal = (i->tagName().compare(node.tagName(),Qt::CaseInsensitive)==0);

			if (equal) 
			{
                // qDebug() << "Found matching tag " << i->tagName().c_str();
				//Closing tag closes this tag
				//Set length to full range between the opening tag and
				//closing tag
				i->length(node.offset() + node.length() - i->offset());
				i->closingText(node.text());

				mCurrentState = mHtmlTree.parent(i);
				found_open = true;
				break;
			} 

            path.push_back(i);

			i = mHtmlTree.parent(i);
		}

		if (found_open)
		{
			//If match was upper in the tree, so we need to invalidate child
			//nodes that were waiting for a close
            for (const auto &it : path)
			{
                mHtmlTree.flatten(it);
			}
		} 
		else 
		{
            // qDebug() << "Unmatched tag " << node.text().c_str();

			// Treat as comment
			node.isTag(false);
			node.isComment(true);
			mHtmlTree.append_child(mCurrentState, node);
		}
	}
}

ostream &HTML::operator<<(ostream &stream, const tree<HTML::Node> &tr)
{

       tree<HTML::Node>::pre_order_iterator it = tr.begin();
       tree<HTML::Node>::pre_order_iterator end = tr.end();

       int rootdepth = tr.depth(it);
       stream << "-----" << endl;

       unsigned int n = 0;
       while ( it != end )
       {

               int cur_depth = tr.depth(it);
               for(int i=0; i < cur_depth - rootdepth; ++i) stream << "  ";
               stream << n << "@";
               stream << "[" << it->offset() << ";";
               stream << it->offset() + it->length() << ") ";
               stream << std::string(*it) << endl;
               ++it, ++n;
       }
       stream << "-----" << endl;
       return stream;
}
