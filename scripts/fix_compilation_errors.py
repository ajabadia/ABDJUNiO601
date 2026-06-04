"""
Fix compilation errors in WebViewEditor.cpp:
1. DynamicObject::getProperty() doesn't accept 2 args (name + default)
2. Fix libName initialization order
"""
import re
import sys
import os

script_dir = os.path.dirname(os.path.abspath(__file__))
project_root = os.path.dirname(script_dir)
cpp_path = os.path.join(project_root, "Source", "UI", "WebView", "WebViewEditor.cpp")

with open(cpp_path, 'r', encoding='utf-8') as f:
    content = f.read()

# Pattern: dynObj->getProperty("name", "default")
# Fix: use a temp var + check
# We need to replace patterns like:
#   dynObj->getProperty("name", "JSON Import").toString()
# With:
#   ([](){ auto v = dynObj->getProperty("name"); return v.toString().isNotEmpty() ? v.toString() : "JSON Import"; })()

# Simpler approach: find each occurrence and fix individually

fixes = [
    # handleImportJson - library name
    (
        'auto name = dynObj->getProperty("name");\n'
        '                                        if (name.toString().isNotEmpty())\n'
        '                                            obj->setProperty("libraryName", name.toString());',
        'auto nameVal = dynObj->getProperty("name");\n'
        '                                        juce::String libNameStr = nameVal.toString().isNotEmpty() ? nameVal.toString() : "";\n'
        '                                        if (libNameStr.isNotEmpty())\n'
        '                                            obj->setProperty("libraryName", libNameStr);'
    ),
    # handleImportJson - category
    (
        'auto cat = dynObj->getProperty("category");\n'
        '                                        if (cat.toString().isNotEmpty())\n'
        '                                            obj->setProperty("category", cat.toString());',
        'auto catVal = dynObj->getProperty("category");\n'
        '                                        juce::String catStr = catVal.toString().isNotEmpty() ? catVal.toString() : "";\n'
        '                                        if (catStr.isNotEmpty())\n'
        '                                            obj->setProperty("category", catStr);'
    ),
    # handleImportJson (in handleLoad) - library name
    (
        'auto name = dynObj->getProperty("name");\n'
        '                                                if (name.toString().isNotEmpty())\n'
        '                                                    obj->setProperty("libraryName", name.toString());',
        'auto nameVal2 = dynObj->getProperty("name");\n'
        '                                                juce::String libNameStr2 = nameVal2.toString().isNotEmpty() ? nameVal2.toString() : "";\n'
        '                                                if (libNameStr2.isNotEmpty())\n'
        '                                                    obj->setProperty("libraryName", libNameStr2);'
    ),
    # handleImportJson (in handleLoad) - category
    (
        'auto cat = dynObj->getProperty("category");\n'
        '                                                if (cat.toString().isNotEmpty())\n'
        '                                                    obj->setProperty("category", cat.toString());',
        'auto catVal2 = dynObj->getProperty("category");\n'
        '                                                juce::String catStr2 = catVal2.toString().isNotEmpty() ? catVal2.toString() : "";\n'
        '                                                if (catStr2.isNotEmpty())\n'
        '                                                    obj->setProperty("category", catStr2);'
    ),
]

for old, new in fixes:
    if old in content:
        content = content.replace(old, new, 1)
        print(f"  Applied fix: {old[:60]}...")
    else:
        print(f"  NOT FOUND: {old[:60]}...")

# Now fix confirmImportFile function
# The issue is:
#   auto libName = dynObj->getProperty("name", "JSON Import").toString();
# Need to change to:
#   auto libNameVal = dynObj->getProperty("name");
#   auto libName = libNameVal.toString().isNotEmpty() ? libNameVal.toString() : "JSON Import";

old1 = 'auto libName = dynObj->getProperty("name", "JSON Import").toString();'
new1 = 'auto libNameVal = dynObj->getProperty("name");\n                                    juce::String libName = libNameVal.toString().isNotEmpty() ? libNameVal.toString() : "JSON Import";'

if old1 in content:
    content = content.replace(old1, new1, 1)
    print(f"  Applied fix: libName in confirmImportFile")
else:
    print(f"  NOT FOUND: {old1}")

# Fix preset construction in confirmImportFile:
# preset.name = pObj->getProperty("name", "Unnamed").toString();
# preset.author = pObj->getProperty("author", "").toString();
# etc.

old2 = 'preset.name = pObj->getProperty("name", "Unnamed").toString();\n'
new2 = 'preset.name = pObj->getProperty("name").toString().isNotEmpty() ? pObj->getProperty("name").toString() : "Unnamed";\n'

if old2 in content:
    content = content.replace(old2, new2, 1)
    print(f"  Applied fix: preset.name")
else:
    print(f"  NOT FOUND: preset.name")

old3 = 'preset.author = pObj->getProperty("author", "").toString();'
new3 = 'preset.author = pObj->getProperty("author").toString();'

if old3 in content:
    content = content.replace(old3, new3, 1)
    print(f"  Applied fix: preset.author")
else:
    print(f"  NOT FOUND: preset.author")

old4 = 'preset.category = pObj->getProperty("category", "User").toString();'
new4 = 'preset.category = pObj->getProperty("category").toString().isNotEmpty() ? pObj->getProperty("category").toString() : "User";'

if old4 in content:
    content = content.replace(old4, new4, 1)
    print(f"  Applied fix: preset.category")
else:
    print(f"  NOT FOUND: preset.category")

old5 = 'preset.tags = pObj->getProperty("tags", "").toString();'
new5 = 'preset.tags = pObj->getProperty("tags").toString();'

if old5 in content:
    content = content.replace(old5, new5, 1)
    print(f"  Applied fix: preset.tags")
else:
    print(f"  NOT FOUND: preset.tags")

old6 = 'preset.notes = pObj->getProperty("notes", "").toString();'
new6 = 'preset.notes = pObj->getProperty("notes").toString();'

if old6 in content:
    content = content.replace(old6, new6, 1)
    print(f"  Applied fix: preset.notes")
else:
    print(f"  NOT FOUND: preset.notes")

old7 = 'preset.isFavorite = (bool)pObj->getProperty("favorite", false);'
new7 = 'preset.isFavorite = (bool)pObj->getProperty("favorite");'

if old7 in content:
    content = content.replace(old7, new7, 1)
    print(f"  Applied fix: preset.isFavorite")
else:
    print(f"  NOT FOUND: preset.isFavorite")

# Fix libName in string construction line:
# lib.name = ... + libName;
# The error is about libName not being initialized. After our fix above, libName is a String.
# But let's check: the original had `auto libName = ...` and then used it.
# Our fix changed it to `juce::String libName = ...` which should be fine.
# Let me check line 1353 issue.

# Check for the '+' operator error with libName
old_libname_use = 'lib.name = juce::String::charToString((juce_wchar)(\\'A\\' + targetIdx)) + " - " + libName;'
# This should be fine now since libName is a juce::String

with open(cpp_path, 'w', encoding='utf-8') as f:
    f.write(content)

print("\nDone. Fixes applied.")
