param([string]$OutputPath = (Join-Path $PSScriptRoot '..\Content\Plaguefront\scenarios\highlands.json'))
$ErrorActionPreference = 'Stop'

# Distribuye las columnas escalonadas con simetría axial de 180 grados: (q,r) se corresponde con (14-q,5-r).
$tiles = @{}
for ($q = 0; $q -lt 15; ++$q) {
    $rows = if ($q % 2 -eq 0) { 13 } else { 12 }
    for ($row = 0; $row -lt $rows; ++$row) {
        $r = $row - [int][Math]::Floor($q / 2)
        $tiles["$q,$r"] = [ordered]@{ pos = @($q, $r); terrain = 'Plain'; height = 0 }
    }
}
function Set-Tile([int]$Q, [int]$Row, [string]$Terrain, [int]$Height = 0) {
    $r = $Row - [int][Math]::Floor($Q / 2)
    $cell = $tiles["$Q,$r"]
    if ($null -eq $cell) { throw "Missing cell $Q,$Row" }
    $cell.terrain = $Terrain; $cell.height = $Height
    $opposite = $tiles["$(14-$Q),$(5-$r)"]
    if ($null -eq $opposite) { throw "Missing mirrored cell $Q,$r" }
    $opposite.terrain = $Terrain; $opposite.height = $Height
}
function Set-Building([int]$Q, [int]$Row, [string]$Building, [int]$Owner = -1) {
    $r = $Row - [int][Math]::Floor($Q / 2)
    $cell = $tiles["$Q,$r"]; $cell.building = $Building; $cell.owner = $Owner
    $opposite = $tiles["$(14-$Q),$(5-$r)"]
    $opposite.building = $Building; $opposite.owner = if ($Owner -lt 0) { -1 } else { 1-$Owner }
}

# Define tres pasos: puente norte, puente central de dos celdas y puente sur; conserva el coste de vadear el río.
for ($row = 0; $row -lt 6; ++$row) { Set-Tile 7 $row 'River' }
Set-Tile 7 0 'DeepWater'
Set-Tile 7 2 'Bridge'
Set-Tile 7 5 'Bridge'

# Refleja terreno y altura en la avenida principal y los dos flancos para conservar costes estratégicos equivalentes.
foreach ($q in 1..6) { Set-Tile $q 5 'Road' }
foreach ($q in 3..6) { Set-Tile $q 2 'Road' }
foreach ($q in 2..6) { Set-Tile $q 9 'Road' }
Set-Tile 6 6 'Road'
Set-Tile 5 6 'Road'

# La escarpadura norte ofrece posiciones elevadas y un rodeo amplio; el bosque sur permite aproximaciones laterales.
Set-Tile 2 1 'Hill' 1; Set-Tile 3 1 'Hill' 1
Set-Tile 3 2 'Hill' 2; Set-Tile 4 1 'Hill' 2
Set-Tile 4 2 'Hill' 1; Set-Tile 4 3 'Hill' 1
Set-Tile 3 8 'Hill' 1; Set-Tile 4 8 'Hill' 1
Set-Tile 2 10 'Hill' 1
Set-Tile 4 0 'Blocked' 2; Set-Tile 5 0 'Blocked' 2
Set-Tile 6 1 'Blocked' 1
foreach ($entry in @(@(2,3),@(3,3),@(3,4),@(4,4),@(2,7),@(3,7),@(4,7),@(4,10),@(5,10),@(5,8))) {
    Set-Tile $entry[0] $entry[1] 'Forest'
}
foreach ($entry in @(@(5,3),@(6,3),@(5,4),@(6,4),@(6,7),@(5,7))) {
    Set-Tile $entry[0] $entry[1] 'Swamp' -1
}
Set-Tile 5 2 'Ruins' 1; Set-Tile 6 9 'Ruins'
Set-Tile 1 10 'Ruins'

# Sitúa casas neutrales simétricas junto al camino y los pasos alternativos para equilibrar la economía inicial.
Set-Building 1 5 'Fortress' 0
Set-Building 4 5 'House'
Set-Building 5 2 'House'
Set-Building 4 9 'House'

$cells = @($tiles.Values | Sort-Object { $_.pos[0] }, { $_.pos[1] })
$spawn0 = @(@(1,5),@(1,4),@(2,4),@(0,6),@(1,6))
$spawn1 = @($spawn0 | ForEach-Object { ,@((14-$_[0]), (5-$_[1])) })
$scenario = [ordered]@{
    version = 1
    name = 'Los tres pasos'
    description = '188 celdas: tres cruces del rio, aldeas neutrales y escarpes simetricos. El vado central admite maniobras; las rutas de ruinas y bosque permiten flanquear.'
    weather_seed = 71
    columns = 15
    rows = 13
    cell_count = $cells.Count
    coordinate_system = 'axial; flat-top odd-q staggered columns (13 even / 12 odd)'
    design = [ordered]@{
        symmetry = '180-degree axial rotation (q,r) -> (14-q,5-r)'
        crossings = @('northern bridge', 'central double-width bridge', 'southern bridge')
        bridge_cells = 4
        valid_cells = 188
    }
    cells = $cells
    armies = @(@('Saber','Archer','Lancer','Rider','Caster'), @('Saber','Archer','Lancer','Rider','Caster'))
    spawns = @($spawn0, $spawn1)
}
$resolved = [IO.Path]::GetFullPath($OutputPath)
[IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($resolved)) | Out-Null
[IO.File]::WriteAllText($resolved, ($scenario | ConvertTo-Json -Depth 12), [Text.UTF8Encoding]::new($false))
Write-Output "Generated $($cells.Count) explicit cells: $resolved"
