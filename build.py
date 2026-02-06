"Build the website by converting MD to HTML and creating index pages."

__version__ = "0.16.1"

import csv
import datetime
import email.utils
from hashlib import sha1
import math
import os
import os.path
from pathlib import Path
import re
import shutil
import time
import unicodedata

import jinja2
import jsonschema
import markupsafe
import marko
import yaml

BASE_URL = "https://pekrau.github.io"    # No trailing "/" slash!
GOODREADS_FILENAME = "source/goodreads_library_export.csv"
AUTHORS_CANONICAL_FILENAME = "source/authors_canonical.csv"

FRONT_MATTER_RX = re.compile(r"^---(.*?)---", re.DOTALL | re.MULTILINE)

POSTS = []                # List of posts sorted by date.
REDIRECTED_POSTS = []     # Deleted posts that redirect to another URL.
PAGES = []                # List of pages.
REDIRECTED_PAGES = []     # Deleted pages that redirect to another URL.
TAGS = {}                 # key: tag name; value: dict(name, value, posts)
CATEGORIES = {}           # key: category name; value: dict(name, value, posts)
TEXTS = {}                # key: "{familyname} {published}", optional resolving suffix
AUTHORS = {}              # key: name; value: canonical name
HTML_FILES = set()        # All HTML files created during a run.
SITEMAP_URLS = {};        # Key: canonical URL, value: lastmod.
MAX_LATEST_ITEMS = 10     # Max latest item on index page and in RSS 'feed.xml' file.
LATEST_ITEMS = []         # The latest items; posts and text reviews.
YEAR_PUBLISHED = {}       # key: year; value: list of texts


# Setup the Jinja2 template processing environment.
def post_link(post):
    return markupsafe.Markup(f"""<a href="{post['path']}">{post['title']}</a>""")


def author_link(author, display=False):
    if display:
        name = author_display(author)
    else:
        name = author
    return markupsafe.Markup(f"""<a href="/library/authors/{author}">{name}</a>""")


def authors_links(text, max=3, display=False):
    if max and len(text["authors"]) > max:
        return markupsafe.Markup("; ".join([author_link(a, display=display)
                                            for a in text["authors"][:max]] + ["..."]))
    else:
        return markupsafe.Markup("; ".join([author_link(a, display=display)
                                            for a in text["authors"]]))


def author_display(author):
    "Display version of the author name: firstname familyname."
    try:
        familyname, firstname = author.split(",", 1)
    except ValueError:
        return author
    else:
        return f"{firstname.strip()} {familyname.strip()}"


def normalize(reference):
    """Normalize the reference for part of a URL.
    - Normalize non-ASCII characters.
    - Convert the string to ASCII.
    - Lowercase.
    - Replace blank with dash.
    """
    return unicodedata.normalize("NFKD", reference).\
        replace("æ", "a").\
        replace("Æ", "A").\
        encode("ASCII", "ignore").\
        decode("utf-8").\
        lower().\
        replace(" ", "-")


def text_link(text, full=False):
    if full:
        return markupsafe.Markup(" ".join(["; ".join(text["authors"]),
                                           text_link(text),
                                           f"({text['published']})"]))
    else:
        return markupsafe.Markup(f"""<a href="{text['path']}">{text['title']}</a>""")


def category_link(category):
    return markupsafe.Markup(f"""<a href="/blog/categories/{category['name']}" class="text-nowrap">{category['value']}</a>""")


def tag_link(tag, sized=True):
    try:
        number = len(tag["posts"])
    except KeyError:
        tag = TAGS[tag["name"]]
        number = len(tag["posts"])
    if sized:
        factor = 50 * (math.log(number) + 1.5)
        span = f"""<span style="font-size: {factor}%;">{tag['value']}</span>"""
    else:
        span = tag['value']
    href = f"/blog/tags/{tag['name']}"
    return markupsafe.Markup(f'<a href="{href}" class="text-nowrap" title="{number}">{span}</a>')


def subject_link(subject):
    title = " ".join([p.capitalize() for p in subject.split("-")])
    return markupsafe.Markup(f"""<a href="/library/subjects/{subject}" class="text-nowrap">{title}</a>""")


