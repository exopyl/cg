// Rendu CGAL de plusieurs dizaines de secondes avec OpenSCAD 2021.01.
minkowski() {
    sphere(r = 5, $fn = 48);
    cube(10);
    difference() { sphere(r = 3, $fn = 30); cube(2); }
}
