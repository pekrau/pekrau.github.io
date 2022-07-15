import os

for filename in os.listdir():
    if not filename.endswith(".md"): continue
    print(filename)
    with open(filename) as infile:
        lines = infile.readlines()
    with open(f"new/{filename}", "w") as outfile:
        in_front_matter = False
        for line in lines:
            if line == "---\n":
                in_front_matter = not in_front_matter
            if in_front_matter and line.startswith("path:"): continue
            if in_front_matter and line.startswith("link:"): continue
            outfile.write(line)

