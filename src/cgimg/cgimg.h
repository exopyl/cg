#pragma once

#include "image.h"

// Traitements, sortis d'Img et regroupes par domaine (cf. le commentaire en tete
// de class Img dans image.h). Inclure cgimg.h donne acces a tout ; le code qui
// n'a besoin que d'une famille peut viser directement son en-tete.
#include "image_io.h"
#include "image_filter.h"
#include "image_binarization.h"
#include "image_drawing.h"
#include "image_quantization.h"
#include "image_geodesic.h"
#include "image_histogram.h"
#include "image_test_pattern.h"

#include "disparity.h"
#include "image_disparity_birchfield.h"