def published_link(year):
    return markupsafe.Markup(f"""<a href="/library/published/{year}" class="text-nowrap">{year}</a>""")


env = jinja2.Environment(
    loader=jinja2.FileSystemLoader("templates"),
    autoescape=jinja2.select_autoescape(['html'])
)
env.globals["post_link"] = post_link
env.globals["author_link"] = author_link
env.globals["authors_links"] = authors_links
env.globals["author_display"] = author_display
env.globals["normalize"] = normalize
env.globals["text_link"] = text_link
env.globals["category_link"] = category_link
env.globals["tag_link"] = tag_link
env.globals["subject_link"] = subject_link
env.globals["published_link"] = published_link
env.globals["len"] = len
env.globals["sorted"] = sorted


class HTMLRenderer(marko.html_renderer.HTMLRenderer):
    "Modify output for Bootstrap 5."

    def render_quote(self, element):
        """Add blockquote output class for Bootstrap 5.
        If the last element is a source, then it is rendered as a blockquote footer.
        """
        source = None
        if len(element.children) >=2:
            source = self.render_children(element.children[-1])
            if source.startswith("source:"):
                source = source[len("source:"):].strip()
                element.children.pop()
            else:
                source = None
        text = self.render_children(element)
        if source:
            return f'''<figure class="ms-3">
<blockquote class="blockquote">{text}</blockquote>
<figcaption class="blockquote-footer">{source}</figcaption>
</figure>'''
        else:
            return f'''<figure class="ms-3">
<blockquote class="blockquote">{text}</blockquote>
</figure>'''

# Markdown converter.
MARKDOWN = marko.Markdown(renderer=HTMLRenderer)


def read_posts():
    "Read all Markdown files for blog posts and pre-process."
    for filepath in sorted(Path("source/posts").glob("*.md")):
        post = read_md(filepath)
        for key in ["name", "date", "categories"]:
            if key not in post:
                raise ValueError(f"post {filepath} lacks '{key}'")
        post["path"] = f"/{post['date'].replace('-','/')}/{post['name']}/"
        if post.get("redirect"):
            REDIRECTED_POSTS.append(post)
        elif post.get("draft"):
            print("Draft >>>", filepath)
        else:
            POSTS.append(post)
    POSTS.sort(key=lambda p: p["date"], reverse=True)
    POSTS[0]["prev"] = POSTS[1]
    POSTS[-1]["next"] = POSTS[-2]
    for i, post in enumerate(POSTS[1:-1], start=1):
        post["next"] = POSTS[i-1]
        post["prev"] = POSTS[i+1]
    # Remove the tag "bok".
    for post in POSTS:
        for pos, tag in enumerate(post.get("tags", [])):
            if tag["name"] == "bok":
                post["tags"].pop(pos)
                break
    # Set points to/from tags.
    for post in POSTS:
        for tag in post.get("tags", []):
            try:
                TAGS[tag["name"]]["posts"].append(post)
            except KeyError:
                tag["posts"] = [post]
                TAGS[tag["name"]] = tag
    for tag in TAGS.values():
        tag["posts"].sort(key=lambda p: p["date"], reverse=True)
    # Set pointers to/from categories.
    for post in POSTS:
        for category in post.get("categories", []):
            try:
                CATEGORIES[category["name"]]["posts"].append(post)
            except KeyError:
                category["posts"] = [post]
                CATEGORIES[category["name"]] = category
    for category in CATEGORIES.values():
        category["posts"].sort(key=lambda p: p["date"], reverse=True)
    # Convert any 'about' text'.
    for post in POSTS:
        if post.get("about"):
            post["about_html"] = MARKDOWN.convert(post["about"])


def read_pages():
    """Read all Markdown files for pages and pre-process.
    Add ancient hard-coded HTML page trees and links to other subsites."""
    for filepath in sorted(Path("source/pages").glob("*.md")):
        page = read_md(filepath)
        if page.get("redirect"):
            REDIRECTED_PAGES.append(page)
        else:
            PAGES.append(page)
    PAGES.sort(key=lambda p: (p.get("ordinal", 999), p["title"].lower()))


