$paths = @("Source/Synth", "Source/Core", "Source/UI", "Source/UI/Sections", "Source/UI/Components")

foreach ($path in $paths) {
    if (Test-Path $path) {
        $files = Get-ChildItem -Path $path -File -Include "*.h", "*.cpp"
        foreach ($file in $files) {
            if ($file.Name -eq "JuceHeader.h" -or $file.Name -eq "BuildVersion.h") { continue }
            
            $content = Get-Content $file.FullName -Raw
            if ($content -notmatch "namespace ABD") {
                # Find the last include
                $matches = [regex]::Matches($content, '#include\s+[^\r\n]+')
                if ($matches.Count -gt 0) {
                    $lastMatch = $matches[$matches.Count - 1]
                    $insertPoint = $lastMatch.Index + $lastMatch.Length
                    
                    # For .h files, also add a closing brace at the end
                    if ($file.Extension -eq ".h") {
                         $newContent = $content.Substring(0, $insertPoint) + "`r`n`r`nnamespace ABD {`r`n" + $content.Substring($insertPoint) + "`r`n} // namespace ABD`r`n"
                    } else {
                         # For .cpp files, wrap the whole body after includes
                         $newContent = $content.Substring(0, $insertPoint) + "`r`n`r`nnamespace ABD {`r`n" + $content.Substring($insertPoint) + "`r`n} // namespace ABD`r`n"
                    }
                    $newContent | Set-Content $file.FullName -Encoding UTF8
                    Write-Host "Wrapped $($file.Name) in namespace ABD"
                }
            }
        }
    }
}
