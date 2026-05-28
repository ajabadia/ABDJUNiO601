$files = Get-ChildItem -Path "Source/UI/Sections/*.h", "Source/UI/*.h", "Source/UI/Components/*.h" -Include "*.h"

foreach ($file in $files) {
    if ($file.Name -eq "JunoUIHelpers.h" -or $file.Name -eq "JuceHeader.h") { continue }
    
    $content = Get-Content $file.FullName -Raw
    if ($content -notmatch "namespace ABD") {
        # Find the last include and insert namespace ABD
        # Or just insert after the #pragma once
        $newContent = $content -replace '(#pragma once)', "`$1`r`n`r`nnamespace ABD {`r`n"
        $newContent += "`r`n} // namespace ABD`r`n"
        $newContent | Set-Content $file.FullName -Encoding UTF8
        Write-Host "Wrapped $($file.Name) in namespace ABD"
    }
}
