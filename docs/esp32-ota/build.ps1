Param(
  [string]$DocsDir = "$PSScriptRoot",
  [string]$OutDir = "$PSScriptRoot"
)

Write-Host "Generating diagrams and PDF..."

# Generate PNGs from Mermaid if mermaid-cli is available
function Test-Command($cmd) {
  $null -ne (Get-Command $cmd -ErrorAction SilentlyContinue)
}

$mermaidOk = Test-Command mmdc
if ($mermaidOk) {
  Write-Host "Found mermaid-cli (mmdc). Exporting PNGs..."
  mmdc -i "$DocsDir\sequence_web_upload.mmd" -o "$OutDir\sequence_web_upload.png" -t default
  mmdc -i "$DocsDir\sequence_backend_push.mmd" -o "$OutDir\sequence_backend_push.png" -t default
  mmdc -i "$DocsDir\flowchart.mmd" -o "$OutDir\flowchart.png" -t default
  mmdc -i "$DocsDir\state_diagram.mmd" -o "$OutDir\state_diagram.png" -t default
} else {
  Write-Warning "mermaid-cli (mmdc) not found. Install via npm: npm i -g @mermaid-js/mermaid-cli"
}

# Export architecture.drawio to PNG if draw.io CLI is available (draw.io supports --export)
$drawioOk = Test-Command drawio
if ($drawioOk) {
  Write-Host "Found draw.io CLI. Exporting architecture PNG..."
  drawio --export "$DocsDir\architecture.drawio" --output "$OutDir\architecture.png"
} else {
  Write-Warning "draw.io CLI not found. Install draw.io desktop and use File > Export, or use CLI if available."
}

# Populate VCS metadata if git is available
$commit = "<unknown>"
$branch = "<unknown>"
$date = (Get-Date).ToString("yyyy-MM-dd")
if (Test-Command git) {
  try { $commit = (git rev-parse HEAD).Trim() } catch {}
  try { $branch = (git rev-parse --abbrev-ref HEAD).Trim() } catch {}
}

# Generate PDF via pandoc if available
if (Test-Command pandoc) {
  Write-Host "Found pandoc. Generating PDF..."
  $md = "$DocsDir\ESP32_OTA_Documentation.md"
  $tmp = Join-Path $OutDir "ESP32_OTA_Documentation_with_meta.md"
  $content = Get-Content $md
  $meta = @(
    "",
    "---",
    "title: ESP32 OTA (ESP_32_OTA) Mechanism",
    "version: '1.0'",
    "commit: '$commit'",
    "branch: '$branch'",
    "date: '$date'",
    "---",
    ""
  )
  $meta + $content | Set-Content -Path $tmp -Encoding UTF8
  pandoc "$tmp" -o "$OutDir\ESP32_OTA_Documentation.pdf" --from gfm --pdf-engine wkhtmltopdf -V margin=1in
  Write-Host "PDF generated at $OutDir\ESP32_OTA_Documentation.pdf"
} else {
  Write-Warning "pandoc not found. Install pandoc and a PDF engine (wkhtmltopdf) to generate the PDF."
}

Write-Host "Done."

