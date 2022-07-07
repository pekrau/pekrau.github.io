"Move unreferenced files to old subdir."

import os

# Read in all file names.
filenames = os.listdir("docs/files")
filenames.remove("old")

# Read in all HTML.
contents = []
for dirpath, dirnames, htmlfilenames in os.walk("docs"):
    for filename in htmlfilenames:
        if not filename.endswith(".html"): continue
        with open(os.path.join(dirpath, filename)) as infile:
            contents.append(infile.read())

for filename in filenames:
    for content in contents:
        if filename in content:
            break
    else:
        print(f"docs/files/{filename}", f"docs/files/old/{filename}")
        os.rename(f"docs/files/{filename}", f"docs/files/old/{filename}")
