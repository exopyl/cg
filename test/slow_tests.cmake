# Inclus par CTest apres les tests decouverts de TU (TEST_INCLUDE_FILES, test/CMakeLists.txt) :
# label `slow` sur les tests longs, que `ctest -LE slow` ecarte (presets ci-linux-asan et
# ci-windows-debug), et TIMEOUT explicite sur les tests du temple raffine. TU_TESTS est la
# liste des tests decouverts (variable TEST_LIST de gtest_discover_tests).
# CTest lit ce fichier sans politique fixee : IN_LIST (CMP0057) y est refuse sous CMake 3.x,
# d'ou list(FIND).
foreach (name
    TEST_cgmesh_implicit_surface.cache_and_direct_evaluation_agree_beyond_float_indices
    TEST_cgmesh_implicit_surface.cache_and_direct_evaluation_agree_over_several_layers
    TEST_cgarchi_classical_orders.every_configuration_lathes_into_a_closed_valid_solid_at_the_default_resolution
    TEST_cgarchi_classical_flutes.every_fluted_configuration_lathes_into_a_valid_solid_at_the_default_resolution
    TEST_cgarchi_classical_temple.refined_stations_always_find_a_place_across_the_sweep
    TEST_cgarchi_classical_temple.refined_temple_has_no_near_vertex_pairs_down_to_the_lower_bound)
  list (FIND TU_TESTS "${name}" index)
  if (NOT index EQUAL -1)
    set_tests_properties ("${name}" PROPERTIES LABELS slow)
  endif ()
endforeach ()

# Temple raffine (CT7, CT8, parties rapide et lente) : 1 800 s, au-dela du defaut de CTest
# (1 500 s) pour les jobs Debug et ASan qui les executent.
foreach (name
    TEST_cgarchi_classical_temple.refined_stations_always_find_a_place_or_are_refused_by_name
    TEST_cgarchi_classical_temple.refined_stations_always_find_a_place_across_the_sweep
    TEST_cgarchi_classical_temple.refined_temple_has_no_near_vertex_pairs_at_the_known_breaking_points
    TEST_cgarchi_classical_temple.refined_temple_has_no_near_vertex_pairs_down_to_the_lower_bound)
  list (FIND TU_TESTS "${name}" index)
  if (NOT index EQUAL -1)
    set_tests_properties ("${name}" PROPERTIES TIMEOUT 1800)
  endif ()
endforeach ()
