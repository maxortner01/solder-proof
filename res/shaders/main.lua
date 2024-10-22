VK_FORMAT_B8G8R8A8_UNORM = 44
VK_FORMAT_D32_SFLOAT_S8_UINT = 130

Polygon = {
    Fill = 0,
    Wireframe = 1
}

Pipeline = {
    formats = { 37, 97, 97 },
    colorFormat = VK_FORMAT_B8G8R8A8_UNORM,
    depthFormat = VK_FORMAT_D32_SFLOAT_S8_UINT,
    depthTesting    = true,
    backfaceCulling = true,
    polygon = Polygon.Fill
}