# Script to bundle all source code into a single timestamped text file
# Usage: .\bundle_code.ps1

$rootDir = $PWD
$timestamp = Get-Date -Format "yyyyMMddHHmm"
$outputFile = "$PWD\TOTALCODE$timestamp.txt"
$controlFile = "$PWD\CONTROL_FILESCODE$timestamp.txt"

# Extensions to include
$includeExtensions = @(".h", ".cpp", ".hpp", ".bat")
$specificFiles = @("CMakeLists.txt")

# Directories to exclude (relative to root)
$excludeDirs = @("build", "build_sandbox", ".git", ".vs", "BRAIN", "DOCS", "tmp", "out", "bin", "obj", "packages")

Write-Host "Bundling code from $rootDir to $outputFile..."
Write-Host "Logging status to $controlFile..."

# Create Writers
$mainWriter = [System.IO.StreamWriter]::new($outputFile, $false, [System.Text.Encoding]::UTF8)
$controlWriter = [System.IO.StreamWriter]::new($controlFile, $false, [System.Text.Encoding]::UTF8)

# Separator Constants
$separator = "`r`n================================================================================`r`n"
$separator2 = "================================================================================`r`n"

try {
    # Recursively find files in Source, plus specific root files
    $sourceFiles = Get-ChildItem -Path "$rootDir\Source" -Recurse 
    $rootFiles = Get-ChildItem -Path $rootDir -Depth 0
    
    $sourceFiles + $rootFiles | Where-Object { 
        $item = $_
        
        # 1. Skip Directories themselves (we want files)
        if ($item.PSIsContainer) { return $false }

        # 2. Check if file is in an excluded directory
        $relativePath = Resolve-Path -Path $item.FullName -Relative
        foreach ($ex in $excludeDirs) {
            if ($relativePath -like "*\$ex\*") { return $false }
            if ($relativePath -like ".\$ex\*") { return $false }
        }

        # 3. Exclude the output files themselves
        if ($item.Name -like "TOTALCODE*") { return $false }
        if ($item.Name -like "CONTROL_FILES*") { return $false }
        if ($item.Name -like "*.log") { return $false }

        # 4. Check Extension OR Specific Filename
        if ($includeExtensions -contains $item.Extension) { return $true }
        if ($specificFiles -contains $item.Name) { return $true }

        return $false
    } | ForEach-Object {
        $filePath = $_.FullName
        $relativePath = Resolve-Path -Path $filePath -Relative
        $status = "KO" # Default
        
        try {
            # Read content
            $content = Get-Content -Path $filePath -Raw
            
            if ([string]::IsNullOrWhiteSpace($content)) {
                $status = "EMPTY"
                $controlWriter.WriteLine("$relativePath : $status")
            }
            else {
                # Append to Bundle
                $header = "FILE: $relativePath`r`n"
                
                $mainWriter.Write($separator)
                $mainWriter.Write($header)
                $mainWriter.Write($separator2)
                $mainWriter.Write($content)
                
                $status = "OK"
                $controlWriter.WriteLine("$relativePath : $status")
                
                Write-Host "Added: $relativePath"
            }
        }
        catch {
            $status = "ERROR: $_"
            $controlWriter.WriteLine("${relativePath} : $status")
            Write-Error "Failed to process ${relativePath}: $_"
        }
    }
}
finally {
    # Close writers explicitly
    $mainWriter.Close()
    $mainWriter.Dispose()
    
    $controlWriter.Close()
    $controlWriter.Dispose()
}

Write-Host "Done! Code bundle: $outputFile"
