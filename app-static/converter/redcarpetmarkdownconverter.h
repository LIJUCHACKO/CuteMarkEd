/*
 * Copyright 2018 Filipe Azevedo <pasnox@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     (1) Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *
 *     (2) Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *
 *     (3) The name of the author may not be used to
 *     endorse or promote products derived from this software without
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#pragma once

#include "markdownconverter.h"

#include <QMap>
#include <QVariant>

using RedcarpetOptions = QMap<QString, bool>;
typedef unsigned long VALUE;

class Ruby : public QObject
{
    Q_OBJECT

public:
    Ruby(QObject *parent = nullptr);
    ~Ruby();

    static QVariant toVariant(VALUE value);
    static QString lastException();
    static QVariant eval(const QString &script, bool *ok = nullptr);
};

class RedCarpetMarkdownConverter : public MarkdownConverter
{
public:
    RedCarpetMarkdownConverter();

    MarkdownDocument *createDocument(const QString &text, ConverterOptions options) override;
    QString renderAsHtml(MarkdownDocument *document) override;
    QString renderAsTableOfContents(MarkdownDocument *document) override;

    Template *templateRenderer() const override;

    ConverterOptions supportedOptions() const override;

private:
    RedcarpetOptions markdownOptions(ConverterOptions options) const;
    RedcarpetOptions rendererOptions(ConverterOptions options) const;
};
