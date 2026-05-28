$files = Get-ChildItem -Path "Source/Core/*.h", "Source/Core/*.cpp", "Source/Synth/*.h", "Source/Synth/*.cpp" -Include "*.h", "*.cpp"

foreach ($file in $files) {
    if ($file.Name -eq "JuceHeader.h" -or $file.Name -eq "BuildVersion.h") { continue }
    
    $content = Get-Content $file.FullName -Raw
    if ($content -notmatch "namespace ABD") {
         # Find the last include
        $matches = [regex]::Matches($content, '#include\s+[^\r\n]+')
        if ($matches.Count -gt 0) {
            $lastIndex = $matches[$matches.Count - 1].Index + $matches[$matches.Count - 1].Length
            $newContent = $content.Substring(0, $lastIndex) + "`r`n`r`nnamespace ABD {`r`n" + $content.Substring($lastIndex) + "`r`n} // namespace ABD`r`n"
            $newContent | Set-Content $file.FullName -Encoding UTF8
            Write-Host "Wrapped $($file.Name) in namespace ABD"
        }
    }
}
