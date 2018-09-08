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
#include "redcarpetmarkdownconverter.h"

#include "markdowndocument.h"
#include "template/htmltemplate.h"

#include <QThread>
#include <QDebug>

#include <ruby.h>

Q_GLOBAL_STATIC(Ruby, getRuby);

Ruby::Ruby(QObject *parent)
    : QObject(parent)
{
    ruby_init();
    ruby_init_loadpath();
    ruby_incpush("/usr/lib/ruby/gems/2.5.0/gems/redcarpet-3.4.0/lib");
    ruby_show_version();
}

Ruby::~Ruby()
{
    ruby_finalize();
}

QVariant Ruby::toVariant(VALUE value)
{
    switch (TYPE(value)) {
        case T_FLOAT:
            return RFLOAT_VALUE(value);
        case T_STRING:
            return QString::fromUtf8(RSTRING_PTR(value), RSTRING_LEN(value));
        case T_TRUE:
            return true;
        case T_FALSE:
            return false;
    }

    qWarning() << "Unhandled type:" << TYPE(value) << value;
    return QVariant();
}

QString Ruby::lastException()
{
    VALUE exception = rb_errinfo();
    rb_set_errinfo(Qnil);
    return RTEST(exception)
            ? toVariant(rb_funcall(exception, rb_intern("full_message"), 0)).toString()
            : QString();
}

QVariant Ruby::eval(const QString &script, bool *ok)
{
    // Ensure Ruby is initialized in the thread of the caller
    Q_UNUSED(getRuby());

    if (getRuby()->thread() != QThread::currentThread()) {
        // Ruby interpreter is not thread safe, calls have to be made in the thread in which it was initialized
        if (ok) {
            *ok = false;
        }
        return QVariant();
    }

    if (ok) {
        *ok = true;
    }

    int state;
    VALUE result = rb_eval_string_protect(qUtf8Printable(script), &state);

    if (state)
    {
        if (ok) {
            *ok = false;
        }

        qWarning("Script error: %s", qPrintable(lastException()));
        qWarning() << "Evaluating:" << qUtf8Printable(script);
    }

    return toVariant(result);
}

class RedCarpetMarkdownDocument : public MarkdownDocument
{
public:
    RedCarpetMarkdownDocument(const QString &content, bool smartyPants, const RedcarpetOptions &markdown, const RedcarpetOptions &renderer)
        : MarkdownDocument()
        , m_content(content)
        , m_smartypants(smartyPants)
        , m_markdownExtensions(markdown)
        , m_rendererOptions(renderer)
    {
        qDebug("RedCarpet document initialized with renderer: %s", qUtf8Printable(this->renderer()));
        qDebug("RedCarpet document initialized with Smartypants: %i", m_smartypants);
        qDebug("RedCarpet document initialized with extensions: %s", qUtf8Printable(toString(m_markdownExtensions)));
        qDebug("RedCarpet document initialized with options: %s", qUtf8Printable(toString(m_rendererOptions)));
    }

    ~RedCarpetMarkdownDocument() = default;

    QString toHtml() const {
        const QString content(contentString());
        if (!content.isEmpty()) {
            const QString script = QString::fromLatin1(
                        "require 'redcarpet'\n"
                        "markdown = Redcarpet::Markdown.new(%1.new(%2), %3)\n"
                        "markdown.render(\"%4\")\n"
                        ).arg(renderer(), toString(m_markdownExtensions), toString(m_rendererOptions), content);
            return Ruby::eval(script).toString();
        }
        return QString();
    }

    QString toHtmlTableOfContent() const {
        const QString content(contentString());
        if (!content.isEmpty()) {
            const QString script = QString::fromLatin1(
                        "require 'redcarpet'\n"
                        "markdown = Redcarpet::Markdown.new(Redcarpet::Render::HTML_TOC.new(%1), %2)\n"
                        "markdown.render(\"%3\")\n"
                        ).arg(toString(m_markdownExtensions), toString(m_rendererOptions), content);
            return Ruby::eval(script).toString();
        }
        return QString();
    }

private:
    QString m_content;
    bool m_smartypants;
    RedcarpetOptions m_markdownExtensions;
    RedcarpetOptions m_rendererOptions;

    QString renderer() const {
        return m_smartypants
                ? QStringLiteral("Redcarpet::Render::SmartyHTML")
                : QStringLiteral("Redcarpet::Render::HTML");
    }

    QString toString(const RedcarpetOptions &options) const {
        QStringList opt;
        for (auto it = options.constBegin(), end = options.constEnd(); it != end; ++it) {
            opt << QString::fromLatin1("%1: %2").arg(it.key(), QVariant(it.value()).toString());
        }
        return opt.join(QStringLiteral(", "));
    }

    QString contentString() const {
        return QString(m_content)
                .replace(QStringLiteral("\""), QStringLiteral("\\\""))
                .replace(QStringLiteral("\n"), QStringLiteral("\\n"))
        ;
    }
};

RedCarpetMarkdownConverter::RedCarpetMarkdownConverter()
    : MarkdownConverter()
{
}

MarkdownDocument *RedCarpetMarkdownConverter::createDocument(const QString &text, ConverterOptions options)
{
    return new RedCarpetMarkdownDocument(text,
                                         !options.testFlag(MarkdownConverter::NoSmartypantsOption),
                                         markdownOptions(options),
                                         rendererOptions(options));
}

QString RedCarpetMarkdownConverter::renderAsHtml(MarkdownDocument *document)
{
    const auto doc = dynamic_cast<RedCarpetMarkdownDocument *>(document);
    return doc ? doc->toHtml() : QString();
}

