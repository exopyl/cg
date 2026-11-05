# Inclus par CTest apres les tests decouverts de TU (TEST_INCLUDE_FILES, test/CMakeLists.txt) :
# label `slow` sur les tests longs, que `ctest -LE slow` ecarte. TU_TESTS est la liste
# des tests decouverts (variable TEST_LIST de gtest_discover_tests).
# CTest lit ce fichier sans politique fixee : IN_LIST (CMP0057) y est refuse sous CMake 3.x,
# d'ou list(FIND).
foreach (name
    TEST_cgmesh_implicit_surface.cache_and_direct_evaluation_agree_beyond_float_indices
    TEST_cgmesh_implicit_surface.cache_and_direct_evaluation_agree_over_several_layers
    TEST_cgarchi_classical_orders.every_configuration_lathes_into_a_closed_valid_solid_at_the_default_resolution
    TEST_cgarchi_classical_flutes.every_fluted_configuration_lathes_into_a_valid_solid_at_the_default_resolution)
  list (FIND TU_TESTS "${name}" index)
  if (NOT index EQUAL -1)
    set_tests_properties ("${name}" PROPERTIES LABELS slow)
  endif ()
endforeach ()
