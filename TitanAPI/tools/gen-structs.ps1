<#
  gen-structs.ps1 â€” re-reference/struct-fields.csv (+ struct-sizes.csv)  ->  op2/abi/structs.scaffold.hpp

  *** SCAFFOLD ONLY â€” not guaranteed complete or compilable. ***
  Field capture is best-effort: rare odd declarations are skipped, base-class fields are NOT inlined
  (the base is shown in a comment), and nested anonymous members are flattened. Use as a curation
  starting point; complete each struct against the live header. The commented static_assert(sizeof)
  lines (from struct-sizes.csv) are the completeness cross-check.
  Re-runnable.  Usage: powershell -File gen-structs.ps1 [-Ref <dir>] [-Out <dir>]
#>
param([string]$Ref="$PSScriptRoot\..\..\re-reference", [string]$Out="$PSScriptRoot\..\include\op2\abi")
$ErrorActionPreference='Stop'
if(-not(Test-Path $Out)){ New-Item -ItemType Directory -Path $Out -Force | Out-Null }
function San([string]$s){ $s=[regex]::Replace($s,'[^A-Za-z0-9_]','_'); if($s -eq ''){ $s='_' }; if($s -match '^[0-9]'){ $s='_'+$s }; return $s }
$map = @{ 'uint8'='std::uint8_t';'uint16'='std::uint16_t';'uint32'='std::uint32_t';'uint64'='std::uint64_t';'int8'='std::int8_t';'int16'='std::int16_t';'int32'='std::int32_t';'int64'='std::int64_t';'ibool'='int' }
function MapType([string]$t){ if($map.ContainsKey($t)){ return $map[$t] }; return $t }

$fields = Import-Csv -LiteralPath (Join-Path $Ref 'struct-fields.csv')
$sizes  = Import-Csv -LiteralPath (Join-Path $Ref 'struct-sizes.csv')
$sizeOf = @{}; foreach($s in $sizes){ if(-not $sizeOf.ContainsKey($s.struct)){ $sizeOf[$s.struct]=$s.size_dec } }

$order = New-Object System.Collections.Generic.List[string]; $seen=@{}
foreach($r in $fields){ if(-not $seen.ContainsKey($r.struct)){ $seen[$r.struct]=$true; $order.Add($r.struct) } }

$L = New-Object System.Collections.Generic.List[string]
$L.Add('// AUTO-GENERATED SCAFFOLD by tools/gen-structs.ps1 from re-reference/struct-fields.csv.')
$L.Add('// *** NOT guaranteed complete or compilable. ***  Base-class fields are NOT inlined (see // base:),')
$L.Add('// rare declarations are skipped, anonymous members are flattened. Curate against the live header;')
$L.Add('// the commented static_assert(sizeof) lines (from struct-sizes.csv) are the completeness check.')
$L.Add('#pragma once')
$L.Add('#include <cstdint>')
$L.Add('')
$L.Add('namespace op2::abi::raw {')
$L.Add('')
$L.Add('// ---- forward declarations ----')
$names = $fields | Select-Object -ExpandProperty struct -Unique | Sort-Object
foreach($n in $names){ $L.Add('struct ' + (San $n) + ';') }
$L.Add('')

$structCount=0; $fieldCount=0
foreach($st in $order){
  $fr = @($fields | Where-Object { $_.struct -eq $st } | Sort-Object { [int]$_.ordinal })
  if($fr.Count -eq 0){ continue }
  $base = $fr[0].base
  $sz=''; if($sizeOf.ContainsKey($st)){ $sz=$sizeOf[$st] }
  $hdr = 'struct ' + (San $st) + ' {'
  if($base -ne ''){ $hdr = $hdr + '  // base: ' + $base }
  if($sz -ne ''){ $hdr = $hdr + '  (size=' + $sz + ')' }
  $L.Add($hdr)
  foreach($r in $fr){
    $decl = '  ' + (MapType $r.type) + ' ' + $r.name
    if($r.array -ne ''){ $decl = $decl + $r.array }
    if($r.bits -ne ''){ $decl = $decl + ' : ' + $r.bits }
    $decl = $decl + ';'
    if($r.offset_hint -ne ''){ $decl = $decl + '  // off~' + $r.offset_hint }
    $L.Add($decl); $fieldCount++
  }
  if($sz -ne ''){ $L.Add('  // static_assert(sizeof(' + (San $st) + ') == ' + $sz + ');  // verify after completing fields') }
  $L.Add('};'); $L.Add(''); $structCount++
}
$L.Add('} // namespace op2::abi::raw')
[IO.File]::WriteAllText((Join-Path $Out 'structs.scaffold.hpp'), ($L -join "`r`n") + "`r`n", (New-Object Text.UTF8Encoding($false)))
"structs.scaffold.hpp written: $structCount structs, $fieldCount fields (SCAFFOLD -- review before use)."