QString RedCarpetMarkdownConverter::renderAsTableOfContents(MarkdownDocument *document)
{
    const auto doc = dynamic_cast<RedCarpetMarkdownDocument *>(document);
    return doc ? doc->toHtmlTableOfContent() : QString();
}

Template *RedCarpetMarkdownConverter::templateRenderer() const
{
    static HtmlTemplate htmlTemplate;
    return &htmlTemplate;
}

MarkdownConverter::ConverterOptions RedCarpetMarkdownConverter::supportedOptions() const
{
    return
            MarkdownConverter::NoSmartypantsOption |
            // Markdown extensions
            MarkdownConverter::NoSuperscriptOption |
            MarkdownConverter::NoTablesOption |
            MarkdownConverter::NoStrikethroughOption |
            MarkdownConverter::AutolinkOption |
            MarkdownConverter::ExtraFootnoteOption |
            // Renderer options
            MarkdownConverter::NoHtmlOption |
            MarkdownConverter::NoImagesOption |
            MarkdownConverter::NoLinksOption |
            MarkdownConverter::TableOfContentsOption |
            MarkdownConverter::NoStyleOption;
}

RedcarpetOptions RedCarpetMarkdownConverter::markdownOptions(MarkdownConverter::ConverterOptions options) const
{
    // *** Markdown extensions
    // :no_intra_emphasis: do not parse emphasis inside of words. Strings such as foo_bar_baz will not generate <em> tags.
    // :tables: parse tables, PHP-Markdown style.
    // :fenced_code_blocks: parse fenced code blocks, PHP-Markdown style. Blocks delimited with 3 or more ~ or backticks will be considered as code, without the need to be indented. An optional language name may be added at the end of the opening fence for the code block.
    // :autolink: parse links even when they are not enclosed in <> characters. Autolinks for the http, https and ftp protocols will be automatically detected. Email addresses and http links without protocol, but starting with www are also handled.
    // :disable_indented_code_blocks: do not parse usual markdown code blocks. Markdown converts text with four spaces at the front of each line to code blocks. This option prevents it from doing so. Recommended to use with fenced_code_blocks: true.
    // :strikethrough: parse strikethrough, PHP-Markdown style. Two ~ characters mark the start of a strikethrough, e.g. this is ~~good~~ bad.
    // :lax_spacing: HTML blocks do not require to be surrounded by an empty line as in the Markdown standard.
    // :space_after_headers: A space is always required between the hash at the beginning of a header and its name, e.g. #this is my header would not be a valid header.
    // :superscript: parse superscripts after the ^ character; contiguous superscripts are nested together, and complex values can be enclosed in parenthesis, e.g. this is the 2^(nd) time.
    // :underline: parse underscored emphasis as underlines. This is _underlined_ but this is still *italic*.
    // :highlight: parse highlights. This is ==highlighted==. It looks like this: <mark>highlighted</mark>
    // :quote: parse quotes. This is a "quote". It looks like this: <q>quote</q>
    // :footnotes:

    RedcarpetOptions opt;
    opt[QStringLiteral("superscript")] = !options.testFlag(MarkdownConverter::NoSuperscriptOption);
    opt[QStringLiteral("tables")] = !options.testFlag(MarkdownConverter::NoTablesOption);
    opt[QStringLiteral("strikethrough")] = !options.testFlag(MarkdownConverter::NoStrikethroughOption);
    opt[QStringLiteral("autolink")] = options.testFlag(MarkdownConverter::AutolinkOption);
    opt[QStringLiteral("footnotes")] = options.testFlag(MarkdownConverter::ExtraFootnoteOption);
    //
//    opt[QStringLiteral("no_intra_emphasis")] = true;
//    opt[QStringLiteral("fenced_code_blocks")] = true;
    return opt;
}

RedcarpetOptions RedCarpetMarkdownConverter::rendererOptions(ConverterOptions options) const
{
    // *** Renderer options
    // :filter_html: do not allow any user-inputted HTML in the output.
    // :no_images: do not generate any <img> tags.
    // :no_links: do not generate any <a> tags.
    // :no_styles: do not generate any <style> tags.
    // :escape_html: escape any HTML tags. This option has precedence over :no_styles, :no_links, :no_images and :filter_html which means that any existing tag will be escaped instead of being removed.
    // :safe_links_only: only generate links for protocols which are considered safe.
    // :with_toc_data: add HTML anchors to each header in the output HTML, to allow linking to each section.
    // :hard_wrap: insert HTML <br> tags inside paragraphs where the original Markdown document had newlines (by default, Markdown ignores these newlines).
    // :xhtml: output XHTML-conformant tags. This option is always enabled in the Render::XHTML renderer.
    // :prettify: add prettyprint classes to <code> tags for google-code-prettify.
    // :link_attributes: hash of extra attributes to add to links.

    RedcarpetOptions opt;
    opt[QStringLiteral("filter_html")] = options.testFlag(MarkdownConverter::NoHtmlOption);
    opt[QStringLiteral("escape_html")] = options.testFlag(MarkdownConverter::NoHtmlOption);
    opt[QStringLiteral("no_images")] = options.testFlag(MarkdownConverter::NoImagesOption);
    opt[QStringLiteral("no_links")] = options.testFlag(MarkdownConverter::NoLinksOption);
    opt[QStringLiteral("with_toc_data")] = options.testFlag(MarkdownConverter::TableOfContentsOption);
    opt[QStringLiteral("no_styles")] = options.testFlag(MarkdownConverter::NoStyleOption);
    return opt;
}
