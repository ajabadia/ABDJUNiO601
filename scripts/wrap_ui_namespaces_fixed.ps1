$files = Get-ChildItem -Path "Source/UI/Sections/*.h", "Source/UI/*.h", "Source/UI/Components/*.h" -Include "*.h"

foreach ($file in $files) {
    if ($file.Name -eq "JunoUIHelpers.h" -or $file.Name -eq "JuceHeader.h") { continue }
    
    $content = Get-Content $file.FullName -Raw
    
    # Remove existing ABD namespace wrapping if it exists (the broken version)
    $content = $content -replace "namespace ABD \{`r`n`r`n", ""
    $content = $content -replace "`r`n\} // namespace ABD", ""
    
    # Find the position of the last #include
    $matches = [regex]::Matches($content, '#include\s+[^\r\n]+')
    if ($matches.Count -gt 0) {
        $lastMatch = $matches[$matches.Count - 1]
        $insertPoint = $lastMatch.Index + $lastMatch.Length
        
        $newContent = $content.Substring(0, $insertPoint) + "`r`n`r`nnamespace ABD {`r`n" + $content.Substring($insertPoint) + "`r`n} // namespace ABD`r`n"
        $newContent | Set-Content $file.FullName -Encoding UTF8
        Write-Host "Corrected $($file.Name) namespace wrapping"
    } else {
        # If no includes, insert after #pragma once
        if ($content -match "#pragma once") {
             $newContent = $content -replace '(#pragma once)', "`$1`r`n`r`nnamespace ABD {`r`n"
             $newContent += "`r`n} // namespace ABD"
             $newContent | Set-Content $file.FullName -Encoding UTF8
        }
    }
}
