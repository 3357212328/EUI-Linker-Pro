Add-Type -AssemblyName System.Drawing

$srcPath = 'C:\Users\33572\.gemini\antigravity\brain\872ac328-a9d7-4e52-9a11-29533b02082a\app_icon_source_1779369561096.png'
$dstPath = 'C:\Users\33572\Desktop\MyModernUI\app.ico'

$sizes = @(16, 24, 32, 48, 64, 128, 256)
$count = $sizes.Count

$ms = New-Object System.IO.MemoryStream

# ICO file header: reserved(2) + type=1(2) + count(2)
$header = [byte[]](0,0, 1,0, [byte]$count, 0)
$ms.Write($header, 0, $header.Length)

$dirOffset = 6 + $count * 16
$imageDataList = [System.Collections.ArrayList]::new()

foreach ($size in $sizes) {
    $src = [System.Drawing.Image]::FromFile($srcPath)
    $bmp = New-Object System.Drawing.Bitmap($size, $size)
    $g   = [System.Drawing.Graphics]::FromImage($bmp)
    $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $g.SmoothingMode     = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $g.DrawImage($src, 0, 0, $size, $size)
    $g.Dispose()
    $src.Dispose()

    $imgStream = New-Object System.IO.MemoryStream
    $bmp.Save($imgStream, [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
    [void]$imageDataList.Add($imgStream.ToArray())
    $imgStream.Dispose()
}

# Write directory (placeholder positions first, then seek-and-fill is not possible with MemoryStream forward-only)
# So build the header block first, then append image data
$dirBlock = New-Object System.IO.MemoryStream
$currentOffset = $dirOffset

for ($i = 0; $i -lt $count; $i++) {
    $sz   = $sizes[$i]
    $data = $imageDataList[$i]
    $szB  = if ($sz -eq 256) { [byte]0 } else { [byte]$sz }

    # ICONDIRENTRY: width(1) height(1) colorCount(1) reserved(1) planes(2) bitCount(2)
    $entry = [byte[]]($szB, $szB, 0, 0, 1, 0, 32, 0)
    $dirBlock.Write($entry, 0, 8)

    $dataLen = [System.BitConverter]::GetBytes([uint32]$data.Length)
    $dataOff = [System.BitConverter]::GetBytes([uint32]$currentOffset)
    $dirBlock.Write($dataLen, 0, 4)
    $dirBlock.Write($dataOff, 0, 4)

    $currentOffset += $data.Length
}

# Now write everything into the final stream
$dirBytes = $dirBlock.ToArray()
$ms.Write($dirBytes, 0, $dirBytes.Length)

foreach ($data in $imageDataList) {
    $ms.Write($data, 0, $data.Length)
}

[System.IO.File]::WriteAllBytes($dstPath, $ms.ToArray())
$ms.Dispose()

$size = (Get-Item $dstPath).Length
Write-Host "ICO created successfully: $dstPath ($size bytes)"