def read_goodreads():
    "Read the Goodreads dump CSV file and apply any corrections."
    non_subjects = set(["currently-reading", "to-read", "read", "reviewed", "svenska"])

    # Read lookup table of canonical author names.
    with open(AUTHORS_CANONICAL_FILENAME) as infile:
        reader = csv.DictReader(infile)
        for row in reader:
            AUTHORS[row["name"]] = row["canonical"]

    # Read in Goodreads CSV file.
    with open(GOODREADS_FILENAME) as infile:
        reader = csv.DictReader(infile)
        rows = list(reader)

    # Convert Goodreads CSV file row into a text item.
    for row in rows:
        book = {
            "type": "book",
            "title": row["Title"],
            "goodreads": row["Book Id"],
            "isbn": row["ISBN13"].lstrip("=").strip('"') or row["ISBN"].lstrip("=").strip('"'),
            "published": row["Original Publication Year"] or row["Year Published"],
            "edition": {"published": row["Year Published"] or row["Original Publication Year"],
                        "publisher": row["Publisher"]}
        }
        book["authors"] = [row["Author l-f"]]
        familyname = book["authors"][0].split(",")[0].strip()
        book["reference"] = f"{familyname} {book['published']}"
        for name in row["Additional Authors"].split(","):
            name = name.strip()
            if not name: continue
            parts = name.split()
            familyname = parts[-1]
            firstname = " ".join(parts[0:-1])
            book["authors"].append(f"{familyname}, {firstname}")
        subjects = [t.strip() for t in row["Bookshelves"].strip('"').split(",")]
        subjects = [t for t in subjects if t]
        if "svenska" in subjects:
            book["language"] = "sv"
        else:
            book["language"] = "en"
        book["subjects"] = sorted(set(subjects).difference(non_subjects))
        if row["My Rating"] and row["My Rating"] != "0":
            book["rating"] = int(row["My Rating"])
        date = row["Date Read"].strip()
        if not date:
            date = row["Date Added"].strip()
        book["lastmod"] = book["date"] = date.replace("/", "-")
        if row["My Review"]:
            book["content"] = row["My Review"].strip('"').replace("<br/>", "\n")
            book["html"] = MARKDOWN.convert(book["content"])
            book["reviewed"] = book["date"]

        # Read corrections file for the book, if any. Any item may be changed.
        try:
            with open(f"source/corrections/{book['goodreads']}.yaml") as infile:
                book.update(yaml.safe_load(infile))
        except IOError:
            pass

        if book["reference"] in TEXTS:
            raise ValueError(f"duplicate reference {book['reference']}, {book['goodreads']}, {book['title']}")
        TEXTS[book["reference"]] = book


def read_references():
    """Read references:
    - Additional books not in Goodreads
    - Articles and links added manually.
    """
    for filepath in Path("source/references").glob("*.yaml"):
        with open(filepath) as infile:
            text = yaml.safe_load(infile)
            if text["reference"] in TEXTS:
                raise ValueError(f"duplicate reference {text['reference']} in {{filepath}}")
        TEXTS[text["reference"]] = text


def fixup():
    "Do some additional fixups for the texts."

    # Normalize author names.
    for text in TEXTS.values():
        for pos, author in enumerate(text["authors"]):
            author = author.replace(".", "").strip().rstrip(",")
            text["authors"][pos] =  AUTHORS.get(author, author)

    # Set 'path'.
    for text in TEXTS.values():
        text["path"] = f"/library/{normalize(text['reference'])}.html"

    # Set 'year' integer value.
    for text in TEXTS.values():
        if text["published"].startswith("-"):
            dateparts = (text["published"],)
        else:
            dateparts = text["published"].split("-")
        parts = dateparts[0].split()
        try:
            if len(parts) == 1 and parts[0][0] == "-":
                text["year"] = int(parts[0])
            elif len(parts) > 1 and parts[-1].lower() in ("bc", "bce", "fkr", "f.kr."):
                text["year"] = - int(parts[0])
            else:
                text["year"] = int(dateparts[0])
        except ValueError:
            print(text["title"], text["authors"], text.get("goodreads"))
            raise

    # Set 'lastmod'.
    for text in TEXTS.values():
        if "lastmod" not in text:
            text["lastmod"] = text["published"]

    # Set up published year bins.
    for text in TEXTS.values():
        try:
            YEAR_PUBLISHED.setdefault(text["year"], list()).append(text)
        except KeyError:
            print(text["title"])
            raise


