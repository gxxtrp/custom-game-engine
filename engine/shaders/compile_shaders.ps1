# Modern HLSL 2021 -> Vulkan 1.3 SPIR-V Shader Compiler using Microsoft DXC
$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$hlslDir = Join-Path $scriptDir "hlsl"
$outDir = $scriptDir

Write-Host "[DXC] Compiling HLSL 2021 Shaders to Vulkan 1.3 SPIR-V..." -ForegroundColor Cyan

$shaders = @(
    @{ File = "Mesh.hlsl";     Stage = "vs_6_6"; Entry = "VSMain"; Output = "mesh_vert.spv";     Symbol = "MESH_VERT_SPV" },
    @{ File = "Mesh.hlsl";     Stage = "ps_6_6"; Entry = "PSMain"; Output = "mesh_frag.spv";     Symbol = "MESH_FRAG_SPV" },
    @{ File = "Shadow.hlsl";   Stage = "vs_6_6"; Entry = "VSMain"; Output = "shadow_vert.spv";   Symbol = "SHADOW_VERT_SPV" },
    @{ File = "Shadow.hlsl";   Stage = "ps_6_6"; Entry = "PSMain"; Output = "shadow_frag.spv";   Symbol = "SHADOW_FRAG_SPV" },
    @{ File = "Tonemap.hlsl";  Stage = "vs_6_6"; Entry = "VSMain"; Output = "tonemap_vert.spv";  Symbol = "TONEMAP_VERT_SPV" },
    @{ File = "Tonemap.hlsl";  Stage = "ps_6_6"; Entry = "PSMain"; Output = "tonemap_frag.spv";  Symbol = "TONEMAP_FRAG_SPV" },
    @{ File = "Triangle.hlsl"; Stage = "vs_6_6"; Entry = "VSMain"; Output = "triangle_vert.spv"; Symbol = "TRIANGLE_VERT_SPV" },
    @{ File = "Triangle.hlsl"; Stage = "ps_6_6"; Entry = "PSMain"; Output = "triangle_frag.spv"; Symbol = "TRIANGLE_FRAG_SPV" }
)

foreach ($s in $shaders) {
    $inputPath = Join-Path $hlslDir $s.File
    $outputPath = Join-Path $outDir $s.Output
    
    Write-Host "  -> Compiling $($s.File) [$($s.Entry) : $($s.Stage)] -> $($s.Output)..."
    
    & dxc.exe -spirv -T $s.Stage -E $s.Entry "-fspv-target-env=vulkan1.3" -fspv-entrypoint-name=main -HV 2021 $inputPath -Fh $outputPath -Vn $s.Symbol
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Failed to compile $($s.File)!"
    }
}

Write-Host "[DXC] All HLSL Shaders compiled successfully to Vulkan 1.3 SPIR-V headers!" -ForegroundColor Green
