import re, os

CSS_DIRS = ['Source/UI/WebUI/css', 'Source/UI/WebUI']
WEBUI_DIR = 'Source/UI/WebUI'

def extract_css_selectors(filepath):
    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()
    classes = set()
    ids = set()
    content = re.sub(r'/\*[^*]*\*+(?:[^/*][^*]*\*+)*/', '', content)
    for m in re.finditer(r'\.([a-zA-Z][\w-]*)', content):
        name = m.group(1)
        if name and not name[0].isdigit():
            classes.add(name)
    for m in re.finditer(r'#([a-zA-Z][\w-]*)', content):
        ids.add(m.group(1))
    return classes, ids

def extract_refs(filepath):
    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()
    refs = {'raw_ids': set(), 'raw_classes': set(), 'inline_ids': set(), 'inline_classes': set()}
    for m in re.finditer(r'''id\s*=\s*['"]([^'"]+)['"]''', content):
        refs['raw_ids'].add(m.group(1))
    for m in re.finditer(r'''class\s*=\s*['"]([^'"]+)['"]''', content):
        for cls in m.group(1).split():
            refs['raw_classes'].add(cls)
    for m in re.finditer(r'''getElementById\s*\(\s*['"]([^'"]+)['"]''', content):
        refs['inline_ids'].add(m.group(1))
    for m in re.finditer(r'''querySelector(?:All)?\s*\(\s*['"]#([a-zA-Z][\w-]*)''', content):
        refs['inline_ids'].add(m.group(1))
    for m in re.finditer(r'''querySelector(?:All)?\s*\(\s*['"]\.([a-zA-Z][\w-]*)''', content):
        refs['inline_classes'].add(m.group(1))
    for m in re.finditer(r'''classList\.(?:add|remove|toggle|contains)\s*\(\s*['"]([^'"]+)['"]''', content):
        refs['inline_classes'].add(m.group(1))
    for m in re.finditer(r'''className\s*=\s*['"]([^'"]+)['"]''', content):
        for cls in m.group(1).split():
            refs['inline_classes'].add(cls)
    for m in re.finditer(r'''toggle\(\s*['"]([a-zA-Z][\w-]*)['"]''', content):
        refs['inline_classes'].add(m.group(1))
    return refs

# Collect CSS
all_css_classes = set()
all_css_ids = set()
css_files = []
for d in CSS_DIRS:
    if not os.path.exists(d):
        continue
    for fname in os.listdir(d):
        if fname.endswith('.css'):
            path = os.path.join(d, fname)
            classes, ids = extract_css_selectors(path)
            all_css_classes.update(classes)
            all_css_ids.update(ids)
            css_files.append(os.path.join(d, fname))

webui_files = []
for fname in os.listdir(WEBUI_DIR):
    if fname.endswith('.html') or fname.endswith('.js'):
        webui_files.append(fname)

file_refs = {}
for fname in sorted(webui_files):
    path = os.path.join(WEBUI_DIR, fname)
    file_refs[fname] = extract_refs(path)

def is_static(name):
    if '${' in name or '+ ' in name or ' +' in name:
        return False
    if re.match(r'^[0-9a-fA-F]{3,6}$', name):
        return False
    if len(name) < 2:
        return False
    if name.isdigit():
        return False
    return True

print('=== IDs HUERFANOS (en HTML/JS pero sin CSS) ===')
print()
total_orphan_ids = 0
orphan_ids_by_file = {}
for fname in sorted(webui_files):
    refs = file_refs[fname]
    all_refs = refs['raw_ids'] | refs['inline_ids']
    orphans = set()
    for rid in sorted(all_refs):
        if is_static(rid) and rid not in all_css_ids:
            orphans.add(rid)
    if orphans:
        orphan_ids_by_file[fname] = sorted(orphans)
        total_orphan_ids += len(orphans)
        print(f'  {fname} ({len(orphans)}):')
        for o in sorted(orphans):
            print(f'    - #{o}')
        print()

print(f'Total IDs huerfanos: {total_orphan_ids}')
print()
print('=== CLASSES HUERFANAS (en HTML/JS pero sin CSS) ===')
print()
total_orphan_classes = 0
orphan_classes_by_file = {}
for fname in sorted(webui_files):
    refs = file_refs[fname]
    all_refs = refs['raw_classes'] | refs['inline_classes']
    orphans = set()
    for rc in sorted(all_refs):
        if is_static(rc) and rc not in all_css_classes:
            orphans.add(rc)
    if orphans:
        orphan_classes_by_file[fname] = sorted(orphans)
        total_orphan_classes += len(orphans)
        print(f'  {fname} ({len(orphans)}):')
        for o in sorted(orphans):
            print(f'    - .{o}')
        print()

print(f'Total Classes huerfanas: {total_orphan_classes}')
print()
print(f'=== RESUMEN ===')
print(f'Archivos CSS: {len(css_files)}')
print(f'Selectores CLASS en CSS: {len(all_css_classes)}')
print(f'Selectores ID en CSS: {len(all_css_ids)}')
print(f'IDs huerfanos: {total_orphan_ids}')
print(f'Classes huerfanas: {total_orphan_classes}')
