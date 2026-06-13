# Staging script for the `dist` target — runs in cmake -P (script) mode.
#
# Invoked as:
#   cmake -DSRC_DIR=<deployed build dir> -DDIST_DIR=<output dir>
#         -DMSVC_RUNTIME_LIBS=<;-list of runtime DLLs> -P MakeDist.cmake
#
# SRC_DIR is the sulina build output AFTER windeployqt has run (so it
# already holds the Qt DLLs / plugins / QML modules). We copy that tree into
# a clean DIST_DIR, drop the dev-only link/debug artifacts, and add the MSVC
# runtime DLLs so the bundle runs on a machine without the VC++ redistributable.

if(NOT SRC_DIR OR NOT DIST_DIR)
    message(FATAL_ERROR "MakeDist: SRC_DIR and DIST_DIR must both be set")
endif()

message(STATUS "dist: clearing ${DIST_DIR}")
file(REMOVE_RECURSE "${DIST_DIR}")
file(MAKE_DIRECTORY "${DIST_DIR}")

# Copy the full deployed tree minus the artifacts a redistributable never needs.
message(STATUS "dist: copying deployed runtime from ${SRC_DIR}")
file(COPY "${SRC_DIR}/" DESTINATION "${DIST_DIR}"
     PATTERN "*.lib" EXCLUDE
     PATTERN "*.exp" EXCLUDE
     PATTERN "*.pdb" EXCLUDE
     PATTERN "*.ilk" EXCLUDE)

# Bundle the MSVC runtime DLLs (vcruntime140.dll, msvcp140.dll, …) next to the
# exe so end users don't have to install vc_redist.x64.exe first.
foreach(_lib ${MSVC_RUNTIME_LIBS})
    if(EXISTS "${_lib}")
        file(COPY "${_lib}" DESTINATION "${DIST_DIR}")
    endif()
endforeach()

message(STATUS "dist: bundle ready at ${DIST_DIR}")

# Zip it up — <parent>/<name>.zip with a top-level <name>/ folder inside, so
# unzipping yields a self-contained sulina/ directory. Run cmake -E tar from
# the parent dir so the archived paths stay relative (no absolute path leakage).
get_filename_component(_dist_parent "${DIST_DIR}" DIRECTORY)
get_filename_component(_dist_name "${DIST_DIR}" NAME)
set(_zip "${_dist_parent}/${_dist_name}.zip")
file(REMOVE "${_zip}")
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E tar cf "${_zip}" --format=zip "${_dist_name}"
    WORKING_DIRECTORY "${_dist_parent}"
    RESULT_VARIABLE _tar_rc)
if(NOT _tar_rc EQUAL 0)
    message(FATAL_ERROR "dist: zip creation failed (exit ${_tar_rc})")
endif()
message(STATUS "dist: archive ready at ${_zip}")
