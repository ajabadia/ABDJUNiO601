param(
    [string[]]$Exclude = @(),   # carpetas/rutas adicionales a excluir (relativas al root)
    [switch]$Compact            # modo compacto
)

# Script to bundle all source code into a JSONL file + metadata
# Usage examples:
#   .\bundle_code_jsonl.ps1
#   .\bundle_code_jsonl.ps1 -Exclude "tests","playground"
#   .\bundle_code_jsonl.ps1 -Compact
#   .\bundle_code_jsonl.ps1 -Exclude "tests","src/legacy" -Compact

# Determine root directory based on script location (now in scripts/)
$scriptDir = $PSScriptRoot
$rootDir = (Get-Item $scriptDir).Parent.FullName

$timestamp = Get-Date -Format "yyyyMMddHHmm"

# Ensure exports directory exists
$exportsDir = Join-Path $rootDir "exports"
if (-not (Test-Path $exportsDir)) {
    New-Item -ItemType Directory -Path $exportsDir | Out-Null
}

$jsonlFile   = Join-Path $exportsDir "project_bundle_$timestamp.jsonl"
$metaFile    = Join-Path $exportsDir "project_metadata_$timestamp.json"
$controlFile = Join-Path $exportsDir "CONTROL_FILESCODE_$timestamp.txt"

# Extensions to include
$includeExtensions = @(".h", ".cpp", ".c", ".hpp", ".js", ".css", ".json", ".md", ".ps1", ".bat", ".yml", ".yaml", ".html")
$specificFiles = @(
    "CMakeLists.txt",
    "build_auto.bat",
    "build_no.txt",
    "README.md",
    ".gitignore"
)

# Folders to explicitly INCLUDE (relative to root)
$foldersToProcess = @("Source", "WebUI", "scripts")

# Priority Files to verify presence (Architecture / Bridge)
$priorityFiles = @(
    "Source\UI\WebView\WebViewEditor.cpp",
    "Source\UI\WebView\WebViewEditor.h",
    "Source\Core\PresetManager.cpp",
    "Source\UI\WebUI\browser.js",
    "Source\UI\WebUI\script.js",
    "Source\UI\Sections\JunoBankSection.cpp"
)

# Directories to exclude (always ignore these)
$excludeDirsBase = @(
    "node_modules", ".next", ".git", ".vscode", "tmp", "out", "bin", "obj",
    "public", ".swc", "coverage", "test-results", "secretos", ".turbo",
    ".vercel", "dist", "build", ".contentlayer", ".pnpm-store",
    ".pnpm-debug", ".idea", "temp_migration_data", "JUCE", "logs", "docs"
)

# Combine base excludes with user-provided ones (normalized as paths starting from root)
$excludeDirs = New-Object System.Collections.Generic.List[string]
$excludeDirs.AddRange([string[]]$excludeDirsBase)

foreach ($ex in $Exclude) {
    if ([string]::IsNullOrWhiteSpace($ex)) { continue }
    # Guardamos tal cual (nombre carpeta o ruta relativa)
    $excludeDirs.Add($ex.Trim())
}

# Files considered "heavy" for Compact mode (by name)
$heavyFilesByName = @(
    "pnpm-lock.yaml",
    "package-lock.json",
    "yarn.lock"
)

# Size threshold in bytes for "heavy" files in Compact mode (5 MB)
$compactSizeThresholdBytes = 5MB

Write-Host "Bundling code from $rootDir to $jsonlFile..."
Write-Host "Metadata file: $metaFile"
Write-Host "Control file:  $controlFile"
Write-Host "Compact mode:  $Compact"
if ($Exclude.Count -gt 0) {
    Write-Host "Additional excludes: $($Exclude -join ', ')"
}

# Helpers
function Get-LanguageFromExtension {
    param(
        [string]$Extension
    )

    switch ($Extension.ToLower()) {
        ".h"    { "cpp" }
        ".hpp"  { "cpp" }
        ".cpp"  { "cpp" }
        ".c"    { "c" }
        ".html" { "html" }
        ".js"   { "javascript" }
        ".css"  { "css" }
        ".json" { "json" }
        ".md"   { "markdown" }
        ".ps1"  { "powershell" }
        ".bat"  { "batch" }
        ".yml"  { "yaml" }
        ".yaml" { "yaml" }
        default { "text" }
    }
}

function Is-InExcludedDir {
    param(
        [string]$RelativePath,
        [System.Collections.Generic.List[string]]$ExcludeList
    )

    foreach ($ex in $ExcludeList) {
        # Normalizamos separadores y comparamos de forma sencilla
        $exNorm = $ex -replace '/', '\'
        if ($RelativePath -like "*\$exNorm\*") { return $true }

        # También contempla casos como ".\ex\..." o "ex\..."
        if ($RelativePath -like ".\$exNorm\*") { return $true }
        if ($RelativePath -like "$exNorm\*") { return $true }
    }

    return $false
}