def check():
    "Check the texts."

    # Check that every book has ISBN, and that it is not duplicated.
    isbns = set()
    for text in TEXTS.values():
        if text["type"] == "book":
            if not text.get("isbn"):
                print(">>> lacking ISBN:", text.get("goodreads"), text["title"])
            if text["isbn"] in isbns:
                print(">>> duplicate isbn:", text.get("goodreads"), text["title"])
            elif text["isbn"]:
                isbns.add(text["isbn"])

    # Base JSON schema for the reference YAML files.
    base_schema = dict(
        type="object",
        properties=dict(
            title=dict(type="string"),
            reference=dict(type="string"),
            authors=dict(
                type="array",
                items=dict(type="string"),
                minItems=1,
                uniqueItems=True, # XXX regex format
            ),
            published=dict(type="string"),
            year=dict(type="integer"),
            language=dict(type="string"),
            path=dict(type="string"),
            content=dict(type="string"),
            html=dict(type="string"),
            rating=dict(type="integer"),
            date=dict(type="string"),
            reviewed=dict(type="string"),
            lastmod=dict(type="string"),
            href=dict(type="string"),
            subjects=dict(
                type="array",
                items=dict(type="string"),
                minItems=1,
                uniqueItems=True,
            ),
        ),
        required=["type", "title", "reference", "authors", "published", "year", "path",],
        additionalProperties=False,
    )

    # Validate book YAML files.
    book_schema = base_schema.copy()
    book_schema["properties"].update(dict(
        type=dict(const="book"),
        subtitle=dict(type="string"),
        goodreads=dict(type="string"),
        isbn=dict(type="string"),
        edition=dict(
            type="object",
            properties=dict(
                published=dict(type="string"),
                publisher=dict(type="string"),
            ),
            required=["published", "publisher"],
            additionalProperties=False,
        ),
    ))
    validator = jsonschema.Draft202012Validator(
        schema=book_schema,
        format_checker=jsonschema.FormatChecker(["regex"])
    )
    for book in [t for t in TEXTS.values() if t["type"] == "book"]:
        try:
            validator.validate(book)
        except jsonschema.exceptions.ValidationError as error:
            print(book["reference"], book.get("goodreads"), error)
            raise

    # Validate article YAML files.
    article_schema = base_schema.copy()
    article_schema["properties"].update(dict(
        type=dict(const="article"),
        abstract=dict(type="string"),
        book=dict(type="string"),
        journal=dict(type="string"),
        issn=dict(type="string"),
        published=dict(type="string"),
        publisher=dict(type="string"),
        eprint=dict(type="string"),
        archiveprefix=dict(type="string"),
        primaryclass=dict(type="string"),
        volume=dict(type="string"),
        issue=dict(type="string"),
        pages=dict(type="string"),
        doi=dict(type="string"),
        pmid=dict(type="string"),
        pmc=dict(type="string"),
    ))
    validator = jsonschema.Draft202012Validator(
        schema=article_schema,
        format_checker=jsonschema.FormatChecker(["regex"])
    )
    for article in [t for t in TEXTS.values() if t["type"] == "article"]:
        try:
            validator.validate(article)
        except jsonschema.exceptions.ValidationError as error:
            print(article["reference"], error)
            raise

    # Validate link YAML files.
    link_schema = base_schema.copy()
    link_schema["properties"].update(dict(
        type=dict(const="link"),
        accessed=dict(type="string"),
    ))
    link_schema["required"].append("href")
    validator = jsonschema.Draft202012Validator(
        schema=link_schema,
        format_checker=jsonschema.FormatChecker(["regex"])
    )
    for link in [t for t in TEXTS.values() if t["type"] == "link"]:
        try:
            validator.validate(link)
        except jsonschema.exceptions.ValidationError as error:
            print(link["reference"], link["href"], error)
            raise


