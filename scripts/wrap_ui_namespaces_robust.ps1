$files = Get-ChildItem -Path "Source/UI/Sections/*.h", "Source/UI/*.h", "Source/UI/Components/*.h" -Include "*.h"

foreach ($file in $files) {
    if ($file.Name -eq "JunoUIHelpers.h" -or $file.Name -eq "JuceHeader.h") { continue }
    
    $content = Get-Content $file.FullName -Raw
    
    # Strip ALL occurrences of 'namespace ABD {' and '} // namespace ABD' that are on their own lines
    # Using a robust regex to handle potential double-wraps or misplaced wraps
    $content = $content -replace "(?m)^namespace ABD \{\s*$", ""
    $content = $content -replace "(?m)^\} // namespace ABD\s*$", ""
    
    # Remove leading/trailing whitespace after stripping
    $content = $content.Trim()
    
    # Find the position of the last #include
    $matches = [regex]::Matches($content, '#include\s+[^\r\n]+')
    if ($matches.Count -gt 0) {
        $lastMatch = $matches[$matches.Count - 1]
        $insertPoint = $lastMatch.Index + $lastMatch.Length
        
        $newContent = $content.Substring(0, $insertPoint) + "`r`n`r`nnamespace ABD {`r`n" + $content.Substring($insertPoint) + "`r`n`r`n} // namespace ABD`r`n"
        $newContent | Set-Content $file.FullName -Encoding UTF8
        Write-Host "Re-wrapped $($file.Name) correctly"
    }
}