function Get-ProjectName {
    param(
        [string]$RootDir
    )

    $pkgPath = Join-Path $RootDir "package.json"
    if (Test-Path $pkgPath) {
        try {
            $pkgJson = Get-Content $pkgPath -Raw | ConvertFrom-Json
            if ($pkgJson.name) {
                return $pkgJson.name
            }
        } catch {
            # ignoramos errores y caemos al nombre de carpeta
        }
    }

    return (Split-Path $RootDir -Leaf)
}

function Get-GitInfo {
    param(
        [string]$RootDir
    )

    $gitInfo = [PSCustomObject]@{
        branch = $null
        commit = $null
        dirty  = $null
    }

    try {
        $gitDir = Join-Path $RootDir ".git"
        if (-not (Test-Path $gitDir)) {
            return $gitInfo
        }

        pushd $RootDir | Out-Null

        try {
            $branch = git rev-parse --abbrev-ref HEAD 2>$null
            $commit = git rev-parse HEAD 2>$null
            $status = git status --porcelain 2>$null

            if ($branch) { $gitInfo.branch = $branch.Trim() }
            if ($commit) { $gitInfo.commit = $commit.Trim() }
            if ($status -ne $null) {
                $gitInfo.dirty = -not [string]::IsNullOrWhiteSpace(($status -join "`n"))
            }
        } finally {
            popd | Out-Null
        }
    } catch {
        # Si git falla, dejamos null
    }

    return $gitInfo
}

# Create Writers
$jsonlWriter   = [System.IO.StreamWriter]::new($jsonlFile, $false, [System.Text.Encoding]::UTF8)
$controlWriter = [System.IO.StreamWriter]::new($controlFile, $false, [System.Text.Encoding]::UTF8)

# Write Summary of Key Files at top for easy review
$controlWriter.WriteLine("================================================================================")
$controlWriter.WriteLine("SUMMARY OF KEY FILES (Architecture / Bridge)")
$controlWriter.WriteLine("================================================================================")
foreach($pf in $priorityFiles) {
    if (Test-Path (Join-Path $rootDir $pf)) {
        $controlWriter.WriteLine("$pf : OK (Architecture)")
    } else {
        $controlWriter.WriteLine("$pf : MISSING (CRITICAL CHECK)")
    }
}
$controlWriter.WriteLine("================================================================================")
$controlWriter.WriteLine("")

# Global stats
$totalFiles       = 0
$totalBytes       = 0
$includedFiles    = 0
$includedBytes    = 0
$skippedCompact   = 0
$skippedExcluded  = 0
$skippedEmpty     = 0
$skippedErrors    = 0