def write_references():
    "Write the reference YAML files and their HTML files for all texts."
    template = env.get_template("library/yaml.html")
    for reference, text in TEXTS.items():
        code = yaml.dump(text, allow_unicode=True)
        norm = normalize(reference)
        with open(f"references/{norm}.yaml", "w") as outfile:
            outfile.write(code)
        filepath = f"docs/library/{norm}-yaml.html"
        with open(filepath, "w") as outfile:
            outfile.write(template.render(code=code))
        HTML_FILES.add(filepath)


def build_index():
    "Build the top index.html file."
    categories = list(CATEGORIES.values())
    categories.sort(key=lambda c: c["value"].lower())
    # Collect the latest blog posts.
    posts = POSTS[:MAX_LATEST_ITEMS]
    # Convert the first paragraph of the latest posts.
    for post in posts:
        try:
            content = post["content"][:post["content"].index("<!-- more -->")]
        except ValueError:
            content = post["content"].split("\n\n", 1)[0]
        post["short_html"] = MARKDOWN.convert(content)
    # Collect the latest book reviews.
    texts = [t for t in TEXTS.values() if t.get("date") and t.get("content")]
    texts.sort(key=lambda t: t["date"], reverse=True)
    texts = texts[:MAX_LATEST_ITEMS]
    for text in texts:
        text["short_html"] = MARKDOWN.convert(text["content"].split("\n", 1)[0])
    items = posts + texts
    items.sort(key=lambda i: i["date"], reverse=True)
    LATEST_ITEMS.extend(items[:MAX_LATEST_ITEMS])
    popular = [p for p in POSTS if p.get("popular")]
    build_html("index.html",
               sitemap=True,
               updated=time.strftime("%Y-%m-%d"),
               categories=categories,
               recent=LATEST_ITEMS,
               popular=popular)


def build_blog():
    "Build blog post files, index.html and list.html files for the blog."
    # Index of all blog posts.
    build_html("blog/index.html", posts=POSTS)
    # Index of all blog posts in English.
    en_posts = [p for p in POSTS if p.get("language") == "en"]
    build_html("blog/en/index.html", posts=en_posts)
    # All blog post pages.
    for post in POSTS:
        try:
            references = []
            for ref in post.get("references", []):
                references.append(TEXTS[ref])
        except KeyError:
            print(f"reference error '{ref}' in post '{post['title']}'")
            references = []
        build_html(f"{post['path'].strip('/')}/index.html",
                   template="blog/post.html", 
                   sitemap=True,
                   post=post,
                   language=post.get("language", "sv"),
                   description=", ".join([c["value"] for c in post.get("categories", [])]),
                   keywords=", ".join([t["value"] for t in post.get("tags", [])]),
                   author=post.get("author", "Per Kraulis"),
                   references=references,
                   lastmod=post["lastmod"])
    # Index of all tags.
    tags = sorted(TAGS.values(), key=lambda t: t["value"].lower())
    build_html("blog/tags/index.html", sitemap=True, tags=tags)
    for tag in tags:
        build_html(f"blog/tags/{tag['name']}/index.html",
                   template="blog/tags/tag.html",
                   tag=tag,
                   posts=tag["posts"])
    # Index of all categories.
    categories = sorted(CATEGORIES.values(), key=lambda c: c["value"].lower())
    build_html("blog/categories/index.html", categories=categories)
    for category in categories:
        build_html(f"blog/categories/{category['name']}/index.html",
                   template="blog/categories/category.html",
                   category=category,
                   posts=category["posts"])
    # All redirected posts.
    for post in REDIRECTED_POSTS:
        build_html(f"{post['path'].strip('/')}/index.html",
                   template="redirect.html", 
                   url=post["redirect"])


def build_pages():
    "Build page files."
    # Site pages.
    for page in PAGES:
        # External pages have no HTML, just links to them.
        if page.get("external"): continue
        path = page["path"].strip("/")
        build_html(f"{path}/index.html",
                   template="page.html", 
                   sitemap=True,
                   page=page,
                   language=page.get("language") or "sv",
                   lastmod=page.get("lastmod"))
    # Deleted, redirected pages.
    for page in REDIRECTED_PAGES:
        path = page["path"].strip("/")
        build_html(f"{path}/index.html",
                   template="redirect.html", 
                   url=page["redirect"])


