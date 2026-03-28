@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
cl /EHsc /c /I "d:\desarrollos\ABDJUNiO601\Source" /I "d:\desarrollos\ABDJUNiO601\build\ABDSimpleJuno106_artefacts\JuceLibraryCode" /I "d:\desarrollos\ABDJUNiO601\build\juce_binarydata_ABDSimpleJuno106_WebUI\JuceLibraryCode" "d:\desarrollos\ABDJUNiO601\Source\UI\WebView\WebViewEditor.cpp" > debug_error.txt 2>&1
