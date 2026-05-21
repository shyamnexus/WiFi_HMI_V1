param(
    [string]$HmiIp,
    [int]$Port = 7001,
    [double]$Interval = 1.0,
    [int]$Count = 0
)

if (-not $HmiIp) {
    Write-Host "Usage: .\\tools\\run_nodes.ps1 -HmiIp <ESP32_IP> [-Port 7001] [-Interval 1.0] [-Count 0]"
    exit 1
}

$nodes = @("NODE-01", "NODE-02", "NODE-03", "NODE-04")
foreach ($n in $nodes) {
    Start-Process -FilePath python -ArgumentList "tools/pc_node_sim.py --hmi-ip $HmiIp --port $Port --node-id $n --interval $Interval --count $Count" -WindowStyle Normal
}

Write-Host "Started $($nodes.Count) node simulators toward ${HmiIp}:$Port interval=$Interval count=$Count"