def build_texts():
    "Build text files."

    # All texts sorted by reference.
    texts = list(sorted(TEXTS.values(), key=lambda t: t["reference"]))

    # Index of all texts and their pages.
    build_html("library/index.html", sitemap=True, texts=texts)

    # References in posts to texts.
    for post in POSTS:
        for reference in post.get("references", []):
            TEXTS[reference].setdefault("posts", []).append(post)
    for text in texts:
        if text["type"] == "book":
            build_html(text["path"].strip("/"),
                       template="library/book.html",
                       sitemap=True,
                       book=text,
                       language=text.get("language") or "sv",
                       lastmod=text.get("lastmod"))
        elif text["type"] == "article":
            build_html(text["path"].strip("/"),
                       template="library/article.html",
                       sitemap=True,
                       article=text,
                       language=text.get("language") or "sv",
                       lastmod=text.get("lastmod"))
        elif text["type"] == "link":
            build_html(text["path"].strip("/"),
                       template="library/link.html",
                       sitemap=True,
                       link=text,
                       language=text.get("language") or "sv",
                       lastmod=text.get("lastmod"))
        else:
            print(text["type"], text["reference"], text["title"])

    # Authors index and pages.
    authors = {}
    for text in texts:
        for author in text["authors"]:
            authors.setdefault(author, []).append(text)
    build_html("library/authors/index.html", sitemap=True, authors=authors)
    for author, author_texts in authors.items():
        build_html(f"library/authors/{author}/index.html",
                   template="library/authors/author.html",
                   sitemap=True,
                   author=author,
                   texts=author_texts)

    # Subject index and pages.
    subjects = dict()
    for text in texts:
        for subject in text.get("subjects", list()):
            subjects.setdefault(subject, []).append(text)
    subjects = list(subjects.items())
    build_html("library/subjects/index.html", sitemap=True, subjects=subjects)
    for subject, subject_texts in subjects:
        # Primary sort by published date, secondary sort by authors.
        # Since Python 'sort' is stable, this works.
        subject_texts.sort(key=lambda b: b["authors"])
        subject_texts.sort(key=lambda b: b["published"], reverse=True)
        build_html(f"library/subjects/{subject}/index.html",
                   template="library/subjects/subject.html",
                   subject=" ".join([p.capitalize() for p in subject.split("-")]),
                   texts=subject_texts)

    # List of texts referred to in posts.
    build_html("library/referred/index.html",
               template="library/referred.html",
               texts=[b for b in texts if b.get("posts")])

    # List of reviewed texts.
    build_html("library/reviewed/index.html",
               template="library/reviewed.html",
               texts=[b for b in texts if b.get("html")])

    # Lists of texts by rating.
    for rating in range(5, 0, -1):
        rated = [b for b in texts if b.get("rating") == rating]
        # Primary sort by published date, secondary sort by authors.
        # Since Python 'sort' is stable, this works.
        rated.sort(key=lambda b: b["authors"])
        rated.sort(key=lambda b: b["published"], reverse=True)
        build_html(f"library/rating/{rating}/index.html",
                   template="library/rating.html",
                   rating=rating,
                   texts=rated)

    # Lists of texts by year published.
    for year, published in sorted(YEAR_PUBLISHED.items()):
        published = sorted(published, key=lambda b: b["reference"])
        build_html(f"library/published/{year}/index.html",
                   template="library/published.html",
                   year=year,
                   texts=published)


def build_html(filepath, template=None, pages=None, sitemap=False, **kwargs):
    "Build a single HTML page from the data for an item."
    if template is None:
        template = filepath
    if pages is None:
        pages = PAGES
    template = env.get_template(template)
    # Compute canonical URL.
    canonical = BASE_URL + "/" + filepath
    try: # Remove any trailing "index.html" from canonical URL.
        canonical = canonical[:canonical.index("index.html")]
    except ValueError:
        pass
    kwargs["canonical"] = canonical
    filepath = os.path.join("docs", filepath)
    dirpath = os.path.dirname(filepath)
    if dirpath and not os.path.exists(dirpath):
        os.makedirs(dirpath)
    with open(filepath, "w") as outfile:
        outfile.write(template.render(pages=pages, **kwargs))
    HTML_FILES.add(filepath)
    if sitemap:
        SITEMAP_URLS[canonical] = kwargs.get("lastmod")

