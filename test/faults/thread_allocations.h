#pragma once
//
//  Compteur et injection d'echec sur les allocations du THREAD APPELANT.
//
// Definis dans tu_cgarchi_gothic_print_faults.cpp, qui remplace l'operator new
// global de TU_faults ; les autres threads ne sont jamais comptes ni touches.
//
#include <cstddef>
#include <functional>

// Allocations du thread appelant pendant f ().
long CountThreadAllocations (const std::function<void ()> &f);

// La countdown-ieme allocation suivante du thread appelant leve std::bad_alloc,
// une fois. DisarmThreadAllocationFailure rend vrai si elle a eu lieu.
void ArmThreadAllocationFailure (long countdown);
bool DisarmThreadAllocationFailure ();

// Pic des octets demandes et des blocs vivants du thread appelant pendant f (), puis ce
// qui en reste vivant a la fin de f (une fuite). Ne compte que les blocs alloues pendant
// f () : l'entree, construite avant, n'y est pas.
struct ThreadAllocationPeak
{
    std::size_t bytes = 0;
    std::size_t blocks = 0;
    std::size_t liveBytes = 0;
    std::size_t liveBlocks = 0;
};
ThreadAllocationPeak MeasureThreadAllocationPeak (const std::function<void ()> &f);

// Vrai sous une bibliotheque standard de debogage, ou des operations noexcept
// allouent : un echec injecte y finirait en std::terminate.
bool DebugStandardLibrary ();
