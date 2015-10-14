#include "ParserSax.h"

void htmlcxx::HTML::ParserSax::parse(const QString &html)
{
    parse(html.begin(), html.end());
}