def read_md(filepath):
    "Return the Markdown file as a dict with front matter and content as items."
    with open(filepath) as infile:
        data = infile.read()
    match = FRONT_MATTER_RX.match(data)
    if match:
        try:
            result = yaml.safe_load(match.group(1)) or {}
        except yaml.scanner.ScannerError as error:
            raise IOError(f"YAML problem in {filepath}: {error}")
        except yaml.composer.ComposerError as error:
            raise IOError(f"YAML problem in {filepath}: {error}")
        except yaml.parser.ParserError as error:
            raise IOError(f"Invalid YAML in {filepath}: {error}")
        result["content"] = data[match.end():]
    else:
        result = {"content": data}
    result["html"] = MARKDOWN.convert(result["content"])
    result.get("tags", []).sort(key=lambda c: c["value"].lower())
    result.get("categories", []).sort(key=lambda c: c["value"].lower())
    result["lastmod"] = time.strftime("%Y-%m-%d", time.gmtime(os.path.getmtime(filepath)))
    return result

def write_rss():
    "Output the RSS feed file."
    template = env.get_template("feed.rss")
    with open("docs/feed.rss", "w") as outfile:
        now = datetime.datetime.now().astimezone()
        pubdate = email.utils.format_datetime(now)
        items = []
        for item in LATEST_ITEMS:
            date = datetime.datetime.fromisoformat(f"{item['date']} 12:00:00").astimezone()
            if item["type"] == "post":
                categories = [c["value"] for c in item.get("categories", [])]
                tags = [t["value"] for t in item.get("tags", [])]
            elif item["type"] == "book":
                categories = ["review"]
                tags = item["subjects"]
            else:
                raise ValueError(f"item {item['title']} has invalid type {item['type']}")
            if item["type"] == "book":
                title = f"{item['authors'][0]}: {item['title']}"
            else:
                title = item["title"]
            item = {"title": title,
                    "url": BASE_URL + item["path"],
                    "category": ", ".join(categories),
                    "description": ", ".join(tags),
                    "pubdate": email.utils.format_datetime(date),
                    "guid": sha1((item["title"] + item["date"]).encode("utf-8")).hexdigest()
            }
            items.append(item)
        outfile.write(template.render(pubdate=pubdate, items=items))

def write_sitemap():
    "Output the sitemap file."
    with open("docs/sitemap.xml", "w") as outfile:
        outfile.write('<?xml version="1.0" encoding="UTF-8"?>\n')
        outfile.write('<urlset xmlns="http://www.sitemaps.org/schemas/sitemap/0.9">\n')
        for url, lastmod in SITEMAP_URLS.items():
            outfile.write("<url>\n")
            outfile.write(f"  <loc>{url}</loc>\n")
            if lastmod:
                outfile.write(f"  <lastmod>{lastmod}</lastmod>\n")
            outfile.write("</url>\n")
        outfile.write("</urlset>\n")

def cleanup_html_files():
    "Remove any HTML files not created during this run."
    CURRENT_FILES = set()
    for dirpath, dirnames, filenames in os.walk("docs"):
        if dirpath.startswith("docs/files"): continue
        if dirpath.startswith("docs/python"): continue
        if dirpath.startswith("docs/lectures"): continue
        for filename in filenames:
            if not filename.endswith("html"): continue
            CURRENT_FILES.add(os.path.join(os.path.normpath(dirpath), filename))
    for filepath in CURRENT_FILES.difference(HTML_FILES):
        os.remove(filepath)
        print("deleted", filepath)
    # Remove directories that have become empty.
    changed = True
    while changed:
        changed = False
        for dirpath, dirnames, filenames in os.walk("docs"):
            if not (dirnames or filenames):
                shutil.rmtree(dirpath)
                print("deleted", dirpath)
                changed = True

if __name__ == "__main__":
    read_posts()
    read_pages()
    read_goodreads()
    read_references()
    fixup()
    check()
    write_references()
    build_index()
    build_blog()
    build_pages()
    build_texts()
    write_rss()
    write_sitemap()
    cleanup_html_files()
