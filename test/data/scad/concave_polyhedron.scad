// Prisme en L donne face par face : polyhedron() est ecrit tel quel, sans CGAL,
// donc avec deux faces CONCAVES a six sommets. Aire du profil 64, volume 128.
polyhedron(
    points = [[10, 4, 0], [4, 4, 0], [4, 10, 0], [0, 10, 0], [0, 0, 0], [10, 0, 0],
              [10, 4, 2], [4, 4, 2], [4, 10, 2], [0, 10, 2], [0, 0, 2], [10, 0, 2]],
    faces  = [[0, 1, 2, 3, 4, 5], [11, 10, 9, 8, 7, 6],
              [0, 6, 7, 1], [1, 7, 8, 2], [2, 8, 9, 3], [3, 9, 10, 4], [4, 10, 11, 5], [5, 11, 6, 0]]);
