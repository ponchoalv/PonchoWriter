#pragma once
#include <QString>
class QTextDocument;
// Bake token formats into the imported Markdown document before HTML export.
void highlightReadCode(QTextDocument &document, const QString &markdown, bool dark);
