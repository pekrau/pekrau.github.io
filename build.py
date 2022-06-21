"Build the website by converting MD to HTML."


__version__ = "0.0.2"


import os
import os.path
import re
import time

import jinja2
import yaml


FRONT_MATTER_RX = re.compile(r"^---(.*?)---", re.DOTALL | re.MULTILINE)

TEMPLATES_PATH = os.path.join(os.getcwd(), "templates")
POSTS_PATH = os.path.join(os.getcwd(), "source/posts")
PAGES_PATH = os.path.join(os.getcwd(), "source/pages")
FILES_PATH = os.path.join(os.getcwd(), "source/files")


# The Jinja2 template processing environment.
env = jinja2.Environment(
    loader=jinja2.FileSystemLoader(TEMPLATES_PATH),
    autoescape=jinja2.select_autoescape(['html'])
)
# env.globals['len'] = len


def build(html_filepath, template_filepath, **kwargs):
    template = env.get_template(template_filepath)
    with open(html_filepath, "w") as outfile:
        outfile.write(template.render(**kwargs))

def build_index():
    "Build the top index.html file."
    build("index.html", "index.html", updated=time.strftime("%Y-%m-%d"))

def build_blog_index():
    "Build the index.html and list.html files for the blog posts."
    dirpath = os.path.join(os.getcwd(), "blog")
    if not os.path.exists(dirpath):
        os.mkdir(dirpath)
    posts = []
    for filename in os.listdir(POSTS_PATH):
        post = read_md(os.path.join(POSTS_PATH, filename))
        posts.append(post)
    posts.sort(key=lambda p: p["date"], reverse=True)
    build("blog/index.html", "blog/index.html", posts=posts)

def read_md(filepath):
    "Read the Markdown file, as a dict with front matter and content as items."
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
    return result


if __name__ == "__main__":
    build_index()
    build_blog_index()
    
