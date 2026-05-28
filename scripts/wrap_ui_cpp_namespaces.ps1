$files = Get-ChildItem -Path "Source/UI/Sections/*.cpp", "Source/UI/*.cpp", "Source/UI/Components/*.cpp" -Include "*.cpp"

foreach ($file in $files) {
    $content = Get-Content $file.FullName -Raw
    if ($content -notmatch "namespace ABD") {
        # Insert after the last include
        $matches = [regex]::Matches($content, '#include\s+[^\r\n]+')
        if ($matches.Count -gt 0) {
            $lastIndex = $matches[$matches.Count - 1].Index + $matches[$matches.Count - 1].Length
            $newContent = $content.Substring(0, $lastIndex) + "`r`n`r`nnamespace ABD {`r`n" + $content.Substring($lastIndex) + "`r`n} // namespace ABD`r`n"
            $newContent | Set-Content $file.FullName -Encoding UTF8
            Write-Host "Wrapped $($file.Name) in namespace ABD"
        }
    }
}
