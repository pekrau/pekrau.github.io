"Build the website by converting MD to HTML."


__version__ = "0.2.0"


import os
import os.path
import re
import time

import jinja2
import marko
import yaml


FRONT_MATTER_RX = re.compile(r"^---(.*?)---", re.DOTALL | re.MULTILINE)

TEMPLATES_PATH = os.path.join(os.getcwd(), "templates")
POSTS_PATH = os.path.join(os.getcwd(), "source/posts")
PAGES_PATH = os.path.join(os.getcwd(), "source/pages")
FILES_PATH = os.path.join(os.getcwd(), "source/files")

POSTS = []                # sorted by date
PAGES = []
TAGS = {}                 # key: tag name; value: dict(name, value, posts)
CATEGORIES = {}           # key: category name; value: dict(name, value, posts)


# The Jinja2 template processing environment.
env = jinja2.Environment(
    loader=jinja2.FileSystemLoader(TEMPLATES_PATH),
    autoescape=jinja2.select_autoescape(['html'])
)
env.globals["len"] = len


class HTMLRenderer(marko.html_renderer.HTMLRenderer):
    "Modify output for Bootstrap and other changes."

    def render_quote(self, element):
        "Add blockquote output class for Bootstrap."
        return '<blockquote class="blockquote ms-4">\n{}</blockquote>\n'.format(
            self.render_children(element)
        )

def get_markdown_converter():
    "Get a converter instance for Markdown to HTML."
    return marko.Markdown(renderer=HTMLRenderer)

def read_posts():
    "Read all Markdown files for blog posts and pre-process."
    converter = get_markdown_converter()
    for filename in os.listdir(POSTS_PATH):
        post = read_md(os.path.join(POSTS_PATH, filename))
        post["html"] = converter(post["content"])
        POSTS.append(post)
    POSTS.sort(key=lambda p: p["date"], reverse=True)
    POSTS[0]["next"] = POSTS[1]
    POSTS[-1]["prev"] = POSTS[-2]
    for i, post in enumerate(POSTS[1:-1], start=1):
        post["prev"] = POSTS[i-1]
        post["next"] = POSTS[i+1]
    for post in POSTS:
        for tag in post.get("tags", []):
            try:
                TAGS[tag["name"]]["posts"].append(post)
            except KeyError:
                tag["posts"] = [post]
                TAGS[tag["name"]] = tag
    for tag in TAGS.values():
        tag["posts"].sort(key=lambda p: p["date"], reverse=True)
    for post in POSTS:
        for category in post.get("categories", []):
            try:
                CATEGORIES[category["name"]]["posts"].append(post)
            except KeyError:
                category["posts"] = [post]
                CATEGORIES[category["name"]] = category
    for category in CATEGORIES.values():
        category["posts"].sort(key=lambda p: p["date"], reverse=True)

def build_index():
    "Build the top index.html file."
    categories = list(CATEGORIES.values())
    categories.sort(key=lambda c: c["value"].lower())
    posts = POSTS[:3]
    converter = get_markdown_converter()
    for post in posts:
        post["short_html"] = converter.convert("\n\n".join(post["content"].split("\n\n")[:2]))
    build_html("index.html",
               updated=time.strftime("%Y-%m-%d"),
               categories=categories,
               posts=posts)

def build_blog():
    "Build blog post files, index.html and list.html files for the blog."
    build_html("blog/index.html", "blog/index.html", posts=POSTS)
    en_posts = [p for p in POSTS if p.get("language") == "en"]
    build_html("blog/en/index.html", "blog/en/index.html", posts=en_posts)
    for post in POSTS:
        build_html(os.path.join(post["path"].strip("/"), "index.html"),
                   template_filepath="blog/post.html", 
                   post=post,
                   language=post.get("language", "sv"))
    tags = sorted(TAGS.values(), key=lambda t: t["name"])
    build_html("blog/tags/index.html", tags=tags)
    for tag in tags:
        build_html(os.path.join("blog/tags", tag["name"], "index.html"),
                   template_filepath="blog/tags/tag.html",
                   tag=tag,
                   posts=tag["posts"])

def build_html(html_filepath, template_filepath=None, **kwargs):
    "Build a single HTML page from the data for an item."
    if template_filepath is None:
        template_filepath = html_filepath
    template = env.get_template(template_filepath)
    dirpath = os.path.dirname(html_filepath)
    if dirpath and not os.path.exists(dirpath):
        os.makedirs(dirpath)
    with open(html_filepath, "w") as outfile:
        outfile.write(template.render(**kwargs))

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
    result.get("tags", []).sort(key=lambda c: c["value"].lower())
    result.get("categories", []).sort(key=lambda c: c["value"].lower())
    return result


if __name__ == "__main__":
    read_posts()
    build_index()
    build_blog()
    
