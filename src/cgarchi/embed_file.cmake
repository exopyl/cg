# Ecrit un fichier source C++ qui embarque le contenu binaire d'un fichier.
#
#   cmake -DINPUT=<fichier> -DOUTPUT=<source.cpp> -DNAMESPACE=<ns>
#         -DFUNCTION=<nom> -DHEADER=<en-tete a inclure> -P embed_file.cmake
#
# La source definit `std::string_view <NAMESPACE>::<FUNCTION> ()`, qui rend les
# octets du fichier tels quels. Un tableau d'octets plutot qu'un litteral de
# chaine : MSVC refuse un litteral de plus de 64 Ko (C2026).

foreach(var INPUT OUTPUT NAMESPACE FUNCTION HEADER)
    if(NOT DEFINED ${var})
        message(FATAL_ERROR "embed_file.cmake : ${var} manquant")
    endif()
endforeach()

file(READ "${INPUT}" hex HEX)
string(LENGTH "${hex}" hexLength)
math(EXPR byteCount "${hexLength} / 2")

# 16 octets par ligne. Les regex de CMake n'ont pas de quantificateur {n} :
# le motif d'une ligne est repete explicitement.
string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," bytes "${hex}")
string(REPEAT "0x[0-9a-f][0-9a-f]," 16 line)
string(REGEX REPLACE "(${line})" "\\1\n" bytes "${bytes}")

file(WRITE "${OUTPUT}.tmp"
"// Genere par embed_file.cmake depuis ${INPUT}. Ne pas editer.
#include <${HEADER}>

namespace ${NAMESPACE}
{

namespace
{

const unsigned char kBytes[${byteCount} + 1] = {
${bytes}
0x00 };

} // namespace

std::string_view ${FUNCTION} ()
{
	return std::string_view (reinterpret_cast<const char *> (kBytes), ${byteCount});
}

} // namespace ${NAMESPACE}
")

# Reecrit seulement si le contenu change : la source generee ne se recompile
# pas a chaque build.
configure_file("${OUTPUT}.tmp" "${OUTPUT}" COPYONLY)
file(REMOVE "${OUTPUT}.tmp")