try {
    # 1. Get files from explicitly included folders
    $allFiles = @()
    foreach ($folder in $foldersToProcess) {
        $folderPath = Join-Path $rootDir $folder
        if (Test-Path $folderPath) {
            $allFiles += Get-ChildItem -Path $folderPath -Recurse -File
        }
    }

    # 2. Get files from root (solo nivel root)
    $rootFiles = Get-ChildItem -Path $rootDir -Depth 0 -File
    $allFiles += $rootFiles

    # Distinct por ruta completa por si acaso
    $allFiles = $allFiles | Sort-Object FullName -Unique

    foreach ($item in $allFiles) {
        $filePath     = $item.FullName
        $relativePath = Resolve-Path -Path $filePath -Relative

        $totalFiles++
        $totalBytes += $item.Length

        # A. Skip by excluded directories
        if (Is-InExcludedDir -RelativePath $relativePath -ExcludeList $excludeDirs) {
            $skippedExcluded++
            $controlWriter.WriteLine("$relativePath : SKIPPED_EXCLUDED_DIR")
            continue
        }

        # B. Exclude the output files themselves and junk
        if ($item.Name -like "project_bundle_*") {
            $skippedExcluded++
            $controlWriter.WriteLine("$relativePath : SKIPPED_OUTPUT")
            continue
        }
        if ($item.Name -like "CONTROL_FILESCODE_*") {
            $skippedExcluded++
            $controlWriter.WriteLine("$relativePath : SKIPPED_OUTPUT")
            continue
        }
        if ($item.Name -like "*.log") {
            $skippedExcluded++
            $controlWriter.WriteLine("$relativePath : SKIPPED_LOG")
            continue
        }
        if ($item.Name -like "*.tmp") {
            $skippedExcluded++
            $controlWriter.WriteLine("$relativePath : SKIPPED_TMP")
            continue
        }
        if ($item.Name -like "*.bak") {
            $skippedExcluded++
            $controlWriter.WriteLine("$relativePath : SKIPPED_BAK")
            continue
        }
        if ($item.Name -eq ".DS_Store") {
            $skippedExcluded++
            $controlWriter.WriteLine("$relativePath : SKIPPED_DS_STORE")
            continue
        }
        if ($item.Name -eq "Thumbs.db") {
            $skippedExcluded++
            $controlWriter.WriteLine("$relativePath : SKIPPED_THUMBS_DB")
            continue
        }
        if ($item.Name -like ".env*" -and $item.Name -ne ".env.example") {
            $skippedExcluded++
            $controlWriter.WriteLine("$relativePath : SKIPPED_ENV")
            continue
        }

        # C. Check extension OR specific filename
        $includeByExt  = $includeExtensions -contains $item.Extension.ToLower()
        $includeByName = $specificFiles -contains $item.Name

        if (-not ($includeByExt -or $includeByName)) {
            $skippedExcluded++
            $controlWriter.WriteLine("$relativePath : SKIPPED_NOT_MATCHING_FILTER")
            continue
        }

        # D. Compact mode: check heavy files
        if ($Compact) {
            $isHeavyByName = $heavyFilesByName -contains $item.Name
            $isHeavyBySize = $item.Length -gt $compactSizeThresholdBytes

            if ($isHeavyByName -or $isHeavyBySize) {
                $skippedCompact++
                $controlWriter.WriteLine("$relativePath : SKIPPED_COMPACT")
                continue
            }
        }

        # E. Read content and write JSONL
        try {
            $content = [System.IO.File]::ReadAllText($filePath)

            if ([string]::IsNullOrWhiteSpace($content)) {
                $skippedEmpty++
                $controlWriter.WriteLine("$relativePath : EMPTY")
                continue
            }

            $language = Get-LanguageFromExtension $item.Extension

            $obj = [PSCustomObject]@{
                type     = "file"
                path     = $relativePath
                language = $language
                size     = $item.Length
                content  = $content
            }

            $json = $obj | ConvertTo-Json -Depth 10 -Compress
            $jsonlWriter.WriteLine($json)

            $includedFiles++
            $includedBytes += $item.Length

            $controlWriter.WriteLine("$relativePath : OK")
            Write-Host "Added: $relativePath"
        }
        catch {
            $skippedErrors++
            $controlWriter.WriteLine("${relativePath} : ERROR $_")
            Write-Error "Failed to process ${relativePath}: $($_)"

        }
    }

    # ---- Global metadata ----
    $projectName = Get-ProjectName -RootDir $rootDir
    $gitInfo     = Get-GitInfo -RootDir $rootDir

    $meta = [PSCustomObject]@{
        projectName = $projectName
        rootDir     = $rootDir
        generatedAt = (Get-Date).ToString("o")
        compactMode = [bool]$Compact
        excludes    = $Exclude
        git         = $gitInfo
        stats       = [PSCustomObject]@{
            totalFiles      = $totalFiles
            totalBytes      = $totalBytes
            includedFiles   = $includedFiles
            includedBytes   = $includedBytes
            skippedExcluded = $skippedExcluded
            skippedCompact  = $skippedCompact
            skippedEmpty    = $skippedEmpty
            skippedErrors   = $skippedErrors
        }
        manifest    = $priorityFiles
    }

    # 1) Escribir metadata como primer registro del JSONL
    $metaJson = $meta | ConvertTo-Json -Depth 10 -Compress
    # Para asegurarnos de que va al principio, lo escribimos al inicio del archivo.
    # Como ya hemos estado escribiendo, necesitamos regenerar el archivo:
    # Estrategia sencilla: cerrar writer, leer el contenido que acabamos de escribir, reabrir y reordenar.

} finally {
    $jsonlWriter.Close()
    $jsonlWriter.Dispose()

    $controlWriter.Close()
    $controlWriter.Dispose()
}

# Reabrimos JSONL para inyectar la línea meta al principio
# (más simple que intentar "seek" hacia el inicio en StreamWriter).
$existingLines = @()
if (Test-Path $jsonlFile) {
    $existingLines = Get-Content $jsonlFile
}

$metaJson = $meta | ConvertTo-Json -Depth 10 -Compress

# Sobrescribimos el archivo con meta primero y luego el resto
$metaJson | Out-File -FilePath $jsonlFile -Encoding UTF8
if ($existingLines.Count -gt 0) {
    $existingLines | Out-File -FilePath $jsonlFile -Encoding UTF8 -Append
}

# Escribimos metadata en JSON separado
$meta | ConvertTo-Json -Depth 10 | Out-File -FilePath $metaFile -Encoding UTF8

Write-Host "Done!"
Write-Host "JSONL bundle: $jsonlFile"
Write-Host "Metadata:      $metaFile"
Write-Host "Control file:  $controlFile"
