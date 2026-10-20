// Cube de 20 centre, evide par une sphere de rayon 12 qui perce ses six faces.
difference() {
    cube(20, center = true);
    sphere(r = 12, $fn = 32);
}
